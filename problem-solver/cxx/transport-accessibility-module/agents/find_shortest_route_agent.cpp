#include "find_shortest_route_agent.hpp"

#include <sc-memory/sc_memory.hpp>
#include <sc-memory/sc_agent.hpp>
#include <sc-memory/sc_iterator.hpp>
#include <sc-memory/sc_link.hpp>

#include "keynodes/transport_accessibility_keynodes.hpp"
#include "utils/Graph.hpp"
#include "utils/BuildGraphFromSc.hpp"

#include <vector>
#include <string>

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

  // Узел-контейнер результата (чтобы sc-web точно отобразил структуру)
  ScAddr resultNode = m_context.GenerateNode(ScType::ConstNodeStructure);
  result << resultNode;

  // Узел–структура для всех кратчайших расстояний
  ScAddr shortestTableNode = m_context.GenerateNode(ScType::ConstNodeStructure);
  result << shortestTableNode;
  // Привязываем таблицу к исходному графу
  ScAddr arcGraph = m_context.GenerateConnector(
      ScType::ConstCommonArc,
      graphAddr,
      shortestTableNode);
  ScAddr arcRelGraph = m_context.GenerateConnector(
      ScType::ConstPermPosArc,
      TransportAccessibilityKeynodes::nrel_shortest_distance,
      arcGraph);
  result << arcGraph << arcRelGraph;
  ScAddr arcTable = m_context.GenerateConnector(
      ScType::ConstPermPosArc,
      resultNode,
      shortestTableNode);
  result << arcTable;

  bool hasPairs = false;

  // Для каждой пары (i,j), i < j, создаём описатель кратчайшего пути
  for (int i = 0; i < n; ++i)
  {
    for (int j = i + 1; j < n; ++j)
    {
      bool reachable = dist[i][j] != INF;

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

      // link с расстоянием в человеко-читаемом виде "A<->B: N" или "A<->B: no path"
      std::string startId = m_context.GetElementSystemIdentifier(districts[i]);
      std::string endId = m_context.GetElementSystemIdentifier(districts[j]);
      if (startId.empty())
        startId = "district_" + std::to_string(i);
      if (endId.empty())
        endId = "district_" + std::to_string(j);

      std::string textValue;
      if (reachable)
        textValue = startId + "<->" + endId + ": " + std::to_string(dist[i][j]);
      else
        textValue = startId + "<->" + endId + ": no path";

      ScAddr distanceLink = m_context.GenerateLink();
      m_context.SetLinkContent(distanceLink, textValue);

      ScAddr arcCommonDist = m_context.GenerateConnector(
          ScType::ConstCommonArc,
          pairNode,
          distanceLink);

      ScAddr arcRelDist = m_context.GenerateConnector(
          ScType::ConstPermPosArc,
          TransportAccessibilityKeynodes::nrel_shortest_distance,
          arcCommonDist);

      hasPairs = true;

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

  if (!hasPairs)
  {
    ScAddr msgLink = m_context.GenerateLink();
    m_context.SetLinkContent(msgLink, std::string("no reachable district pairs (graph empty or disconnected)"));
    ScAddr arcMsg = m_context.GenerateConnector(ScType::ConstPermPosArc, resultNode, msgLink);
    result << msgLink << arcMsg << graphAddr;
  }

  // 5. Возвращаем структуру
  action.SetResult(result);
  m_logger.Info("FindShortestRouteAgent finished successfully");
  return action.FinishSuccessfully();
}
