#pragma once

#include <sc-memory/sc_addr.hpp>
#include <sc-memory/sc_keynodes.hpp>

class StreetCleaningKeynodes : public ScKeynodes
{
public:
  // actions
  static inline ScKeynode const action_graph_analyse{"action_graph_analyse", ScType::ConstNodeClass};
  static inline ScKeynode const action_find_route{"action_find_route", ScType::ConstNodeClass};

  // nrels
  static inline ScKeynode const nrel_connects{"nrel_connects", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_optimal_route{"nrel_optimal_route", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_route_length{"nrel_route_length", ScType::ConstNodeNonRole};
  static inline ScKeynode const nrel_street_length{"nrel_street_length", ScType::ConstNodeNonRole};
  static inline ScKeynode const rrel_position{"rrel_position", ScType::ConstNodeRole};
  static inline ScKeynode const nrel_vertex_degree{"nrel_vertex_degree", ScType::ConstNodeNonRole};
  static inline ScKeynode const rrel_start{"rrel_start", ScType::ConstNodeRole};
  static inline ScKeynode const rrel_finish{"rrel_finish", ScType::ConstNodeRole};
  static inline ScKeynode const nrel_main_idtf{"nrel_main_idtf", ScType::ConstNodeNonRole};

  // concepts
  static inline ScKeynode const concept_intersection{"concept_intersection", ScType::ConstNodeClass};
  static inline ScKeynode const concept_network_with_euler_cycle{
      "concept_network_with_euler_cycle",
      ScType::ConstNodeClass};
  static inline ScKeynode const concept_street_network{"concept_street_network", ScType::ConstNodeClass};
  static inline ScKeynode const concept_street{"concept_street", ScType::ConstNodeClass};
};
