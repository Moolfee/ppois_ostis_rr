#include "find_central_district_agent.hpp"

#include <sc-memory/sc_memory.hpp>
#include <sc-memory/sc_agent.hpp>
#include <sc-memory/sc_iterator.hpp>
#include <sc-memory/sc_link.hpp>

#include "keynodes/transport_accessibility_keynodes.hpp"
#include "utils/Graph.hpp"
#include "utils/BuildGraphFromSc.hpp"

#include <vector>

ScAddr FindCentralDistrictAgent::GetActionClass() const
{
  return TransportAccessibilityKeynodes::action_find_central_district;
}

ScResult FindCentralDistrictAgent::DoProgram(ScAction & action)
{
  m_logger.Info("FindCentralDistrictAgent started");

  // 1. Берём аргумент действия: узел графа transport network
  auto const & [graphAddr] = action.GetArguments<1>();

  if (!m_context.IsElement(graphAddr))
  {
    m_logger.Error("Graph node not found");
    return action.FinishWithError();
  }

  // 2. Строим C++-граф и список районов из SC-памяти
  GraphFromScResult gr = BuildGraphFromSc(m_context, graphAddr);
  Graph & g = gr.graph;
  std::vector<ScAddr> const & districts = gr.districts;

  int n = g.n;
  if (n == 0)
  {
    m_logger.Warning("No districts in graph");
    ScStructure emptyResult = m_context.GenerateStructure();
    action.SetResult(emptyResult);
    return action.FinishSuccessfully();
  }

  // 3. Считаем матрицу кратчайших расстояний
  int const INF = 1000000000;
  std::vector<std::vector<int>> dist = g.FloydWarshall(INF);

  // 4. Находим центральную вершину (минимальный эксцентриситет)
  int bestIndex = g.FindCentralVertex(dist, INF);

  if (bestIndex < 0 || bestIndex >= n)
  {
    m_logger.Warning("Central district could not be determined (graph is disconnected)");
    ScStructure emptyResult = m_context.GenerateStructure();
    action.SetResult(emptyResult);
    return action.FinishSuccessfully();
  }

  ScAddr centralDistrict = districts[bestIndex];

  // 5. Формируем SC-структуру результата
  ScStructure result = m_context.GenerateStructure();

  // Узел-структура результата (можно использовать как «контейнер»)
  ScAddr resultNode = m_context.CreateNode(ScType::ConstNodeStruct);
  result << resultNode;

  // 5.1. Факт: для данного графа центральный район = centralDistrict
  //
  // graphAddr => nrel_is_it_central_district: centralDistrict;;
  //
  ScAddr arcCommon = m_context.CreateConnector(
      ScType::ConstCommonArc,
      graphAddr,
      centralDistrict);

  ScAddr arcRel = m_context.CreateConnector(
      ScType::ConstPermPosArc,
      TransportAccessibilityKeynodes::nrel_is_it_central_district,
      arcCommon);

  result << graphAddr << centralDistrict << arcCommon << arcRel;

  // 5.2. Можно включить центральный район внутрь resultNode как элемент
  ScAddr arcToSet = m_context.CreateConnector(
      ScType::ConstPermPosArc,
      resultNode,
      centralDistrict);

  result << arcToSet;

  // 6. Отдаём результат действию
  action.SetResult(result);

  m_logger.Info("FindCentralDistrictAgent finished successfully");
  return action.FinishSuccessfully();
}
