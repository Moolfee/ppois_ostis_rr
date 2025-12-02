#include <sc-memory/test/sc_test.hpp>
#include <sc-memory/sc_memory.hpp>

#include "agents/analyze_transport_accessibility_agent.hpp"
#include "keynodes/transport_accessibility_keynodes.hpp"

using AgentTest = ScMemoryTest;

// Кольцо из четырёх районов: 0-1-2-3-0 (диаметр 2, связный)
static ScAddr BuildRing4(ScMemoryContext & ctx)
{
  ScAddr graph = ctx.GenerateNode(ScType::ConstNodeStructure);

  std::vector<ScAddr> d(4);
  for (int i = 0; i < 4; ++i)
  {
    d[i] = ctx.GenerateNode(ScType::ConstNode);
    ctx.GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::concept_district,
        d[i]);
    ctx.GenerateConnector(ScType::ConstPermPosArc, graph, d[i]);
  }

  auto addRoute = [&](int a, int b)
  {
    ScAddr route = ctx.GenerateNode(ScType::ConstNode);
    ctx.GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::concept_public_transport_route,
        route);
    ctx.GenerateConnector(ScType::ConstPermPosArc, graph, route);

    ScAddr set = ctx.GenerateNode(ScType::ConstNodeStructure);
    ctx.GenerateConnector(ScType::ConstPermPosArc, set, d[a]);
    ctx.GenerateConnector(ScType::ConstPermPosArc, set, d[b]);

    ScAddr arcCommon = ctx.GenerateConnector(
        ScType::ConstCommonArc,
        route,
        set);
    ctx.GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_connects_districts,
        arcCommon);
  };

  addRoute(0, 1);
  addRoute(1, 2);
  addRoute(2, 3);
  addRoute(3, 0);

  return graph;
}

TEST_F(AgentTest, AnalyzeReturnsConnectivityAndDiameter)
{
  m_ctx->SubscribeAgent<AnalyzeTransportAccessibilityAgent>();

  ScAddr graph = BuildRing4(*m_ctx);

  ScAction action = m_ctx->GenerateAction(
      TransportAccessibilityKeynodes::action_analyze_transport_accessibility,
      graph);

  EXPECT_TRUE(action.InitiateAndWait());
  EXPECT_TRUE(action.IsFinishedSuccessfully());

  ScStructure const result = action.GetResult();
  ASSERT_FALSE(result.IsEmpty());

  // graph -> nrel_graph_connectivity: link("true")
  {
    ScIterator5Ptr it = m_ctx->CreateIterator5(
        graph,
        ScType::ConstCommonArc,
        ScType::ConstLink,
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_graph_connectivity);
    ASSERT_TRUE(it->Next());
    ScAddr link = it->Get(2);
    ScLinkContent content;
    m_ctx->GetLinkContent(link, content);
    EXPECT_EQ(content.AsString(), "true");
  }

  // graph -> nrel_network_diameter: link("2")
  {
    ScIterator5Ptr it = m_ctx->CreateIterator5(
        graph,
        ScType::ConstCommonArc,
        ScType::ConstLink,
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_network_diameter);
    ASSERT_TRUE(it->Next());
    ScAddr link = it->Get(2);
    ScLinkContent content;
    m_ctx->GetLinkContent(link, content);
    EXPECT_EQ(content.AsString(), "2");
  }

  m_ctx->UnsubscribeAgent<AnalyzeTransportAccessibilityAgent>();
}
