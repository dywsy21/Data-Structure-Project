#include "tinyxml/tinyxml.h"
#include "path_finding.h"
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: backend <start_node_id> <end_node_id>" << std::endl;
        return 1;
    }

    std::string start = argv[1];
    std::string end = argv[2];

    // Parse the XML and build the graph
    std::unordered_map<std::string, std::vector<Edge>> graph;

    // Use raw string literal for Windows path
    bool success = load_graph(R"(E:\BaiduSyncdisk\Code Projects\PyQt Projects\Data Structure Project\backend\data\map_large)", graph);
    if (!success) {
        return 1;
    }

    // Find the shortest path using Dijkstra's algorithm
    std::vector<std::string> path = dijkstra(graph, start, end);

    if (!path.empty()) {
        for (const auto& node_id : path) {
            std::cout << node_id << std::endl;
        }
    } else {
        std::cerr << "No path found." << std::endl;
        return 1;
    }

    return 0;
}

// ...existing code...
