#include "path_finding.h"
#include "tinyxml/tinyxml.h" // Ensure the path is correct
#include <iostream>
#include <queue>
#include <limits>
#include <algorithm>

// Function to load the graph from an XML file
bool load_graph(const std::string& xml_file, std::unordered_map<std::string, std::vector<Edge>>& graph) {
    TiXmlDocument doc(xml_file.c_str());
    if (!doc.LoadFile()) {
        std::cerr << "Failed to load XML file: " << xml_file << std::endl;
        return false;
    }

    TiXmlElement* root = doc.RootElement();

    for (TiXmlElement* way = root->FirstChildElement("way"); way != nullptr; way = way->NextSiblingElement("way")) {
        std::vector<std::string> node_refs;
        for (TiXmlElement* nd = way->FirstChildElement("nd"); nd != nullptr; nd = nd->NextSiblingElement("nd")) {
            const char* ref = nd->Attribute("ref");
            if (ref) {
                node_refs.emplace_back(ref);
            }
        }

        // Assuming bidirectional edges with weight 1.0
        for (size_t i = 0; i < node_refs.size() - 1; ++i) {
            graph[node_refs[i]].push_back(Edge{node_refs[i + 1], 1.0});
            graph[node_refs[i + 1]].push_back(Edge{node_refs[i], 1.0});
        }
    }

    return true;
}

// Function to find the shortest path using Dijkstra's algorithm
std::vector<std::string> dijkstra(const std::unordered_map<std::string, std::vector<Edge>>& graph,
                                  const std::string& start,
                                  const std::string& end) {
    std::unordered_map<std::string, double> distances;
    std::unordered_map<std::string, std::string> previous;

    for (const auto& pair : graph) {
        distances[pair.first] = std::numeric_limits<double>::infinity();
    }
    distances[start] = 0.0;

    auto cmp = [&distances](const std::string& a, const std::string& b) {
        return distances[a] > distances[b];
    };
    std::priority_queue<std::string, std::vector<std::string>, decltype(cmp)> queue(cmp);
    queue.push(start);

    while (!queue.empty()) {
        std::string current = queue.top();
        queue.pop();

        if (current == end) break;

        for (const auto& edge : graph.at(current)) {
            double alt = distances[current] + edge.weight;
            if (alt < distances[edge.to]) {
                distances[edge.to] = alt;
                previous[edge.to] = current;
                queue.push(edge.to);
            }
        }
    }

    // Reconstruct path
    std::vector<std::string> path;
    std::string u = end;
    if (previous.find(u) != previous.end() || u == start) {
        while (u != start) {
            path.push_back(u);
            u = previous[u];
        }
        path.push_back(start);
        std::reverse(path.begin(), path.end());
    }

    return path;
}
