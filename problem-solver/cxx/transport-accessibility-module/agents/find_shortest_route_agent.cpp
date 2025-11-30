#include "find_shortest_route_agent.hpp"

#include <sc-memory/sc_memory.hpp>
#include <sc-memory/sc_agent.hpp>
#include <sc-memory/sc_iterator.hpp>
#include <sc-memory/sc_link.hpp>

#include "keynodes/transport_accessibility_keynodes.hpp"
#include "utils/Graph.hpp"
#include "utils/BuildGraphFromSc.hpp"

#include <vector>

ScAddr FindShortestRouteAgent::GetActionClass() const
{
  // Класс действия должен быть описан в keynodes/scs:
  // action_find_shortest_route <- concept_action;;
  return TransportAccessibilityKeynodes::action_find_shortest_route;
}

ScResult FindShortestRouteAgent::DoProgram(ScAction & action)
{
  m_logger.Info("FindShortestRouteAgent started");

  // 1. Аргумент действия — граф транспортной сети
  auto const & [graphAddr] = action.GetArguments<1>();

  if (!m_context.IsElement(graphAddr))
  {
    m_logger.Error("Graph node not found");
    return action.FinishWithError();
  }

  // 2. Строим C++ граф и список районов
  GraphFromScResult gr = BuildGraphFromSc(m_context, graphAddr);
  Graph & g = gr.graph;
  std::vector<ScAddr> const & districts = gr.districts;

  int n = g.n;
  if (n == 0)
  {
    m_logger.Warning("Graph has no districts");
    ScStructure result = m_context.GenerateStructure();
    action.SetResult(result);
    return action.FinishSuccessfully();
  }

  // 3. Матрица кратчайших расстояний (Floyd–Warshall)
  int const INF = 1000000000;
  std::vector<std::vector<int>> dist = g.FloydWarshall(INF);

  // 4. Формируем SC-структуру результата
  ScStructure result = m_context.GenerateStructure();

  // Узел–структура для всех кратчайших расстояний
  ScAddr shortestTableNode = m_context.GenerateNode(ScType::ConstNodeStructure);
  result << shortestTableNode;

  // Для каждой пары (i,j), i < j, создаём описатель кратчайшего пути:
  //
  // shortest_pair_ij
  //   -> rrel_start_district: district_i;
  //   -> rrel_end_district:   district_j;
  //   => nrel_shortest_distance: distanceLink;
  //
  for (int i = 0; i < n; ++i)
  {
    for (int j = i + 1; j < n; ++j)
    {
      if (dist[i][j] == INF)
      {
        // нет пути между районами — можно пропустить
        continue;
      }

      ScAddr pairNode = m_context.GenerateNode(ScType::ConstNodeStructure);
      result << pairNode;

      // start_district
      ScAddr arcStart = m_context.GenerateConnector(
          ScType::ConstCommonArc,
          pairNode,
          districts[i]);

      ScAddr arcStartRole = m_context.GenerateConnector(
          ScType::ConstPermPosArc,
          TransportAccessibilityKeynodes::rrel_start_district,
          arcStart);

      // end_district
      ScAddr arcEnd = m_context.GenerateConnector(
          ScType::ConstCommonArc,
          pairNode,
          districts[j]);

      ScAddr arcEndRole = m_context.GenerateConnector(
          ScType::ConstPermPosArc,
          TransportAccessibilityKeynodes::rrel_end_district,
          arcEnd);

      // link с расстоянием
      ScAddr distanceLink = m_context.GenerateLink();
      m_context.SetLinkContent(distanceLink, dist[i][j]);

      ScAddr arcCommonDist = m_context.GenerateConnector(
          ScType::ConstCommonArc,
          pairNode,
          distanceLink);

      ScAddr arcRelDist = m_context.GenerateConnector(
          ScType::ConstPermPosArc,
          TransportAccessibilityKeynodes::nrel_shortest_distance,
          arcCommonDist);

      // включаем pairNode в общую структуру
      ScAddr arcToTable = m_context.GenerateConnector(
          ScType::ConstPermPosArc,
          shortestTableNode,
          pairNode);

      result << arcStart << arcStartRole
             << arcEnd << arcEndRole
             << distanceLink << arcCommonDist << arcRelDist
             << arcToTable;
    }
  }

  // 5. Возвращаем структуру
  action.SetResult(result);
  m_logger.Info("FindShortestRouteAgent finished successfully");
  return action.FinishSuccessfully();
}
