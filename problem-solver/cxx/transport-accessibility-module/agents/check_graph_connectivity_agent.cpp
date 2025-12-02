#include "check_graph_connectivity_agent.hpp"

#include <sc-memory/sc_memory.hpp>
#include <sc-memory/sc_agent.hpp>
#include <sc-memory/sc_iterator.hpp>
#include <sc-memory/sc_link.hpp>

#include "keynodes/transport_accessibility_keynodes.hpp"
#include "utils/BuildGraphFromSc.hpp"

#include <vector>

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

  // --- 2. Собираем граф через общий BuildGraphFromSc (учитывает const/var дуги и прямые связи)
  GraphFromScResult gr = BuildGraphFromSc(m_context, graphAddr);
  Graph & g = gr.graph;
  std::vector<ScAddr> const & districts = gr.districts;

  int n = g.n;
  if (n == 0)
  {
    m_logger.Error("No districts found inside graph structure");
    return action.FinishWithError();
  }

  // --- 3. DFS от 0-й вершины
  std::vector<bool> visited = g.DfsFrom(0);

  std::vector<ScAddr> unreachable;

  for (int i = 0; i < n; i++)
  {
    if (!visited[i])
      unreachable.push_back(districts[i]);
  }

  bool isConnected = unreachable.empty();

  ScStructure result = m_context.GenerateStructure();

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

  // Если есть недостижимые районы — возвращаем отдельное множество
  if (!isConnected)
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
