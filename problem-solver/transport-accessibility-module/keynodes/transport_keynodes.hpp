
#pragma once

#include <sc-memory/sc_addr.hpp>
#include <sc-memory/sc_keynodes.hpp>

class TransportKeynodes : public ScKeynodes
{
public:
  static inline ScKeynode const action_build_graph{"action_build_graph", ScType::ConstNodeClass};
  static inline ScKeynode const action_check_DFS_connectivity{"action_check_DFS_connectivity", ScType::ConstNodeClass};
  static inline ScKeynode const action_find_shortest_paths{"action_find_shortest_paths", ScType::ConstNodeClass};
  static inline ScKeynode const action_find_central_district{"action_find_central_district", ScType::ConstNodeClass};
  static inline ScKeynode const action_find_bridge_edges{"action_find_bridge_edges", ScType::ConstNodeClass};
  static inline ScKeynode const action_calculate_diameter{"action_calculate_diameter",ScType::ConstNodeClass};
};
