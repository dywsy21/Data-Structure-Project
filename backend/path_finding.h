#ifndef PATH_FINDING_H
#define PATH_FINDING_H

#include <string>
#include <unordered_map>
#include <vector>

// Structure to represent an edge in the graph
struct Edge {
    std::string to;
    double weight;
};

// Function to load the graph from an XML file
bool load_graph(const std::string& filename, std::unordered_map<std::string, std::vector<Edge>>& graph);
// Function to find the shortest path using Dijkstra's algorithm
std::vector<std::string> dijkstra(const std::unordered_map<std::string, std::vector<Edge>>& graph,
                                  const std::string& start,
                                  const std::string& end);

// Function to find the shortest path using A* algorithm
std::vector<std::string> a_star(const std::unordered_map<std::string, std::vector<Edge>>& graph,
                                const std::string& start,
                                const std::string& end);

// Function to find the shortest path using Bellman-Ford algorithm
std::vector<std::string> bellman_ford(const std::unordered_map<std::string, std::vector<Edge>>& graph,
                                      const std::string& start,
                                      const std::string& end);

// Function to find the shortest path using Floyd-Warshall algorithm
std::vector<std::string> floyd_warshall(const std::unordered_map<std::string, std::vector<Edge>>& graph,
                                        const std::string& start,
                                        const std::string& end);

#endif // PATH_FINDING_H
