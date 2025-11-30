#include <sc-memory/test/sc_test.hpp>
#include <sc-memory/sc_memory.hpp>

#include "agents/find_central_district_agent.hpp"
#include "keynodes/transport_accessibility_keynodes.hpp"

using AgentTest = ScMemoryTest;

// Строим простой граф-цепочку из трёх районов:
// d1 --(route1)-- d2 --(route2)-- d3
// Центральным (по эксцентриситету) должен быть d2.
static void BuildThreeDistrictPathGraph(
    ScMemoryContext & ctx,
    ScAddr & graphAddr,
    ScAddr & centralDistrictAddr)
{
  graphAddr = ctx.GenerateNode(ScType::ConstNodeStruct);

  // районы
  ScAddr const d1 = ctx.GenerateNode(ScType::ConstNode);
  ScAddr const d2 = ctx.GenerateNode(ScType::ConstNode);
  ScAddr const d3 = ctx.GenerateNode(ScType::ConstNode);

  centralDistrictAddr = d2;

  // помечаем как concept_district
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

  // включаем в граф
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

  ScAddr const set12 = ctx.GenerateNode(ScType::ConstNodeStruct);
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

  ScAddr const set23 = ctx.GenerateNode(ScType::ConstNodeStruct);
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
}

TEST_F(AgentTest, FindCentralDistrictAgentFinishedSuccessfully)
{
  // 1. Регистрируем агент в тестовой sc-памяти
  m_ctx->SubscribeAgent<FindCentralDistrictAgent>();

  // 2. Строим тестовый граф и запоминаем ожидаемый центральный район (d2)
  ScAddr graphAddr;
  ScAddr expectedCentralDistrict;
  BuildThreeDistrictPathGraph(*m_ctx, graphAddr, expectedCentralDistrict);

  // 3. Создаём действие соответствующего класса
  ScAction action = m_ctx->GenerateAction(
      TransportAccessibilityKeynodes::action_find_central_district);

  // 4. Передаём граф как аргумент
  action.SetArguments(graphAddr);

  // 5. Запускаем действие
  EXPECT_TRUE(action.InitiateAndWait());
  EXPECT_TRUE(action.IsFinishedSuccessfully());

  // 6. Получаем структуру результата
  ScStructure const resultStructure = action.GetResult();
  EXPECT_FALSE(resultStructure.IsEmpty());

  // 7. Проверяем факт:
  // graphAddr => nrel_is_it_central_district: expectedCentralDistrict;;
  ScIterator5Ptr it = m_ctx->CreateIterator5(
      graphAddr,
      ScType::ConstCommonArc,
      expectedCentralDistrict,
      ScType::ConstPermPosArc,
      TransportAccessibilityKeynodes::nrel_is_it_central_district);

  EXPECT_TRUE(it->Next());

  m_ctx->UnsubscribeAgent<FindCentralDistrictAgent>();

}
