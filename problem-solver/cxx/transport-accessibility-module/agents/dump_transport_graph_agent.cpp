#include "dump_transport_graph_agent.hpp"

#include <sc-memory/sc_agent.hpp>
#include <sc-memory/sc_link.hpp>
#include <sc-memory/sc_memory.hpp>

#include "keynodes/transport_accessibility_keynodes.hpp"
#include "utils/BuildGraphFromSc.hpp"
#include "utils/Graph.hpp"

#include <string>
#include <vector>

ScAddr DumpTransportGraphAgent::GetActionClass() const
{
  return TransportAccessibilityKeynodes::action_dump_transport_graph;
}

ScResult DumpTransportGraphAgent::DoProgram(ScAction & action)
{
  m_logger.Info("DumpTransportGraphAgent started");

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
    ScStructure empty = m_context.GenerateStructure();
    action.SetResult(empty);
    m_logger.Warning("Graph has no districts");
    return action.FinishSuccessfully();
  }

  ScStructure result = m_context.GenerateStructure();
  result << graphAddr;

  ScAddr root = m_context.GenerateNode(ScType::ConstNodeStructure);
  result << root;

  ScAddr districtSet = m_context.GenerateNode(ScType::ConstNodeStructure);
  result << districtSet;
  for (ScAddr const & d : districts)
  {
    ScAddr arc = m_context.GenerateConnector(
        ScType::ConstPermPosArc,
        districtSet,
        d);
    result << arc;
  }

  ScAddr edgeSet = m_context.GenerateNode(ScType::ConstNodeStructure);
  result << edgeSet;

  auto idtf = [&](int idx) -> std::string
  {
    std::string id = m_context.GetElementSystemIdentifier(districts[idx]);
    if (id.empty())
      id = "district_" + std::to_string(idx);
    return id;
  };

  for (int u = 0; u < n; ++u)
  {
    for (int v : g.adj[u])
    {
      if (u >= v)
        continue;  // только u < v, чтобы не дублировать неориентированное ребро

      ScAddr edgeNode = m_context.GenerateNode(ScType::ConstNodeStructure);
      result << edgeNode;

      ScAddr arcStart = m_context.GenerateConnector(
          ScType::ConstCommonArc,
          edgeNode,
          districts[u]);
      ScAddr arcStartRole = m_context.GenerateConnector(
          ScType::ConstPermPosArc,
          TransportAccessibilityKeynodes::rrel_start_district,
          arcStart);

      ScAddr arcEnd = m_context.GenerateConnector(
          ScType::ConstCommonArc,
          edgeNode,
          districts[v]);
      ScAddr arcEndRole = m_context.GenerateConnector(
          ScType::ConstPermPosArc,
          TransportAccessibilityKeynodes::rrel_end_district,
          arcEnd);

      std::string text = idtf(u) + "<->" + idtf(v);
      ScAddr link = m_context.GenerateLink();
      m_context.SetLinkContent(link, text);

      ScAddr arcCommonLink = m_context.GenerateConnector(
          ScType::ConstCommonArc,
          edgeNode,
          link);
      ScAddr arcRel = m_context.GenerateConnector(
          ScType::ConstPermPosArc,
          TransportAccessibilityKeynodes::nrel_shortest_distance,
          arcCommonLink);

      ScAddr arcToSet = m_context.GenerateConnector(
          ScType::ConstPermPosArc,
          edgeSet,
          edgeNode);

      result << arcStart << arcStartRole
             << arcEnd << arcEndRole
             << link << arcCommonLink << arcRel
             << arcToSet;
    }
  }

  action.SetResult(result);
  m_logger.Info("DumpTransportGraphAgent finished successfully");
  return action.FinishSuccessfully();
}
