#pragma once

#include <sc-memory/sc_agent.hpp>

class CheckGraphConnectivityAgent : public ScActionInitiatedAgent
{
public:
  ScAddr GetActionClass() const override;

  ScResult DoProgram(ScAction & action) override;
};
