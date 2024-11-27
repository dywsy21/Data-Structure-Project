#include "path_finding.h"
#include "defs.h"
#include "k-dtree.h"

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <chrono> // Include for timing
#include <cstdint>

// Define the global variable
std::vector<uint64_t> index_to_node_id;

int main(int argc, char* argv[]) {
    bool pedestrian_enabled = true;
    bool riding_enabled = true;
    bool driving_enabled = true;
    bool pubTransport_enabled = true;

    std::string algorithm;
    int node_number;
    std::vector<std::pair<double, double>> node_coords;
    std::vector<uint64_t> node_ids;
    std::string filepath = "../backend/data/map.osm";
    
    KdTree kd_tree(2); // Initialize KdTree with 2 dimensions (latitude and longitude)
    std::unordered_map<uint64_t, uint32_t> node_id_to_index;
    std::unordered_map<uint64_t, std::string> node_tags;

    auto load_start_time = std::chrono::high_resolution_clock::now();
    if (!load_graph(filepath, node_id_to_index, node_tags, pedestrian_enabled, riding_enabled, driving_enabled, pubTransport_enabled, kd_tree)) {
        std::cerr << "Failed to load graph." << std::endl;
        return -1;
    }
    auto load_end_time = std::chrono::high_resolution_clock::now();
    auto load_duration = std::chrono::duration_cast<std::chrono::milliseconds>(load_end_time - load_start_time).count();
    std::cout << "Graph loaded in " << load_duration << "ms" << std::endl;

    while (std::cin >> algorithm >> pedestrian_enabled >> riding_enabled >> driving_enabled >> pubTransport_enabled >> node_number) {
        node_coords.clear();
        node_ids.clear();

        for (int i = 0; i < node_number; i++) {
            double lat, lon;
            std::cin >> lat >> lon;
            node_coords.push_back({lat, lon});
        }

        // Function to find nearest node IDs from coordinates
        for (const auto& coord : node_coords) {
            uint64_t node_id = get_nearest_node_id(kd_tree, coord.first, coord.second, node_tags, pedestrian_enabled, riding_enabled, driving_enabled, pubTransport_enabled);
            node_ids.push_back(node_id);
        }

        // if any id of the node is 0, print "NO PATH" and continue
        if (std::find(node_ids.begin(), node_ids.end(), 0) != node_ids.end()) {
            std::cout << "NO PATH" << std::endl;
            continue;
        }

        #ifdef DEBUG
            std::cout << "[DEBUG] Node IDs: ";
            for (uint64_t node_id : node_ids) {
                std::cout << node_id << " ";
            }
            std::cout << std::endl;
        #endif

        auto start_time = std::chrono::high_resolution_clock::now();

        std::vector<uint64_t> full_path;

        for (size_t i = 0; i < node_ids.size() - 1; ++i) {
            uint64_t start_id = node_ids[i];
            uint64_t end_id = node_ids[i + 1];

            std::vector<uint32_t> path_segment;
            if (algorithm == "Dijkstra") {
                path_segment = dijkstra(kd_tree, node_id_to_index[start_id], node_id_to_index[end_id], node_tags, pedestrian_enabled, riding_enabled, driving_enabled, pubTransport_enabled);
            } else if (algorithm == "A*") {
                path_segment = a_star(kd_tree, node_id_to_index[start_id], node_id_to_index[end_id], node_coords, node_tags, pedestrian_enabled, riding_enabled, driving_enabled, pubTransport_enabled);
            } else if (algorithm == "Bellman-Ford") {
                path_segment = bellman_ford(kd_tree, node_id_to_index[start_id], node_id_to_index[end_id], node_tags, pedestrian_enabled, riding_enabled, driving_enabled, pubTransport_enabled);
            } else {
                std::cerr << "Unknown algorithm: " << algorithm << std::endl;
                continue;
            }

            if (path_segment.empty()) {
                std::cout << "NO PATH" << std::endl;
                break;
            }

            if (i > 0) path_segment.erase(path_segment.begin());
            full_path.insert(full_path.end(), path_segment.begin(), path_segment.end());
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        std::cout << "TIME " << duration << "ms" << std::endl;

        if (!full_path.empty()) {
            for (uint64_t node_id : full_path) {
                std::cout << node_id << std::endl;
            }
            std::cout << "END" << std::endl;
        } else {
            std::cout << "NO PATH" << std::endl;
        }
    }

    return 0;
}
