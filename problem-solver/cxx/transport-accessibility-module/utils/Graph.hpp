#pragma once

#include <vector>
#include <utility>

// Простой неориентированный граф с вершинами 0..n-1.
// НИКАКОГО sc-memory, только чистый C++.
struct Graph
{
  int n;                                // количество вершин
  std::vector<std::vector<int>> adj;    // список смежности

  explicit Graph(int n = 0);

  // Добавить неориентированное ребро между u и v (0 <= u,v < n)
  void AddEdge(int u, int v);

  // DFS из start, возвращает вектор visited (true = достижима)
  std::vector<bool> DfsFrom(int start) const;

  // Матрица кратчайших расстояний между всеми парами вершин.
  // Вес всех рёбер считаем равным 1.
  // НЕДОСТИЖИМЫЕ вершины имеют расстояние INF.
  std::vector<std::vector<int>> FloydWarshall(int INF = 1000000000) const;

  // Эксцентриситет вершины v: max_j dist[v][j]
  // Если вершина не связана с кем-то (есть INF), эксцентриситет = INF.
  int Eccentricity(int v,
                   std::vector<std::vector<int>> const & dist,
                   int INF = 1000000000) const;

  // Найти индекс "центральной" вершины:
  // вершина с минимальным эксцентриситетом.
  // Если граф полностью разорван, может вернуть -1.
  int FindCentralVertex(std::vector<std::vector<int>> const & dist,
                        int INF = 1000000000) const;

  // Диаметр графа: максимальное конечное расстояние между парами вершин.
  // Если граф несвязен, диаметр считается по компонентам (игнорируя INF).
  int Diameter(std::vector<std::vector<int>> const & dist,
               int INF = 1000000000) const;

  // Поиск мостов (bridge edges) в неориентированном графе.
  // Возвращает список пар (u, v) — рёбра, удаление которых увеличивает
  // количество компонент связности.
  std::vector<std::pair<int, int>> FindBridges() const;
};
