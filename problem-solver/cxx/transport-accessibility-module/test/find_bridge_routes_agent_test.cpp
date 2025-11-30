// Тесты агентов (как в методичке)
#include <sc-memory/test/sc_test.hpp>
#include <sc-memory/sc_memory.hpp>

#include "agents/find_bridge_routes_agent.hpp"
#include "keynodes/transport_accessibility_keynodes.hpp"

using AgentTest = ScMemoryTest;

// Строим простой граф-цепочку из трёх районов:
// d1 --(route1)--> d2 --(route2)--> d3
// Оба маршрута в такой цепочке являются мостами.
static ScAddr BuildPathGraphWithBridges(ScMemoryContext & ctx)
{
  ScAddr const graphAddr = ctx.GenerateNode(ScType::ConstNodeStructure);

  // Районы
  ScAddr const d1 = ctx.GenerateNode(ScType::ConstNode);
  ScAddr const d2 = ctx.GenerateNode(ScType::ConstNode);
  ScAddr const d3 = ctx.GenerateNode(ScType::ConstNode);

  // Помечаем как concept_district
  ctx.GenerateConnector(
      ScType::ConstPermPosArc,
      TransportAccessibilityKeynodes::concept_district,
      d1);
  ctx.GenerateConnector(
      ScType::ConstPermPosArc,
      TransportAccessibilityKeynodes::concept_district,
      d2);
  ctx.GenerateConnector(
      ScType::ConstPermPosArc,
      TransportAccessibilityKeynodes::concept_district,
      d3);

  // Включаем в граф
  ctx.GenerateConnector(ScType::ConstPermPosArc, graphAddr, d1);
  ctx.GenerateConnector(ScType::ConstPermPosArc, graphAddr, d2);
  ctx.GenerateConnector(ScType::ConstPermPosArc, graphAddr, d3);

  // ---- route1: d1 <-> d2 ----
  ScAddr const route1 = ctx.GenerateNode(ScType::ConstNode);
  ctx.GenerateConnector(
      ScType::ConstPermPosArc,
      TransportAccessibilityKeynodes::concept_public_transport_route,
      route1);
  ctx.GenerateConnector(ScType::ConstPermPosArc, graphAddr, route1);

  ScAddr const set12 = ctx.GenerateNode(ScType::ConstNodeStructure);
  ctx.GenerateConnector(ScType::ConstPermPosArc, set12, d1);
  ctx.GenerateConnector(ScType::ConstPermPosArc, set12, d2);

  {
    ScAddr const arcCommon = ctx.GenerateConnector(
        ScType::ConstCommonArc,
        route1,
        set12);

    ctx.GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_connects_districts,
        arcCommon);
  }

  // ---- route2: d2 <-> d3 ----
  ScAddr const route2 = ctx.GenerateNode(ScType::ConstNode);
  ctx.GenerateConnector(
      ScType::ConstPermPosArc,
      TransportAccessibilityKeynodes::concept_public_transport_route,
      route2);
  ctx.GenerateConnector(ScType::ConstPermPosArc, graphAddr, route2);

  ScAddr const set23 = ctx.GenerateNode(ScType::ConstNodeStructure);
  ctx.GenerateConnector(ScType::ConstPermPosArc, set23, d2);
  ctx.GenerateConnector(ScType::ConstPermPosArc, set23, d3);

  {
    ScAddr const arcCommon = ctx.GenerateConnector(
        ScType::ConstCommonArc,
        route2,
        set23);

    ctx.GenerateConnector(
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_connects_districts,
        arcCommon);
  }

  return graphAddr;
}

TEST_F(AgentTest, FindBridgeRoutesAgentFinishedSuccessfully)
{
  // 1. Регистрируем агент
  m_ctx->SubscribeAgent<FindBridgeRoutesAgent>();

  // 2. Строим тестовый граф-цепочку, где все маршруты — мосты
  ScAddr const graphAddr = BuildPathGraphWithBridges(*m_ctx);

  // 3. Создаём действие нужного класса
  ScAction action = m_ctx->GenerateAction(
      TransportAccessibilityKeynodes::action_find_bridge_routes);

  // 4. Передаём граф как аргумент
  action.SetArguments(graphAddr);

  // 5. Запускаем действие
  EXPECT_TRUE(action.InitiateAndWait());
  EXPECT_TRUE(action.IsFinishedSuccessfully());

  // 6. Получаем структуру результата
  ScStructure const resultStructure = action.GetResult();
  EXPECT_FALSE(resultStructure.IsEmpty());

  // 7. Проверяем, что появились маршруты,
  // помеченные rrel_bridge_route
  bool hasBridgeRoute = false;

  ScIterator5Ptr it = m_ctx->CreateIterator5(
      TransportAccessibilityKeynodes::rrel_bridge_route,
      ScType::ConstPermPosArc,
      ScType::ConstPermPosArc,   // дуга: bridgeRoutesSet -> route
      ScType::ConstPermPosArc,
      ScType::ConstNode);        // сам маршрут

  while (it->Next())
  {
    ScAddr const route = it->Get(4);
    if (route.IsValid())
    {
      hasBridgeRoute = true;
      break;
    }
  }

  EXPECT_TRUE(hasBridgeRoute);

  m_ctx->UnsubscribeAgent<FindBridgeRoutesAgent>();
}
