#include "find_shortest_route_agent.hpp"

#include <sc-memory/sc_agent.hpp>
#include <sc-memory/sc_link.hpp>
#include <sc-memory/sc_memory.hpp>

#include "keynodes/transport_accessibility_keynodes.hpp"
#include "utils/BuildGraphFromSc.hpp"
#include "utils/Graph.hpp"

#include <string>
#include <vector>

namespace
{
constexpr int kInf = 1000000000;
}

ScAddr FindShortestRouteAgent::GetActionClass() const
{
  return TransportAccessibilityKeynodes::action_find_shortest_route;
}

ScResult FindShortestRouteAgent::DoProgram(ScAction & action)
{
  m_logger.Info("FindShortestRouteAgent started");

  auto const & [graphAddr] = action.GetArguments<1>();
  if (!m_context.IsElement(graphAddr))
  {
    m_logger.Error("Graph node not found");
    return action.FinishWithError();
  }

  GraphFromScResult gr = BuildGraphFromSc(m_context, graphAddr);
  Graph & g = gr.graph;
  std::vector<ScAddr> const & districts = gr.districts;

  int n = g.n;
  if (n == 0)
  {
    ScStructure emptyResult = m_context.GenerateStructure();
    action.SetResult(emptyResult);
    m_logger.Warning("Graph has no districts");
    return action.FinishSuccessfully();
  }

  std::vector<std::vector<int>> dist = g.FloydWarshall(kInf);
  int diameter = g.Diameter(dist, kInf);
  bool hasInf = false;
  for (int i = 0; i < n && !hasInf; ++i)
  {
    for (int j = 0; j < n; ++j)
    {
      if (dist[i][j] == kInf)
      {
        hasInf = true;
        break;
      }
    }
  }

  ScStructure result = m_context.GenerateStructure();
  result << graphAddr;

  ScAddr resultNode = m_context.GenerateNode(ScType::ConstNodeStructure);
  result << resultNode;

  ScAddr tableNode = m_context.GenerateNode(ScType::ConstNodeStructure);
  result << tableNode;

  ScAddr arcGraph = m_context.GenerateConnector(
      ScType::ConstCommonArc,
      graphAddr,
      tableNode);
  ScAddr arcGraphRel = m_context.GenerateConnector(
      ScType::ConstPermPosArc,
      TransportAccessibilityKeynodes::nrel_shortest_distance,
      arcGraph);
  result << arcGraph << arcGraphRel;

  ScAddr arcTable = m_context.GenerateConnector(
      ScType::ConstPermPosArc,
      resultNode,
      tableNode);
  result << arcTable;

  {
    ScAddr diamLink = m_context.GenerateLink();
    m_context.SetLinkContent(
        diamLink,
        hasInf ? std::string("diameter: undefined (disconnected)") : std::string("diameter: ") + std::to_string(diameter));

    ScAddr arcCommon = m_context.GenerateConnector(
        ScType::ConstCommonArc,
        graphAddr,
        diamLink);

    ScAddr arcRel = m_context.GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_network_diameter,
        arcCommon);

    ScAddr arcToResult = m_context.GenerateConnector(
        ScType::ConstPermPosArc,
        resultNode,
        diamLink);

    ScAddr arcRole = m_context.GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::rrel_diameter_value,
        arcToResult);

    result << diamLink << arcCommon << arcRel << arcToResult << arcRole;
  }

  auto makeId = [&](int idx) -> std::string
  {
    std::string idtf = m_context.GetElementSystemIdentifier(districts[idx]);
    if (idtf.empty())
      idtf = "district_" + std::to_string(idx);
    return idtf;
  };

  auto addPair = [&](int i, int j)
  {
    bool reachable = dist[i][j] != kInf;

    ScAddr pairNode = m_context.GenerateNode(ScType::ConstNodeStructure);
    result << pairNode;

    ScAddr arcStart = m_context.GenerateConnector(
        ScType::ConstCommonArc,
        pairNode,
        districts[i]);
    ScAddr arcStartRole = m_context.GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::rrel_start_district,
        arcStart);

    ScAddr arcEnd = m_context.GenerateConnector(
        ScType::ConstCommonArc,
        pairNode,
        districts[j]);
    ScAddr arcEndRole = m_context.GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::rrel_end_district,
        arcEnd);

    std::string text = makeId(i) + "<->" + makeId(j) + ": ";
    text += reachable ? std::to_string(dist[i][j]) : "no path";

    ScAddr distLink = m_context.GenerateLink();
    m_context.SetLinkContent(distLink, text);

    ScAddr arcCommonDist = m_context.GenerateConnector(
        ScType::ConstCommonArc,
        pairNode,
        distLink);
    ScAddr arcRelDist = m_context.GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_shortest_distance,
        arcCommonDist);

    ScAddr arcToTable = m_context.GenerateConnector(
        ScType::ConstPermPosArc,
        tableNode,
        pairNode);

    result << arcStart << arcStartRole
           << arcEnd << arcEndRole
           << distLink << arcCommonDist << arcRelDist
           << arcToTable;
  };

  bool hasPairs = false;

  for (int i = 0; i < n; ++i)
  {
    for (int j = i + 1; j < n; ++j)
    {
      addPair(i, j);
      hasPairs = true;
    }
  }

  if (!hasPairs)
  {
    ScAddr msgLink = m_context.GenerateLink();
    m_context.SetLinkContent(msgLink, std::string("no district pairs"));
    ScAddr arcMsg = m_context.GenerateConnector(
        ScType::ConstPermPosArc,
        resultNode,
        msgLink);
    result << msgLink << arcMsg;
  }

  action.SetResult(result);
  m_logger.Info("FindShortestRouteAgent finished successfully");
  return action.FinishSuccessfully();
}
