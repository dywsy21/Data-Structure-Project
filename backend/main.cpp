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
#include <csignal> // Include for signal handling

// Define the global variable
std::vector<uint64_t> index_to_node_id;

void signalHandler(int signal) {
    if (signal == SIGSEGV) {
        std::cerr << "Error: Segmentation fault (signal " << signal << ")" << std::endl;
    } else if (signal == SIGABRT) {
        std::cerr << "Error: Aborted (signal " << signal << ")" << std::endl;
    } else {
        std::cerr << "Error: Signal " << signal << " received" << std::endl;
    }
    exit(signal);
}

int main(int argc, char* argv[]) {
    std::ios::sync_with_stdio(0); std::cin.tie(0); std::cout.tie(0);

    // Register signal handler
    std::signal(SIGSEGV, signalHandler);
    std::signal(SIGABRT, signalHandler);
    std::signal(SIGFPE, signalHandler);
    std::signal(SIGILL, signalHandler);
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    bool pedestrian_enabled = true;
    bool riding_enabled = true;
    bool driving_enabled = true;
    bool pubTransport_enabled = true;

    std::string algorithm;
    int node_number;
    std::vector<std::pair<double, double>> node_coords;
    std::vector<uint64_t> node_ids;
    std::unordered_map<uint64_t, std::pair<double, double>> node_id_to_coords;

    // std::string filepath = "../backend/data/map.osm";
    std::string filepath = "E:\\BaiduSyncdisk\\Code Projects\\PyQt Projects\\Data Structure Project\\backend\\data\\map.osm";
    
    KdTree kd_tree(2); // Initialize KdTree with 2 dimensions (latitude and longitude)
    std::unordered_map<uint64_t, uint32_t> node_id_to_index;
    std::unordered_map<uint64_t, std::string> node_tags;

    index_to_node_id.clear(); // Clear any existing data
    node_id_to_coords.clear(); // Clear any existing data

    auto load_start_time = std::chrono::high_resolution_clock::now();
    if (!load_graph(filepath, node_id_to_index, node_tags, node_id_to_coords, pedestrian_enabled, riding_enabled, driving_enabled, pubTransport_enabled, kd_tree)) {
        std::cout << "Failed to load graph." << std::endl;
        return -1;
    }
    auto load_end_time = std::chrono::high_resolution_clock::now();
    auto load_duration = std::chrono::duration_cast<std::chrono::milliseconds>(load_end_time - load_start_time).count();
    std::cout << "Graph loaded in " << (double)load_duration / (double)1000 << "s" << std::endl;

    // for(auto [k, v]: node_id_to_coords) {
    //     std::cout << k << " " << v.first << " " << v.second << std::endl;
    // }

    while (std::cin >> algorithm >> pedestrian_enabled >> riding_enabled >> driving_enabled >> pubTransport_enabled >> node_number) { 
        try {
            if (!pedestrian_enabled && !riding_enabled && !driving_enabled && !pubTransport_enabled) {
                for (int i = 0; i < node_number; i++) {
                    double lat, lon;
                    std::cin >> lat >> lon;
                }
                std::cout << "NO PATH" << std::endl;
                continue;
            }

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

            // for(auto [k, v]: node_id_to_index) {
            //     std::cout << k << " " << v << std::endl;
            // }

            // for (auto& v: kd_tree.points) {
            //     // set precision to 10 decimal places
            //     std::cout.precision(10);
            //     std::cout << v[0] << " " << v[1] << std::endl;
            // }

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
                    std::cout << "Unknown algorithm: " << algorithm << std::endl;
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
                std::cout.precision(10); // Set precision to 10 decimal places
                for (uint64_t node_index : full_path) {
                    auto v = kd_tree.points[node_index];
                    std::cout << v[0] << ' ' << v[1] << '\n';
                }
                std::cout << "END" << std::endl;
            } else {
                std::cout << "NO PATH" << std::endl;
            }
        }
        catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << std::endl;
        }
    }

    std::cout << "Invalid input, exiting..." << std::endl;
    return 0;
}
