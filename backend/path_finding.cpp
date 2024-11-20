#include "path_finding.h"
#include "tinyxml/tinyxml.h" // Ensure the path is correct
#include "sqlite3.h"
#include "utils.h"

#include <iostream>
#include <sstream> // Include this header for std::istringstream
#include <queue>
#include <limits>
#include <algorithm>
#include <unordered_set>
#include <cmath>
#include <map>
#include <unordered_map>
#include <fstream>

// Use memory pools for Edge allocations
class EdgeAllocator {
public:
    // Custom allocator implementation
    // ...code to manage memory allocations for Edge structs...
};

extern std::vector<uint64_t> index_to_node_id;

// Function to load the graph from an XML file
bool load_graph(const std::string& filepath,
                std::vector<std::vector<Edge>>& graph,
                std::unordered_map<uint64_t, uint32_t>& node_id_to_index,
                std::vector<uint64_t>& index_to_node_id,
                std::unordered_map<uint64_t, std::pair<double, double>>& node_coords_map,
                std::unordered_map<uint64_t, std::string>& node_tags,
                bool pedestrian, bool riding, bool driving, bool pubTransport) {
    TiXmlDocument doc(filepath.c_str());
    if (!doc.LoadFile()) {
        std::cerr << "Failed to load file: " << filepath << std::endl;
        return false;
    }

    TiXmlElement* root = doc.RootElement();
    if (!root) {
        std::cerr << "Invalid XML format: No root element." << std::endl;
        return false;
    }

    // Iterate through XML elements
    uint32_t index = 0;
    for (TiXmlElement* elem = root->FirstChildElement(); elem != nullptr; elem = elem->NextSiblingElement()) {
        if (std::string(elem->Value()) == "node") {
            uint64_t id = std::stoull(elem->Attribute("id"));
            double lat = std::stod(elem->Attribute("lat"));
            double lon = std::stod(elem->Attribute("lon"));
            node_id_to_index[id] = index;
            index_to_node_id.push_back(id);
            node_coords_map[id] = {lat, lon};
            graph.emplace_back(); // Initialize adjacency list
            ++index;
        } else if (std::string(elem->Value()) == "way") {
            // Parse way elements and populate graph accordingly
            uint64_t way_id = std::stoull(elem->Attribute("id"));
            std::vector<uint64_t> node_refs;
            TiXmlElement* nd = elem->FirstChildElement("nd");
            while (nd) {
                uint64_t ref = std::stoull(nd->Attribute("ref"));
                node_refs.push_back(ref);
                nd = nd->NextSiblingElement("nd");
            }

            // Add edges between consecutive nodes
            for (size_t i = 0; i < node_refs.size() - 1; ++i) {
                uint64_t from_id = node_refs[i];
                uint64_t to_id = node_refs[i + 1];
                if (node_id_to_index.find(from_id) != node_id_to_index.end() &&
                    node_id_to_index.find(to_id) != node_id_to_index.end()) {
                    uint32_t from_idx = node_id_to_index[from_id];
                    uint32_t to_idx = node_id_to_index[to_id];
                    // For simplicity, assume weight as Euclidean distance
                    double lat1 = node_coords_map[from_id].first;
                    double lon1 = node_coords_map[from_id].second;
                    double lat2 = node_coords_map[to_id].first;
                    double lon2 = node_coords_map[to_id].second;
                    double distance = std::sqrt(std::pow(lat2 - lat1, 2) + std::pow(lon2 - lon1, 2));
                    graph[from_idx].push_back(Edge{to_idx, distance});
                    graph[to_idx].push_back(Edge{from_idx, distance}); // Assuming undirected graph
                }
            }

            // Handle tags for ways
            TiXmlElement* tag = elem->FirstChildElement("tag");
            while (tag) {
                std::string key = tag->Attribute("k");
                std::string value = tag->Attribute("v");
                if (key == "highway") {
                    for (uint64_t node_id : node_refs) {
                        node_tags[node_id] = value;
                    }
                }
                tag = tag->NextSiblingElement("tag");
            }
        }
        // Handle other element types if necessary
    }

    // Further processing if needed, such as filtering based on node_tags and whitelist flags

    return true;
}


// Function to find the shortest path using Dijkstra's algorithm
std::vector<uint32_t> dijkstra(const std::vector<std::vector<Edge>>& graph, uint32_t start, uint32_t end,
                               const std::unordered_map<uint64_t, std::string>& node_tags,
                               bool pedestrian, bool riding, bool driving, bool pubTransport) {
    std::vector<double> distances(graph.size(), std::numeric_limits<double>::infinity());
    std::vector<uint32_t> previous(graph.size(), -1);
    std::priority_queue<std::pair<double, uint32_t>,
                        std::vector<std::pair<double, uint32_t>>,
                        std::greater<std::pair<double, uint32_t>>> queue;

    distances[start] = 0.0;
    queue.emplace(0.0, start);

    while (!queue.empty()) {
        auto [dist, current] = queue.top();
        queue.pop();

        if (current == end) break;

        if (dist > distances[current]) continue;

        for (const auto& edge : graph[current]) {
            uint32_t neighbor = edge.to;
            double new_dist = dist + edge.weight;

            // Get node_id from index_to_node_id
            uint64_t neighbor_id = index_to_node_id[neighbor];

            // std::cout << "Before check" << std::endl;
            // Check whitelist
            if (!is_node_allowed(neighbor_id, node_tags, pedestrian, riding, driving, pubTransport))
                continue;

            if (new_dist < distances[neighbor]) {
                distances[neighbor] = new_dist;
                previous[neighbor] = current;
                queue.emplace(new_dist, neighbor);
            }
        }
    }

    // Reconstruct path
    std::vector<uint32_t> path;
    if (distances[end] == std::numeric_limits<double>::infinity())
        return path; // No path found

    for (uint32_t at = end; at != start; at = previous[at]) {
        path.push_back(at);
        if (previous[at] == static_cast<uint32_t>(-1))
            return std::vector<uint32_t>(); // No path found
    }
    path.push_back(start);
    std::reverse(path.begin(), path.end());
    return path;
}

// Function to find the shortest path using Bidirectional A* algorithm
std::vector<uint32_t> a_star(const std::vector<std::vector<Edge>>& graph,
                             uint32_t start_idx,
                             uint32_t end_idx,
                             const std::vector<std::pair<double, double>>& node_coords,
                             const std::unordered_map<uint64_t, std::string>& node_tags,
                             bool pedestrian, bool riding, bool driving, bool pubTransport) {
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
            if (closed_set_start.find(edge.to) != closed_set_start.end()) continue;

            float tentative_g_score = g_score_start[current_start] + edge.weight;

            // Get node_id from index_to_node_id
            uint64_t neighbor_id = index_to_node_id[edge.to];

            // Check whitelist
            if (!is_node_allowed(neighbor_id, node_tags, pedestrian, riding, driving, pubTransport))
                continue;

            if (tentative_g_score < g_score_start[edge.to]) {
                came_from_start[edge.to] = current_start;
                g_score_start[edge.to] = tentative_g_score;
                f_score_start[edge.to] = g_score_start[edge.to] + heuristic(edge.to, end_idx);
                open_set_start.push(edge.to);
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
            if (closed_set_end.find(edge.to) != closed_set_end.end()) continue;

            float tentative_g_score = g_score_end[current_end] + edge.weight;

            // Get node_id from index_to_node_id
            uint64_t neighbor_id = index_to_node_id[edge.to];

            // Check whitelist
            if (!is_node_allowed(neighbor_id, node_tags, pedestrian, riding, driving, pubTransport))
                continue;

            if (tentative_g_score < g_score_end[edge.to]) {
                came_from_end[edge.to] = current_end;
                g_score_end[edge.to] = tentative_g_score;
                f_score_end[edge.to] = g_score_end[edge.to] + heuristic(edge.to, start_idx);
                open_set_end.push(edge.to);
            }
        }
    }

    return {}; // No path found
}

// Function to find the shortest path using Bellman-Ford algorithm
std::vector<uint32_t> bellman_ford(const std::vector<std::vector<Edge>>& graph,
                                   uint32_t start_idx,
                                   uint32_t end_idx,
                                   const std::unordered_map<uint64_t, std::string>& node_tags,
                                   bool pedestrian, bool riding, bool driving, bool pubTransport) {
    std::vector<float> distances(graph.size(), std::numeric_limits<float>::infinity());
    std::vector<uint32_t> previous(graph.size(), -1);

    distances[start_idx] = 0.0f;

    int cur_progress = 0;

    for (size_t i = 0; i < graph.size() - 1; ++i) {
        for (size_t u = 0; u < graph.size(); ++u) {
            for (const auto& edge : graph[u]) {
                // Get node_id from index_to_node_id
                uint64_t neighbor_id = index_to_node_id[edge.to];

                if (!is_node_allowed(neighbor_id, node_tags, pedestrian, riding, driving, pubTransport))
                    continue;

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
                                     uint32_t end_idx,
                                     const std::unordered_map<uint64_t, std::string>& node_tags,
                                     bool pedestrian, bool riding, bool driving, bool pubTransport) {
    std::vector<std::vector<float>> dist(graph.size(), std::vector<float>(graph.size(), std::numeric_limits<float>::infinity()));
    std::vector<std::vector<uint32_t>> next(graph.size(), std::vector<uint32_t>(graph.size(), -1));

    for (size_t u = 0; u < graph.size(); ++u) {
        dist[u][u] = 0.0f;
        for (const auto& edge : graph[u]) {
            // Get node_id from index_to_node_id
            uint64_t neighbor_id = index_to_node_id[edge.to];

            if (!is_node_allowed(neighbor_id, node_tags, pedestrian, riding, driving, pubTransport))
                continue;

            dist[u][edge.to] = edge.weight;
            next[u][edge.to] = edge.to;
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
