#pragma once

#include <sc-memory/sc_agent.hpp>

class FindCentralDistrictAgent : public ScActionInitiatedAgent
{
public:
  // Агент запускается по классу действия поиска центрального района
  ScAddr GetActionClass() const override;
  
  // Выполняет поиск района с минимальным эксцентриситетом
  ScResult DoProgram(ScAction & action) override;
};
