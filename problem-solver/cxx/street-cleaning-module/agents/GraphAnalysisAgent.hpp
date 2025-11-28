#pragma once

#include <sc-memory/sc_agent.hpp>
#include "utils/EulerianStatus.hpp"

class GraphAnalysisAgent : public ScActionInitiatedAgent
{
public:
  ScAddr GetActionClass() const override;
  ScResult DoProgram(ScAction & action) override;

private:
  ScAddrToValueUnorderedMap<int> CalculateVertexesDegrees(ScAddr const & networkAddr);
  EulerianStatus GetGraphEulerianStatus(ScAddr const & networkAddr, ScAddrToValueUnorderedMap<int> intersections);
  ScStructure CreateAnalysisResult(ScAddr const & networkAddr, ScAddrToValueUnorderedMap<int> vertexes, EulerianStatus);
};
