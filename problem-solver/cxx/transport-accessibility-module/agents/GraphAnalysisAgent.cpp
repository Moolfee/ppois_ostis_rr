#include "GraphAnalysisAgent.hpp"

#include <sc-memory/sc_memory.hpp>

#include "keynodes/StreetCleaningKeynodes.hpp"

#include <exception>
#include <string>

ScAddr GraphAnalysisAgent::GetActionClass() const
{
  return StreetCleaningKeynodes::action_graph_analyse;
}

ScResult GraphAnalysisAgent::DoProgram(ScAction & action)
{
  auto const & [networkAddr] = action.GetArguments<1>();

  if (!m_context.IsElement(networkAddr))
  {
    m_logger.Error("Дорожная сеть не найдена");
    return action.FinishWithError();
  }

  m_logger.Info("Начало анализа сети: " + m_context.GetElementSystemIdentifier(networkAddr));
  ScAddrToValueUnorderedMap<int> vertexes = CalculateVertexesDegrees(networkAddr);

  m_logger.Info("Рассчет вершин окончен. Найдено: " + std::to_string(vertexes.size()) + " вершин.");

  EulerianStatus graphStatus;
  graphStatus = GetGraphEulerianStatus(networkAddr, vertexes);

  ScStructure analysisResult = CreateAnalysisResult(networkAddr, vertexes, graphStatus);
  action.SetResult(analysisResult);

  ScAction routeAction = m_context.GenerateAction(StreetCleaningKeynodes::action_find_route);
  routeAction.SetArguments(networkAddr);
  routeAction.Initiate();

  return action.FinishSuccessfully();
}

ScAddrToValueUnorderedMap<int> GraphAnalysisAgent::CalculateVertexesDegrees(ScAddr const & networkAddr)
{
  ScAddrToValueUnorderedMap<int> vertexes;
  m_logger.Debug("Поиск перекрестков...");

  ScIterator3Ptr const networkIt3 = m_context.CreateIterator3(networkAddr, ScType::ConstPermPosArc, ScType::Unknown);

  while (networkIt3->Next())
  {
    ScAddr const vertexAddr = networkIt3->Get(2);

    ScIterator3Ptr const isIntersectionIt =
        m_context.CreateIterator3(StreetCleaningKeynodes::concept_intersection, ScType::ConstPermPosArc, vertexAddr);

    if (isIntersectionIt->Next())
    {
      int degree = 0;
      ScIterator5Ptr const vertexIt5 = m_context.CreateIterator5(
          ScType::Unknown,
          ScType::ConstCommonArc,
          vertexAddr,
          ScType::ConstPermPosArc,
          StreetCleaningKeynodes::nrel_connects);

      while (vertexIt5->Next())
      {
        degree++;
      }

      m_logger.Info(
          "Найден перекресток: "
          + (m_context.GetElementSystemIdentifier(vertexAddr).empty()
                 ? "addr:" + std::to_string(vertexAddr.Hash())
                 : m_context.GetElementSystemIdentifier(vertexAddr))
          + " | Степень: " + std::to_string(degree));

      vertexes[vertexAddr] = degree;
    }
  }

  return vertexes;
}

EulerianStatus GraphAnalysisAgent::GetGraphEulerianStatus(
    ScAddr const & networkAddr,
    ScAddrToValueUnorderedMap<int> intersections)
{
  int oddDegreeCount = 0;
  int intersectionsCount = 0;
  ScAddr startBfsNode = ScAddr::Empty;

  for (auto const & [vertex, degree] : intersections)
  {
    if (degree % 2 != 0)
    {
      oddDegreeCount++;
    }

    if (degree > 0)
    {
      intersectionsCount++;
      if (!startBfsNode.IsValid())
        startBfsNode = vertex;
    }
  }
  m_logger.Debug("Количество нечетных вершин: " + std::to_string(oddDegreeCount));

  ScAddrSet visitedIntersections;
  ScAddrQueue queue;

  if (startBfsNode.IsValid())
  {
    queue.push(startBfsNode);
    visitedIntersections.insert(startBfsNode);
  }

  while (!queue.empty())
  {
    ScAddr const currentIntersection = queue.front();
    queue.pop();

    ScIterator5Ptr const streetIt = m_context.CreateIterator5(
        ScType::Unknown,
        ScType::ConstCommonArc,
        currentIntersection,
        ScType::ConstPermPosArc,
        StreetCleaningKeynodes::nrel_connects);

    while (streetIt->Next())
    {
      ScAddr const streetNode = streetIt->Get(0);

      ScIterator5Ptr const neighborIt = m_context.CreateIterator5(
          streetNode,
          ScType::ConstCommonArc,
          StreetCleaningKeynodes::concept_intersection,
          ScType::ConstPermPosArc,
          StreetCleaningKeynodes::nrel_connects);

      while (neighborIt->Next())
      {
        ScAddr const nextIntersection = neighborIt->Get(2);

        if (nextIntersection != currentIntersection && intersections.find(nextIntersection) != intersections.end()
            && visitedIntersections.find(nextIntersection) == visitedIntersections.end())
        {
          visitedIntersections.insert(nextIntersection);
          queue.push(nextIntersection);
        }
      }
    }
  }

  if (oddDegreeCount == 0 && visitedIntersections.size() == intersectionsCount)
  {
    return EulerianStatus::Cycle;
  }
  else
  {
    return EulerianStatus::None;
  }
}

ScStructure GraphAnalysisAgent::CreateAnalysisResult(
    ScAddr const & networkAddr,
    ScAddrToValueUnorderedMap<int> vertexes,
    EulerianStatus status)
{
  ScStructure resultStructure = m_context.GenerateStructure();

  if (status == EulerianStatus::Cycle)
  {
    ScAddr const graphType = m_context.GenerateConnector(
        ScType::ConstPermPosArc, StreetCleaningKeynodes::concept_network_with_euler_cycle, networkAddr);
    resultStructure << graphType;
  }

  resultStructure << networkAddr;

  for (auto const & [vertexAddr, degree] : vertexes)
  {
    ScAddr const & vertexDegreeAddr = m_context.GenerateLink(ScType::ConstNodeLink);
    try
    {
      m_context.SetLinkContent(vertexDegreeAddr, std::to_string(degree));
    }
    catch (std::exception & e)
    {
      m_logger.Error(e.what());
    }

    ScAddr const & arcCommonAddr = m_context.GenerateConnector(ScType::ConstCommonArc, vertexAddr, vertexDegreeAddr);
    ScAddr const & nrelVertexDegreeAddr =
        m_context.GenerateConnector(ScType::ConstPermPosArc, StreetCleaningKeynodes::nrel_vertex_degree, arcCommonAddr);

    ScIterator3Ptr edgeIt = m_context.CreateIterator3(networkAddr, ScType::ConstPermPosArc, vertexAddr);
    if (edgeIt->Next())
      resultStructure << edgeIt->Get(1) << edgeIt->Get(2);

    resultStructure << vertexDegreeAddr << arcCommonAddr << nrelVertexDegreeAddr;
  }
  return resultStructure;
}