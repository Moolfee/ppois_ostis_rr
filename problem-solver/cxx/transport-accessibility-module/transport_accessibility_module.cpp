#include "transport_accessibility_module.hpp"
#include "agents/find_shortest_route_agent.hpp"
#include "agents/dump_transport_graph_agent.hpp"
#include "agents/analyze_transport_accessibility_agent.hpp"

SC_MODULE_REGISTER(TransportAccessibilityModule)
  ->Agent<FindShortestRouteAgent>()
  ->Agent<DumpTransportGraphAgent>()
  ->Agent<AnalyzeTransportAccessibilityAgent>();
