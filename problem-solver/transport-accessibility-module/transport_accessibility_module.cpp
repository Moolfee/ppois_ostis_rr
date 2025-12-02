
#include "transport-accessibility-module.hpp"
#include "agents/BridgeEdgesAgent.hpp"
#include "agents/BuildGraphAgent.hpp"
#include "agents/CentralDistrictAgent.hpp"
#include "agents/DFSConnectivityAgent.hpp"
#include "agents/SetDiameterAgent.hpp"
#include "agents/ShortestPathsAgent.hpp"


SC_MODULE_REGISTER(TransportAccessibilityModule)
    ->Agent<BridgeEdgesAgent>()
    ->Agent<BuildGgraphAgent>()
    ->Agent<CentralDistrictAgent>()
    ->Agent<DFSConnectivityAgent>()
    ->Agent<SetDiameterAgent>()
    ->Agent<ShortestPathsAgent>();
