#include "find_central_district_agent.hpp"

#include <sc-memory/sc_memory.hpp>
#include <sc-memory/sc_agent.hpp>
#include <sc-memory/sc_iterator.hpp>
#include <sc-memory/sc_link.hpp>

#include "keynodes/transport_accessibility_keynodes.hpp"
#include "utils/Graph.hpp"
#include "utils/BuildGraphFromSc.hpp"

#include <vector>
#include <string>

ScAddr FindCentralDistrictAgent::GetActionClass() const
{
  // Класс действия должен быть описан в SCs (подключён через keynodes)
  return TransportAccessibilityKeynodes::action_find_central_district;
}

ScResult FindCentralDistrictAgent::DoProgram(ScAction & action)
{
  m_logger.Info("FindCentralDistrictAgent started");

  // 1) Аргумент действия — узел графа
  auto const & [graphAddr] = action.GetArguments<1>();
  if (!m_context.IsElement(graphAddr))
  {
    m_logger.Error("Graph node not found");
    return action.FinishWithError();
  }

  // 2) Строим граф и список районов из SC-памяти
  GraphFromScResult gr = BuildGraphFromSc(m_context, graphAddr);
  Graph & g = gr.graph;
  std::vector<ScAddr> const & districts = gr.districts;

  int n = g.n;
  if (n == 0)
  {
    // Ничего не нашли — возвращаем понятное сообщение
    ScStructure result = m_context.GenerateStructure();
    ScAddr resultNode = m_context.GenerateNode(ScType::ConstNodeStructure);
    result << resultNode;

    ScAddr msgLink = m_context.GenerateLink();
    m_context.SetLinkContent(msgLink, std::string("central district is undefined: no districts in graph"));
    ScAddr arc = m_context.GenerateConnector(ScType::ConstPermPosArc, resultNode, msgLink);
    result << msgLink << arc << graphAddr;

    action.SetResult(result);
    m_logger.Warning("Graph has no districts");
    return action.FinishSuccessfully();
  }

  // 3) Быстрый тест связности (DFS) по списку смежности, чтобы дать понятное сообщение
  std::vector<bool> reachable = g.DfsFrom(0);
  std::vector<ScAddr> unreachableDistricts;
  for (int i = 0; i < n; ++i)
  {
    if (!reachable[i])
      unreachableDistricts.push_back(districts[i]);
  }

  // 4) Матрица кратчайших расстояний (Флойд–Уоршелл, см. CLRS, ch. 25)
  int const INF = 1000000000;
  std::vector<std::vector<int>> dist = g.FloydWarshall(INF);

  // 5) Находим вершину с минимальным эксцентриситетом:
  //     ecc(v) = max_j dist[v][j]
  // Если граф несвязен — эксцентриситет бесконечен, центральной вершины нет.
  int bestIndex = unreachableDistricts.empty() ? g.FindCentralVertex(dist, INF) : -1;

  // 6) Формируем результат
  ScStructure result = m_context.GenerateStructure();
  ScAddr resultNode = m_context.GenerateNode(ScType::ConstNodeStructure);
  result << resultNode;

  if (bestIndex < 0 || bestIndex >= n)
  {
    // Явно возвращаем сообщение + список недостижимых районов (если есть)
    ScAddr msgLink = m_context.GenerateLink();
    m_context.SetLinkContent(
        msgLink,
        std::string("central district is undefined: graph is disconnected"));
    ScAddr arc = m_context.GenerateConnector(ScType::ConstPermPosArc, resultNode, msgLink);
    result << msgLink << arc << graphAddr;

    if (!unreachableDistricts.empty())
    {
      ScAddr unreachableSet = m_context.GenerateNode(ScType::ConstNodeStructure);
      result << unreachableSet;

      for (ScAddr const & d : unreachableDistricts)
      {
        ScAddr arcU = m_context.GenerateConnector(ScType::ConstPermPosArc, unreachableSet, d);
        result << arcU << d;
      }

      ScAddr arcRole = m_context.GenerateConnector(
          ScType::ConstPermPosArc,
          TransportAccessibilityKeynodes::nrel_graph_connectivity,
          unreachableSet);
      result << arcRole;
    }

    action.SetResult(result);
    m_logger.Warning("Central district undefined (graph disconnected)");
    return action.FinishSuccessfully();
  }

  ScAddr centralDistrict = districts[bestIndex];

  // graphAddr => nrel_is_it_central_district: centralDistrict;;
  ScAddr arcCommon = m_context.GenerateConnector(
      ScType::ConstCommonArc,
      graphAddr,
      centralDistrict);
  ScAddr arcRel = m_context.GenerateConnector(
      ScType::ConstPermPosArc,
      TransportAccessibilityKeynodes::nrel_is_it_central_district,
      arcCommon);

  // Включаем центральный район в результирующую структуру
  ScAddr arcToResult = m_context.GenerateConnector(
      ScType::ConstPermPosArc,
      resultNode,
      centralDistrict);

  result << graphAddr << centralDistrict << arcCommon << arcRel << arcToResult;

  // Дополнительно кладём человеко-читаемый link для UI
  ScAddr infoLink = m_context.GenerateLink();
  m_context.SetLinkContent(infoLink, std::string("central district: ") + m_context.GetElementSystemIdentifier(centralDistrict));
  ScAddr arcInfo = m_context.GenerateConnector(ScType::ConstPermPosArc, resultNode, infoLink);
  result << infoLink << arcInfo;

  action.SetResult(result);
  m_logger.Info("FindCentralDistrictAgent finished successfully");
  return action.FinishSuccessfully();
}
