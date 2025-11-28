#include "GraphAnalysisAgent.hpp"

#include <sc-memory/sc_memory.hpp>

#include "keynodes/StreetCleaningKeynodes.hpp"

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

  ScAddrToValueUnorderedMap<int> vertexes = CalculateVertexesDegrees(networkAddr);

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

  ScIterator3Ptr const networkIt3 =
      m_context.CreateIterator3(networkAddr, ScType::ConstPermPosArc, StreetCleaningKeynodes::concept_intersection);

  while (networkIt3->Next())
  {
    ScAddr const vertexAddr = networkIt3->Get(2);
    int degree = 0;

    ScIterator5Ptr const vertexIt5 = m_context.CreateIterator5(
        StreetCleaningKeynodes::concept_street,
        ScType::ConstCommonArc,
        vertexAddr,
        ScType::ConstPermPosArc,
        StreetCleaningKeynodes::nrel_connects);

    while (vertexIt5->Next())
    {
      degree++;
    }

    vertexes[vertexAddr] = degree;
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

  for (auto const & [vertexAddr, degree] : vertexes)
  {
    ScAddr const & vertexDegreeAddr = m_context.GenerateLink(ScType::ConstNodeLink);
    m_context.SetLinkContent(vertexDegreeAddr, degree);

    ScAddr const & arcCommonAddr = m_context.GenerateConnector(ScType::ConstCommonArc, vertexAddr, vertexDegreeAddr);
    ScAddr const & nrelVertexDegreeAddr =
        m_context.GenerateConnector(ScType::ConstPermPosArc, StreetCleaningKeynodes::nrel_vertex_degree, arcCommonAddr);

    resultStructure << vertexDegreeAddr << arcCommonAddr << nrelVertexDegreeAddr << vertexAddr;
  }
  return resultStructure;
}