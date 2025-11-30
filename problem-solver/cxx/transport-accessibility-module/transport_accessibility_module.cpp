#include <StreetCleaningModule.hpp>

#include "agents/GraphAnalysisAgent.hpp"
#include "agents/FindRouteAgent.hpp"

SC_MODULE_REGISTER(StreetCleaningModule)->Agent<GraphAnalysisAgent>()->Agent<FindRouteAgent>();