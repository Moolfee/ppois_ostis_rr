#include "BuildGraphFromSc.hpp"

#include <map>

GraphFromScResult BuildGraphFromSc(ScMemoryContext & ctx, ScAddr const & graphAddr)
{
  GraphFromScResult result;

  // ------------------------------
  // 1. Собираем районы
  // ------------------------------
  std::vector<ScAddr> districts;

  {
    ScIterator3Ptr it = ctx.CreateIterator3(
        graphAddr,
        ScType::ConstPermPosArc,
        ScType::ConstNode);

    while (it->Next())
    {
      ScAddr elem = it->Get(2);

      // elem <- concept_district ?
      ScIterator3Ptr itDistrictClass = ctx.CreateIterator3(
          TransportAccessibilityKeynodes::concept_district,
          ScType::ConstPermPosArc,
          elem);

      if (itDistrictClass->Next())
        districts.push_back(elem);
    }
  }

  int n = static_cast<int>(districts.size());
  result.districts = districts;
  result.graph = Graph(n);

  if (n == 0)
    return result;

  // ------------------------------
  // 2. Индекс района ScAddr -> int
  // ------------------------------
  std::map<ScAddr, int, ScAddrLessFunc> index;

  for (int i = 0; i < n; ++i)
    index[districts[i]] = i;

  // ------------------------------
  // 3. Собираем маршруты
  // ------------------------------
  std::vector<ScAddr> routes;

  {
    ScIterator3Ptr it = ctx.CreateIterator3(
        graphAddr,
        ScType::ConstPermPosArc,
        ScType::ConstNode);

    while (it->Next())
    {
      ScAddr elem = it->Get(2);

      // elem <- concept_public_transport_route ?
      ScIterator3Ptr itRouteClass = ctx.CreateIterator3(
          TransportAccessibilityKeynodes::concept_public_transport_route,
          ScType::ConstPermPosArc,
          elem);

      if (itRouteClass->Next())
        routes.push_back(elem);
    }
  }

  // ------------------------------
  // 4. Для каждого маршрута добавляем ребро
  // ------------------------------
  for (ScAddr const & route : routes)
  {
    // route => nrel_connects_districts: districtSet;;
    ScAddr districtSet;

    {
      ScIterator5Ptr it = ctx.CreateIterator5(
          route,
          ScType::ConstCommonArc,
          ScType::ConstNode,  // множество
          ScType::ConstPermPosArc,
          TransportAccessibilityKeynodes::nrel_connects_districts);

      if (!it->Next())
        continue;

      districtSet = it->Get(2);
    }

    std::vector<ScAddr> pairDistricts;

    {
      ScIterator3Ptr it = ctx.CreateIterator3(
          districtSet,
          ScType::ConstPermPosArc,
          ScType::ConstNode);

      while (it->Next())
        pairDistricts.push_back(it->Get(2));
    }

    if (pairDistricts.size() != 2)
      continue;

    ScAddr d1 = pairDistricts[0];
    ScAddr d2 = pairDistricts[1];

    if (!index.count(d1) || !index.count(d2))
      continue;

    int u = index[d1];
    int v = index[d2];

    result.graph.AddEdge(u, v);
  }

  return result;
}
