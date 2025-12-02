#include "analyze_transport_accessibility_agent.hpp"

#include <sc-memory/sc_memory.hpp>
#include <sc-memory/sc_agent.hpp>
#include <sc-memory/sc_iterator.hpp>
#include <sc-memory/sc_link.hpp>

#include "keynodes/transport_accessibility_keynodes.hpp"
#include "utils/Graph.hpp"
#include "utils/BuildGraphFromSc.hpp"

#include <map>
#include <vector>

ScAddr AnalyzeTransportAccessibilityAgent::GetActionClass() const
{
  return TransportAccessibilityKeynodes::action_analyze_transport_accessibility;
}

ScResult AnalyzeTransportAccessibilityAgent::DoProgram(ScAction & action)
{
  m_logger.Info("AnalyzeTransportAccessibilityAgent started");

  // 1. Аргумент — граф транспортной сети
  auto const & [graphAddr] = action.GetArguments<1>();

  if (!m_context.IsElement(graphAddr))
  {
    m_logger.Error("Graph node not found");
    return action.FinishWithError();
  }

  // 2. Строим C++ Graph
  GraphFromScResult gr = BuildGraphFromSc(m_context, graphAddr);
  Graph & g = gr.graph;
  std::vector<ScAddr> const & districts = gr.districts;

  int n = g.n;
  if (n == 0)
  {
    m_logger.Warning("Graph has no districts");
    ScStructure emptyResult = m_context.GenerateStructure();
    action.SetResult(emptyResult);
    return action.FinishSuccessfully();
  }

  int const INF = 1000000000;

  // 3. Проверяем связность (DFS от 0)
  std::vector<bool> visited = g.DfsFrom(0);
  bool isConnected = true;
  for (bool v : visited)
  {
    if (!v)
    {
      isConnected = false;
      break;
    }
  }

  // 4. Считаем матрицу расстояний
  std::vector<std::vector<int>> dist = g.FloydWarshall(INF);

  std::map<ScAddr, int, ScAddrLessFunc> index;
  for (int i = 0; i < n; ++i)
    index[districts[i]] = i;

  // 5. Центральный район (только если связен)
  int centralIndex = isConnected ? g.FindCentralVertex(dist, INF) : -1;
  ScAddr centralDistrict = ScAddr::Empty;
  if (centralIndex >= 0 && centralIndex < n)
    centralDistrict = districts[centralIndex];

  // 6. Диаметр
  int diameter = g.Diameter(dist, INF);
  bool hasInf = false;
  for (int i = 0; i < n && !hasInf; ++i)
  {
    for (int j = 0; j < n; ++j)
    {
      if (dist[i][j] == INF)
      {
        hasInf = true;
        break;
      }
    }
  }

  // 7. Мосты (количество)
  std::vector<std::pair<int,int>> bridges = g.FindBridges();
  int bridgesCount = static_cast<int>(bridges.size());

  // 7.1 Собираем маршруты и карту ребро -> маршруты (как в find_bridge_routes_agent)
  std::vector<ScAddr> routes;
  auto collectRoutes = [&](ScType arcType)
  {
    ScIterator3Ptr it = m_context.CreateIterator3(
        graphAddr,
        arcType,
        ScType::Node);

    while (it->Next())
    {
      ScAddr elem = it->Get(2);

      ScIterator3Ptr itRouteClassConst = m_context.CreateIterator3(
          TransportAccessibilityKeynodes::concept_public_transport_route,
          ScType::ConstPermPosArc,
          elem);
      ScIterator3Ptr itRouteClassVar = m_context.CreateIterator3(
          TransportAccessibilityKeynodes::concept_public_transport_route,
          ScType::VarPermPosArc,
          elem);

      if (itRouteClassConst->Next() || itRouteClassVar->Next())
        routes.push_back(elem);
    }
  };
  collectRoutes(ScType::ConstPermPosArc);
  collectRoutes(ScType::VarPermPosArc);

  std::map<std::pair<int,int>, std::vector<ScAddr>> edgeToRoutes;

  for (ScAddr const & route : routes)
  {
    std::vector<ScAddr> pair;

    {
      ScAddr districtSet;
      auto tryFindSet = [&](ScType arcTypeRole) -> bool
      {
        ScIterator5Ptr it = m_context.CreateIterator5(
            route,
            ScType::ConstCommonArc,
            ScType::Node,
            arcTypeRole,
            TransportAccessibilityKeynodes::nrel_connects_districts);
        if (it->Next())
        {
          districtSet = it->Get(2);
          return true;
        }
        return false;
      };

      if (tryFindSet(ScType::ConstPermPosArc) || tryFindSet(ScType::VarPermPosArc))
      {
        auto collectFromSet = [&](ScType arcType)
        {
          ScIterator3Ptr it = m_context.CreateIterator3(
              districtSet,
              arcType,
              ScType::Node);
          while (it->Next())
            pair.push_back(it->Get(2));
        };
        collectFromSet(ScType::ConstPermPosArc);
        collectFromSet(ScType::VarPermPosArc);
      }
    }

    auto collectDirect = [&](ScType arcCommonType, ScType arcRoleType)
    {
      ScIterator5Ptr it = m_context.CreateIterator5(
          route,
          arcCommonType,
          ScType::Node,
          arcRoleType,
          TransportAccessibilityKeynodes::nrel_connects_districts);
      while (it->Next())
      {
        ScAddr cand = it->Get(2);
        ScIterator3Ptr itDistrictClassConst = m_context.CreateIterator3(
            TransportAccessibilityKeynodes::concept_district,
            ScType::ConstPermPosArc,
            cand);
        ScIterator3Ptr itDistrictClassVar = m_context.CreateIterator3(
            TransportAccessibilityKeynodes::concept_district,
            ScType::VarPermPosArc,
            cand);
        if (itDistrictClassConst->Next() || itDistrictClassVar->Next())
          pair.push_back(cand);
      }
    };
    collectDirect(ScType::ConstCommonArc, ScType::ConstPermPosArc);
    collectDirect(ScType::ConstCommonArc, ScType::VarPermPosArc);
    collectDirect(ScType::VarCommonArc, ScType::ConstPermPosArc);
    collectDirect(ScType::VarCommonArc, ScType::VarPermPosArc);

    // удаляем дубликаты, оставляем только пары
    {
      std::set<ScAddr, ScAddrLessFunc> uniq;
      std::vector<ScAddr> filtered;
      for (ScAddr const & d : pair)
      {
        if (uniq.insert(d).second)
          filtered.push_back(d);
      }
      pair.swap(filtered);
    }

    if (pair.size() != 2)
      continue;

    auto it1 = index.find(pair[0]);
    auto it2 = index.find(pair[1]);
    if (it1 == index.end() || it2 == index.end())
      continue;

    int u = it1->second;
    int v = it2->second;
    if (u > v) std::swap(u, v);

    edgeToRoutes[{u, v}].push_back(route);
  }

  // 8. Формируем общую SC-структуру результата
  ScStructure result = m_context.GenerateStructure();

  ScAddr analysisNode = m_context.GenerateNode(ScType::ConstNodeStructure);
  result << analysisNode << graphAddr;

  // 8.1. Связность
  {
    ScAddr connectivityLink = m_context.GenerateLink();
    m_context.SetLinkContent(connectivityLink, isConnected ? std::string("true") : std::string("false"));

    ScAddr arcCommon = m_context.GenerateConnector(
        ScType::ConstCommonArc,
        graphAddr,
        connectivityLink);

    ScAddr arcRel = m_context.GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_graph_connectivity,
        arcCommon);

    result << connectivityLink << arcCommon << arcRel;
  }

  // 8.2. Центральный район (если удалось определить)
  if (centralDistrict.IsValid())
  {
    ScAddr arcCommon = m_context.GenerateConnector(
        ScType::ConstCommonArc,
        graphAddr,
        centralDistrict);

    ScAddr arcRel = m_context.GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_is_it_central_district,
        arcCommon);

    result << centralDistrict << arcCommon << arcRel;
  }

  // 8.3. Диаметр
  {
    ScAddr diameterLink = m_context.GenerateLink();
    m_context.SetLinkContent(
        diameterLink,
        hasInf ? std::string("undefined (disconnected graph)") : std::to_string(diameter));

    ScAddr arcCommon = m_context.GenerateConnector(
        ScType::ConstCommonArc,
        graphAddr,
        diameterLink);

    ScAddr arcRel = m_context.GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_network_diameter,
        arcCommon);

    // также привяжем к analysisNode через rrel_diameter_value
    ScAddr arcToAnalysis = m_context.GenerateConnector(
        ScType::ConstPermPosArc,
        analysisNode,
        diameterLink);

    ScAddr arcRole = m_context.GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::rrel_diameter_value,
        arcToAnalysis);

    result << diameterLink << arcCommon << arcRel << arcToAnalysis << arcRole;
  }

  // 8.4. Количество мостов (сохраним в link)
  {
    ScAddr bridgesCountLink = m_context.GenerateLink();
    m_context.SetLinkContent(bridgesCountLink, std::to_string(bridgesCount));

    ScAddr arcCommon = m_context.GenerateConnector(
        ScType::ConstCommonArc,
        graphAddr,
        bridgesCountLink);

    ScAddr arcRel = m_context.GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_bridge_routes_count,
        arcCommon);  // это отношение тебе нужно завести в KB

    result << bridgesCountLink << arcCommon << arcRel;
  }

  // 8.5. Перечень мостовых маршрутов (как в find_bridge_routes_agent)
  if (!bridges.empty())
  {
    ScAddr bridgeRoutesSet = m_context.GenerateNode(ScType::ConstNodeStructure);
    result << bridgeRoutesSet;

    for (auto const & e : bridges)
    {
      int u = e.first;
      int v = e.second;
      if (u > v) std::swap(u, v);

      auto it = edgeToRoutes.find({u, v});
      if (it == edgeToRoutes.end())
        continue;

      for (ScAddr const & route : it->second)
      {
        ScAddr arcCommon = m_context.GenerateConnector(
            ScType::ConstCommonArc,
            route,
            graphAddr);

        ScAddr arcRel = m_context.GenerateConnector(
            ScType::ConstPermPosArc,
            TransportAccessibilityKeynodes::nrel_is_it_bridge_connection,
            arcCommon);

        ScAddr arcToSet = m_context.GenerateConnector(
            ScType::ConstPermPosArc,
            bridgeRoutesSet,
            route);

        ScAddr arcRole = m_context.GenerateConnector(
            ScType::ConstPermPosArc,
            TransportAccessibilityKeynodes::rrel_bridge_route,
            arcToSet);

        result << arcCommon << arcRel << arcToSet << arcRole << route;
      }
    }
  }

  action.SetResult(result);
  m_logger.Info("AnalyzeTransportAccessibilityAgent finished successfully");
  return action.FinishSuccessfully();
}
