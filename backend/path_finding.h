#ifndef PATH_FINDING_H
#define PATH_FINDING_H

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint> // Include for fixed-width integer types

struct Edge {
    uint32_t target; // Use 32-bit unsigned int for node index
    float weight;    // Use 32-bit float for weight
};

bool load_graph(const std::string& xml_file,
                std::vector<std::vector<Edge>>& graph,
                std::unordered_map<uint64_t, uint32_t>& node_id_to_index,
                std::vector<uint64_t>& index_to_node_id,
                std::unordered_map<uint64_t, std::pair<double, double>>& node_coords_map);

std::vector<uint32_t> dijkstra(const std::vector<std::vector<Edge>>& graph,
                               uint32_t start_idx,
                               uint32_t end_idx);

std::vector<uint32_t> a_star(const std::vector<std::vector<Edge>>& graph,
                             uint32_t start_idx,
                             uint32_t end_idx,
                             const std::vector<std::pair<double, double>>& node_coords);

std::vector<uint32_t> bellman_ford(const std::vector<std::vector<Edge>>& graph,
                                   uint32_t start_idx,
                                   uint32_t end_idx);

std::vector<uint32_t> floyd_warshall(const std::vector<std::vector<Edge>>& graph,
                                     uint32_t start_idx,
                                     uint32_t end_idx);

#endif // PATH_FINDING_H
