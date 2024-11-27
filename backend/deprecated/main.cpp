#include "tinyxml/tinyxml.h"
#include "path_finding.h"
#include "sqlite/sqlite3.h"
#include "utils.h"
#include "defs.h"

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
    // Get database connection
    sqlite3* db = open_database_connection("../backend/data/map_for_backend.db");
    if (!db) {
        std::cerr << "Failed to connect to database." << std::endl;
        return 1;
    }

    bool pedestrian_enabled = true;
    bool riding_enabled = true;
    bool driving_enabled = true;
    bool pubTransport_enabled = true;

    std::string algorithm;
    int node_number;
    std::vector<std::pair<double, double>> node_coords;
    std::vector<uint64_t> node_ids;

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
            // Implement logic to find nearest node_id from lat, lon using the database
            // For simplicity, assume we have a function get_nearest_node_id in utils.cpp
            uint64_t node_id = get_nearest_node_id(db, coord.first, coord.second, pedestrian_enabled, riding_enabled, driving_enabled, pubTransport_enabled);
            node_ids.push_back(node_id);
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

            std::vector<uint64_t> path_segment;
            if (algorithm == "Dijkstra") {
                path_segment = dijkstra(db, start_id, end_id, pedestrian_enabled, riding_enabled, driving_enabled, pubTransport_enabled);
            } else if (algorithm == "A*") {
                path_segment = a_star(db, start_id, end_id, pedestrian_enabled, riding_enabled, driving_enabled, pubTransport_enabled);
            } else if (algorithm == "Bellman-Ford") {
                path_segment = bellman_ford(db, start_id, end_id, pedestrian_enabled, riding_enabled, driving_enabled, pubTransport_enabled);
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

    close_database_connection(db);
    return 0;
}
