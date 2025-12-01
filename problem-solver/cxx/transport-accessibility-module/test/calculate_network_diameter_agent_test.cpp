#include <sc-memory/test/sc_test.hpp>
#include <sc-memory/sc_memory.hpp>

#include "agents/calculate_network_diameter_agent.hpp"
#include "keynodes/transport_accessibility_keynodes.hpp"

using AgentTest = ScMemoryTest;

// Строим простой граф-цепочку из трёх районов:
// d1 --(route1)-- d2 --(route2)-- d3
// В таком графе диаметр = 2.
static ScAddr BuildThreeDistrictPathGraph(ScMemoryContext & ctx)
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

TEST_F(AgentTest, CalculateNetworkDiameterAgentFinishedSuccessfully)
{
  // 1. Регистрируем агент в sc-памяти (как в методичке)
  m_ctx->SubscribeAgent<CalculateNetworkDiameterAgent>();

  // 2. Строим тестовый граф
  ScAddr const graphAddr = BuildThreeDistrictPathGraph(*m_ctx);

  // 3. Создаём действие нужного класса
  ScAction action = m_ctx->GenerateAction(
      TransportAccessibilityKeynodes::action_calculate_network_diameter);

  // 4. Передаём граф как аргумент действия
  action.SetArguments(graphAddr);
  // Альтернатива в стиле методички:
  // action.SetArgument(1, graphAddr);

  // 5. Инициируем действие и ждём завершения
  EXPECT_TRUE(action.InitiateAndWait());
  EXPECT_TRUE(action.IsFinishedSuccessfully());

  // 6. Получаем структуру результата
  ScStructure const resultStructure = action.GetResult();
  EXPECT_FALSE(resultStructure.IsEmpty());

  // 7. Ищем отношение:
  // graphAddr => nrel_network_diametr: <link>;
  ScAddr diameterLink;

  ScIterator5Ptr it = m_ctx->CreateIterator5(
      graphAddr,
      ScType::ConstCommonArc,
      ScType::ConstNodeLink,
      ScType::ConstPermPosArc,
      TransportAccessibilityKeynodes::nrel_network_diameter);

  if (it->Next())
    diameterLink = it->Get(2);

  EXPECT_TRUE(diameterLink.IsValid());

  // 8. Проверяем значение в линку (ожидаем диаметр = 2)
  uint32_t diameter = 0;
  EXPECT_TRUE(m_ctx->GetLinkContent(diameterLink, diameter));
  EXPECT_EQ(diameter, 2u);

  // 9. Дерегистрируем агент (как в листинге 44 методички)
  m_ctx->UnsubscribeAgent<CalculateNetworkDiameterAgent>();
}
