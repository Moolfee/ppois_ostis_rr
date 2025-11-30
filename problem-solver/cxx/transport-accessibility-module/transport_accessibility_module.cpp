#include "transport_accessibility_module.hpp"
#include "agent/check_graph_connectivity_agent.hpp"
#include "agent/find_shortest_route_agent.hpp"
#include "agent/find_central_district_agent.hpp"
#include "agent/find_bridge_routes_agent.hpp"
#include "agent/calculate_network_diameter_agent.hpp"
#include "agent/analyze_transport_accessibility_agent.hpp"

SC_MODULE_REGISTER(TransportAccessibilityModule)
  ->Agent<CheckGraphConnectivityAgent>()
  ->Agent<FindShortestRouteAgent>()
  ->Agent<FindCentralDistrictAgent>()
  ->Agent<FindBridgeRoutesAgent>()
  ->Agent<CalculateNetworkDiameterAgent>()
  ->Agent<AnalyzeTransportAccessibilityAgent>();
