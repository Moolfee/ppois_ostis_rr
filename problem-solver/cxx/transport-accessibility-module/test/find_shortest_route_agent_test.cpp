#include <sc-memory/test/sc_test.hpp>
#include <sc-memory/sc_memory.hpp>

#include "agents/find_shortest_route_agent.hpp"
#include "keynodes/transport_accessibility_keynodes.hpp"

using AgentTest = ScMemoryTest;

// Строим граф из трёх районов:
//
//   d1 --(route1)-- d2 --(route2)-- d3
//
// Кратчайший путь между d1 и d3 должен содержать 2 маршрута: route1 и route2.
static void BuildThreeDistrictPathGraph(
    ScMemoryContext & ctx,
    ScAddr & graphAddr,
    ScAddr & startDistrict,
    ScAddr & endDistrict)
{
  graphAddr = ctx.GenerateNode(ScType::ConstNodeStruct);

  // Районы
  ScAddr const d1 = ctx.GenerateNode(ScType::ConstNode);
  ScAddr const d2 = ctx.GenerateNode(ScType::ConstNode);
  ScAddr const d3 = ctx.GenerateNode(ScType::ConstNode);

  startDistrict = d1;
  endDistrict = d3;

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

TEST_F(AgentTest, FindShortestRouteAgentFindsCorrectPath)
{
  // 1. Регистрируем агент в sc-памяти
  m_ctx->SubscribeAgent<FindShortestRouteAgent>();

  // 2. Строим тестовый граф и задаём старт/финиш
  ScAddr graphAddr;
  ScAddr startDistrict;
  ScAddr endDistrict;
  BuildThreeDistrictPathGraph(*m_ctx, graphAddr, startDistrict, endDistrict);

  // 3. Создаём действие класса action_find_shortest_route
  ScAction action = m_ctx->GenerateAction(
      TransportAccessibilityKeynodes::action_find_shortest_route);

  // 4. Передаём аргументы: граф, стартовый район, конечный район
  // (ориентируемся на современный ostis, где SetArguments поддерживает несколько аргументов)
  action.SetArguments(graphAddr, startDistrict, endDistrict);

  // 5. Инициируем действие и ждём завершения
  EXPECT_TRUE(action.InitiateAndWait());
  EXPECT_TRUE(action.IsFinishedSuccessfully());

  // 6. Структура результата не должна быть пустой
  ScStructure const resultStructure = action.GetResult();
  EXPECT_FALSE(resultStructure.IsEmpty());

  // 7. Ищем "кортеж кратчайшего маршрута":
  //
  // graphAddr => nrel_shortest_route: routeTuple;;
  //
  ScAddr routeTuple;

  {
    ScIterator5Ptr it = m_ctx->CreateIterator5(
        graphAddr,
        ScType::ConstCommonArc,
        ScType::ConstNode,  // routeTuple
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::nrel_shortest_route);

    ASSERT_TRUE(it->Next()) << "Shortest route tuple not found";
    routeTuple = it->Get(2);
    EXPECT_TRUE(routeTuple.IsValid());
  }

  // 8. Проверяем, что старт и финиш помечены правильно:
  //
  // routeTuple ->(common)-> startDistrict (* rrel_start_district *);
  // routeTuple ->(common)-> endDistrict   (* rrel_end_district   *);
  //
  {
    ScIterator5Ptr itStart = m_ctx->CreateIterator5(
        routeTuple,
        ScType::ConstCommonArc,
        startDistrict,
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::rrel_start_district);

    EXPECT_TRUE(itStart->Next()) << "Start district not marked correctly";

    ScIterator5Ptr itEnd = m_ctx->CreateIterator5(
        routeTuple,
        ScType::ConstCommonArc,
        endDistrict,
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::rrel_end_district);

    EXPECT_TRUE(itEnd->Next()) << "End district not marked correctly";
  }

  // 9. Считаем количество path-element'ов:
  //
  // routeTuple ->(common)-> route_i (* rrel_path_element *);
  //
  uint32_t pathEdgesCount = 0;

  {
    ScIterator5Ptr it = m_ctx->CreateIterator5(
        routeTuple,
        ScType::ConstCommonArc,
        ScType::ConstNode,  // маршрут
        ScType::ConstPermPosArc,
        TransportAccessibilityKeynodes::rrel_path_element);

    while (it->Next())
    {
      ++pathEdgesCount;
    }
  }

  // Кратчайший путь между d1 и d3 в нашем графе должен состоять ровно из 2 маршрутов
  EXPECT_EQ(pathEdgesCount, 2u);

  m_ctx->UnsubscribeAgent<FindShortestRouteAgent>();
}
