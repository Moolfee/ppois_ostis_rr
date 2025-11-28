#pragma once

#include <sc-memory/sc_memory.hpp>

struct Edge
{
  ScAddr street;
  ScAddr targetIntersection;
  bool isUsed;

  bool operator==(Edge const & other) const
  {
    return street == other.street && targetIntersection == other.targetIntersection;
  }
};