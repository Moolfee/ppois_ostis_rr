#include "Graph.hpp"

#include <limits>
#include <algorithm>

Graph::Graph(int k)
  : n(k)
    , adj(k)
{
}

void Graph::AddEdge(int u, int v)
{
  if (u < 0 || v < 0 || u >= n || v >= n)
    return;
  adj[u].push_back(v);
  adj[v].push_back(u);
}

std::vector<bool> Graph::DfsFrom(int start) const
{
  std::vector<bool> visited(n, false);
  if (start < 0 || start >= n)
    return visited;

  std::vector<int> stack;
  stack.push_back(start);
  visited[start] = true;

  while (!stack.empty())
  {
    int v = stack.back();
    stack.pop_back();

    for (int to : adj[v])
    {
      if (!visited[to])
      {
        visited[to] = true;
        stack.push_back(to);
      }
    }
  }

  return visited;
}

std::vector<std::vector<int>> Graph::FloydWarshall(int INF) const
{
  std::vector<std::vector<int>> dist(n, std::vector<int>(n, INF));

  for (int i = 0; i < n; ++i)
    dist[i][i] = 0;

  for (int u = 0; u < n; ++u)
  {
    for (int v : adj[u])
    {
      // Неориентированный граф, вес ребра = 1
      dist[u][v] = std::min(dist[u][v], 1);
      dist[v][u] = std::min(dist[v][u], 1);
    }
  }

  for (int k = 0; k < n; ++k)
  {
    for (int i = 0; i < n; ++i)
    {
      if (dist[i][k] == INF)
        continue;
      for (int j = 0; j < n; ++j)
      {
        if (dist[k][j] == INF)
          continue;
        int cand = dist[i][k] + dist[k][j];
        if (cand < dist[i][j])
          dist[i][j] = cand;
      }
    }
  }

  return dist;
}

int Graph::Eccentricity(int v,
                         std::vector<std::vector<int>> const & dist,
                         int INF) const
{
  if (v < 0 || v >= n)
    return INF;

  int ecc = 0;
  for (int j = 0; j < n; ++j)
  {
    if (dist[v][j] == INF)
    {
      // Есть недостижимая вершина — эксцентриситет бесконечный
      return INF;
    }
    if (dist[v][j] > ecc)
      ecc = dist[v][j];
  }
  return ecc;
}

int Graph::FindCentralVertex(std::vector<std::vector<int>> const & dist,
                             int INF) const
{
  int bestVertex = -1;
  int bestEcc = INF;

  for (int v = 0; v < n; ++v)
  {
    int ecc = Eccentricity(v, dist, INF);
    if (ecc < bestEcc)
    {
      bestEcc = ecc;
      bestVertex = v;
    }
  }

  return bestVertex;
}

int Graph::Diameter(std::vector<std::vector<int>> const & dist,
                    int INF) const
{
  int diam = 0;

  for (int i = 0; i < n; ++i)
  {
    for (int j = 0; j < n; ++j)
    {
      if (dist[i][j] != INF && dist[i][j] > diam)
        diam = dist[i][j];
    }
  }

  return diam;
}

// --- Вспомогательное DFS для поиска мостов (Tarjan) ---

namespace
{
  void BridgesDfs(int v,
                  int parent,
                  int & timer,
                  std::vector<int> & tin,
                  std::vector<int> & low,
                  std::vector<bool> & visited,
                  Graph const & g,
                  std::vector<std::pair<int, int>> & bridges)
  {
    visited[v] = true;
    tin[v] = low[v] = timer++;

    for (int to : g.adj[v])
    {
      if (to == parent)
        continue;

      if (visited[to])
      {
        // обратное ребро
        low[v] = std::min(low[v], tin[to]);
      }
      else
      {
        BridgesDfs(to, v, timer, tin, low, visited, g, bridges);
        low[v] = std::min(low[v], low[to]);

        if (low[to] > tin[v])
        {
          // ребро (v, to) — мост
          bridges.emplace_back(v, to);
        }
      }
    }
  }
}

std::vector<std::pair<int, int>> Graph::FindBridges() const
{
  std::vector<std::pair<int, int>> bridges;
  std::vector<bool> visited(n, false);
  std::vector<int> tin(n, -1), low(n, -1);
  int timer = 0;

  for (int i = 0; i < n; ++i)
  {
    if (!visited[i])
      BridgesDfs(i, -1, timer, tin, low, visited, *this, bridges);
  }

  return bridges;
}
