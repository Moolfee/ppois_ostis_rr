#include "check_graph_connectivity_agent.hpp"

#include <sc-memory/sc_memory.hpp>
#include <sc-memory/sc_agent.hpp>
#include <sc-memory/sc_iterator.hpp>
#include <sc-memory/sc_link.hpp>

#include "keynodes/transport_accessibility_keynodes.hpp"

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

  // --- 1. Получение аргумента действия (узла графа) ---
  auto const & [graphAddr] = action.GetArguments<1>();

  if (!m_context.IsElement(graphAddr))
  {
    m_logger.Error("Graph node is not found");
    return action.FinishWithError();
  }

  // --- 2. Собираем список районов и маршрутов, которые входят в граф ---
  std::vector<ScAddr> districts;
  std::vector<ScAddr> routes;

  {
    ScIterator3Ptr it = m_context.CreateIterator3(
        graphAddr,
        ScType::ConstPermPosArc,
        ScType::ConstNode);

    while (it->Next())
    {
      ScAddr elem = it->Get(2);

      // район?
      if (m_context.CheckConnector(
              TransportAccessibilityKeynodes::concept_district,
              elem,
              ScType::ConstPermPosArc))
      {
        districts.push_back(elem);
        continue;
      }

      // маршрут?
      if (m_context.CheckConnector(
              TransportAccessibilityKeynodes::concept_public_transport_route,
              elem,
              ScType::ConstPermPosArc))
      {
        routes.push_back(elem);
        continue;
      }
    }
  }

  if (districts.empty())
  {
    m_logger.Error("No districts found inside graph structure");
    return action.FinishWithError();
  }

  std::map<ScAddr, int, ScAddrLessFunc> districtIndex;
  for (int i = 0; i < (int)districts.size(); i++)
    districtIndex[districts[i]] = i;

  int n = districts.size();

  std::vector<std::vector<int>> adj(n);
  struct RouteEdge
  {
    ScAddr route;
    ScAddr d1;
    ScAddr d2;
  };
  std::vector<RouteEdge> edges;

  for (ScAddr const & route : routes)
  {
    ScAddr connectsSet;

    {
      ScIterator5Ptr it = m_context.CreateIterator5(
          route,
          ScType::ConstCommonArc,
          ScType::ConstNode,
          ScType::ConstPermPosArc,
          TransportAccessibilityKeynodes::nrel_connects_districts);

      if (!it->Next())
        continue; 

      connectsSet = it->Get(2);
    }

    std::vector<ScAddr> pairDistricts;

    {
      ScIterator3Ptr it = m_context.CreateIterator3(
          connectsSet,
          ScType::ConstPermPosArc,
          ScType::ConstNode);

      while (it->Next())
      {
        pairDistricts.push_back(it->Get(2));
      }
    }

    if (pairDistricts.size() != 2)
    {
      m_logger.Warning("Route has invalid number of connected districts (expected 2)");
      continue;
    }

    ScAddr d1 = pairDistricts[0];
    ScAddr d2 = pairDistricts[1];

    if (!districtIndex.count(d1) || !districtIndex.count(d2))
      continue; 
    int u = districtIndex[d1];
    int v = districtIndex[d2];

    adj[u].push_back(v);
    adj[v].push_back(u);

    edges.push_back(RouteEdge{route, d1, d2});
  }

  std::vector<bool> visited(n, false);

  // Точка старта DFS — первый найденный район в графе
  int const startIndex = 0;
  std::stack<int> st;
  st.push(startIndex);
  visited[startIndex] = true;

  while (!st.empty())
  {
    int v = st.top();
    st.pop();

    for (int to : adj[v])
    {
      if (!visited[to])
      {
        visited[to] = true;
        st.push(to);
      }
    }
  }

  std::vector<ScAddr> unreachable;

  for (int i = 0; i < n; i++)
  {
    if (!visited[i])
      unreachable.push_back(districts[i]);
  }

  bool isConnected = unreachable.empty();

  ScStructure result = m_context.GenerateStructure();

  // Помечаем стартовую вершину обхода
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

  // Фиксируем факт связности: graph --nrel_graph_connectivity--> link(true/false)
  ScAddr connectivityLink = m_context.GenerateLink();
  m_context.SetLinkContent(connectivityLink, isConnected ? "true" : "false");

  ScAddr connectivityArc = m_context.GenerateConnector(
      ScType::ConstCommonArc,
      graphAddr,
      connectivityLink);

  ScAddr connectivityRel = m_context.GenerateConnector(
      ScType::ConstPermPosArc,
      TransportAccessibilityKeynodes::nrel_graph_connectivity,
      connectivityArc);

  result << connectivityLink << connectivityArc << connectivityRel
         << TransportAccessibilityKeynodes::nrel_graph_connectivity;

  // Множество достижимых районов (и маршрутов между ними)
  ScAddr reachableSet = m_context.GenerateNode(ScType::ConstNodeStructure);
  result << reachableSet;
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

  // Если есть недостижимые районы — добавляем отдельное множество
  if (!isConnected && !unreachable.empty())
  {
    ScAddr unreachableSet = m_context.GenerateNode(ScType::ConstNodeStructure);
    result << unreachableSet;

    for (ScAddr const & d : unreachable)
    {
      ScAddr arc = m_context.GenerateConnector(
          ScType::ConstPermPosArc,
          unreachableSet,
          d);

      result << arc << d;
    }
  }

  action.SetResult(result);

  if (isConnected)
  {
    m_logger.Info("Graph is connected");
  }
  else
  {
    m_logger.Info("Graph is NOT connected. Unreachable districts found");
  }

  return action.FinishSuccessfully();
}
