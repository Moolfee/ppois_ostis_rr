#include "BuildGraphFromSc.hpp"

#include <map>

GraphFromScResult BuildGraphFromSc(ScMemoryContext & ctx, ScAddr const & graphAddr)
{
  GraphFromScResult result;

  // ------------------------------
  // 1. Собираем районы
  // ------------------------------
  std::vector<ScAddr> districts;

  auto collectDistricts = [&](ScType const arcType)
  {
    ScIterator3Ptr it = ctx.CreateIterator3(
        graphAddr,
        arcType,
        ScType::Node);  // принимаем любой node (const/var)

    while (it->Next())
    {
      ScAddr elem = it->Get(2);

      // elem <- concept_district ? (const/var дуга)
      ScIterator3Ptr itDistrictClassConst = ctx.CreateIterator3(
          TransportAccessibilityKeynodes::concept_district,
          ScType::ConstPermPosArc,
          elem);

      ScIterator3Ptr itDistrictClassVar = ctx.CreateIterator3(
          TransportAccessibilityKeynodes::concept_district,
          ScType::VarPermPosArc,
          elem);

      if (itDistrictClassConst->Next() || itDistrictClassVar->Next())
        districts.push_back(elem);
    }
  };

  collectDistricts(ScType::ConstPermPosArc);
  collectDistricts(ScType::VarPermPosArc);

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

  auto collectRoutes = [&](ScType const arcType)
  {
    ScIterator3Ptr it = ctx.CreateIterator3(
        graphAddr,
        arcType,
        ScType::Node);

    while (it->Next())
    {
      ScAddr elem = it->Get(2);

      // elem <- concept_public_transport_route ? (const/var дуга)
      ScIterator3Ptr itRouteClassConst = ctx.CreateIterator3(
          TransportAccessibilityKeynodes::concept_public_transport_route,
          ScType::ConstPermPosArc,
          elem);

      ScIterator3Ptr itRouteClassVar = ctx.CreateIterator3(
          TransportAccessibilityKeynodes::concept_public_transport_route,
          ScType::VarPermPosArc,
          elem);

      if (itRouteClassConst->Next() || itRouteClassVar->Next())
        routes.push_back(elem);
    }
  };

  collectRoutes(ScType::ConstPermPosArc);
  collectRoutes(ScType::VarPermPosArc);

  // ------------------------------
  // 4. Для каждого маршрута добавляем ребро
  // ------------------------------
  for (ScAddr const & route : routes)
  {
    std::vector<ScAddr> pairDistricts;

    // --- Вариант 1: через множество districtSet
    {
      ScAddr districtSet;

      auto tryFindSet = [&](ScType arcTypeRole) -> bool
      {
        ScIterator5Ptr it = ctx.CreateIterator5(
            route,
            ScType::ConstCommonArc,
            ScType::Node,
            arcTypeRole,
            TransportAccessibilityKeynodes::nrel_connects_districts);

        if (it->Next())
        {
          districtSet = it->Get(2);
          return true;
        }
        return false;
      };

      if (tryFindSet(ScType::ConstPermPosArc) || tryFindSet(ScType::VarPermPosArc))
      {
        auto collectFromSet = [&](ScType arcType)
        {
          ScIterator3Ptr it = ctx.CreateIterator3(
              districtSet,
              arcType,
              ScType::Node);

          while (it->Next())
            pairDistricts.push_back(it->Get(2));
        };

        collectFromSet(ScType::ConstPermPosArc);
        collectFromSet(ScType::VarPermPosArc);
      }
    }

    // --- Вариант 2: прямые дуги route => nrel_connects_districts: district
    auto collectDirect = [&](ScType arcCommonType, ScType arcRoleType)
    {
      ScIterator5Ptr it = ctx.CreateIterator5(
          route,
          arcCommonType,
          ScType::Node,
          arcRoleType,
          TransportAccessibilityKeynodes::nrel_connects_districts);

      while (it->Next())
      {
        ScAddr candidate = it->Get(2);
        ScIterator3Ptr itDistrictClassConst = ctx.CreateIterator3(
            TransportAccessibilityKeynodes::concept_district,
            ScType::ConstPermPosArc,
            candidate);
        ScIterator3Ptr itDistrictClassVar = ctx.CreateIterator3(
            TransportAccessibilityKeynodes::concept_district,
            ScType::VarPermPosArc,
            candidate);

        if (itDistrictClassConst->Next() || itDistrictClassVar->Next())
          pairDistricts.push_back(candidate);
      }
    };

    collectDirect(ScType::ConstCommonArc, ScType::ConstPermPosArc);
    collectDirect(ScType::ConstCommonArc, ScType::VarPermPosArc);
    collectDirect(ScType::VarCommonArc, ScType::ConstPermPosArc);
    collectDirect(ScType::VarCommonArc, ScType::VarPermPosArc);

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
