#include "tinyxml/tinyxml.h"
#include "path_finding.h"
#include "sqlite3.h"
#include "utils.h"

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
    std::vector<std::vector<Edge>> graph;
    std::unordered_map<uint64_t, uint32_t> node_id_to_index;
    // Remove the local declaration of index_to_node_id
    // std::vector<uint64_t> index_to_node_id; // Remove this line
    std::unordered_map<uint64_t, std::pair<double, double>> node_coords_map;
    std::vector<std::pair<double, double>> node_coords;

    bool pedestrian_enabled = true;
    bool riding_enabled = true;
    bool driving_enabled = true;
    bool pubTransport_enabled = true;

    // Define node_tags with uint64_t keys
    std::unordered_map<uint64_t, std::string> node_tags; // Initialize appropriately

    // Load the graph with the whitelist flags and node_tags
    bool success = load_graph(R"(E:\BaiduSyncdisk\Code Projects\PyQt Projects\Data Structure Project\backend\data\map_large)", 
                              graph, 
                              node_id_to_index, 
                              index_to_node_id, 
                              node_coords_map,
                              node_tags,
                              pedestrian_enabled,
                              riding_enabled,
                              driving_enabled,
                              pubTransport_enabled);
    if (!success) {
        std::cerr << "Failed to load graph data." << std::endl;
        return 1;
    }
    std::cout << "Graph loaded successfully." << std::endl;

    // Prepare node coordinates for A* algorithm
    for (const auto& [node_id, index] : node_id_to_index) {
        node_coords.push_back(node_coords_map[node_id]);
    }

    std::string algorithm;
    int node_number;
    std::vector<std::pair<double, double>> node_coords;
    std::vector<int> node_ids;

    while (std::cin >> algorithm >> pedestrian_enabled >> riding_enabled >> driving_enabled >> pubTransport_enabled >> node_number) {

        for(int i = 0; i < node_number; i++) {
            double lat, lon;
            std::cin >> lat >> lon;
            node_coords.push_back({lat, lon});
        }

        preprocess_node_ids(node_coords, node_ids, pedestrian_enabled, riding_enabled, driving_enabled, pubTransport_enabled);

        auto start_time = std::chrono::high_resolution_clock::now(); // Start timing


        std::vector<uint32_t> path_indices;
        for(int i = 0; i < node_number; i++){

        }
        if (algorithm == "Dijkstra") {
            path_indices.insert(path_indices.end(), dijkstra(graph, start_idx, end_idx, node_tags, pedestrian_enabled, riding_enabled, driving_enabled, pubTransport_enabled));
        } else if (algorithm == "A*") {
            path_indices.insert(path_indices.end(), a_star(graph, start_idx, end_idx, node_coords, node_tags, pedestrian_enabled, riding_enabled, driving_enabled, pubTransport_enabled));
        } else if (algorithm == "Bellman-Ford") {
            path_indices.insert(path_indices.end(), bellman_ford(graph, start_idx, end_idx, node_tags, pedestrian_enabled, riding_enabled, driving_enabled, pubTransport_enabled));
        } else if (algorithm == "Floyd-Warshall") {
            path_indices.insert(path_indices.end(), floyd_warshall(graph, start_idx, end_idx, node_tags, pedestrian_enabled, riding_enabled, driving_enabled, pubTransport_enabled));
        } else {
            std::cerr << "Unknown algorithm: " << algorithm << std::endl;
            continue;
        }

        auto end_time = std::chrono::high_resolution_clock::now(); // End timing
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        std::cout << "TIME " << duration << "ms" << std::endl; // Output the elapsed time

        if (!path_indices.empty()) {
            for (uint32_t idx : path_indices) {
                std::cout << index_to_node_id[idx] << std::endl; // Convert index back to node ID
            }
            std::cout << "END" << std::endl;
        } else {
            std::cout << "NO PATH" << std::endl;
        }
    }

    return 0;
}
