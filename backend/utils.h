#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <cmath>
#include <unordered_map>
#include <utility>

void preprocess_node_ids(const std::vector<std::pair<double, double>>& node_coords, 
                         std::vector<int>& node_ids, 
                         const std::unordered_map<int, std::string>& node_tags);
bool is_node_allowed(uint64_t node_id, const std::unordered_map<uint64_t, std::string>& node_tags,
                    bool pedestrian, bool riding, bool driving, bool pubTransport);



#endif // UTILS_H