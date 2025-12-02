// problem-solver/transport-accessibility-module/keynodes/transport_accessibility_keynodes.hpp

#pragma once

#include <sc-memory/sc_keynodes.hpp>

class TransportAccessibilityKeynodes : public ScKeynodes
{
public:
  static inline ScKeynode const action_check_graph_connectivity{"action_check_graph_connectivity", ScType::ConstNodeClass};
  static inline ScKeynode const action_find_shortest_route{"action_find_shortest_route", ScType::ConstNodeClass};
  static inline ScKeynode const action_analyze_transport_accessibility{"action_analyze_transport_accessibility", ScType::ConstNodeClass};

  static inline ScKeynode const concept_district{"concept_district", ScType::ConstNodeClass};
  static inline ScKeynode const concept_public_transport_route{"concept_public_transport_route", ScType::ConstNodeClass};

  static inline ScKeynode const nrel_graph_connectivity{"nrel_graph_connectivity", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_shortest_route{"nrel_shortest_route", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_shortest_distance{"nrel_shortest_distance", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_is_it_central_district{"nrel_is_it_central_district", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_is_it_bridge_connection{"nrel_is_it_bridge_connection", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_network_diameter{"nrel_network_diameter", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_bridge_routes_count{"nrel_bridge_routes_count", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_district_availability{"nrel_district_availability", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_connects_districts{"nrel_connects_districts", ScType::ConstNodeNonRole};

  static inline ScKeynode const rrel_start_district{"rrel_start_district", ScType::ConstNodeRole};
  static inline ScKeynode const rrel_end_district{"rrel_end_district", ScType::ConstNodeRole};
  static inline ScKeynode const rrel_path_element{"rrel_path_element", ScType::ConstNodeRole};
  static inline ScKeynode const rrel_bridge_route{"rrel_bridge_route", ScType::ConstNodeRole};
  static inline ScKeynode const rrel_diameter_value{"rrel_diameter_value", ScType::ConstNodeRole};
};
