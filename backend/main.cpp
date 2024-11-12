#include "tinyxml/tinyxml.h"
#include "path_finding.h"
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

int main(int argc, char* argv[]) {
    std::unordered_map<std::string, std::vector<Edge>> graph;
    // Load the graph once at startup
    bool success = load_graph(R"(E:\BaiduSyncdisk\Code Projects\PyQt Projects\Data Structure Project\backend\data\map_large)", graph);
    if (!success) {
        std::cerr << "Failed to load graph data." << std::endl;
        return 1;
    }
    std::cout << "Graph loaded successfully." << std::endl;

    std::string start, end;
    while (std::cin >> start >> end) {
        std::vector<std::string> path = dijkstra(graph, start, end);
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

// ...existing code...
