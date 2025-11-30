#pragma once

#include <sc-memory/sc_agent.hpp>

#include "utils/Edge.hpp"
#include "utils/PathResult.hpp"

class FindRouteAgent : public ScActionInitiatedAgent
{
public:
  ScAddr GetActionClass() const override;
  ScResult DoProgram(ScAction & action) override;

private:
  using AdjacencyList = std::map<ScAddr, std::vector<Edge>, ScAddrLessFunc>;
  using PQPair = std::pair<double, ScAddr>;

  ScAddrToValueUnorderedMap<ScAddr> tempToRealStreetMapping;

  void UpgradeToEulerianGraph(ScAddr const & networkAddr, ScAddrVector & tempElements);
  ScAddrVector FindEulerianCycle(ScAddr const & networkAddr);
  ScStructure CreateRouteStructure(ScAddr const & networkAddr, ScAddrVector route);
  void Cleanup(ScAddrVector const & tempElements);
  int GetVertexDegree(ScAddr const & vertexAddr);
  ScAddr GetStartVertex(ScAddr const & networkAddr);

  void GetMatchingPairs(
      int mask,
      int n,
      std::vector<std::vector<double>> const & dists,
      std::vector<double> const & memo,
      std::vector<std::pair<int, int>> & resultPairs);
  double SolveMatching(int mask, int n, std::vector<std::vector<double>> const & dists, std::vector<double> & memo);
  PathResult FindShortestPath(ScAddr const & start, ScAddr const & end);
  double GetStreetLength(ScAddr const & streetAddr);
};