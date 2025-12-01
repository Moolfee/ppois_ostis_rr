#include "calculate_network_diameter_agent.hpp"

#include <sc-memory/sc_memory.hpp>
#include <sc-memory/sc_agent.hpp>
#include <sc-memory/sc_iterator.hpp>
#include <sc-memory/sc_link.hpp>

#include "keynodes/transport_accessibility_keynodes.hpp"
#include "utils/Graph.hpp"
#include "utils/BuildGraphFromSc.hpp"

#include <vector>

ScAddr CalculateNetworkDiameterAgent::GetActionClass() const
{
  return TransportAccessibilityKeynodes::action_calculate_network_diameter;
}

ScResult CalculateNetworkDiameterAgent::DoProgram(ScAction & action)
{
  m_logger.Info("CalculateNetworkDiameterAgent started");

  // 1. Аргумент действия — граф транспортной сети
  auto const & [graphAddr] = action.GetArguments<1>();

  if (!m_context.IsElement(graphAddr))
  {
    m_logger.Error("Graph node not found");
    return action.FinishWithError();
  }

  // 2. Строим C++-граф из SC-памяти
  GraphFromScResult gr = BuildGraphFromSc(m_context, graphAddr);
  Graph & g = gr.graph;

  int n = g.n;
  if (n == 0)
  {
    m_logger.Warning("Graph has no districts, diameter is undefined");
    ScStructure emptyResult = m_context.GenerateStructure();
    action.SetResult(emptyResult);
    return action.FinishSuccessfully();
  }

  // 3. Считаем матрицу кратчайших расстояний (Floyd-Warshall)
  int const INF = 1000000000;
  std::vector<std::vector<int>> dist = g.FloydWarshall(INF);

  // 4. Вычисляем диаметр графа
  int diameter = g.Diameter(dist, INF);

  // 5. Формируем SC-структуру результата
  ScStructure result = m_context.GenerateStructure();

  // Узел-структура результата (контейнер, куда будем вешать rrel_diameter_value)
  ScAddr resultNode = m_context.GenerateNode(ScType::ConstNodeStructure);
  result << resultNode;

  // 5.1. Link с численным значением диаметра
  ScAddr diameterLink = m_context.GenerateLink();
  m_context.SetLinkContent(diameterLink, diameter);

  // graphAddr => nrel_network_diameter: diameterLink;;
  ScAddr arcCommon = m_context.GenerateConnector(
      ScType::ConstCommonArc,
      graphAddr,
      diameterLink);

  ScAddr arcRel = m_context.GenerateConnector(
      ScType::ConstPermPosArc,
      TransportAccessibilityKeynodes::nrel_network_diameter,
      arcCommon);

  // Включаем link в структуру результата с ролевым отношением rrel_diameter_value
  ScAddr arcToResult = m_context.GenerateConnector(
      ScType::ConstPermPosArc,
      resultNode,
      diameterLink);

  ScAddr arcRole = m_context.GenerateConnector(
      ScType::ConstPermPosArc,
      TransportAccessibilityKeynodes::rrel_diameter_value,
      arcToResult);

  result << diameterLink << arcCommon << arcRel << arcToResult << arcRole << graphAddr;

  // 6. Отдаём результат действию
  action.SetResult(result);

  m_logger.Info("CalculateNetworkDiameterAgent finished successfully");
  return action.FinishSuccessfully();
}
