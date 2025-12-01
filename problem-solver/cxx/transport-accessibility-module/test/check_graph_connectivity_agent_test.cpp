#include <sc-memory/test/sc_test.hpp>
#include <sc-memory/sc_memory.hpp>

#include "agents/check_graph_connectivity_agent.hpp"
#include "keynodes/transport_accessibility_keynodes.hpp"

using AgentTest = ScMemoryTest;

// Вспомогательная функция: строим простой СВЯЗНЫЙ граф из двух районов
// d1 -- route -- d2
static ScAddr BuildConnectedTestGraph(ScMemoryContext & ctx)
{
  // узел графа
  ScAddr const graphAddr = ctx.GenerateNode(ScType::ConstNodeStructure);

  // два района
  ScAddr const district1 = ctx.GenerateNode(ScType::ConstNode);
  ScAddr const district2 = ctx.GenerateNode(ScType::ConstNode);

  // помечаем их как районы (concept_district)
  ctx.GenerateConnector(
      ScType::ConstPermPosArc,
      TransportAccessibilityKeynodes::concept_district,
      district1);

  ctx.GenerateConnector(
      ScType::ConstPermPosArc,
      TransportAccessibilityKeynodes::concept_district,
      district2);

  // включаем районы в граф
  ctx.GenerateConnector(ScType::ConstPermPosArc, graphAddr, district1);
  ctx.GenerateConnector(ScType::ConstPermPosArc, graphAddr, district2);

  // маршрут, соединяющий районы
  ScAddr const route = ctx.GenerateNode(ScType::ConstNode);

  ctx.GenerateConnector(
      ScType::ConstPermPosArc,
      TransportAccessibilityKeynodes::concept_public_transport_route,
      route);

  // включаем маршрут в граф
  ctx.GenerateConnector(ScType::ConstPermPosArc, graphAddr, route);

  // множество районов, которые соединяет маршрут
  ScAddr const districtsSet = ctx.GenerateNode(ScType::ConstNodeStructure);
  ctx.GenerateConnector(ScType::ConstPermPosArc, districtsSet, district1);
  ctx.GenerateConnector(ScType::ConstPermPosArc, districtsSet, district2);

  // route => nrel_connects_districts: districtsSet;;
  ScAddr const arcCommon = ctx.GenerateConnector(
      ScType::ConstCommonArc,
      route,
      districtsSet);

  ctx.GenerateConnector(
      ScType::ConstPermPosArc,
      TransportAccessibilityKeynodes::nrel_connects_districts,
      arcCommon);

  return graphAddr;
}

TEST_F(AgentTest, CheckGraphConnectivityAgentFinishedSuccessfully)
{
  // 1. Регистрируем агент в тестовой sc-памяти
  m_ctx->SubscribeAgent<CheckGraphConnectivityAgent>();

  // 2. Строим тестовый СВЯЗНЫЙ граф
  ScAddr const graphAddr = BuildConnectedTestGraph(*m_ctx);

  // 3. Создаём действие с нужным классом (как в методичке)
  ScAction action = m_ctx->GenerateAction(
      TransportAccessibilityKeynodes::action_check_graph_connectivity);

  // 4. Передаём граф как аргумент действия
  action.SetArguments(graphAddr);

  // 5. Инициируем действие и ждём завершения
  EXPECT_TRUE(action.InitiateAndWait());

  // 6. Проверяем, что действие завершилось успешно
  EXPECT_TRUE(action.IsFinishedSuccessfully());

  // 7. Получаем структуру результата
  ScStructure const resultStructure = action.GetResult();
  EXPECT_FALSE(resultStructure.IsEmpty());

  // 8. Ищем в памяти отношение graphAddr => nrel_graph_connectivity: <link>;
  ScAddr connectivityLink;

  ScIterator5Ptr it = m_ctx->CreateIterator5(
      graphAddr,
      ScType::ConstCommonArc,
      ScType::ConstNodeLink,
      ScType::ConstPermPosArc,
      TransportAccessibilityKeynodes::nrel_graph_connectivity);

  if (it->Next())
    connectivityLink = it->Get(2);

  // Должен быть найден link с результатом
  EXPECT_TRUE(connectivityLink.IsValid());

  // 9. Проверяем значение link'а (1 — граф связный)
  int connectivityValue = 0;
  EXPECT_TRUE(m_ctx->GetLinkContent(connectivityLink, connectivityValue));
  EXPECT_EQ(1, connectivityValue);

    m_ctx->UnsubscribeAgent<CheckGraphConnectivityAgent>();

}
