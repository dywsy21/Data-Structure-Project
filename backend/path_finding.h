#ifndef PATH_FINDING_H
#define PATH_FINDING_H

#include <vector>
#include <unordered_map>
#include <utility>
#include <cstdint>
#include <string>
#include "k-dtree.h"

extern std::vector<uint64_t> index_to_node_id;

// Define Edge structure
struct Edge {
    uint32_t to;
    double weight;
};

// Function declarations with whitelist parameters and correct node_tags type
std::vector<uint32_t> dijkstra(const std::vector<std::vector<Edge>>& graph, uint32_t start, uint32_t end,
                               const std::unordered_map<uint64_t, std::string>& node_tags,
                               bool pedestrian, bool riding, bool driving, bool pubTransport);

std::vector<uint32_t> a_star(const std::vector<std::vector<Edge>>& graph, uint32_t start, uint32_t end,
                            const std::vector<std::pair<double, double>>& coords,
                            const std::unordered_map<uint64_t, std::string>& node_tags,
                            bool pedestrian, bool riding, bool driving, bool pubTransport);

std::vector<uint32_t> bellman_ford(const std::vector<std::vector<Edge>>& graph, uint32_t start, uint32_t end,
                                   const std::unordered_map<uint64_t, std::string>& node_tags,
                                   bool pedestrian, bool riding, bool driving, bool pubTransport);

std::vector<uint32_t> floyd_warshall(const std::vector<std::vector<Edge>>& graph, uint32_t start, uint32_t end,
                                     const std::unordered_map<uint64_t, std::string>& node_tags,
                                     bool pedestrian, bool riding, bool driving, bool pubTransport);

// Declaration of load_graph function
bool load_graph(const std::string& filepath,
                std::unordered_map<uint64_t, uint32_t>& node_id_to_index,
                std::unordered_map<uint64_t, std::string>& node_tags,
                bool pedestrian, bool riding, bool driving, bool pubTransport,
                KdTree& kd_tree);

#endif // PATH_FINDING_H
