#pragma once

#include <sc-memory/sc_agent.hpp>

class BridgeEdgesAgent : public ScActionInitiatedAgent
{
public:
    BridgeEdgesAgent();  

    ScAddr GetActionClass() const override;
    
    ScResult DoProgram(ScAction & action) override;
};
