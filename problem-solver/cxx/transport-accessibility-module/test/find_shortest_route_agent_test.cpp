#include <sc-memory/test/sc_test.hpp>
#include <sc-memory/sc_memory.hpp>

#include "agents/find_shortest_route_agent.hpp"
#include "keynodes/transport_accessibility_keynodes.hpp"

using AgentTest = ScMemoryTest;

// Строит цепочку из трёх районов: d0 - d1 - d2
static ScAddr BuildChain3(ScMemoryContext & ctx)
{
  ScAddr graph = ctx.GenerateNode(ScType::ConstNodeStructure);

  ScAddr d0 = ctx.GenerateNode(ScType::ConstNode);
  ScAddr d1 = ctx.GenerateNode(ScType::ConstNode);
  ScAddr d2 = ctx.GenerateNode(ScType::ConstNode);

  auto markDistrict = [&](ScAddr const & d)
  {
    ctx.GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::concept_district,
        d);
    ctx.GenerateConnector(ScType::ConstPermPosArc, graph, d);
  };

  markDistrict(d0);
  markDistrict(d1);
  markDistrict(d2);

  auto addRoute = [&](ScAddr const & dA, ScAddr const & dB)
  {
    ScAddr route = ctx.GenerateNode(ScType::ConstNode);
    ctx.GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::concept_public_transport_route,
        route);
    ctx.GenerateConnector(ScType::ConstPermPosArc, graph, route);

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

  addRoute(d0, d1);
  addRoute(d1, d2);

  return graph;
}

TEST_F(AgentTest, FindsAllUndirectedPairs)
{
  m_ctx->SubscribeAgent<FindShortestRouteAgent>();

  ScAddr graph = BuildChain3(*m_ctx);

  ScAction action = m_ctx->GenerateAction(
      TransportAccessibilityKeynodes::action_find_shortest_route,
      graph);

  EXPECT_TRUE(action.InitiateAndWait());
  EXPECT_TRUE(action.IsFinishedSuccessfully());

  ScStructure const result = action.GetResult();
  ASSERT_FALSE(result.IsEmpty());

  // graph -> nrel_shortest_distance: tableNode
  ScAddr tableNode;
  {
    ScIterator5Ptr it = m_ctx->CreateIterator5(
        graph,
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

  // Проверим, что расстояние между крайними районами равно 2
  bool found02 = false;
  {
    ScIterator5Ptr it = m_ctx->CreateIterator5(
        tableNode,
        ScType::ConstPermPosArc,
        ScType::ConstNode,
        ScType::ConstCommonArc,
        ScType::ConstLink);
    while (it->Next())
    {
      ScAddr link = it->Get(4);
      ScLinkContent content;
      m_ctx->GetLinkContent(link, content);
      std::string val = content.AsString();
      if (val.find("district_0<->district_2") != std::string::npos)
      {
        found02 = true;
        EXPECT_NE(val.find(": 2"), std::string::npos);
        break;
      }
    }
  }
  EXPECT_TRUE(found02);

  m_ctx->UnsubscribeAgent<FindShortestRouteAgent>();
}
