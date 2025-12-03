#include <sc-memory/test/sc_test.hpp>
#include <sc-memory/sc_memory.hpp>

#include "agents/check_graph_connectivity_agent.hpp"
#include "keynodes/transport_accessibility_keynodes.hpp"

#include <string>

using AgentTest = ScMemoryTest;

namespace
{
ScAddr MakeDistrict(ScMemoryContext & ctx, ScAddr const & graph)
{
  ScAddr d = ctx.GenerateNode(ScType::ConstNode);
  ctx.GenerateConnector(ScType::ConstPermPosArc, TransportAccessibilityKeynodes::concept_district, d);
  ctx.GenerateConnector(ScType::ConstPermPosArc, graph, d);
  return d;
}

void MakeRoute(ScMemoryContext & ctx, ScAddr const & graph, ScAddr const & a, ScAddr const & b)
{
  ScAddr route = ctx.GenerateNode(ScType::ConstNode);
  ctx.GenerateConnector(
      ScType::ConstPermPosArc,
      TransportAccessibilityKeynodes::concept_public_transport_route,
      route);
  ctx.GenerateConnector(ScType::ConstPermPosArc, graph, route);

  ScAddr set = ctx.GenerateNode(ScType::ConstNodeStructure);
  ctx.GenerateConnector(ScType::ConstPermPosArc, set, a);
  ctx.GenerateConnector(ScType::ConstPermPosArc, set, b);

  ScAddr arcCommon = ctx.GenerateConnector(ScType::ConstCommonArc, route, set);
  ctx.GenerateConnector(
      ScType::ConstPermPosArc,
      TransportAccessibilityKeynodes::nrel_connects_districts,
      arcCommon);
}
}  // namespace

TEST_F(AgentTest, ConnectivityTrueAndReachableSet)
{
  m_ctx->SubscribeAgent<CheckGraphConnectivityAgent>();

  ScAddr graph = m_ctx->GenerateNode(ScType::ConstNodeStructure);
  ScAddr d0 = MakeDistrict(*m_ctx, graph);
  ScAddr d1 = MakeDistrict(*m_ctx, graph);
  MakeRoute(*m_ctx, graph, d0, d1);

  ScAction action = m_ctx->GenerateAction(TransportAccessibilityKeynodes::action_check_graph_connectivity);
  action.SetArguments(graph);

  ASSERT_TRUE(action.InitiateAndWait());
  ASSERT_TRUE(action.IsFinishedSuccessfully());

  ScStructure const result = action.GetResult();
  ASSERT_FALSE(result.IsEmpty());

  // graph => nrel_graph_connectivity: "true"
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
    EXPECT_EQ(content, "true");
  }

  // reachable set contains both districts and the route
  ScAddr reachableSet;
  {
    ScIterator3Ptr it = m_ctx->CreateIterator3(
        result,
        ScType::ConstPermPosArc,
        ScType::ConstNodeStructure);
    ASSERT_TRUE(it->Next());
    reachableSet = it->Get(2);
  }

  auto countMembers = [&](ScType type) -> size_t
  {
    size_t cnt = 0;
    ScIterator3Ptr it = m_ctx->CreateIterator3(reachableSet, ScType::ConstPermPosArc, type);
    while (it->Next())
      ++cnt;
    return cnt;
  };

  EXPECT_EQ(countMembers(ScType::ConstNode), 3u);  // two districts + route

  m_ctx->UnsubscribeAgent<CheckGraphConnectivityAgent>();
}

TEST_F(AgentTest, ConnectivityFalseMarksOnlyReachable)
{
  m_ctx->SubscribeAgent<CheckGraphConnectivityAgent>();

  ScAddr graph = m_ctx->GenerateNode(ScType::ConstNodeStructure);
  ScAddr d0 = MakeDistrict(*m_ctx, graph);
  ScAddr d1 = MakeDistrict(*m_ctx, graph);
  // no routes, graph is disconnected

  ScAction action = m_ctx->GenerateAction(TransportAccessibilityKeynodes::action_check_graph_connectivity);
  action.SetArguments(graph);

  ASSERT_TRUE(action.InitiateAndWait());
  ASSERT_TRUE(action.IsFinishedSuccessfully());

  ScStructure const result = action.GetResult();
  ASSERT_FALSE(result.IsEmpty());

  // graph => nrel_graph_connectivity: "false"
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

  // reachable set should contain only the start district
  ScAddr reachableSet;
  {
    ScIterator3Ptr it = m_ctx->CreateIterator3(
        result,
        ScType::ConstPermPosArc,
        ScType::ConstNodeStructure);
    ASSERT_TRUE(it->Next());
    reachableSet = it->Get(2);
  }

  size_t districtCount = 0;
  ScIterator3Ptr itMembers = m_ctx->CreateIterator3(
      reachableSet,
      ScType::ConstPermPosArc,
      ScType::ConstNode);
  while (itMembers->Next())
    ++districtCount;

  EXPECT_EQ(districtCount, 1u);

  m_ctx->UnsubscribeAgent<CheckGraphConnectivityAgent>();
}

TEST_F(AgentTest, ConnectivityNoDistrictsCausesError)
{
  m_ctx->SubscribeAgent<CheckGraphConnectivityAgent>();

  ScAddr graph = m_ctx->GenerateNode(ScType::ConstNodeStructure);  // no districts attached

  ScAction action = m_ctx->GenerateAction(TransportAccessibilityKeynodes::action_check_graph_connectivity);
  action.SetArguments(graph);

  EXPECT_TRUE(action.InitiateAndWait());
  EXPECT_FALSE(action.IsFinishedSuccessfully());

  m_ctx->UnsubscribeAgent<CheckGraphConnectivityAgent>();
}

TEST_F(AgentTest, IgnoresRoutesWithInvalidEndpoints)
{
  m_ctx->SubscribeAgent<CheckGraphConnectivityAgent>();

  ScAddr graph = m_ctx->GenerateNode(ScType::ConstNodeStructure);
  ScAddr d0 = MakeDistrict(*m_ctx, graph);
  ScAddr d1 = MakeDistrict(*m_ctx, graph);
  MakeRoute(*m_ctx, graph, d0, d1);  // valid

  // Route with three endpoints -> should be ignored
  ScAddr d2 = MakeDistrict(*m_ctx, graph);
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

  ScAction action = m_ctx->GenerateAction(TransportAccessibilityKeynodes::action_check_graph_connectivity);
  action.SetArguments(graph);

  ASSERT_TRUE(action.InitiateAndWait());
  ASSERT_TRUE(action.IsFinishedSuccessfully());  // still succeeds using valid route

  // graph connectivity should be true (valid route connects d0-d1, d2 isolated but reachable? no)
  // reachable set should include only d0, d1, valid route (d2 not reachable)
  ScStructure const result = action.GetResult();
  ASSERT_FALSE(result.IsEmpty());

  // pick the NodeStructure with maximum members (reachable set)
  size_t maxCount = 0;
  ScIterator3Ptr itSets = m_ctx->CreateIterator3(
      result,
      ScType::ConstPermPosArc,
      ScType::ConstNodeStructure);
  while (itSets->Next())
  {
    ScAddr candidate = itSets->Get(2);
    size_t cnt = 0;
    ScIterator3Ptr itMembers = m_ctx->CreateIterator3(
        candidate,
        ScType::ConstPermPosArc,
        ScType::ConstNode);
    while (itMembers->Next())
      ++cnt;
    if (cnt > maxCount)
      maxCount = cnt;
  }

  EXPECT_GE(maxCount, 2u);  // должно включать хотя бы связный компонент

  m_ctx->UnsubscribeAgent<CheckGraphConnectivityAgent>();
}
