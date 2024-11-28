#include "path_finding.h"
#include "pugixml/pugixml.hpp" // Include pugixml header
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
#include "k-dtree.h"
#include <string.h>
#include "defs.h"

extern std::vector<uint64_t> index_to_node_id;

// Function to load the graph from an XML file
bool load_graph(const std::string& filepath,
                std::unordered_map<uint64_t, uint32_t>& node_id_to_index,
                std::unordered_map<uint64_t, std::string>& node_tags,
                std::unordered_map<uint64_t, std::pair<double, double>>& node_id_to_coords,
                bool pedestrian, bool riding, bool driving, bool pubTransport,
                KdTree& kd_tree) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filepath.c_str());
    if (!result) {
        std::cout << "Failed to load file: " << filepath << std::endl;
        return false;
    }

    pugi::xml_node root = doc.child("osm");
    if (!root) {
        std::cout << "Invalid XML format: No root element." << std::endl;
        return false;
    }

    // First pass: Estimate counts to reserve space
    size_t node_count = 0;
    size_t way_count = 0;
    std::unordered_set<uint64_t> highway_node_ids;
    for (pugi::xml_node elem = root.first_child(); elem; elem = elem.next_sibling()) {
        const char* name = elem.name();
        if (strcmp(name, "node") == 0) {
            ++node_count;
        } else if (strcmp(name, "way") == 0) {
            ++way_count;
            bool has_highway_tag = false;
            for (pugi::xml_node tag = elem.child("tag"); tag; tag = tag.next_sibling("tag")) {
                const char* key = tag.attribute("k").as_string();
                if (strcmp(key, "highway") == 0) {
                    has_highway_tag = true;
                    break;
                }
            }
            if (has_highway_tag) {
                for (pugi::xml_node nd = elem.child("nd"); nd; nd = nd.next_sibling("nd")) {
                    uint64_t ref = nd.attribute("ref").as_ullong();
                    highway_node_ids.insert(ref);
                }
            }
        }
    }

    // Reserve space to avoid reallocations
    index_to_node_id.clear(); // Clear any existing data
    index_to_node_id.reserve(node_count);
    node_id_to_index.clear(); // Clear any existing data
    node_id_to_index.reserve(node_count);
    kd_tree.reserve(node_count);
    node_tags.clear(); // Clear any existing data
    node_tags.reserve(node_count);
    node_id_to_coords.clear(); // Clear any existing data

    // Second pass: Parse nodes and ways
    uint32_t index = 0;
    size_t processed_elements = 0;
    size_t total_elements = node_count + way_count;
    int last_progress = -1;

    for (pugi::xml_node elem = root.first_child(); elem; elem = elem.next_sibling()) {
        const char* name = elem.name();
        if (strcmp(name, "node") == 0) {
            uint64_t id = elem.attribute("id").as_ullong();
            double lat = elem.attribute("lat").as_double();
            double lon = elem.attribute("lon").as_double();
            if (highway_node_ids.find(id) != highway_node_ids.end()) {
                node_id_to_index[id] = index;
                index_to_node_id.push_back(id);
                kd_tree.insert({lat, lon}, index); // Pass index here
                ++index;
            }
            node_id_to_coords[id] = std::make_pair(lat, lon);

        } else if (strcmp(name, "way") == 0) {
            std::vector<uint64_t> node_refs;
            for (pugi::xml_node nd = elem.child("nd"); nd; nd = nd.next_sibling("nd")) {
                uint64_t ref = nd.attribute("ref").as_ullong();
                node_refs.push_back(ref);
            }

            // Add edges between consecutive nodes
            for (size_t i = 0; i < node_refs.size() - 1; ++i) {
                uint64_t from_id = node_refs[i];
                uint64_t to_id = node_refs[i + 1];
                auto from_it = node_id_to_index.find(from_id);
                auto to_it = node_id_to_index.find(to_id);
                if (from_it != node_id_to_index.end() && to_it != node_id_to_index.end()) {
                    uint32_t from_idx = from_it->second;
                    uint32_t to_idx = to_it->second;
                    // Use precomputed coordinates
                    std::vector<double> from_coords = kd_tree.getPoint(from_idx);
                    std::vector<double> to_coords = kd_tree.getPoint(to_idx);
                    double distance = std::hypot(to_coords[0] - from_coords[0], to_coords[1] - from_coords[1]);
                    
                    kd_tree.insertEdge(from_idx, to_idx, distance);
                    kd_tree.insertEdge(to_idx, from_idx, distance); // Assuming undirected graph
                }
            }

            // Handle tags for ways
            for (pugi::xml_node tag = elem.child("tag"); tag; tag = tag.next_sibling("tag")) {
                const char* key = tag.attribute("k").value();
                if (strcmp(key, "highway") == 0) {
                    const char* value = tag.attribute("v").value();
                    for (uint64_t node_id : node_refs) {
                        node_tags[node_id] = value;
                    }
                    break; // Assuming one highway tag per way
                }
            }
        }

        // Update progress
        ++processed_elements;
        int progress = static_cast<int>((processed_elements * 100.0) / total_elements);
        if(last_progress != progress)
        {
            last_progress = progress;
            std::cout << "\rLoading graph: " << progress << "%";
            std::cout.flush();
        }
    }

    std::cout << std::endl; // Move to the next line after progress is complete

    // Further processing if needed, such as filtering based on node_tags and whitelist flags

    return true;
}

// Helper function to determine if a node is allowed based on whitelist
bool is_node_allowed(uint64_t node_id, const std::unordered_map<uint64_t, std::string>& node_tags,
                    bool pedestrian, bool riding, bool driving, bool pubTransport) {
    auto it = node_tags.find(node_id);
    if (it == node_tags.end()) return false;

    const std::string& highway_type = it->second;

    if (pedestrian && (highway_type == "pedestrian" || highway_type == "footway" ||
                      highway_type == "steps" || highway_type == "path" ||
                      highway_type == "living_street"))
        return true;
    if (riding && (highway_type == "cycleway" || highway_type == "path" ||
                  highway_type == "track"))
        return true;
    if (driving && (highway_type == "motorway" || highway_type == "trunk" ||
                   highway_type == "primary" || highway_type == "secondary" ||
                   highway_type == "tertiary" || highway_type == "service" ||
                   highway_type == "motorway_link" || highway_type == "trunk_link" ||
                   highway_type == "primary_link" || highway_type == "secondary_link" ||
                   highway_type == "residential"))
        return true;
    if (pubTransport && (highway_type == "bus_stop" || highway_type == "motorway_junction" ||
                        highway_type == "traffic_signals" || highway_type == "crossing"))
        return true;

    return false;
}

// Function to find the shortest path using Dijkstra's algorithm
std::vector<uint32_t> dijkstra(const KdTree& kd_tree, uint32_t start, uint32_t end,
                               const std::unordered_map<uint64_t, std::string>& node_tags,
                               bool pedestrian, bool riding, bool driving, bool pubTransport) {
    std::vector<double> distances(kd_tree.size(), std::numeric_limits<double>::infinity());
    std::vector<uint32_t> previous(kd_tree.size(), -1);
    std::priority_queue<std::pair<double, uint32_t>,
                        std::vector<std::pair<double, uint32_t>>,
                        std::greater<std::pair<double, uint32_t>>> queue;

    distances[start] = 0.0;
    queue.emplace(0.0, start);

    #ifdef DEBUG
    std::cout << "[DEBUG] Starting Dijkstra's algorithm from node " << start << " to node " << end << std::endl;
    #endif

    while (!queue.empty()) {
        auto [dist, current] = queue.top();
        queue.pop();

        #ifdef DEBUG
        std::cout << "[DEBUG] Visiting node " << current << " with current distance " << dist << std::endl;
        #endif

        if (current == end) {
            #ifdef DEBUG
            std::cout << "[DEBUG] Reached the destination node " << end << std::endl;
            #endif
            break;
        }

        if (dist > distances[current]) continue;

        for (const auto& edge : kd_tree.getEdges(current)) {
            uint32_t neighbor = edge.first;
            double new_dist = dist + edge.second;

            // Get node_id from index_to_node_id
            uint64_t neighbor_id = index_to_node_id[neighbor];

            // Check whitelist
            if (!is_node_allowed(neighbor_id, node_tags, pedestrian, riding, driving, pubTransport)) {
                #ifdef DEBUG
                std::cout << "[DEBUG] Node " << neighbor << " is not allowed based on the whitelist" << std::endl;
                #endif
                continue;
            }

            #ifdef DEBUG
            std::cout << "[DEBUG] Checking neighbor node " << neighbor << " with edge weight " << edge.second << std::endl;
            #endif

            if (new_dist < distances[neighbor]) {
                distances[neighbor] = new_dist;
                previous[neighbor] = current;
                queue.emplace(new_dist, neighbor);
                #ifdef DEBUG
                std::cout << "[DEBUG] Updated distance for node " << neighbor << " to " << new_dist << std::endl;
                #endif
            }
        }
    }

    // Reconstruct path
    std::vector<uint32_t> path;
    if (distances[end] == std::numeric_limits<double>::infinity()) {
        #ifdef DEBUG
        std::cout << "[DEBUG] No path found to node " << end << std::endl;
        #endif
        return path; // No path found
    }

    for (uint32_t at = end; at != start; at = previous[at]) {
        path.push_back(at);
        if (previous[at] == static_cast<uint32_t>(-1)) {
            #ifdef DEBUG
            std::cout << "[DEBUG] No path found during reconstruction" << std::endl;
            #endif
            return std::vector<uint32_t>(); // No path found
        }
    }
    path.push_back(start);
    std::reverse(path.begin(), path.end());

    #ifdef DEBUG
    std::cout << "[DEBUG] Path found: ";
    for (uint32_t node : path) {
        std::cout << node << " ";
    }
    std::cout << std::endl;
    #endif

    return path;
}

// Function to find the shortest path using Bidirectional A* algorithm
std::vector<uint32_t> a_star(const KdTree& kd_tree, uint32_t start, uint32_t end,
                             const std::vector<std::pair<double, double>>& coords,
                             const std::unordered_map<uint64_t, std::string>& node_tags,
                             bool pedestrian, bool riding, bool driving, bool pubTransport) {
    auto heuristic = [&coords](uint32_t a, uint32_t b) {
        // Implement a heuristic function here, for simplicity, we use 0 (equivalent to Dijkstra)
        return 0.0f;
    };

    std::vector<float> g_score_start(kd_tree.size(), std::numeric_limits<float>::infinity());
    std::vector<float> g_score_end(kd_tree.size(), std::numeric_limits<float>::infinity());
    std::vector<float> f_score_start(kd_tree.size(), std::numeric_limits<float>::infinity());
    std::vector<float> f_score_end(kd_tree.size(), std::numeric_limits<float>::infinity());
    std::vector<uint32_t> came_from_start(kd_tree.size(), -1);
    std::vector<uint32_t> came_from_end(kd_tree.size(), -1);

    g_score_start[start] = 0.0f;
    f_score_start[start] = heuristic(start, end);
    g_score_end[end] = 0.0f;
    f_score_end[end] = heuristic(end, start);

    auto cmp_start = [&f_score_start](uint32_t a, uint32_t b) {
        return f_score_start[a] > f_score_start[b];
    };
    auto cmp_end = [&f_score_end](uint32_t a, uint32_t b) {
        return f_score_end[a] > f_score_end[b];
    };

    std::priority_queue<uint32_t, std::vector<uint32_t>, decltype(cmp_start)> open_set_start(cmp_start);
    std::priority_queue<uint32_t, std::vector<uint32_t>, decltype(cmp_end)> open_set_end(cmp_end);

    open_set_start.push(start);
    open_set_end.push(end);

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

        for (const auto& edge : kd_tree.getEdges(current_start)) {
            if (closed_set_start.find(edge.first) != closed_set_start.end()) continue;

            float tentative_g_score = g_score_start[current_start] + edge.second;

            // Get node_id from index_to_node_id
            uint64_t neighbor_id = index_to_node_id[edge.first];

            // Check whitelist
            if (!is_node_allowed(neighbor_id, node_tags, pedestrian, riding, driving, pubTransport))
                continue;

            if (tentative_g_score < g_score_start[edge.first]) {
                came_from_start[edge.first] = current_start;
                g_score_start[edge.first] = tentative_g_score;
                f_score_start[edge.first] = g_score_start[edge.first] + heuristic(edge.first, end);
                open_set_start.push(edge.first);
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

        for (const auto& edge : kd_tree.getEdges(current_end)) {
            if (closed_set_end.find(edge.first) != closed_set_end.end()) continue;

            float tentative_g_score = g_score_end[current_end] + edge.second;

            // Get node_id from index_to_node_id
            uint64_t neighbor_id = index_to_node_id[edge.first];

            // Check whitelist
            if (!is_node_allowed(neighbor_id, node_tags, pedestrian, riding, driving, pubTransport))
                continue;

            if (tentative_g_score < g_score_end[edge.first]) {
                came_from_end[edge.first] = current_end;
                g_score_end[edge.first] = tentative_g_score;
                f_score_end[edge.first] = g_score_end[edge.first] + heuristic(edge.first, start);
                open_set_end.push(edge.first);
            }
        }
    }

    return {}; // No path found
}

// Function to find the shortest path using Bellman-Ford algorithm
std::vector<uint32_t> bellman_ford(const KdTree& kd_tree, uint32_t start, uint32_t end,
                                   const std::unordered_map<uint64_t, std::string>& node_tags,
                                   bool pedestrian, bool riding, bool driving, bool pubTransport) {
    std::vector<float> distances(kd_tree.size(), std::numeric_limits<float>::infinity());
    std::vector<uint32_t> previous(kd_tree.size(), -1);

    distances[start] = 0.0f;

    int cur_progress = 0;

    for (size_t i = 0; i < kd_tree.size() - 1; ++i) {
        for (size_t u = 0; u < kd_tree.size(); ++u) {
            for (const auto& edge : kd_tree.getEdges(u)) {
                // Get node_id from index_to_node_id
                uint64_t neighbor_id = index_to_node_id[edge.first];

                if (!is_node_allowed(neighbor_id, node_tags, pedestrian, riding, driving, pubTransport))
                    continue;

                if (distances[u] + edge.second < distances[edge.first]) {
                    distances[edge.first] = distances[u] + edge.second;
                    previous[edge.first] = u;
                }
            }
        }
        int progress = static_cast<int>((i * 100.0) / kd_tree.size());
        if (progress <= 100 && progress > cur_progress) {
            std::cout << progress << std::endl;
            cur_progress = progress;
        }
    }

    // Reconstruct path
    std::vector<uint32_t> path;
    uint32_t u = end;
    if (previous[u] != -1 || u == start) {
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
std::vector<uint32_t> floyd_warshall(const KdTree& kd_tree, uint32_t start, uint32_t end,
                                     const std::unordered_map<uint64_t, std::string>& node_tags,
                                     bool pedestrian, bool riding, bool driving, bool pubTransport) {
    std::vector<std::vector<float>> dist(kd_tree.size(), std::vector<float>(kd_tree.size(), std::numeric_limits<float>::infinity()));
    std::vector<std::vector<uint32_t>> next(kd_tree.size(), std::vector<uint32_t>(kd_tree.size(), -1));

    for (size_t u = 0; u < kd_tree.size(); ++u) {
        dist[u][u] = 0.0f;
        for (const auto& edge : kd_tree.getEdges(u)) {
            // Get node_id from index_to_node_id
            uint64_t neighbor_id = index_to_node_id[edge.first];

            if (!is_node_allowed(neighbor_id, node_tags, pedestrian, riding, driving, pubTransport))
                continue;

            dist[u][edge.first] = edge.second;
            next[u][edge.first] = edge.first;
        }
    }

    size_t total_nodes = kd_tree.size();
    size_t processed_nodes = 0;
    int last_progress = 0;

    for (size_t k = 0; k < kd_tree.size(); ++k) {
        for (size_t i = 0; i < kd_tree.size(); ++i) {
            for (size_t j = 0; j < kd_tree.size(); ++j) {
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
    if (next[start][end] != -1) {
        uint32_t u = start;
        while (u != end) {
            path.push_back(u);
            u = next[u][end];
        }
        path.push_back(end);
    }

    return path;
}

uint64_t get_nearest_node_id(const KdTree& kd_tree, double lat, double lon,
                             const std::unordered_map<uint64_t, std::string>& node_tags,
                             bool pedestrian, bool riding, bool driving, bool pubTransport) {
    std::vector<double> point = {lat, lon};
    int k = 1;

    static const int MAXK = 10;

    while (k < MAXK) {
        std::vector<std::vector<double>> nearest_points = kd_tree.findKthNearestNeighbor(point, k);
        if (!nearest_points.empty()) {
            const auto& nearest_point = nearest_points.back(); // Only process the k-th nearest point
            auto it = std::find(kd_tree.points.begin(), kd_tree.points.end(), nearest_point);
            if (it != kd_tree.points.end()) {
                size_t index = std::distance(kd_tree.points.begin(), it);
                if (index < index_to_node_id.size()) { // Ensure index is within bounds
                    uint64_t node_id = index_to_node_id[index];
                    if (is_node_allowed(node_id, node_tags, pedestrian, riding, driving, pubTransport)) {
                        return node_id;
                    }
                }
            }
        }
        k++;
    }

    return 0; // Return 0 if no nearest node is found
}
