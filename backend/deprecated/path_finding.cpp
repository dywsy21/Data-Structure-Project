#include "path_finding.h"
#include "tinyxml/tinyxml.h" // Ensure the path is correct
#include "utils.h"
#include "sqlite/sqlite3.h"

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

extern std::vector<uint64_t> index_to_node_id;

// // Function to load the graph from an XML file
// bool load_graph(const std::string& filepath,
//                 std::vector<std::vector<Edge>>& graph,
//                 std::unordered_map<uint64_t, uint32_t>& node_id_to_index,
//                 std::vector<uint64_t>& index_to_node_id,
//                 std::unordered_map<uint64_t, std::pair<double, double>>& node_coords_map,
//                 std::unordered_map<uint64_t, std::string>& node_tags,
//                 bool pedestrian, bool riding, bool driving, bool pubTransport) {
//     TiXmlDocument doc(filepath.c_str());
//     if (!doc.LoadFile()) {
//         std::cout << "Failed to load file: " << filepath << std::endl;
//         return false;
//     }

//     TiXmlElement* root = doc.RootElement();
//     if (!root) {
//         std::cout << "Invalid XML format: No root element." << std::endl;
//         return false;
//     }

//     // Iterate through XML elements
//     uint32_t index = 0;
//     for (TiXmlElement* elem = root->FirstChildElement(); elem != nullptr; elem = elem->NextSiblingElement()) {
//         if (std::string(elem->Value()) == "node") {
//             uint64_t id = std::stoull(elem->Attribute("id"));
//             double lat = std::stod(elem->Attribute("lat"));
//             double lon = std::stod(elem->Attribute("lon"));
//             node_id_to_index[id] = index;
//             index_to_node_id.push_back(id);
//             node_coords_map[id] = {lat, lon};
//             graph.emplace_back(); // Initialize adjacency list
//             ++index;
//         } else if (std::string(elem->Value()) == "way") {
//             // Parse way elements and populate graph accordingly
//             uint64_t way_id = std::stoull(elem->Attribute("id"));
//             std::vector<uint64_t> node_refs;
//             TiXmlElement* nd = elem->FirstChildElement("nd");
//             while (nd) {
//                 uint64_t ref = std::stoull(nd->Attribute("ref"));
//                 node_refs.push_back(ref);
//                 nd = nd->NextSiblingElement("nd");
//             }

//             // Add edges between consecutive nodes
//             for (size_t i = 0; i < node_refs.size() - 1; ++i) {
//                 uint64_t from_id = node_refs[i];
//                 uint64_t to_id = node_refs[i + 1];
//                 if (node_id_to_index.find(from_id) != node_id_to_index.end() &&
//                     node_id_to_index.find(to_id) != node_id_to_index.end()) {
//                     uint32_t from_idx = node_id_to_index[from_id];
//                     uint32_t to_idx = node_id_to_index[to_id];
//                     // For simplicity, assume weight as Euclidean distance
//                     double lat1 = node_coords_map[from_id].first;
//                     double lon1 = node_coords_map[from_id].second;
//                     double lat2 = node_coords_map[to_id].first;
//                     double lon2 = node_coords_map[to_id].second;
//                     double distance = std::sqrt(std::pow(lat2 - lat1, 2) + std::pow(lon2 - lon1, 2));
//                     graph[from_idx].push_back(Edge{to_idx, distance});
//                     graph[to_idx].push_back(Edge{from_idx, distance}); // Assuming undirected graph
//                 }
//             }

//             // Handle tags for ways
//             TiXmlElement* tag = elem->FirstChildElement("tag");
//             while (tag) {
//                 std::string key = tag->Attribute("k");
//                 std::string value = tag->Attribute("v");
//                 if (key == "highway") {
//                     for (uint64_t node_id : node_refs) {
//                         node_tags[node_id] = value;
//                     }
//                 }
//                 tag = tag->NextSiblingElement("tag");
//             }
//         }
//         // Handle other element types if necessary
//     }

//     // Further processing if needed, such as filtering based on node_tags and whitelist flags

//     return true;
// }


// Function to find the shortest path using Dijkstra's algorithm
std::vector<uint64_t> dijkstra(sqlite3* db, uint64_t start_id, uint64_t end_id,
                               bool pedestrian, bool riding, bool driving, bool pubTransport) {
    std::unordered_map<uint64_t, double> distances;
    std::unordered_map<uint64_t, uint64_t> previous;
    auto cmp = [&distances](uint64_t left, uint64_t right) { return distances[left] > distances[right]; };
    std::priority_queue<uint64_t, std::vector<uint64_t>, decltype(cmp)> queue(cmp);

    distances[start_id] = 0.0;
    queue.push(start_id);

    while (!queue.empty()) {
        uint64_t current_node = queue.top();
        queue.pop();

        if (current_node == end_id) break;

        if (!is_node_allowed(current_node, db, pedestrian, riding, driving, pubTransport))
            continue;

        auto adjacent_nodes = get_adjacent_nodes(db, current_node);
        for (uint64_t neighbor : adjacent_nodes) {
            if (!is_node_allowed(neighbor, db, pedestrian, riding, driving, pubTransport))
                continue;
            double weight = get_distance(db, current_node, neighbor);
            double tentative_dist = distances[current_node] + weight;
            if (distances.find(neighbor) == distances.end() || tentative_dist < distances[neighbor]) {
                distances[neighbor] = tentative_dist;
                previous[neighbor] = current_node;
                queue.push(neighbor);
            }
        }
    }

    std::vector<uint64_t> path;
    uint64_t current = end_id;
    if (previous.find(current) == previous.end()) return path; // No path found
    while (current != start_id) {
        path.push_back(current);
        current = previous[current];
    }
    path.push_back(start_id);
    std::reverse(path.begin(), path.end());
    return path;
}

// Function to find the shortest path using Bidirectional A* algorithm
std::vector<uint64_t> a_star(sqlite3* db, uint64_t start_id, uint64_t end_id,
                             bool pedestrian, bool riding, bool driving, bool pubTransport) {
    auto heuristic = [](uint64_t a, uint64_t b) {
        // Implement a heuristic function here, for simplicity, we use 0 (equivalent to Dijkstra)
        return 0.0f;
    };

    std::unordered_map<uint64_t, float> g_score_start, g_score_end, f_score_start, f_score_end;
    std::unordered_map<uint64_t, uint64_t> came_from_start, came_from_end;

    g_score_start[start_id] = 0.0f;
    f_score_start[start_id] = heuristic(start_id, end_id);
    g_score_end[end_id] = 0.0f;
    f_score_end[end_id] = heuristic(end_id, start_id);

    auto cmp_start = [&f_score_start](uint64_t a, uint64_t b) {
        return f_score_start[a] > f_score_start[b];
    };
    auto cmp_end = [&f_score_end](uint64_t a, uint64_t b) {
        return f_score_end[a] > f_score_end[b];
    };

    std::priority_queue<uint64_t, std::vector<uint64_t>, decltype(cmp_start)> open_set_start(cmp_start);
    std::priority_queue<uint64_t, std::vector<uint64_t>, decltype(cmp_end)> open_set_end(cmp_end);

    open_set_start.push(start_id);
    open_set_end.push(end_id);

    std::unordered_set<uint64_t> closed_set_start, closed_set_end;

    while (!open_set_start.empty() && !open_set_end.empty()) {
        // Process from start
        uint64_t current_start = open_set_start.top();
        open_set_start.pop();
        closed_set_start.insert(current_start);

        if (closed_set_end.find(current_start) != closed_set_end.end()) {
            // Path found
            std::vector<uint64_t> path;
            uint64_t u = current_start;
            while (u != start_id) {
                path.push_back(u);
                u = came_from_start[u];
            }
            path.push_back(start_id);
            std::reverse(path.begin(), path.end());

            u = current_start;
            while (u != end_id) {
                u = came_from_end[u];
                path.push_back(u);
            }

            return path;
        }

        for (uint64_t neighbor : get_adjacent_nodes(db, current_start)) {
            if (closed_set_start.find(neighbor) != closed_set_start.end()) continue;

            float tentative_g_score = g_score_start[current_start] + get_distance(db, current_start, neighbor);

            // Check whitelist
            if (!is_node_allowed(neighbor, db, pedestrian, riding, driving, pubTransport))
                continue;

            if (tentative_g_score < g_score_start[neighbor]) {
                came_from_start[neighbor] = current_start;
                g_score_start[neighbor] = tentative_g_score;
                f_score_start[neighbor] = g_score_start[neighbor] + heuristic(neighbor, end_id);
                open_set_start.push(neighbor);
            }
        }

        // Process from end
        uint64_t current_end = open_set_end.top();
        open_set_end.pop();
        closed_set_end.insert(current_end);

        if (closed_set_start.find(current_end) != closed_set_start.end()) {
            // Path found
            std::vector<uint64_t> path;
            uint64_t u = current_end;
            while (u != end_id) {
                path.push_back(u);
                u = came_from_end[u];
            }
            path.push_back(end_id);
            std::reverse(path.begin(), path.end());

            u = current_end;
            while (u != start_id) {
                u = came_from_start[u];
                path.push_back(u);
            }

            return path;
        }

        for (uint64_t neighbor : get_adjacent_nodes(db, current_end)) {
            if (closed_set_end.find(neighbor) != closed_set_end.end()) continue;

            float tentative_g_score = g_score_end[current_end] + get_distance(db, current_end, neighbor);

            // Check whitelist
            if (!is_node_allowed(neighbor, db, pedestrian, riding, driving, pubTransport))
                continue;

            if (tentative_g_score < g_score_end[neighbor]) {
                came_from_end[neighbor] = current_end;
                g_score_end[neighbor] = tentative_g_score;
                f_score_end[neighbor] = g_score_end[neighbor] + heuristic(neighbor, start_id);
                open_set_end.push(neighbor);
            }
        }
    }

    return {}; // No path found
}

// Function to find the shortest path using Bellman-Ford algorithm
std::vector<uint64_t> bellman_ford(sqlite3* db, uint64_t start_id, uint64_t end_id,
                                   bool pedestrian, bool riding, bool driving, bool pubTransport) {
    std::unordered_map<uint64_t, float> distances;
    std::unordered_map<uint64_t, uint64_t> previous;

    distances[start_id] = 0.0f;

    int cur_progress = 0;

    for (size_t i = 0; i < index_to_node_id.size() - 1; ++i) {
        for (uint64_t u : index_to_node_id) {
            for (uint64_t neighbor : get_adjacent_nodes(db, u)) {
                // Check whitelist
                if (!is_node_allowed(neighbor, db, pedestrian, riding, driving, pubTransport))
                    continue;

                float weight = get_distance(db, u, neighbor);
                if (distances[u] + weight < distances[neighbor]) {
                    distances[neighbor] = distances[u] + weight;
                    previous[neighbor] = u;
                }
            }
        }
        int progress = static_cast<int>((i * 100.0) / index_to_node_id.size());
        if (progress <= 100 && progress > cur_progress) {
            std::cout << progress << std::endl;
            cur_progress = progress;
        }
    }

    // Reconstruct path
    std::vector<uint64_t> path;
    uint64_t u = end_id;
    if (previous[u] != start_id || u == start_id) {
        while (u != start_id) {
            path.push_back(u);
            u = previous[u];
        }
        path.push_back(start_id);
        std::reverse(path.begin(), path.end());
    }

    return path;
}

// Function to find the shortest path using Floyd-Warshall algorithm
std::vector<uint32_t> floyd_warshall(sqlite3* db,
                                     uint32_t start_idx,
                                     uint32_t end_idx,
                                     const std::unordered_map<uint64_t, std::string>& node_tags,
                                     bool pedestrian, bool riding, bool driving, bool pubTransport) {
    std::vector<std::vector<float>> dist(index_to_node_id.size(), std::vector<float>(index_to_node_id.size(), std::numeric_limits<float>::infinity()));
    std::vector<std::vector<uint32_t>> next(index_to_node_id.size(), std::vector<uint32_t>(index_to_node_id.size(), -1));

    for (size_t u = 0; u < index_to_node_id.size(); ++u) {
        dist[u][u] = 0.0f;
        auto adjacent_nodes = get_adjacent_nodes(db, index_to_node_id[u]);
        for (uint64_t neighbor : adjacent_nodes) {
            if (!is_node_allowed(neighbor, db, pedestrian, riding, driving, pubTransport))
                continue;

            uint32_t v = std::distance(index_to_node_id.begin(), std::find(index_to_node_id.begin(), index_to_node_id.end(), neighbor));
            double weight = get_distance(db, index_to_node_id[u], neighbor);
            dist[u][v] = weight;
            next[u][v] = v;
        }
    }

    size_t total_nodes = index_to_node_id.size();
    size_t processed_nodes = 0;
    int last_progress = 0;

    for (size_t k = 0; k < index_to_node_id.size(); ++k) {
        for (size_t i = 0; i < index_to_node_id.size(); ++i) {
            for (size_t j = 0; i < index_to_node_id.size(); ++j) {
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
