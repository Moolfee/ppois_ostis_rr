#include "BuildGraphFromSc.hpp"

#include <map>
#include <set>

GraphFromScResult BuildGraphFromSc(ScMemoryContext & ctx, ScAddr const & graphAddr)
{
  GraphFromScResult result;

  // ------------------------------
  // 1. Собираем районы (включая те, что встретятся только в маршрутах)
  // ------------------------------
  std::vector<ScAddr> districts;
  std::set<ScAddr, ScAddrLessFunc> districtSeen;

  auto addDistrict = [&](ScAddr const & d)
  {
    if (d.IsValid() && districtSeen.insert(d).second)
      districts.push_back(d);
  };

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
        addDistrict(elem);
    }
  };

  collectDistricts(ScType::ConstPermPosArc);
  collectDistricts(ScType::VarPermPosArc);

  // ------------------------------
  // 2. Собираем маршруты и их конечные районы
  // ------------------------------
  std::vector<ScAddr> routes;
  std::vector<std::vector<ScAddr>> routeEndpoints;  // по индексу в routes

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
      {
        routes.push_back(elem);
        routeEndpoints.emplace_back();
      }
    }
  };

  collectRoutes(ScType::ConstPermPosArc);
  collectRoutes(ScType::VarPermPosArc);

  // убираем дубликаты маршрутов (сохраняем первый встретившийся)
  {
    std::set<ScAddr, ScAddrLessFunc> seen;
    std::vector<ScAddr> uniqueRoutes;
    std::vector<std::vector<ScAddr>> uniqueEndpoints;
    for (size_t idx = 0; idx < routes.size(); ++idx)
    {
      ScAddr const & r = routes[idx];
      if (seen.insert(r).second)
      {
        uniqueRoutes.push_back(r);
        uniqueEndpoints.push_back(routeEndpoints[idx]);
      }
    }
    routes.swap(uniqueRoutes);
    routeEndpoints.swap(uniqueEndpoints);
  }

  // ------------------------------
  // 3. Для каждого маршрута собираем районы, параллельно пополняя districts
  // ------------------------------
  for (size_t routeIdx = 0; routeIdx < routes.size(); ++routeIdx)
  {
    ScAddr const & route = routes[routeIdx];
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

    // удаляем дубликаты районов в маршруте
    {
      std::set<ScAddr, ScAddrLessFunc> uniqueSet;
      std::vector<ScAddr> filtered;
      for (ScAddr const & d : pairDistricts)
      {
        if (uniqueSet.insert(d).second)
          filtered.push_back(d);
      }
      pairDistricts.swap(filtered);
    }

    routeEndpoints[routeIdx] = pairDistricts;

    // добавляем новые районы в общий список
    if (pairDistricts.size() == 2)
    {
      addDistrict(pairDistricts[0]);
      addDistrict(pairDistricts[1]);
    }
  }

  // ------------------------------
  // 4. Создаём граф и индекс ScAddr -> int
  // ------------------------------
  int n = static_cast<int>(districts.size());
  result.districts = districts;
  result.graph = Graph(n);

  if (n == 0)
    return result;

  std::map<ScAddr, int, ScAddrLessFunc> index;
  for (int i = 0; i < n; ++i)
    index[districts[i]] = i;

  // ------------------------------
  // 5. Добавляем рёбра на основе сохранённых endpoints
  // ------------------------------
  for (auto const & endpoints : routeEndpoints)
  {
    if (endpoints.size() != 2)
      continue;

    ScAddr d1 = endpoints[0];
    ScAddr d2 = endpoints[1];

    auto it1 = index.find(d1);
    auto it2 = index.find(d2);
    if (it1 == index.end() || it2 == index.end())
      continue;

    result.graph.AddEdge(it1->second, it2->second);
  }

  return result;
}
