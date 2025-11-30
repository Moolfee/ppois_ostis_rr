#include <sc-memory/test/sc_test.hpp>
#include <sc-memory/sc_memory.hpp>

#include "agents/analyze_transport_accessibility_agent.hpp"
#include "keynodes/transport_accessibility_keynodes.hpp"

using AgentTest = ScMemoryTest;

// Граф-цепочка из трёх районов:
//
//   d1 --(route1)-- d2 --(route2)-- d3
//
// Свойства, которые мы ожидаем после работы агента анализа:
// - граф связный (nrel_graph_connectivity = 1);
// - диаметр сети = 2 (nrel_network_diametr = 2);
// - центральный район = d2 (nrel_is_it_central_district).
static void BuildThreeDistrictPathGraph(
    ScMemoryContext & ctx,
    ScAddr & graphAddr,
    ScAddr & centralDistrictAddr)
{
  graphAddr = ctx.GenerateNode(ScType::ConstNodeStruct);

  // Районы
  ScAddr const d1 = ctx.GenerateNode(ScType::ConstNode);
  ScAddr const d2 = ctx.GenerateNode(ScType::ConstNode);
  ScAddr const d3 = ctx.GenerateNode(ScType::ConstNode);

  centralDistrictAddr = d2;

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

  // Включаем районы в граф
  ctx.GenerateConnector(ScType::ConstPermPosArc, graphAddr, d1);
  ctx.GenerateConnector(ScType::ConstPermPosArc, graphAddr, d2);
  ctx.GenerateConnector(ScType::ConstPermPosArc, graphAddr, d3);

  // ---------- route1: d1 <-> d2 ----------
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

  // ---------- route2: d2 <-> d3 ----------
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

TEST_F(AgentTest, AnalyzeTransportAccessibilityAgentCalculatesAllMetrics)
{
  // 1. Регистрируем агент анализа
  m_ctx->SubscribeAgent<AnalyzeTransportAccessibilityAgent>();

  // 2. Строим тестовый граф и запоминаем ожидаемый центральный район (d2)
  ScAddr graphAddr;
  ScAddr expectedCentralDistrict;
  BuildThreeDistrictPathGraph(*m_ctx, graphAddr, expectedCentralDistrict);

  // 3. Создаём действие типа action_analyze_transport_accessibility
  ScAction action = m_ctx->GenerateAction(
      TransportAccessibilityKeynodes::action_analyze_transport_accessibility);

  // 4. Передаём граф как аргумент
  action.SetArguments(graphAddr);

  // 5. Запускаем действие
  EXPECT_TRUE(action.InitiateAndWait());
  EXPECT_TRUE(action.IsFinishedSuccessfully());

  // 6. Структура результата не должна быть пустой
  ScStructure const resultStructure = action.GetResult();
  EXPECT_FALSE(resultStructure.IsEmpty());

  // ---------- Проверка связности ----------
  {
    ScAddr connectivityLink;

    ScIterator5Ptr it = m_ctx->CreateIterator5(
        graphAddr,
        ScType::ConstCommonArc,
        ScType::ConstNodeLink,
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_graph_connectivity);

    if (it->Next())
      connectivityLink = it->Get(2);

    EXPECT_TRUE(connectivityLink.IsValid()) << "nrel_graph_connectivity not found";

    uint32_t connectivity = 0;
    EXPECT_TRUE(m_ctx->GetLinkContent(connectivityLink, connectivity));
    EXPECT_EQ(connectivity, 1u) << "Graph must be connected";
  }

  // ---------- Проверка диаметра ----------
  {
    ScAddr diameterLink;

    ScIterator5Ptr it = m_ctx->CreateIterator5(
        graphAddr,
        ScType::ConstCommonArc,
        ScType::ConstNodeLink,
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_network_diametr);

    if (it->Next())
      diameterLink = it->Get(2);

    EXPECT_TRUE(diameterLink.IsValid()) << "nrel_network_diametr not found";

    uint32_t diameter = 0;
    EXPECT_TRUE(m_ctx->GetLinkContent(diameterLink, diameter));
    EXPECT_EQ(diameter, 2u) << "Diameter for path of length 2 must be 2";
  }

  // ---------- Проверка центрального района ----------
  {
    ScIterator5Ptr it = m_ctx->CreateIterator5(
        graphAddr,
        ScType::ConstCommonArc,
        expectedCentralDistrict,
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_is_it_central_district);

    EXPECT_TRUE(it->Next())
        << "Central district was not marked via nrel_is_it_central_district";
  }

  m_ctx->UnsubscribeAgent<AnalyzeTransportAccessibilityAgent>();
}
