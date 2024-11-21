#include "utils.h"
#include <cmath>
#include <limits>
#include <sqlite3.h>

static const double M_PI = 3.14159265358979323846;
static const double PRECISION = 1e-4;

// // Function to calculate tile coordinates from latitude and longitude
// std::pair<int, int> lat_lon_to_tile(double lat, double lon, int zoom) {
//     double lat_rad = lat * M_PI / 180.0;
//     double n = std::pow(2.0, zoom);
//     int xtile = static_cast<int>((lon + 180.0) / 360.0 * n);
//     int ytile = static_cast<int>((1.0 - std::log(std::tan(lat_rad) + 1.0 / std::cos(lat_rad)) / M_PI) / 2.0 * n);
//     return {xtile, ytile};
// }

// Helper function to determine if a node is allowed based on whitelist
bool is_node_allowed(uint64_t node_id, const std::unordered_map<uint64_t, std::string>& node_tags,
                    bool pedestrian, bool riding, bool driving, bool pubTransport) {
    auto it = node_tags.find(node_id);
    if (it == node_tags.end()) return false;

    const std::string& highway_type = it->second;

    if (pedestrian && (highway_type == "pedestrian" || highway_type == "footway" ||
                      highway_type == "steps" || highway_type == "path" ||
                      highway_type == "living_street"))
        return true;
    if (riding && (highway_type == "cycleway" || highway_type == "path" ||
                  highway_type == "track"))
        return true;
    if (driving && (highway_type == "motorway" || highway_type == "trunk" ||
                   highway_type == "primary" || highway_type == "secondary" ||
                   highway_type == "tertiary" || highway_type == "service" ||
                   highway_type == "motorway_link" || highway_type == "trunk_link" ||
                   highway_type == "primary_link" || highway_type == "secondary_link" ||
                   highway_type == "residential"))
        return true;
    if (pubTransport && (highway_type == "bus_stop" || highway_type == "motorway_junction" ||
                        highway_type == "traffic_signals" || highway_type == "crossing"))
        return true;

    return false;
}


void preprocess_node_ids(const std::vector<std::pair<double, double>>& node_coords, // lat, lon
                         std::vector<int>& node_ids,
                         bool pedestrian, bool riding, bool driving, bool pubTransport) 
{
    sqlite3* db;
    sqlite3_open("data/map.db", &db);

    for (const auto& [lat, lon] : node_coords) {
        double min_distance = std::numeric_limits<double>::max();
        int nearest_node_id = -1;

        std::string query = "SELECT id, lat, lon FROM nodes WHERE lat BETWEEN ? AND ? AND lon BETWEEN ? AND ?";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        sqlite3_bind_double(stmt, 1, lat - PRECISION); // Adjust the range as needed
        sqlite3_bind_double(stmt, 2, lat + PRECISION);
        sqlite3_bind_double(stmt, 3, lon - PRECISION);
        sqlite3_bind_double(stmt, 4, lon + PRECISION);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int node_id = sqlite3_column_int(stmt, 0);
            double node_lat = sqlite3_column_double(stmt, 1);
            double node_lon = sqlite3_column_double(stmt, 2);

            double distance = std::sqrt(std::pow(lat - node_lat, 2) + std::pow(lon - node_lon, 2));
            if (distance < min_distance) {
                min_distance = distance;
                nearest_node_id = node_id;
            }
        }

        sqlite3_finalize(stmt);
        if (nearest_node_id != -1) {
            node_ids.push_back(nearest_node_id);
        }
    }

    sqlite3_close(db);
}