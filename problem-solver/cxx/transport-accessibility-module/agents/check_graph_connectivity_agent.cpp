#include "check_graph_connectivity_agent.hpp"

#include <sc-memory/sc_memory.hpp>
#include <sc-memory/sc_agent.hpp>
#include <sc-memory/sc_iterator.hpp>
#include <sc-memory/sc_link.hpp>

#include "keynodes/transport_accessibility_keynodes.hpp"
#include "utils/BuildGraphFromSc.hpp"
#include "utils/Graph.hpp"

#include <vector>
#include <map>
#include <stack>

ScAddr CheckGraphConnectivityAgent::GetActionClass() const
{
  return TransportAccessibilityKeynodes::action_check_graph_connectivity;
}

ScResult CheckGraphConnectivityAgent::DoProgram(ScAction & action)
{
  m_logger.Info("CheckGraphConnectivityAgent started");

  // 1. Аргумент действия — узел графа (например, novosibirsk_transport_disconnected_graph)
  auto const & [graphAddr] = action.GetArguments<1>();

  if (!m_context.IsElement(graphAddr))
  {
    m_logger.Error("Graph node is not found");
    return action.FinishWithError();
  }

  // 2. Строим граф и список районов через BuildGraphFromSc
  GraphFromScResult gr = BuildGraphFromSc(m_context, graphAddr);
  Graph & g = gr.graph;
  std::vector<ScAddr> const & districts = gr.districts;

  int const n = g.n;

  if (n == 0)
  {
    m_logger.Error("BuildGraphFromSc: graph has no districts");
    return action.FinishWithError();
  }

  // Мапа район -> индекс
  std::map<ScAddr, int, ScAddrLessFunc> districtIndex;
  for (int i = 0; i < n; ++i)
    districtIndex[districts[i]] = i;

  // 3. Собираем маршруты и запоминаем, какие районы они соединяют
  struct RouteEdge
  {
    ScAddr route;
    ScAddr d1;
    ScAddr d2;
  };

  std::vector<RouteEdge> edges;

  {
    ScIterator3Ptr it = m_context.CreateIterator3(
        graphAddr,
        ScType::ConstPermPosArc,
        ScType::Node);

    while (it->Next())
    {
      ScAddr elem = it->Get(2);

      // elem <- concept_public_transport_route ?
      ScIterator3Ptr itRouteClassConst = m_context.CreateIterator3(
          TransportAccessibilityKeynodes::concept_public_transport_route,
          ScType::ConstPermPosArc,
          elem);

      ScIterator3Ptr itRouteClassVar = m_context.CreateIterator3(
          TransportAccessibilityKeynodes::concept_public_transport_route,
          ScType::VarPermPosArc,
          elem);

      if (!(itRouteClassConst->Next() || itRouteClassVar->Next()))
        continue;

      ScAddr const route = elem;

      std::vector<ScAddr> pairDistricts;

      // Вариант 1: множество districtSet
      {
        ScAddr districtSet;

        auto tryFindSet = [&](ScType arcRoleType) -> bool
        {
          ScIterator5Ptr itSet = m_context.CreateIterator5(
              route,
              ScType::ConstCommonArc,
              ScType::Node,
              arcRoleType,
              TransportAccessibilityKeynodes::nrel_connects_districts);

          if (itSet->Next())
          {
            districtSet = itSet->Get(2);
            return true;
          }
          return false;
        };

        if (tryFindSet(ScType::ConstPermPosArc) || tryFindSet(ScType::VarPermPosArc))
        {
          auto collectFromSet = [&](ScType arcType)
          {
            ScIterator3Ptr itInSet = m_context.CreateIterator3(
                districtSet,
                arcType,
                ScType::Node);

            while (itInSet->Next())
              pairDistricts.push_back(itInSet->Get(2));
          };

          collectFromSet(ScType::ConstPermPosArc);
          collectFromSet(ScType::VarPermPosArc);
        }
      }

      // Вариант 2: прямые дуги route => nrel_connects_districts: district;;
      auto collectDirect = [&](ScType arcCommonType, ScType arcRoleType)
      {
        ScIterator5Ptr itDirect = m_context.CreateIterator5(
            route,
            arcCommonType,
            ScType::Node,
            arcRoleType,
            TransportAccessibilityKeynodes::nrel_connects_districts);

        while (itDirect->Next())
        {
          ScAddr candidate = itDirect->Get(2);

          ScIterator3Ptr itDistrictClassConst = m_context.CreateIterator3(
              TransportAccessibilityKeynodes::concept_district,
              ScType::ConstPermPosArc,
              candidate);

          ScIterator3Ptr itDistrictClassVar = m_context.CreateIterator3(
              TransportAccessibilityKeynodes::concept_district,
              ScType::VarPermPosArc,
              candidate);

          if (itDistrictClassConst->Next() || itDistrictClassVar->Next())
            pairDistricts.push_back(candidate);
        }
      };

      collectDirect(ScType::ConstCommonArc, ScType::ConstPermPosArc);
      collectDirect(ScType::ConstCommonArc, ScType::VarPermPosArc);
      collectDirect(ScType::VarCommonArc,  ScType::ConstPermPosArc);
      collectDirect(ScType::VarCommonArc,  ScType::VarPermPosArc);

      if (pairDistricts.size() != 2)
      {
        m_logger.Warning("Route has invalid number of connected districts (expected 2)");
        continue;
      }

      ScAddr const d1 = pairDistricts[0];
      ScAddr const d2 = pairDistricts[1];

      if (!districtIndex.count(d1) || !districtIndex.count(d2))
        continue;

      edges.push_back(RouteEdge{route, d1, d2});
    }
  }

  // 4. DFS от стартового района (первый район в списке districts)
  std::vector<bool> visited(n, false);

  int const startIndex = 0;  // в твоём графе это сейчас Ленинский район
  std::stack<int> st;
  st.push(startIndex);
  visited[startIndex] = true;

  while (!st.empty())
  {
    int v = st.top();
    st.pop();

    for (int to : g.adj[v])
    {
      if (!visited[to])
      {
        visited[to] = true;
        st.push(to);
      }
    }
  }

  // 5. Считаем, какие районы недостижимы (только для логов)
  std::vector<ScAddr> unreachable;
  for (int i = 0; i < n; ++i)
  {
    if (!visited[i])
      unreachable.push_back(districts[i]);
  }

  bool const isConnected = unreachable.empty();

  if (isConnected)
    m_logger.Info("Graph is connected (all districts are reachable from the start district).");
  else
  {
    m_logger.Info("Graph is NOT connected. Unreachable districts:");
    for (ScAddr const & d : unreachable)
    {
      ScAddr nameLink;
      // можно здесь найти nrel_main_idtf и вывести в лог, если захочешь
      (void)nameLink;
    }
  }

  // 6. Формируем структуру результата: ТОЛЬКО связная компонента + факт связности

  ScStructure result = m_context.GenerateStructure();

  // 6.1. Помечаем стартовый район
  {
    ScAddr startArc = m_context.GenerateConnector(
        ScType::ConstPermPosArc,
        result,
        districts[startIndex]);

    m_context.GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::rrel_start_district,
        startArc);
  }

  // 6.2. Факт связности graphAddr => nrel_graph_connectivity: <"true"/"false">;;
  ScAddr connectivityLink = m_context.GenerateLink(ScType::ConstNodeLink);
  m_context.SetLinkContent(connectivityLink, isConnected ? std::string("true") : std::string("false"));

  ScAddr connectivityArc = m_context.GenerateConnector(
      ScType::ConstCommonArc,
      graphAddr,
      connectivityLink);

  ScAddr connectivityRel = m_context.GenerateConnector(
      ScType::ConstPermPosArc,
      TransportAccessibilityKeynodes::nrel_graph_connectivity,
      connectivityArc);

  result << connectivityLink << connectivityArc << connectivityRel << graphAddr;

  // 6.3. Множество ДОСТИЖИМЫХ районов и маршрутов (компонента связности)
  ScAddr reachableSet = m_context.GenerateNode(ScType::ConstNodeStructure);
  result << reachableSet;

  // Районы
  for (int i = 0; i < n; ++i)
  {
    if (visited[i])
    {
      ScAddr arc = m_context.GenerateConnector(
          ScType::ConstPermPosArc,
          reachableSet,
          districts[i]);

      result << arc << districts[i];
    }
  }

  // Маршруты, которые соединяют только достижимые районы
  for (auto const & e : edges)
  {
    int u = districtIndex[e.d1];
    int v = districtIndex[e.d2];

    if (visited[u] && visited[v])
    {
      ScAddr arc = m_context.GenerateConnector(
          ScType::ConstPermPosArc,
          reachableSet,
          e.route);

      result << arc << e.route;
    }
  }

  // 7. Отдаём структуру результата действию
  action.SetResult(result);

  m_logger.Info("CheckGraphConnectivityAgent finished");
  return action.FinishSuccessfully();
}
