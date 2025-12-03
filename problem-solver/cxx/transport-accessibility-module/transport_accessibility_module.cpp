#include "transport_accessibility_module.hpp"
#include "agents/check_graph_connectivity_agent.hpp"
#include "agents/find_shortest_route_agent.hpp"
#include "agents/analyze_transport_accessibility_agent.hpp"

SC_MODULE_REGISTER(TransportAccessibilityModule)
  ->Agent<CheckGraphConnectivityAgent>()
  ->Agent<FindShortestRouteAgent>()
  ->Agent<AnalyzeTransportAccessibilityAgent>();
