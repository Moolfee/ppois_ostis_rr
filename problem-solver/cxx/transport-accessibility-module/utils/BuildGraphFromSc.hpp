#pragma once

#include <sc-memory/sc_memory.hpp>
#include <sc-memory/sc_iterator.hpp>
#include <sc-memory/sc_link.hpp>

#include "Graph.hpp"
#include "../keynodes/transport_accessibility_keynodes.hpp"

// Результат построения графа из SC-памяти
struct GraphFromScResult
{
    Graph graph;                      // наш C++ граф
    std::vector<ScAddr> districts;    // список районов (индекс → ScAddr)
};

// Построение графа транспортной сети из узла graphAddr.
// Использует sc-итераторы для извлечения районов и маршрутов.
GraphFromScResult BuildGraphFromSc(ScMemoryContext & ctx, ScAddr const & graphAddr);
