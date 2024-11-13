#include "path_finding.h"
#include "tinyxml/tinyxml.h" // Ensure the path is correct
#include <iostream>
#include <sstream> // Include this header for std::istringstream
#include <queue>
#include <limits>
#include <algorithm>
#include <unordered_set>
#include <cmath>

// Function to load the graph from an XML file
bool load_graph(const std::string& xml_file, std::unordered_map<std::string, std::vector<Edge>>& graph) {
    TiXmlDocument doc(xml_file.c_str());
    if (!doc.LoadFile()) {
        std::cerr << "Failed to load XML file: " << xml_file << std::endl;
        return false;
    }

    TiXmlElement* root = doc.RootElement();

    for (TiXmlElement* way = root->FirstChildElement("way"); way != nullptr; way = way->NextSiblingElement("way")) {
        std::vector<std::string> node_refs;
        for (TiXmlElement* nd = way->FirstChildElement("nd"); nd != nullptr; nd = nd->NextSiblingElement("nd")) {
            const char* ref = nd->Attribute("ref");
            if (ref) {
                node_refs.emplace_back(ref);
            }
        }

        // Assuming bidirectional edges with weight 1.0
        for (size_t i = 0; i < node_refs.size() - 1; ++i) {
            graph[node_refs[i]].push_back(Edge{node_refs[i + 1], 1.0});
            graph[node_refs[i + 1]].push_back(Edge{node_refs[i], 1.0});
        }
    }

    return true;
}

// Function to find the shortest path using Dijkstra's algorithm
std::vector<std::string> dijkstra(const std::unordered_map<std::string, std::vector<Edge>>& graph,
                                  const std::string& start,
                                  const std::string& end) {
    std::unordered_map<std::string, double> distances;
    std::unordered_map<std::string, std::string> previous;

    for (const auto& pair : graph) {
        distances[pair.first] = std::numeric_limits<double>::infinity();
    }
    distances[start] = 0.0;

    auto cmp = [&distances](const std::string& a, const std::string& b) {
        return distances[a] > distances[b];
    };
    std::priority_queue<std::string, std::vector<std::string>, decltype(cmp)> queue(cmp);
    queue.push(start);

    size_t total_nodes = graph.size();
    size_t processed_nodes = 0;
    int last_progress = 0;

    while (!queue.empty()) {
        std::string current = queue.top();
        queue.pop();

        if (current == end) break;

        for (const auto& edge : graph.at(current)) {
            double alt = distances[current] + edge.weight;
            if (alt < distances[edge.to]) {
                distances[edge.to] = alt;
                previous[edge.to] = current;
                queue.push(edge.to);
            }
        }

        processed_nodes++;
        int progress = static_cast<int>((processed_nodes * 100.0) / total_nodes);
        if (progress > last_progress) {
            std::cout << progress << std::endl;
            last_progress = progress;
        }
    }

    // Reconstruct path
    std::vector<std::string> path;
    std::string u = end;
    if (previous.find(u) != previous.end() || u == start) {
        while (u != start) {
            path.push_back(u);
            u = previous[u];
        }
        path.push_back(start);
        std::reverse(path.begin(), path.end());
    }

    return path;
}

// Function to find the shortest path using Bidirectional A* algorithm
std::vector<std::string> a_star(const std::unordered_map<std::string, std::vector<Edge>>& graph,
                                const std::string& start,
                                const std::string& end) {
    auto heuristic = [](const std::string& a, const std::string& b) {
        // Implement a heuristic function here, for simplicity, we use 0 (equivalent to Dijkstra)
        return 0.0;
    };

    std::unordered_map<std::string, double> g_score_start, g_score_end;
    std::unordered_map<std::string, double> f_score_start, f_score_end;
    std::unordered_map<std::string, std::string> came_from_start, came_from_end;

    for (const auto& pair : graph) {
        g_score_start[pair.first] = std::numeric_limits<double>::infinity();
        g_score_end[pair.first] = std::numeric_limits<double>::infinity();
        f_score_start[pair.first] = std::numeric_limits<double>::infinity();
        f_score_end[pair.first] = std::numeric_limits<double>::infinity();
    }

    g_score_start[start] = 0.0;
    f_score_start[start] = heuristic(start, end);
    g_score_end[end] = 0.0;
    f_score_end[end] = heuristic(end, start);

    auto cmp_start = [&f_score_start](const std::string& a, const std::string& b) {
        return f_score_start[a] > f_score_start[b];
    };
    auto cmp_end = [&f_score_end](const std::string& a, const std::string& b) {
        return f_score_end[a] > f_score_end[b];
    };

    std::priority_queue<std::string, std::vector<std::string>, decltype(cmp_start)> open_set_start(cmp_start);
    std::priority_queue<std::string, std::vector<std::string>, decltype(cmp_end)> open_set_end(cmp_end);

    open_set_start.push(start);
    open_set_end.push(end);

    std::unordered_set<std::string> closed_set_start, closed_set_end;

    while (!open_set_start.empty() && !open_set_end.empty()) {
        // Process from start
        std::string current_start = open_set_start.top();
        open_set_start.pop();
        closed_set_start.insert(current_start);

        if (closed_set_end.find(current_start) != closed_set_end.end()) {
            // Path found
            std::vector<std::string> path;
            std::string u = current_start;
            while (u != start) {
                path.push_back(u);
                u = came_from_start[u];
            }
            path.push_back(start);
            std::reverse(path.begin(), path.end());

            u = current_start;
            while (u != end) {
                u = came_from_end[u];
                path.push_back(u);
            }

            return path;
        }

        for (const auto& edge : graph.at(current_start)) {
            if (closed_set_start.find(edge.to) != closed_set_start.end()) continue;

            double tentative_g_score = g_score_start[current_start] + edge.weight;
            if (tentative_g_score < g_score_start[edge.to]) {
                came_from_start[edge.to] = current_start;
                g_score_start[edge.to] = tentative_g_score;
                f_score_start[edge.to] = g_score_start[edge.to] + heuristic(edge.to, end);
                open_set_start.push(edge.to);
            }
        }

        // Process from end
        std::string current_end = open_set_end.top();
        open_set_end.pop();
        closed_set_end.insert(current_end);

        if (closed_set_start.find(current_end) != closed_set_start.end()) {
            // Path found
            std::vector<std::string> path;
            std::string u = current_end;
            while (u != end) {
                path.push_back(u);
                u = came_from_end[u];
            }
            path.push_back(end);
            std::reverse(path.begin(), path.end());

            u = current_end;
            while (u != start) {
                u = came_from_start[u];
                path.push_back(u);
            }

            return path;
        }

        for (const auto& edge : graph.at(current_end)) {
            if (closed_set_end.find(edge.to) != closed_set_end.end()) continue;

            double tentative_g_score = g_score_end[current_end] + edge.weight;
            if (tentative_g_score < g_score_end[edge.to]) {
                came_from_end[edge.to] = current_end;
                g_score_end[edge.to] = tentative_g_score;
                f_score_end[edge.to] = g_score_end[edge.to] + heuristic(edge.to, start);
                open_set_end.push(edge.to);
            }
        }
    }

    return {}; // No path found
}

// Function to find the shortest path using Bellman-Ford algorithm
std::vector<std::string> bellman_ford(const std::unordered_map<std::string, std::vector<Edge>>& graph,
                                      const std::string& start,
                                      const std::string& end) {
    std::unordered_map<std::string, double> distances;
    std::unordered_map<std::string, std::string> previous;

    for (const auto& pair : graph) {
        distances[pair.first] = std::numeric_limits<double>::infinity();
    }
    distances[start] = 0.0;

    int cur_progress = 0;

    for (size_t i = 0; i < graph.size() - 1; ++i) {
        for (const auto& pair : graph) {
            const std::string& u = pair.first;
            for (const auto& edge : pair.second) {
                if (distances[u] + edge.weight < distances[edge.to]) {
                    distances[edge.to] = distances[u] + edge.weight;
                    previous[edge.to] = u;
                }
            }
        }
        int progress = static_cast<int>((i * 100.0) / graph.size());
        if (progress <= 100 && progress > cur_progress) {
            std::cout << progress << std::endl;
            cur_progress = progress;
        }
    }

    // Reconstruct path
    std::vector<std::string> path;
    std::string u = end;
    if (previous.find(u) != previous.end() || u == start) {
        while (u != start) {
            path.push_back(u);
            u = previous[u];
        }
        path.push_back(start);
        std::reverse(path.begin(), path.end());
    }

    return path;
}

// Function to find the shortest path using Floyd-Warshall algorithm
std::vector<std::string> floyd_warshall(const std::unordered_map<std::string, std::vector<Edge>>& graph,
                                        const std::string& start,
                                        const std::string& end) {
    std::unordered_map<std::string, std::unordered_map<std::string, double>> dist;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> next;

    for (const auto& pair : graph) {
        const std::string& u = pair.first;
        dist[u][u] = 0;
        for (const auto& edge : pair.second) {
            dist[u][edge.to] = edge.weight;
            next[u][edge.to] = edge.to;
        }
    }

    size_t total_nodes = graph.size();
    size_t processed_nodes = 0;
    int last_progress = 0;

    for (const auto& k_pair : graph) {
        const std::string& k = k_pair.first;
        for (const auto& i_pair : graph) {
            const std::string& i = i_pair.first;
            for (const auto& j_pair : graph) {
                const std::string& j = j_pair.first;
                if (dist[i][k] + dist[k][j] < dist[i][j]) {
                    dist[i][j] = dist[i][k] + dist[k][j];
                    next[i][j] = next[i][k];
                }
            }
        }

        processed_nodes++;
        int progress = static_cast<int>((processed_nodes * 100.0) / total_nodes);
        if (progress > last_progress) {
            std::cout << progress << std::endl;
            last_progress = progress;
        }
    }

    // Reconstruct path
    std::vector<std::string> path;
    if (next[start].find(end) != next[start].end()) {
        std::string u = start;
        while (u != end) {
            path.push_back(u);
            u = next[u][end];
        }
        path.push_back(end);
    }

    return path;
}
