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

  {
    ScIterator3Ptr it = m_context.CreateIterator3(
        graphAddr,
        ScType::ConstPermPosArc,
        ScType::ConstNode);

    while (it->Next())
    {
      ScAddr elem = it->Get(2);

      ScIterator3Ptr itRouteClass = m_context.CreateIterator3(
          TransportAccessibilityKeynodes::concept_public_transport_route,
          ScType::ConstPermPosArc,
          elem);

      if (itRouteClass->Next())
        routes.push_back(elem);
    }
  }

  // 5. Строим карту (u,v) -> маршруты
  std::map<std::pair<int,int>, std::vector<ScAddr>> edgeToRoutes;

  std::map<ScAddr, int, ScAddrLessFunc> index;
  for (int i = 0; i < n; ++i) index[districts[i]] = i;

  for (ScAddr const & route : routes)
  {
    ScAddr districtSet;

    {
      ScIterator5Ptr it = m_context.CreateIterator5(
          route,
          ScType::ConstCommonArc,
          ScType::ConstNode,
          ScType::ConstPermPosArc,
          TransportAccessibilityKeynodes::nrel_connects_districts);

      if (!it->Next()) continue;
      districtSet = it->Get(2);
    }

    std::vector<ScAddr> pair;

    {
      ScIterator3Ptr it = m_context.CreateIterator3(
          districtSet,
          ScType::ConstPermPosArc,
          ScType::ConstNode);

      while (it->Next())
        pair.push_back(it->Get(2));
    }

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
