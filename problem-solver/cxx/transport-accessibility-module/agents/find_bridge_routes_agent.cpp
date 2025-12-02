#include "find_bridge_routes_agent.hpp"

#include <sc-memory/sc_memory.hpp>
#include <sc-memory/sc_agent.hpp>
#include <sc-memory/sc_iterator.hpp>
#include <sc-memory/sc_link.hpp>

#include "keynodes/transport_accessibility_keynodes.hpp"
#include "utils/Graph.hpp"
#include "utils/BuildGraphFromSc.hpp"

#include <vector>
#include <map>
#include <utility>

ScAddr FindBridgeRoutesAgent::GetActionClass() const
{
  return TransportAccessibilityKeynodes::action_find_bridge_routes;
}

ScResult FindBridgeRoutesAgent::DoProgram(ScAction & action)
{
  m_logger.Info("FindBridgeRoutesAgent started");

  // 1. Получаем аргумент действия — граф транспортной сети
  auto const & [graphAddr] = action.GetArguments<1>();

  if (!m_context.IsElement(graphAddr))
  {
    m_logger.Error("Graph node not found");
    return action.FinishWithError();
  }

  // 2. Строим C++-граф и список районов
  GraphFromScResult res = BuildGraphFromSc(m_context, graphAddr);
  Graph & g = res.graph;
  auto const & districts = res.districts;

  int n = g.n;
  if (n == 0)
  {
    ScStructure result = m_context.GenerateStructure();
    action.SetResult(result);
    return action.FinishSuccessfully();
  }

  // 3. Находим мостовые рёбра (по индексам)
  std::vector<std::pair<int, int>> bridges = g.FindBridges();

  if (bridges.empty())
  {
    m_logger.Info("No bridges found in graph");
    ScStructure result = m_context.GenerateStructure();
    action.SetResult(result);
    return action.FinishSuccessfully();
  }

  // 4. Собираем маршруты
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

  // 5. Строим карту (u,v) -> маршруты
  std::map<std::pair<int,int>, std::vector<ScAddr>> edgeToRoutes;

  std::map<ScAddr, int, ScAddrLessFunc> index;
  for (int i = 0; i < n; ++i) index[districts[i]] = i;

  for (ScAddr const & route : routes)
  {
    std::vector<ScAddr> pair;

    // через множество
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

    // прямые дуги
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

    if (pair.size() != 2) continue;

    ScAddr d1 = pair[0];
    ScAddr d2 = pair[1];

    if (!index.count(d1) || !index.count(d2)) continue;

    int u = index[d1];
    int v = index[d2];
    if (u > v) std::swap(u, v);

    edgeToRoutes[{u, v}].push_back(route);
  }

  // 6. Создаём структуру результата
  ScStructure result = m_context.GenerateStructure();

  ScAddr bridgeRoutesSet = m_context.GenerateNode(ScType::ConstNodeStructure);
  result << bridgeRoutesSet;

  // 7. Для каждого мостового ребра — помечаем соответствующие маршруты
  for (auto const & e : bridges)
  {
    int u = e.first;
    int v = e.second;
    if (u > v) std::swap(u, v);

    auto it = edgeToRoutes.find({u, v});
    if (it == edgeToRoutes.end()) continue;

    for (ScAddr const & route : it->second)
    {
      // Связь route -> graphAddr через nrel_is_it_bridge_connection

      ScAddr arcCommon = m_context.GenerateConnector(
          ScType::ConstCommonArc,
          route,
          graphAddr);

      ScAddr arcRel = m_context.GenerateConnector(
          ScType::ConstPermPosArc,
          TransportAccessibilityKeynodes::nrel_is_it_bridge_connection,
          arcCommon);

      // Добавляем маршрут в результирующее множество с ролью rrel_bridge_route
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

  action.SetResult(result);
  return action.FinishSuccessfully();
}
