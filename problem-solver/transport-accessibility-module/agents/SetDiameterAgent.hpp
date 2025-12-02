
#pragma once

#include <sc-memory/sc_agent.hpp>

class SetDiameterAgent : public ScActionInitiatedAgent
{
public:
    SetDiameterAgent();  

    ScAddr GetActionClass() const override;
    
    ScResult DoProgram(ScAction & action) override;
};
