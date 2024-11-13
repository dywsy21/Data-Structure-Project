#include "path_finding.h"
#include "tinyxml/tinyxml.h" // Ensure the path is correct
#include <iostream>
#include <sstream> // Include this header for std::istringstream
#include <queue>
#include <limits>
#include <algorithm>
#include <unordered_set>
#include <cmath>
#include <map>
#include <unordered_map>

// Use memory pools for Edge allocations
class EdgeAllocator {
public:
    // Custom allocator implementation
    // ...code to manage memory allocations for Edge structs...
};

// Function to load the graph from an XML file
bool load_graph(const std::string& xml_file,
                std::vector<std::vector<Edge>>& graph,
                std::unordered_map<uint64_t, uint32_t>& node_id_to_index,
                std::vector<uint64_t>& index_to_node_id,
                std::unordered_map<uint64_t, std::pair<double, double>>& node_coords_map) {
    TiXmlDocument doc(xml_file.c_str());
    if (!doc.LoadFile()) {
        std::cerr << "Failed to load XML file: " << xml_file << std::endl;
        return false;
    }

    TiXmlElement* root = doc.RootElement();
    if (!root) {
        std::cerr << "Failed to get root element in XML file: " << xml_file << std::endl;
        return false;
    }

    // First pass: Parse nodes and store their coordinates
    for (TiXmlElement* element = root->FirstChildElement("node"); element != nullptr; element = element->NextSiblingElement("node")) {
        const char* id_str = element->Attribute("id");
        const char* lat = element->Attribute("lat");
        const char* lon = element->Attribute("lon");
        if (id_str && lat && lon) {
            uint64_t node_id = std::stoull(id_str); // Convert node ID to uint64_t
            double latitude = std::stod(lat);
            double longitude = std::stod(lon);
            // Assign index to each node ID
            uint32_t index = static_cast<uint32_t>(node_id_to_index.size());
            node_id_to_index[node_id] = index;
            index_to_node_id.push_back(node_id);
            node_coords_map[node_id] = {latitude, longitude};
        }
    }

    // Resize the graph
    graph.resize(node_id_to_index.size());

    // Second pass: Parse ways and build the graph
    for (TiXmlElement* way = root->FirstChildElement("way"); way != nullptr; way = way->NextSiblingElement("way")) {
        std::vector<std::string> node_refs;
        for (TiXmlElement* nd = way->FirstChildElement("nd"); nd != nullptr; nd = nd->NextSiblingElement("nd")) {
            const char* ref = nd->Attribute("ref");
            if (ref) {
                node_refs.emplace_back(ref);
            }
        }

        // Build edges using node indices
        for (size_t i = 1; i < node_refs.size(); ++i) {
            uint64_t id_u = std::stoull(node_refs[i - 1]);
            uint64_t id_v = std::stoull(node_refs[i]);
            uint32_t u = node_id_to_index[id_u];
            uint32_t v = node_id_to_index[id_v];
            float weight = 1.0f; // Use float for weight
            graph[u].push_back({v, weight});
            graph[v].push_back({u, weight}); // If the graph is undirected
        }
    }

    return true;
}

// Function to find the shortest path using Dijkstra's algorithm
std::vector<uint32_t> dijkstra(const std::vector<std::vector<Edge>>& graph,
                               uint32_t start_idx,
                               uint32_t end_idx) {
    std::vector<float> distances(graph.size(), std::numeric_limits<float>::infinity());
    std::vector<uint32_t> previous(graph.size(), -1);

    distances[start_idx] = 0.0f;

    auto cmp = [&distances](uint32_t a, uint32_t b) {
        return distances[a] > distances[b];
    };
    std::priority_queue<uint32_t, std::vector<uint32_t>, decltype(cmp)> queue(cmp);
    queue.push(start_idx);

    size_t total_nodes = graph.size();
    size_t processed_nodes = 0;
    int last_progress = 0;

    while (!queue.empty()) {
        uint32_t current = queue.top();
        queue.pop();

        if (current == end_idx) break;

        for (const auto& edge : graph[current]) {
            float alt = distances[current] + edge.weight;
            if (alt < distances[edge.target]) {
                distances[edge.target] = alt;
                previous[edge.target] = current;
                queue.push(edge.target);
            }
        }

        // processed_nodes++;
        // int progress = static_cast<int>((processed_nodes * 100.0) / total_nodes);
        // if (progress > last_progress) {
        //     std::cout << progress << std::endl;
        //     last_progress = progress;
        // }
    }

    // Reconstruct path
    std::vector<uint32_t> path;
    uint32_t u = end_idx;
    if (previous[u] != -1 || u == start_idx) {
        while (u != start_idx) {
            path.push_back(u);
            u = previous[u];
        }
        path.push_back(start_idx);
        std::reverse(path.begin(), path.end());
    }

    return path;
}

// Function to find the shortest path using Bidirectional A* algorithm
std::vector<uint32_t> a_star(const std::vector<std::vector<Edge>>& graph,
                             uint32_t start_idx,
                             uint32_t end_idx,
                             const std::vector<std::pair<double, double>>& node_coords) {
    auto heuristic = [&node_coords](uint32_t a, uint32_t b) {
        // Implement a heuristic function here, for simplicity, we use 0 (equivalent to Dijkstra)
        return 0.0f;
    };

    std::vector<float> g_score_start(graph.size(), std::numeric_limits<float>::infinity());
    std::vector<float> g_score_end(graph.size(), std::numeric_limits<float>::infinity());
    std::vector<float> f_score_start(graph.size(), std::numeric_limits<float>::infinity());
    std::vector<float> f_score_end(graph.size(), std::numeric_limits<float>::infinity());
    std::vector<uint32_t> came_from_start(graph.size(), -1);
    std::vector<uint32_t> came_from_end(graph.size(), -1);

    g_score_start[start_idx] = 0.0f;
    f_score_start[start_idx] = heuristic(start_idx, end_idx);
    g_score_end[end_idx] = 0.0f;
    f_score_end[end_idx] = heuristic(end_idx, start_idx);

    auto cmp_start = [&f_score_start](uint32_t a, uint32_t b) {
        return f_score_start[a] > f_score_start[b];
    };
    auto cmp_end = [&f_score_end](uint32_t a, uint32_t b) {
        return f_score_end[a] > f_score_end[b];
    };

    std::priority_queue<uint32_t, std::vector<uint32_t>, decltype(cmp_start)> open_set_start(cmp_start);
    std::priority_queue<uint32_t, std::vector<uint32_t>, decltype(cmp_end)> open_set_end(cmp_end);

    open_set_start.push(start_idx);
    open_set_end.push(end_idx);

    std::unordered_set<uint32_t> closed_set_start, closed_set_end;

    while (!open_set_start.empty() && !open_set_end.empty()) {
        // Process from start
        uint32_t current_start = open_set_start.top();
        open_set_start.pop();
        closed_set_start.insert(current_start);

        if (closed_set_end.find(current_start) != closed_set_end.end()) {
            // Path found
            std::vector<uint32_t> path;
            uint32_t u = current_start;
            while (u != start_idx) {
                path.push_back(u);
                u = came_from_start[u];
            }
            path.push_back(start_idx);
            std::reverse(path.begin(), path.end());

            u = current_start;
            while (u != end_idx) {
                u = came_from_end[u];
                path.push_back(u);
            }

            return path;
        }

        for (const auto& edge : graph[current_start]) {
            if (closed_set_start.find(edge.target) != closed_set_start.end()) continue;

            float tentative_g_score = g_score_start[current_start] + edge.weight;
            if (tentative_g_score < g_score_start[edge.target]) {
                came_from_start[edge.target] = current_start;
                g_score_start[edge.target] = tentative_g_score;
                f_score_start[edge.target] = g_score_start[edge.target] + heuristic(edge.target, end_idx);
                open_set_start.push(edge.target);
            }
        }

        // Process from end
        uint32_t current_end = open_set_end.top();
        open_set_end.pop();
        closed_set_end.insert(current_end);

        if (closed_set_start.find(current_end) != closed_set_start.end()) {
            // Path found
            std::vector<uint32_t> path;
            uint32_t u = current_end;
            while (u != end_idx) {
                path.push_back(u);
                u = came_from_end[u];
            }
            path.push_back(end_idx);
            std::reverse(path.begin(), path.end());

            u = current_end;
            while (u != start_idx) {
                u = came_from_start[u];
                path.push_back(u);
            }

            return path;
        }

        for (const auto& edge : graph[current_end]) {
            if (closed_set_end.find(edge.target) != closed_set_end.end()) continue;

            float tentative_g_score = g_score_end[current_end] + edge.weight;
            if (tentative_g_score < g_score_end[edge.target]) {
                came_from_end[edge.target] = current_end;
                g_score_end[edge.target] = tentative_g_score;
                f_score_end[edge.target] = g_score_end[edge.target] + heuristic(edge.target, start_idx);
                open_set_end.push(edge.target);
            }
        }
    }

    return {}; // No path found
}

// Function to find the shortest path using Bellman-Ford algorithm
std::vector<uint32_t> bellman_ford(const std::vector<std::vector<Edge>>& graph,
                                   uint32_t start_idx,
                                   uint32_t end_idx) {
    std::vector<float> distances(graph.size(), std::numeric_limits<float>::infinity());
    std::vector<uint32_t> previous(graph.size(), -1);

    distances[start_idx] = 0.0f;

    int cur_progress = 0;

    for (size_t i = 0; i < graph.size() - 1; ++i) {
        for (size_t u = 0; u < graph.size(); ++u) {
            for (const auto& edge : graph[u]) {
                if (distances[u] + edge.weight < distances[edge.target]) {
                    distances[edge.target] = distances[u] + edge.weight;
                    previous[edge.target] = u;
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
    std::vector<uint32_t> path;
    uint32_t u = end_idx;
    if (previous[u] != -1 || u == start_idx) {
        while (u != start_idx) {
            path.push_back(u);
            u = previous[u];
        }
        path.push_back(start_idx);
        std::reverse(path.begin(), path.end());
    }

    return path;
}

// Function to find the shortest path using Floyd-Warshall algorithm
std::vector<uint32_t> floyd_warshall(const std::vector<std::vector<Edge>>& graph,
                                     uint32_t start_idx,
                                     uint32_t end_idx) {
    std::vector<std::vector<float>> dist(graph.size(), std::vector<float>(graph.size(), std::numeric_limits<float>::infinity()));
    std::vector<std::vector<uint32_t>> next(graph.size(), std::vector<uint32_t>(graph.size(), -1));

    for (size_t u = 0; u < graph.size(); ++u) {
        dist[u][u] = 0.0f;
        for (const auto& edge : graph[u]) {
            dist[u][edge.target] = edge.weight;
            next[u][edge.target] = edge.target;
        }
    }

    size_t total_nodes = graph.size();
    size_t processed_nodes = 0;
    int last_progress = 0;

    for (size_t k = 0; k < graph.size(); ++k) {
        for (size_t i = 0; i < graph.size(); ++i) {
            for (size_t j = 0; j < graph.size(); ++j) {
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
    std::vector<uint32_t> path;
    if (next[start_idx][end_idx] != -1) {
        uint32_t u = start_idx;
        while (u != end_idx) {
            path.push_back(u);
            u = next[u][end_idx];
        }
        path.push_back(end_idx);
    }

    return path;
}