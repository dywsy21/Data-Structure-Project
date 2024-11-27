#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <cmath>
#include <unordered_map>
#include <utility>
#include "sqlite/sqlite3.h"
#include "defs.h"
#include <iostream>

// ...existing code...

extern sqlite3* open_database_connection(const std::string& db_path);
extern void close_database_connection(sqlite3* db);

std::vector<uint64_t> get_adjacent_nodes(sqlite3* db, uint64_t node_id);
double get_distance(sqlite3* db, uint64_t from_node_id, uint64_t to_node_id);
std::pair<double, double> get_node_coords(sqlite3* db, uint64_t node_id);
bool is_node_allowed(uint64_t node_id, sqlite3* db,
                     bool pedestrian, bool riding, bool driving, bool pubTransport);
uint64_t get_nearest_node_id(sqlite3* db, double lat, double lon,
                             bool pedestrian, bool riding, bool driving, bool pubTransport);
void preprocess_node_ids(const std::vector<std::pair<double, double>>& node_coords, 
                         std::vector<int>& node_ids, 
                         const std::unordered_map<int, std::string>& node_tags);

#endif // UTILS_H