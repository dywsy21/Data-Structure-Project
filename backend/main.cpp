#include "tinyxml/tinyxml.h"
#include "path_finding.h"
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <chrono> // Include for timing
#include <cstdint>

int main(int argc, char* argv[]) {
    std::vector<std::vector<Edge>> graph;
    std::unordered_map<uint64_t, uint32_t> node_id_to_index;
    std::vector<uint64_t> index_to_node_id;
    std::unordered_map<uint64_t, std::pair<double, double>> node_coords_map;
    std::vector<std::pair<double, double>> node_coords;

    // Load the graph once at startup
    bool success = load_graph(R"(E:\BaiduSyncdisk\Code Projects\PyQt Projects\Data Structure Project\backend\data\map_large)", graph, node_id_to_index, index_to_node_id, node_coords_map);
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
    uint64_t start_id, end_id;
    while (std::cin >> algorithm >> start_id >> end_id) {
        // Convert node IDs to indices
        if (node_id_to_index.find(start_id) == node_id_to_index.end() ||
            node_id_to_index.find(end_id) == node_id_to_index.end()) {
            std::cerr << "Invalid node IDs provided." << std::endl;
            continue;
        }
        uint32_t start_idx = node_id_to_index[start_id];
        uint32_t end_idx = node_id_to_index[end_id];

        auto start_time = std::chrono::high_resolution_clock::now(); // Start timing

        std::vector<uint32_t> path_indices;
        if (algorithm == "Dijkstra") {
            path_indices = dijkstra(graph, start_idx, end_idx);
        } else if (algorithm == "A*") {
            path_indices = a_star(graph, start_idx, end_idx, node_coords);
        } else if (algorithm == "Bellman-Ford") {
            path_indices = bellman_ford(graph, start_idx, end_idx);
        } else if (algorithm == "Floyd-Warshall") {
            path_indices = floyd_warshall(graph, start_idx, end_idx);
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
