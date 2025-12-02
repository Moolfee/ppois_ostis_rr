#pragma once

#include <sc-memory/sc_agent.hpp>

// Агент строит кратчайшие расстояния между всеми парами районов (включая недостижимые),
// основываясь на графе транспортной сети, переданном в действие.
class FindShortestRouteAgent : public ScActionInitiatedAgent
{
public:
  ScAddr GetActionClass() const override;
  ScResult DoProgram(ScAction & action) override;
};
