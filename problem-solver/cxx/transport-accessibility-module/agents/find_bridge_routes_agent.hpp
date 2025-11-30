#pragma once

#include <sc-memory/sc_agent.hpp>

class FindBridgeRoutesAgent : public ScActionInitiatedAgent
{
public:
  ScAddr GetActionClass() const override;
  
  ScResult DoProgram(ScAction & action) override;
};
