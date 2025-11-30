#include "analyze_transport_accessibility_agent.hpp"

#include <sc-memory/sc_memory.hpp>
#include <sc-memory/sc_agent.hpp>
#include <sc-memory/sc_iterator.hpp>
#include <sc-memory/sc_link.hpp>

#include "keynodes/transport_accessibility_keynodes.hpp"
#include "utils/Graph.hpp"
#include "utils/BuildGraphFromSc.hpp"

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

  // 5. Центральный район
  int centralIndex = g.FindCentralVertex(dist, INF);
  ScAddr centralDistrict = ScAddr::Empty;
  if (centralIndex >= 0 && centralIndex < n)
    centralDistrict = districts[centralIndex];

  // 6. Диаметр
  int diameter = g.Diameter(dist, INF);

  // 7. Мосты (количество)
  std::vector<std::pair<int,int>> bridges = g.FindBridges();
  int bridgesCount = static_cast<int>(bridges.size());

  // 8. Формируем общую SC-структуру результата
  ScStructure result = m_context.GenerateStructure();

  ScAddr analysisNode = m_context.CreateNode(ScType::ConstNodeStruct);
  result << analysisNode << graphAddr;

  // 8.1. Связность
  {
    ScAddr connectivityLink = m_context.CreateLink();
    m_context.SetLinkContent(connectivityLink, isConnected ? 1 : 0);

    ScAddr arcCommon = m_context.CreateConnector(
        ScType::ConstCommonArc,
        graphAddr,
        connectivityLink);

    ScAddr arcRel = m_context.CreateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_graph_connectivity,
        arcCommon);

    result << connectivityLink << arcCommon << arcRel;
  }

  // 8.2. Центральный район (если удалось определить)
  if (centralDistrict.IsValid())
  {
    ScAddr arcCommon = m_context.CreateConnector(
        ScType::ConstCommonArc,
        graphAddr,
        centralDistrict);

    ScAddr arcRel = m_context.CreateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_is_it_central_district,
        arcCommon);

    result << centralDistrict << arcCommon << arcRel;
  }

  // 8.3. Диаметр
  {
    ScAddr diameterLink = m_context.CreateLink();
    m_context.SetLinkContent(diameterLink, diameter);

    ScAddr arcCommon = m_context.CreateConnector(
        ScType::ConstCommonArc,
        graphAddr,
        diameterLink);

    ScAddr arcRel = m_context.CreateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_network_diameter,
        arcCommon);

    // также привяжем к analysisNode через rrel_diameter_value
    ScAddr arcToAnalysis = m_context.CreateConnector(
        ScType::ConstPermPosArc,
        analysisNode,
        diameterLink);

    ScAddr arcRole = m_context.CreateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::rrel_diameter_value,
        arcToAnalysis);

    result << diameterLink << arcCommon << arcRel << arcToAnalysis << arcRole;
  }

  // 8.4. Количество мостов (сохраним в link)
  {
    ScAddr bridgesCountLink = m_context.CreateLink();
    m_context.SetLinkContent(bridgesCountLink, bridgesCount);

    ScAddr arcCommon = m_context.CreateConnector(
        ScType::ConstCommonArc,
        graphAddr,
        bridgesCountLink);

    ScAddr arcRel = m_context.CreateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_bridge_routes_count,
        arcCommon);  // это отношение тебе нужно завести в KB

    result << bridgesCountLink << arcCommon << arcRel;
  }

  action.SetResult(result);
  m_logger.Info("AnalyzeTransportAccessibilityAgent finished successfully");
  return action.FinishSuccessfully();
}
