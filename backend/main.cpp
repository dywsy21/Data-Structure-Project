#include "tinyxml/tinyxml.h"
#include "path_finding.h"
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <chrono> // Include for timing

int main(int argc, char* argv[]) {
    std::unordered_map<std::string, std::vector<Edge>> graph;
    // Load the graph once at startup
    bool success = load_graph(R"(E:\BaiduSyncdisk\Code Projects\PyQt Projects\Data Structure Project\backend\data\map_large)", graph);
    if (!success) {
        std::cerr << "Failed to load graph data." << std::endl;
        return 1;
    }
    std::cout << "Graph loaded successfully." << std::endl;

    std::string algorithm, start, end;
    while (std::cin >> algorithm >> start >> end) {
        auto start_time = std::chrono::high_resolution_clock::now(); // Start timing

        std::vector<std::string> path;
        if (algorithm == "Dijkstra") {
            path = dijkstra(graph, start, end);
        } else if (algorithm == "A*") {
            path = a_star(graph, start, end);
        } else if (algorithm == "Bellman-Ford") {
            path = bellman_ford(graph, start, end);
        } else if (algorithm == "Floyd-Warshall") {
            path = floyd_warshall(graph, start, end);
        } else {
            std::cerr << "Unknown algorithm: " << algorithm << std::endl;
            continue;
        }

        auto end_time = std::chrono::high_resolution_clock::now(); // End timing
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        std::cout << "TIME " << duration << "ms" << std::endl; // Output the elapsed time

        if (!path.empty()) {
            for (const auto& node_id : path) {
                std::cout << node_id << std::endl;
            }
            std::cout << "END" << std::endl;
        } else {
            std::cout << "NO PATH" << std::endl;
        }
    }

    return 0;
}
