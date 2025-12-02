#include <sc-memory/test/sc_test.hpp>
#include <sc-memory/sc_memory.hpp>

#include "agents/find_shortest_route_agent.hpp"
#include "keynodes/transport_accessibility_keynodes.hpp"

#include <string>

using AgentTest = ScMemoryTest;

struct Chain3
{
  ScAddr graph;
  ScAddr d0;
  ScAddr d1;
  ScAddr d2;
};

// Строит цепочку из трёх районов: d0 - d1 - d2
static Chain3 BuildChain3(ScMemoryContext & ctx)
{
  Chain3 res;
  res.graph = ctx.GenerateNode(ScType::ConstNodeStructure);

  res.d0 = ctx.GenerateNode(ScType::ConstNode);
  res.d1 = ctx.GenerateNode(ScType::ConstNode);
  res.d2 = ctx.GenerateNode(ScType::ConstNode);

  auto markDistrict = [&](ScAddr const & d)
  {
    ctx.GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::concept_district,
        d);
    ctx.GenerateConnector(ScType::ConstPermPosArc, res.graph, d);
  };

  markDistrict(res.d0);
  markDistrict(res.d1);
  markDistrict(res.d2);

  auto addRoute = [&](ScAddr const & dA, ScAddr const & dB)
  {
    ScAddr route = ctx.GenerateNode(ScType::ConstNode);
    ctx.GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::concept_public_transport_route,
        route);
    ctx.GenerateConnector(ScType::ConstPermPosArc, res.graph, route);

    ScAddr set = ctx.GenerateNode(ScType::ConstNodeStructure);
    ctx.GenerateConnector(ScType::ConstPermPosArc, set, dA);
    ctx.GenerateConnector(ScType::ConstPermPosArc, set, dB);

    ScAddr arcCommon = ctx.GenerateConnector(
        ScType::ConstCommonArc,
        route,
        set);
    ctx.GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_connects_districts,
        arcCommon);
  };

  addRoute(res.d0, res.d1);
  addRoute(res.d1, res.d2);

  return res;
}

TEST_F(AgentTest, FindsAllUndirectedPairs)
{
  m_ctx->SubscribeAgent<FindShortestRouteAgent>();

  Chain3 chain = BuildChain3(*m_ctx);

  ScAction action =
      m_ctx->GenerateAction(TransportAccessibilityKeynodes::action_find_shortest_route);
  action.SetArguments(chain.graph);

  EXPECT_TRUE(action.InitiateAndWait());
  EXPECT_TRUE(action.IsFinishedSuccessfully());

  ScStructure const result = action.GetResult();
  ASSERT_FALSE(result.IsEmpty());

  // graph -> nrel_shortest_distance: tableNode
  ScAddr tableNode;
  {
    ScIterator5Ptr it = m_ctx->CreateIterator5(
        chain.graph,
        ScType::ConstCommonArc,
        ScType::ConstNode,
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_shortest_distance);
    ASSERT_TRUE(it->Next());
    tableNode = it->Get(2);
  }

  // В таблице должно быть C(3,2) = 3 неориентированных пар
  uint32_t pairCount = 0;
  {
    ScIterator3Ptr it = m_ctx->CreateIterator3(
        tableNode,
        ScType::ConstPermPosArc,
        ScType::ConstNode);
    while (it->Next())
      ++pairCount;
  }
  EXPECT_EQ(pairCount, 3u);

  // Проверим, что расстояние между крайними районами (d0, d2) равно 2
  bool found02 = false;
  ScIterator3Ptr itPairs = m_ctx->CreateIterator3(
      tableNode,
      ScType::ConstPermPosArc,
      ScType::ConstNodeStructure);
  while (itPairs->Next())
  {
    ScAddr pairNode = itPairs->Get(2);

    auto getEndpoint = [&](ScAddr const & role) -> ScAddr
    {
      ScIterator5Ptr it = m_ctx->CreateIterator5(
          pairNode,
          ScType::ConstCommonArc,
          ScType::ConstNode,
          ScType::ConstPermPosArc,
          role);
      if (it->Next())
        return it->Get(2);
      return {};
    };

    ScAddr start = getEndpoint(TransportAccessibilityKeynodes::rrel_start_district);
    ScAddr end = getEndpoint(TransportAccessibilityKeynodes::rrel_end_district);

    auto matchesEnds = [&](ScAddr const & a, ScAddr const & b)
    {
      return (a == chain.d0 && b == chain.d2) || (a == chain.d2 && b == chain.d0);
    };

    if (!matchesEnds(start, end))
      continue;

    ScIterator5Ptr itDist = m_ctx->CreateIterator5(
        pairNode,
        ScType::ConstCommonArc,
        ScType::ConstNodeLink,
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_shortest_distance);
    ASSERT_TRUE(itDist->Next());

    std::string val;
    m_ctx->GetLinkContent(itDist->Get(2), val);
    EXPECT_NE(val.find(": 2"), std::string::npos);
    found02 = true;
    break;
  }
  EXPECT_TRUE(found02);

  m_ctx->UnsubscribeAgent<FindShortestRouteAgent>();
}
