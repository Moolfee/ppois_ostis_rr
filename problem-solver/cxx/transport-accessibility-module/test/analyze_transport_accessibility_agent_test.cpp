#include <sc-memory/test/sc_test.hpp>
#include <sc-memory/sc_memory.hpp>

#include "agents/analyze_transport_accessibility_agent.hpp"
#include "keynodes/transport_accessibility_keynodes.hpp"

#include <string>

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
      TransportAccessibilityKeynodes::action_analyze_transport_accessibility);
  action.SetArguments(graph);

  EXPECT_TRUE(action.InitiateAndWait());
  EXPECT_TRUE(action.IsFinishedSuccessfully());

  ScStructure const result = action.GetResult();
  ASSERT_FALSE(result.IsEmpty());

  // graph -> nrel_graph_connectivity: link("true")
  {
    ScIterator5Ptr it = m_ctx->CreateIterator5(
        graph,
        ScType::ConstCommonArc,
        ScType::ConstNodeLink,
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_graph_connectivity);
    ASSERT_TRUE(it->Next());
    ScAddr link = it->Get(2);
    std::string content;
    m_ctx->GetLinkContent(link, content);
    EXPECT_EQ(content, "true");
  }

  // graph -> nrel_network_diameter: link("2")
  {
    ScIterator5Ptr it = m_ctx->CreateIterator5(
        graph,
        ScType::ConstCommonArc,
        ScType::ConstNodeLink,
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_network_diameter);
    ASSERT_TRUE(it->Next());
    ScAddr link = it->Get(2);
    std::string content;
    m_ctx->GetLinkContent(link, content);
    EXPECT_EQ(content, "2");
  }

  m_ctx->UnsubscribeAgent<AnalyzeTransportAccessibilityAgent>();
}

TEST_F(AgentTest, DisconnectedGraphHasNoCentralAndUndefinedDiameter)
{
  m_ctx->SubscribeAgent<AnalyzeTransportAccessibilityAgent>();

  ScAddr graph = m_ctx->GenerateNode(ScType::ConstNodeStructure);
  ScAddr d0 = m_ctx->GenerateNode(ScType::ConstNode);
  ScAddr d1 = m_ctx->GenerateNode(ScType::ConstNode);
  auto markDistrict = [&](ScAddr const & d)
  {
    m_ctx->GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::concept_district,
        d);
    m_ctx->GenerateConnector(ScType::ConstPermPosArc, graph, d);
  };
  markDistrict(d0);
  markDistrict(d1);
  // нет маршрутов => граф несвязен, диаметр undefined, центральный район не назначен

  ScAction action =
      m_ctx->GenerateAction(TransportAccessibilityKeynodes::action_analyze_transport_accessibility);
  action.SetArguments(graph);

  ASSERT_TRUE(action.InitiateAndWait());
  ASSERT_TRUE(action.IsFinishedSuccessfully());

  ScStructure const result = action.GetResult();
  ASSERT_FALSE(result.IsEmpty());

  // graph -> nrel_graph_connectivity: false
  {
    ScIterator5Ptr it = m_ctx->CreateIterator5(
        graph,
        ScType::ConstCommonArc,
        ScType::ConstNodeLink,
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_graph_connectivity);
    ASSERT_TRUE(it->Next());
    std::string content;
    m_ctx->GetLinkContent(it->Get(2), content);
    EXPECT_EQ(content, "false");
  }

  // диаметр = "undefined (disconnected graph)"
  {
    ScIterator5Ptr it = m_ctx->CreateIterator5(
        graph,
        ScType::ConstCommonArc,
        ScType::ConstNodeLink,
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_network_diameter);
    ASSERT_TRUE(it->Next());
    std::string content;
    m_ctx->GetLinkContent(it->Get(2), content);
    EXPECT_NE(content.find("undefined"), std::string::npos);
  }

  // центральный район отсутствует
  {
    ScIterator5Ptr it = m_ctx->CreateIterator5(
        graph,
        ScType::ConstCommonArc,
        ScType::ConstNode,
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_is_it_central_district);
    EXPECT_FALSE(it->Next());
  }

  m_ctx->UnsubscribeAgent<AnalyzeTransportAccessibilityAgent>();
}

TEST_F(AgentTest, DetectsBridgeRoutesAndCount)
{
  m_ctx->SubscribeAgent<AnalyzeTransportAccessibilityAgent>();

  ScAddr graph = m_ctx->GenerateNode(ScType::ConstNodeStructure);
  ScAddr d0 = m_ctx->GenerateNode(ScType::ConstNode);
  ScAddr d1 = m_ctx->GenerateNode(ScType::ConstNode);
  ScAddr d2 = m_ctx->GenerateNode(ScType::ConstNode);

  auto markDistrict = [&](ScAddr const & d)
  {
    m_ctx->GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::concept_district,
        d);
    m_ctx->GenerateConnector(ScType::ConstPermPosArc, graph, d);
  };
  markDistrict(d0);
  markDistrict(d1);
  markDistrict(d2);

  auto addRoute = [&](ScAddr const & a, ScAddr const & b) -> ScAddr
  {
    ScAddr route = m_ctx->GenerateNode(ScType::ConstNode);
    m_ctx->GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::concept_public_transport_route,
        route);
    m_ctx->GenerateConnector(ScType::ConstPermPosArc, graph, route);
    ScAddr set = m_ctx->GenerateNode(ScType::ConstNodeStructure);
    m_ctx->GenerateConnector(ScType::ConstPermPosArc, set, a);
    m_ctx->GenerateConnector(ScType::ConstPermPosArc, set, b);
    ScAddr arcCommon = m_ctx->GenerateConnector(ScType::ConstCommonArc, route, set);
    m_ctx->GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_connects_districts,
        arcCommon);
    return route;
  };

  ScAddr r01 = addRoute(d0, d1);
  ScAddr r12 = addRoute(d1, d2);  // оба маршрута — мосты в цепочке из 3 вершин

  ScAction action =
      m_ctx->GenerateAction(TransportAccessibilityKeynodes::action_analyze_transport_accessibility);
  action.SetArguments(graph);

  ASSERT_TRUE(action.InitiateAndWait());
  ASSERT_TRUE(action.IsFinishedSuccessfully());

  ScStructure const result = action.GetResult();
  ASSERT_FALSE(result.IsEmpty());

  // количество мостов = 2
  {
    ScIterator5Ptr it = m_ctx->CreateIterator5(
        graph,
        ScType::ConstCommonArc,
        ScType::ConstNodeLink,
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_bridge_routes_count);
    ASSERT_TRUE(it->Next());
    std::string content;
    m_ctx->GetLinkContent(it->Get(2), content);
    EXPECT_EQ(content, "2");
  }

  // маршруты отмечены как мостовые
  int markedRoutes = 0;
  ScIterator5Ptr itRoutes = m_ctx->CreateIterator5(
      r01,
      ScType::ConstCommonArc,
      graph,
      ScType::ConstPermPosArc,
      TransportAccessibilityKeynodes::nrel_is_it_bridge_connection);
  if (itRoutes->Next())
    ++markedRoutes;
  itRoutes = m_ctx->CreateIterator5(
      r12,
      ScType::ConstCommonArc,
      graph,
      ScType::ConstPermPosArc,
      TransportAccessibilityKeynodes::nrel_is_it_bridge_connection);
  if (itRoutes->Next())
    ++markedRoutes;
  EXPECT_EQ(markedRoutes, 2);

  m_ctx->UnsubscribeAgent<AnalyzeTransportAccessibilityAgent>();
}

TEST_F(AgentTest, AnalyzeEmptyGraphReturnsEmptyResult)
{
  m_ctx->SubscribeAgent<AnalyzeTransportAccessibilityAgent>();

  ScAddr graph = m_ctx->GenerateNode(ScType::ConstNodeStructure);

  ScAction action =
      m_ctx->GenerateAction(TransportAccessibilityKeynodes::action_analyze_transport_accessibility);
  action.SetArguments(graph);

  ASSERT_TRUE(action.InitiateAndWait());
  ASSERT_TRUE(action.IsFinishedSuccessfully());

  ScStructure const result = action.GetResult();
  EXPECT_TRUE(result.IsEmpty());

  m_ctx->UnsubscribeAgent<AnalyzeTransportAccessibilityAgent>();
}

TEST_F(AgentTest, SkipsRoutesWithBadEndpoints)
{
  m_ctx->SubscribeAgent<AnalyzeTransportAccessibilityAgent>();

  ScAddr graph = m_ctx->GenerateNode(ScType::ConstNodeStructure);
  ScAddr d0 = m_ctx->GenerateNode(ScType::ConstNode);
  ScAddr d1 = m_ctx->GenerateNode(ScType::ConstNode);
  ScAddr d2 = m_ctx->GenerateNode(ScType::ConstNode);

  auto markDistrict = [&](ScAddr const & d)
  {
    m_ctx->GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::concept_district,
        d);
    m_ctx->GenerateConnector(ScType::ConstPermPosArc, graph, d);
  };
  markDistrict(d0);
  markDistrict(d1);
  markDistrict(d2);

  // valid route d0-d1
  auto addRoute = [&](ScAddr const & a, ScAddr const & b, bool markConcept = true) -> ScAddr
  {
    ScAddr route = m_ctx->GenerateNode(ScType::ConstNode);
    if (markConcept)
    {
      m_ctx->GenerateConnector(
          ScType::ConstPermPosArc,
          TransportAccessibilityKeynodes::concept_public_transport_route,
          route);
    }
    m_ctx->GenerateConnector(ScType::ConstPermPosArc, graph, route);
    ScAddr set = m_ctx->GenerateNode(ScType::ConstNodeStructure);
    m_ctx->GenerateConnector(ScType::ConstPermPosArc, set, a);
    m_ctx->GenerateConnector(ScType::ConstPermPosArc, set, b);
    ScAddr arcCommon = m_ctx->GenerateConnector(ScType::ConstCommonArc, route, set);
    m_ctx->GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_connects_districts,
        arcCommon);
    return route;
  };
  addRoute(d0, d1);  // valid

  // malformed route: three endpoints -> should be skipped when mapping routes
  {
    ScAddr route = m_ctx->GenerateNode(ScType::ConstNode);
    m_ctx->GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::concept_public_transport_route,
        route);
    m_ctx->GenerateConnector(ScType::ConstPermPosArc, graph, route);
    ScAddr set = m_ctx->GenerateNode(ScType::ConstNodeStructure);
    m_ctx->GenerateConnector(ScType::ConstPermPosArc, set, d0);
    m_ctx->GenerateConnector(ScType::ConstPermPosArc, set, d1);
    m_ctx->GenerateConnector(ScType::ConstPermPosArc, set, d2);
    ScAddr arcCommon = m_ctx->GenerateConnector(ScType::ConstCommonArc, route, set);
    m_ctx->GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_connects_districts,
        arcCommon);
  }

  ScAction action =
      m_ctx->GenerateAction(TransportAccessibilityKeynodes::action_analyze_transport_accessibility);
  action.SetArguments(graph);

  ASSERT_TRUE(action.InitiateAndWait());
  ASSERT_TRUE(action.IsFinishedSuccessfully());

  ScStructure const result = action.GetResult();
  ASSERT_FALSE(result.IsEmpty());

  // connectivity should be false because d2 is unreachable (invalid route ignored)
  {
    ScIterator5Ptr it = m_ctx->CreateIterator5(
        graph,
        ScType::ConstCommonArc,
        ScType::ConstNodeLink,
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_graph_connectivity);
    ASSERT_TRUE(it->Next());
    std::string content;
    m_ctx->GetLinkContent(it->Get(2), content);
    EXPECT_EQ(content, "false");
  }

  m_ctx->UnsubscribeAgent<AnalyzeTransportAccessibilityAgent>();
}
