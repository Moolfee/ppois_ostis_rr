
#pragma once

#include <sc-memory/sc_agent.hpp>

class BuildGraphAgent : public ScActionInitiatedAgent
{
public:
    BuildGraphAgent();  

    ScAddr GetActionClass() const override;
    
    ScResult DoProgram(ScAction & action) override;
};

