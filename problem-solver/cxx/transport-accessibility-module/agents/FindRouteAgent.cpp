#include "FindRouteAgent.hpp"

#include <sc-memory/sc_memory.hpp>
#include <sc-memory/sc_agent.hpp>
#include <sc-memory/sc_iterator.hpp>
#include <sc-memory/sc_link.hpp>

#include "keynodes/StreetCleaningKeynodes.hpp"
#include "errors/AgentsErrors.hpp"

#include <stack>
#include <map>
#include <algorithm>
#include <string>

#define EPS 1e-9
#define INF 1e9

ScAddr FindRouteAgent::GetActionClass() const
{
  return StreetCleaningKeynodes::action_find_route;
}

ScResult FindRouteAgent::DoProgram(ScAction & action)
{
  auto const & [networkAddr] = action.GetArguments<1>();

  if (!m_context.IsElement(networkAddr))
  {
    m_logger.Error("Дорожная сеть не найдена.");
    return action.FinishWithError();
  }

  ScAddrVector tempElements;
  tempToRealStreetMapping.clear();

  try
  {
    UpgradeToEulerianGraph(networkAddr, tempElements);

    ScAddr startVertex = GetStartVertex(networkAddr);

    ScAddrVector route = FindEulerianCycle(networkAddr);

    ScStructure resultStructure = CreateRouteStructure(networkAddr, route);
    action.SetResult(resultStructure);

    Cleanup(tempElements);
  }
  catch (std::exception & e)
  {
    m_logger.Error(e.what());
    Cleanup(tempElements);
    return action.FinishWithError();
  }

  return action.FinishSuccessfully();
}

ScAddrVector FindRouteAgent::FindEulerianCycle(ScAddr const & networkAddr)
{
  ScAddr startVertex = GetStartVertex(networkAddr);

  AdjacencyList adj;

  ScIterator3Ptr intersectionIt =
      m_context.CreateIterator3(networkAddr, ScType::ConstPermPosArc, StreetCleaningKeynodes::concept_intersection);

  while (intersectionIt->Next())
  {
    ScAddr intersection = intersectionIt->Get(2);

    ScIterator5Ptr streetIt = m_context.CreateIterator5(
        ScType::Unknown,
        ScType::ConstCommonArc,
        intersection,
        ScType::ConstPermPosArc,
        StreetCleaningKeynodes::nrel_connects);

    while (streetIt->Next())
    {
      ScAddr streetNode = streetIt->Get(0);

      ScIterator5Ptr neighborIt = m_context.CreateIterator5(
          streetNode,
          ScType::ConstCommonArc,
          ScType::Unknown,
          ScType::ConstPermPosArc,
          StreetCleaningKeynodes::nrel_connects);

      while (neighborIt->Next())
      {
        ScAddr neighbor = neighborIt->Get(2);
        if (neighbor != intersection)
          adj[intersection].push_back({streetNode, neighbor, false});
      }
    }
  }

  std::stack<std::pair<ScAddr, ScAddr>> pathStack;
  ScAddrVector path;

  pathStack.push({startVertex, ScAddr::Empty});

  while (!pathStack.empty())
  {
    ScAddr currentV = pathStack.top().first;
    bool hasUnusedEdge = false;

    if (adj.find(currentV) != adj.end())
    {
      for (auto & edge : adj[currentV])
      {
        if (!edge.isUsed)
        {
          edge.isUsed = true;

          for (auto & backEdge : adj[edge.targetIntersection])
          {
            if (backEdge.street == edge.street && backEdge.targetIntersection == currentV && !backEdge.isUsed)
            {
              backEdge.isUsed = true;
              break;
            }
          }

          pathStack.push({edge.targetIntersection, edge.street});
          hasUnusedEdge = true;
          break;
        }
      }
    }

    if (!hasUnusedEdge)
    {
      ScAddr street = pathStack.top().second;
      if (street.IsValid())
      {
        if (tempToRealStreetMapping.find(street) != tempToRealStreetMapping.end())
        {
          path.push_back(tempToRealStreetMapping[street]);
        }
        else
        {
          path.push_back(street);
        }
      }
      pathStack.pop();
    }
  }

  return path;
}

int FindRouteAgent::GetVertexDegree(ScAddr const & vertexAddr)
{
  ScIterator5Ptr it = m_context.CreateIterator5(
      vertexAddr,
      ScType::ConstCommonArc,
      ScType::ConstNodeLink,
      ScType::ConstPermPosArc,
      StreetCleaningKeynodes::nrel_vertex_degree);

  if (it->Next())
  {
    ScAddr link = it->Get(2);
    int degree = 0;
    m_context.GetLinkContent(link, degree);
    return degree;
  }
  else
    throw VertexDegreeNotFoundError("Степень вершины не посчитана");
}

ScAddr FindRouteAgent::GetStartVertex(ScAddr const & networkAddr)
{
  ScIterator5Ptr const it = m_context.CreateIterator5(
      networkAddr,
      ScType::ConstPermPosArc,
      StreetCleaningKeynodes::concept_intersection,
      ScType::ConstPermPosArc,
      StreetCleaningKeynodes::rrel_start);

  if (it->Next())
    return it->Get(2);
  else
    throw StartNodeNotFoundError("Начальная вершина не задана");
}

void FindRouteAgent::Cleanup(ScAddrVector const & tempElements)
{
  for (auto const & item : tempElements)
  {
    if (m_context.IsElement(item))
      m_context.EraseElement(item);
  }
}

ScStructure FindRouteAgent::CreateRouteStructure(ScAddr const & networkAddr, ScAddrVector route)
{
  ScStructure result = m_context.GenerateStructure();

  int pos = 1;
  double length = 0.0;

  ScAddr routeTuple = m_context.GenerateNode(ScType::ConstNodeClass);
  ScAddr const & arcCommonAddr = m_context.GenerateConnector(ScType::ConstCommonArc, networkAddr, routeTuple);
  ScAddr const & rrelOptimalRoute =
      m_context.GenerateConnector(ScType::ConstPermPosArc, StreetCleaningKeynodes::nrel_optimal_route, arcCommonAddr);

  ScAddr const & arcPermPosAddr = m_context.GenerateConnector(ScType::ConstPermPosArc, networkAddr, route[0]);
  ScAddr const & rrelStart =
      m_context.GenerateConnector(ScType::ConstPermPosArc, StreetCleaningKeynodes::rrel_start, arcPermPosAddr);

  result << routeTuple << networkAddr << arcCommonAddr << rrelOptimalRoute << arcPermPosAddr << rrelStart;
  for (auto const & node : route)
  {
    ScAddr const & positionNumber = m_context.CreateNode(ScType::ConstNode);
    ScAddr const & positionIdentificator = m_context.GenerateLink(ScType::ConstNodeLink);
    m_context.SetLinkContent(positionIdentificator, pos);

    ScAddr const & arcCommonAddr =
        m_context.GenerateConnector(ScType::ConstCommonArc, positionNumber, positionIdentificator);
    ScAddr const & nrelMainId =
        m_context.GenerateConnector(ScType::ConstPermPosArc, StreetCleaningKeynodes::nrel_main_idtf, arcCommonAddr);

    ScAddr const & arcPermPosAddr = m_context.GenerateConnector(ScType::ConstPermPosArc, routeTuple, node);
    ScAddr const & rrelPosition = m_context.GenerateConnector(ScType::ConstPermPosArc, positionNumber, arcPermPosAddr);
    result << node << positionNumber << positionIdentificator << arcCommonAddr << nrelMainId << arcPermPosAddr
           << rrelPosition;

    length += GetStreetLength(node);
    pos++;
  }

  ScAddr const & routeLength = m_context.GenerateLink(ScType::ConstNodeLink);
  m_context.SetLinkContent(routeLength, length);

  ScAddr const & arcCommonAddr2 = m_context.GenerateConnector(ScType::ConstCommonArc, routeTuple, routeLength);
  ScAddr const & nrelRouteLength =
      m_context.GenerateConnector(ScType::ConstPermPosArc, StreetCleaningKeynodes::nrel_route_length, arcCommonAddr2);

  result << routeLength << arcCommonAddr << nrelRouteLength;
  return result;
}

void FindRouteAgent::UpgradeToEulerianGraph(ScAddr const & networkAddr, ScAddrVector & tempElements)
{
  ScIterator3Ptr networkIt = m_context.CreateIterator3(
      StreetCleaningKeynodes::concept_network_with_euler_cycle, ScType::ConstPermPosArc, networkAddr);
  if (networkIt->Next())
    return;

  ScAddrVector oddVertices;
  ScIterator3Ptr intersectionIt =
      m_context.CreateIterator3(networkAddr, ScType::ConstPermPosArc, StreetCleaningKeynodes::concept_intersection);

  while (intersectionIt->Next())
  {
    ScAddr const vertex = intersectionIt->Get(2);
    if (GetVertexDegree(vertex) % 2 != 0)
      oddVertices.push_back(vertex);
  }

  int n = oddVertices.size();
  if (!oddVertices.size())
    return;

  std::vector<std::vector<double>> dists(n, std::vector<double>(n));
  std::map<std::pair<int, int>, std::list<ScAddr>> pathsCache;

  for (int i = 0; i < n; i++)
  {
    for (int j = i + 1; j < n; j++)
    {
      PathResult result = FindShortestPath(oddVertices[i], oddVertices[j]);

      if (result.edges.empty() && oddVertices[i] != oddVertices[j])
        result.distance = INF;

      dists[i][j] = dists[j][i] = result.distance;
      pathsCache[{i, j}] = result.edges;
      pathsCache[{j, i}] = ScAddrList(result.edges.rbegin(), result.edges.rend());
    }
  }

  int fullMask = (1 << n) - 1;
  std::vector<double> memo(1 << n, -1.0);

  double minWeight = SolveMatching(fullMask, n, dists, memo);

  std::vector<std::pair<int, int>> bestPairs;
  GetMatchingPairs(fullMask, n, dists, memo, bestPairs);

  for (auto const & pair : bestPairs)
  {
    ScAddrList edgesToDuplicate = pathsCache[pair];

    for (ScAddr const & street : edgesToDuplicate)
    {
      ScAddr intersection1, intersection2;
      bool foundEnds = false;

      ScIterator5Ptr endsIt = m_context.CreateIterator5(
          street,
          ScType::ConstCommonArc,
          StreetCleaningKeynodes::concept_intersection,
          ScType::ConstPermPosArc,
          StreetCleaningKeynodes::nrel_connects);

      if (endsIt->Next())
        intersection1 = endsIt->Get(2);
      if (endsIt->Next())
      {
        intersection2 = endsIt->Get(2);
        foundEnds = true;
      }

      if (foundEnds)
      {
        ScAddr tempStreet = m_context.GenerateNode(ScType::ConstNode);

        tempToRealStreetMapping[tempStreet] = street;
        ScAddr arcCommonAddr1 = m_context.GenerateConnector(ScType::ConstCommonArc, tempStreet, intersection1);
        ScAddr arcCommonAddr2 = m_context.GenerateConnector(ScType::ConstCommonArc, tempStreet, intersection2);
        ScAddr rel1 =
            m_context.GenerateConnector(ScType::ConstPermPosArc, StreetCleaningKeynodes::nrel_connects, arcCommonAddr1);
        ScAddr rel2 =
            m_context.GenerateConnector(ScType::ConstPermPosArc, StreetCleaningKeynodes::nrel_connects, arcCommonAddr2);

        tempElements.push_back(tempStreet);
        tempElements.push_back(arcCommonAddr1);
        tempElements.push_back(arcCommonAddr2);
        tempElements.push_back(rel1);
        tempElements.push_back(rel2);
      }
    }
  }
}

PathResult FindRouteAgent::FindShortestPath(ScAddr const & start, ScAddr const & end)
{
  if (start == end)
    return {0.0, {}};

  struct PQCompare
  {
    bool operator()(PQPair const & a, PQPair const & b) const
    {
      return a.first > b.first;
    }
  };

  std::priority_queue<PQPair, std::vector<PQPair>, PQCompare> pq;

  ScAddrToValueUnorderedMap<double> dist;
  ScAddrToValueUnorderedMap<ScAddr> parentNode;
  ScAddrToValueUnorderedMap<ScAddr> parentEdge;
  ScAddrToValueUnorderedMap<bool> visited;

  dist[start] = 0.0;
  pq.push({0.0, start});

  while (!pq.empty())
  {
    auto [length, intersection] = pq.top();
    pq.pop();

    if (visited[intersection])
      continue;
    visited[intersection] = true;

    if (intersection == end)
      break;

    ScIterator5Ptr streetIt = m_context.CreateIterator5(
        StreetCleaningKeynodes::concept_street,
        ScType::ConstCommonArc,
        intersection,
        ScType::ConstPermPosArc,
        StreetCleaningKeynodes::nrel_connects);

    while (streetIt->Next())
    {
      ScAddr street = streetIt->Get(0);
      double weight = GetStreetLength(street);

      ScIterator5Ptr neighborIt = m_context.CreateIterator5(
          street,
          ScType::ConstCommonArc,
          StreetCleaningKeynodes::concept_intersection,
          ScType::ConstPermPosArc,
          StreetCleaningKeynodes::nrel_connects);

      while (neighborIt->Next())
      {
        ScAddr neighbor = neighborIt->Get(2);
        if (neighbor == intersection)
          continue;

        double newDist = length + weight;

        if (dist.find(neighbor) == dist.end() || newDist < dist[neighbor])
        {
          dist[neighbor] = newDist;
          parentNode[neighbor] = intersection;
          parentEdge[neighbor] = street;
          pq.push({newDist, neighbor});
        }
      }
    }
  }

  if (dist.find(end) == dist.end())
  {
    return {INF, {}};
  }

  ScAddrList path;
  ScAddr curr = end;
  while (curr != start)
  {
    if (parentEdge.find(curr) == parentEdge.end())
    {
      return {INF, {}};
    }
    path.push_front(parentEdge[curr]);
    curr = parentNode[curr];
  }

  return {dist[end], path};
}

double FindRouteAgent::GetStreetLength(ScAddr const & streetAddr)
{
  ScIterator5Ptr it = m_context.CreateIterator5(
      streetAddr,
      ScType::ConstCommonArc,
      ScType::ConstNodeLink,
      ScType::ConstPermPosArc,
      StreetCleaningKeynodes::nrel_street_length);

  if (it->Next())
  {
    ScAddr linkAddr = it->Get(2);
    std::string lengthStr;

    if (m_context.GetLinkContent(linkAddr, lengthStr))
    {
      double length = std::stod(lengthStr);
      return length;
    }

    // int lengthInt = 0;
    // if (m_context.GetLinkContent(linkAddr, lengthInt))
    //   return static_cast<double>(lengthInt);
  }
  else
    throw NodeIsNotStreetError("узел не является улицей");
}

double FindRouteAgent::SolveMatching(
    int mask,
    int n,
    std::vector<std::vector<double>> const & dists,
    std::vector<double> & memo)
{
  if (mask == 0)
    return 0.0;
  if (memo[mask] >= 0.0)
    return memo[mask];

  double minVal = INF;

  int i = 0;
  while (!((mask >> i) & 1))
    i++;

  int maskWithoutI = mask ^ (1 << i);

  for (int j = i + 1; j < n; j++)
  {
    if ((mask >> j) & 1)
    {
      double res = SolveMatching(maskWithoutI ^ (1 << j), n, dists, memo);
      if (dists[i][j] + res < minVal)
        minVal = dists[i][j] + res;
    }
  }
  return memo[mask] = minVal;
}

void FindRouteAgent::GetMatchingPairs(
    int mask,
    int n,
    std::vector<std::vector<double>> const & dists,
    std::vector<double> const & memo,
    std::vector<std::pair<int, int>> & resultPairs)
{
  if (!mask)
    return;

  int i = 0;
  while (!((mask >> i) & 1))
    i++;

  double minVal = memo[mask];
  int maskWithoutI = mask ^ (1 << i);

  for (int j = i + 1; j < n; j++)
  {
    if ((mask >> j) & 1)
    {
      double subRes = memo[maskWithoutI ^ (1 << j)];
      if (subRes >= 0.0 && std::abs(dists[i][j] + subRes - minVal) < EPS)
      {
        resultPairs.push_back({i, j});
        GetMatchingPairs(maskWithoutI ^ (1 << j), n, dists, memo, resultPairs);
        return;
      }
    }
  }
}
