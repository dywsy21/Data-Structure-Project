#include "defs.h"
#include "utils.h"
#include <cmath>
#include <limits>

// static const double M_PI = 3.14159265358979323846;
static const double PRECISION = 0.001;

// // Function to calculate tile coordinates from latitude and longitude
// std::pair<int, int> lat_lon_to_tile(double lat, double lon, int zoom) {
//     double lat_rad = lat * M_PI / 180.0;
//     double n = std::pow(2.0, zoom);
//     int xtile = static_cast<int>((lon + 180.0) / 360.0 * n);
//     int ytile = static_cast<int>((1.0 - std::log(std::tan(lat_rad) + 1.0 / std::cos(lat_rad)) / M_PI) / 2.0 * n);
//     return {xtile, ytile};
// }

sqlite3* open_database_connection(const std::string& db_path) {
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
        sqlite3_close(db);
        return nullptr;
    }
    return db;
}

void close_database_connection(sqlite3* db) {
    sqlite3_close(db);
}

std::vector<uint64_t> get_adjacent_nodes(sqlite3* db, uint64_t node_id) {
    std::vector<uint64_t> adjacent_nodes;
    const char* sql = "SELECT wn2.node_id FROM way_nodes wn1 "
                      "JOIN way_nodes wn2 ON wn1.way_id = wn2.way_id "
                      "WHERE wn1.node_id = ? AND wn2.node_id != ?";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, node_id);
    sqlite3_bind_int64(stmt, 2, node_id);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        uint64_t adjacent_node_id = sqlite3_column_int64(stmt, 0);
        adjacent_nodes.push_back(adjacent_node_id);
    }
    sqlite3_finalize(stmt);
    #ifdef DEBUG
        std::cout << "[DEBUG] Adjacent Nodes returned: ";
        for (uint64_t node_id : adjacent_nodes) {
            std::cout << node_id << " ";
        }
        std::cout << '\n';
    #endif
    return adjacent_nodes;
}

double get_distance(sqlite3* db, uint64_t from_node_id, uint64_t to_node_id) {
    auto from_coords = get_node_coords(db, from_node_id);
    auto to_coords = get_node_coords(db, to_node_id);
    double lat_diff = from_coords.first - to_coords.first;
    double lon_diff = from_coords.second - to_coords.second;
    return std::sqrt(lat_diff * lat_diff + lon_diff * lon_diff);
}

std::pair<double, double> get_node_coords(sqlite3* db, uint64_t node_id) {
    double lat = 0.0, lon = 0.0;
    const char* sql = "SELECT lat, lon FROM nodes WHERE id = ?";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, node_id);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        lat = sqlite3_column_double(stmt, 0);
        lon = sqlite3_column_double(stmt, 1);
    }
    sqlite3_finalize(stmt);
    return {lat, lon};
}

bool is_node_allowed(uint64_t node_id, sqlite3* db,
                     bool pedestrian, bool riding, bool driving, bool pubTransport) {
    const char* sql = "SELECT v FROM node_tags WHERE node_id = ? AND k = 'highway'";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, node_id);
    bool allowed = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* highway_type = sqlite3_column_text(stmt, 0);
        std::string type = reinterpret_cast<const char*>(highway_type);

        if (pedestrian && (type == "pedestrian" || type == "footway" ||
            type == "steps" || type == "path" || type == "living_street"))
            allowed = true;
        if (riding && (type == "cycleway" || type == "path" || type == "track"))
            allowed = true;
        if (driving && (type == "motorway" || type == "trunk" ||
            type == "primary" || type == "secondary" ||
            type == "tertiary" || type == "service" ||
            type == "motorway_link" || type == "trunk_link" ||
            type == "primary_link" || type == "secondary_link" ||
            type == "residential"))
            allowed = true;
        if (pubTransport && (type == "bus_stop" || type == "motorway_junction" ||
            type == "traffic_signals" || type == "crossing"))
            allowed = true;
    }
    sqlite3_finalize(stmt);
    return allowed;
}

uint64_t get_nearest_node_id(sqlite3* db, double lat, double lon,
                             bool pedestrian, bool riding, bool driving, bool pubTransport) {
    double min_distance = std::numeric_limits<double>::max();
    uint64_t nearest_node_id = 0;
    
    int loop_count = 0;
    while(nearest_node_id == 0) {
        double cur_precision = PRECISION * std::pow(2, loop_count);
        double lat_min = lat - PRECISION;
        double lat_max = lat + PRECISION;
        double lon_min = lon - PRECISION;
        double lon_max = lon + PRECISION;

        const char* sql = "SELECT id, lat, lon FROM nodes WHERE lat BETWEEN ? AND ? AND lon BETWEEN ? AND ?";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
            return 0;
        }
        sqlite3_bind_double(stmt, 1, lat_min);
        sqlite3_bind_double(stmt, 2, lat_max);
        sqlite3_bind_double(stmt, 3, lon_min);
        sqlite3_bind_double(stmt, 4, lon_max);

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            uint64_t node_id = sqlite3_column_int64(stmt, 0);
            double node_lat = sqlite3_column_double(stmt, 1);
            double node_lon = sqlite3_column_double(stmt, 2);

            // #ifdef DEBUG
            //     std::cout << "[DEBUG] In get_nearest node id, Node ID: " << node_id << " Lat: " << node_lat << " Lon: " << node_lon << '\n';
            // #endif

            if (is_node_allowed(node_id, db, pedestrian, riding, driving, pubTransport)) {
                double lat_diff = node_lat - lat;
                double lon_diff = node_lon - lon;
                double distance = lat_diff * lat_diff + lon_diff * lon_diff;
                if (distance < min_distance) {
                    min_distance = distance;
                    nearest_node_id = node_id;
                }
            }
        }

        if (rc != SQLITE_DONE) {
            std::cerr << "Failed to execute statement: " << sqlite3_errmsg(db) << std::endl;
        }

        sqlite3_finalize(stmt);
        loop_count++;
    }

    #ifdef DEBUG
        std::cout << "[DEBUG] Nearest Node ID returned: " << nearest_node_id << '\n';
    #endif
        
    return nearest_node_id;
}

void preprocess_node_ids(const std::vector<std::pair<double, double>>& node_coords, // lat, lon
                         std::vector<int>& node_ids,
                         bool pedestrian, bool riding, bool driving, bool pubTransport) 
{
    sqlite3* db;
    sqlite3_open("data/map_for_backend.db", &db);

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
