#include "BuildGraphFromSc.hpp"

GraphFromScResult BuildGraphFromSc(ScMemoryContext & ctx, ScAddr const & graphAddr)
{
    GraphFromScResult result;

    // ------------------------------------------------------------
    // 1. Собираем районы (districts)
    // ------------------------------------------------------------
    std::vector<ScAddr> districts;

    {
        ScIterator3Ptr it = ctx.CreateIterator3(
            graphAddr,
            ScType::ConstPermPosArc,
            ScType::ConstNode
        );

        while (it->Next())
        {
            ScAddr elem = it->Get(2);

            if (ctx.CheckConnector(
                    TransportAccessibilityKeynodes::concept_district,
                    elem))
            {
                districts.push_back(elem);
            }
        }
    }

    int n = districts.size();
    result.districts = districts;
    result.graph = Graph(n);      // создаём C++ граф с n вершинами

    if (n == 0)
        return result;            // граф пустой – просто возвращаем что есть

    // ------------------------------------------------------------
    // 2. Создаём отображение ScAddr → индекс
    // ------------------------------------------------------------
    std::map<ScAddr, int, ScAddrLessFunc> index;

    for (int i = 0; i < n; i++)
        index[districts[i]] = i;

    // ------------------------------------------------------------
    // 3. Собираем маршруты (public transport routes)
    // ------------------------------------------------------------
    std::vector<ScAddr> routes;

    {
        ScIterator3Ptr it = ctx.CreateIterator3(
            graphAddr,
            ScType::ConstPermPosArc,
            ScType::ConstNode
        );

        while (it->Next())
        {
            ScAddr elem = it->Get(2);

            if (ctx.CheckConnector(
                    TransportAccessibilityKeynodes::concept_public_transport_route,
                    elem))
            {
                routes.push_back(elem);
            }
        }
    }

    // ------------------------------------------------------------
    // 4. Для каждого маршрута определяем пару районов
    // ------------------------------------------------------------
    for (ScAddr const & route : routes)
    {
        // Ищем множество [* d1; d2 *]
        ScAddr districtSet;

        {
            ScIterator5Ptr it = ctx.CreateIterator5(
                route,
                ScType::ConstCommonArc,
                ScType::ConstNode,                      // множество районов
                ScType::ConstPermPosArc,
                TransportAccessibilityKeynodes::nrel_connects_districts
            );

            if (!it->Next())
                continue; // у маршрута нет связей (ошибка в KB?)

            districtSet = it->Get(2);
        }

        // Достаём элементы множества
        std::vector<ScAddr> pairDistricts;

        {
            ScIterator3Ptr it = ctx.CreateIterator3(
                districtSet,
                ScType::ConstPermPosArc,
                ScType::ConstNode
            );

            while (it->Next())
                pairDistricts.push_back(it->Get(2));
        }

        // Проверка: должен быть ровно 2 района
        if (pairDistricts.size() != 2)
            continue;

        ScAddr d1 = pairDistricts[0];
        ScAddr d2 = pairDistricts[1];

        if (!index.count(d1) || !index.count(d2))
            continue; // район не в graphAddr

        int u = index[d1];
        int v = index[d2];

        result.graph.AddEdge(u, v);  // добавляем неориентированное ребро
    }

    return result;
}
