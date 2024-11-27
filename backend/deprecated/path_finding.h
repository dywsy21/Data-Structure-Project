#ifndef PATH_FINDING_H
#define PATH_FINDING_H

#include <vector>
#include <unordered_map>
#include <utility>
#include <cstdint>
#include <string>
#include "sqlite/sqlite3.h"
#include "defs.h"

extern std::vector<uint64_t> index_to_node_id;

// Define Edge structure
struct Edge {
    uint32_t to;
    double weight;
};

// Function declarations with whitelist parameters and correct node_tags type
std::vector<uint64_t> dijkstra(sqlite3* db, uint64_t start_id, uint64_t end_id,
                               bool pedestrian, bool riding, bool driving, bool pubTransport);

std::vector<uint64_t> a_star(sqlite3* db, uint64_t start_id, uint64_t end_id,
                             bool pedestrian, bool riding, bool driving, bool pubTransport);

std::vector<uint64_t> bellman_ford(sqlite3* db, uint64_t start_id, uint64_t end_id,
                                   bool pedestrian, bool riding, bool driving, bool pubTransport);

std::vector<uint32_t> floyd_warshall(const std::vector<std::vector<Edge>>& graph, uint32_t start, uint32_t end,
                                     const std::unordered_map<uint64_t, std::string>& node_tags,
                                     bool pedestrian, bool riding, bool driving, bool pubTransport);

// // Declaration of load_graph function
// bool load_graph(const std::string& filepath,
//                 std::vector<std::vector<Edge>>& graph,
//                 std::unordered_map<uint64_t, uint32_t>& node_id_to_index,
//                 std::vector<uint64_t>& index_to_node_id,
//                 std::unordered_map<uint64_t, std::pair<double, double>>& node_coords_map,
//                 std::unordered_map<uint64_t, std::string>& node_tags,
//                 bool pedestrian, bool riding, bool driving, bool pubTransport);

#endif // PATH_FINDING_H
