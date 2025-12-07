#include "server.h"
#include <iostream>
#include <vector>
#include <tuple>

DbApi::DbApi(const std::string& conninfo)
    : conninfo_(conninfo), conn_(nullptr) {}

DbApi::~DbApi() {
    disconnect();
}

bool DbApi::connect() {
    if (conn_) return true;
    conn_ = PQconnectdb(conninfo_.c_str());
    if (PQstatus(conn_) != CONNECTION_OK) {
        std::cerr << "Connection to database failed: " << PQerrorMessage(conn_) << std::endl;
        PQfinish(conn_);
        conn_ = nullptr;
        return false;
    }
    return true;
}

void DbApi::disconnect() {
    if (conn_) {
        PQfinish(conn_);
        conn_ = nullptr;
    }
}

bool DbApi::save_measurement(int device_id, double temperature, double target_temperature, bool is_active, const std::string& mode) {
    // Legacy method - redirect to power measurement for backward compatibility
    return save_power_measurement(device_id, temperature);
}

bool DbApi::save_power_measurement(uint32_t device_id, float power_watts) {
    if (!connect()) return false;
    
    char query[256];
    snprintf(query, sizeof(query),
        "INSERT INTO power_measurements (device_id, power_consumption) VALUES (%u, %f)",
        device_id, power_watts);
        
    PGresult* res = PQexec(conn_, query);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::cerr << "INSERT into power_measurements failed: " << PQerrorMessage(conn_) << std::endl;
        PQclear(res);
        return false;
    }
    PQclear(res);
    return true;
}

std::vector<std::tuple<std::string, double, double, bool, std::string>> DbApi::get_measurements(int device_id, const std::string& from, const std::string& to) {
    // Legacy method - kept for backward compatibility
    std::vector<std::tuple<std::string, double, double, bool, std::string>> result;
    return result; // Return empty for now
}

std::vector<std::tuple<std::string, uint32_t, float>> DbApi::get_power_measurements(uint32_t device_id, const std::string& from, const std::string& to) {
    std::vector<std::tuple<std::string, uint32_t, float>> result;
    if (!connect()) return result;
    
    char query[512];
    snprintf(query, sizeof(query),
        "SELECT measured_at, device_id, power_consumption FROM power_measurements WHERE device_id = %u AND measured_at BETWEEN '%s' AND '%s' ORDER BY measured_at ASC",
        device_id, from.c_str(), to.c_str());
        
    PGresult* res = PQexec(conn_, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::cerr << "SELECT power_measurements failed: " << PQerrorMessage(conn_) << std::endl;
        PQclear(res);
        return result;
    }
    
    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        std::string measured_at = PQgetvalue(res, i, 0);
        uint32_t device_id_result = static_cast<uint32_t>(atoi(PQgetvalue(res, i, 1)));
        float power_consumption = static_cast<float>(atof(PQgetvalue(res, i, 2)));
        result.emplace_back(measured_at, device_id_result, power_consumption);
    }
    PQclear(res);
    return result;
}

std::vector<std::tuple<std::string, uint32_t, float>> DbApi::get_recent_power_measurements(int hours) {
    std::vector<std::tuple<std::string, uint32_t, float>> result;
    if (!connect()) return result;
    
    char query[256];
    snprintf(query, sizeof(query),
        "SELECT measured_at, device_id, power_consumption FROM power_measurements WHERE measured_at > NOW() - INTERVAL '%d hours' ORDER BY measured_at DESC LIMIT 1000",
        hours);
        
    PGresult* res = PQexec(conn_, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::cerr << "SELECT recent power_measurements failed: " << PQerrorMessage(conn_) << std::endl;
        PQclear(res);
        return result;
    }
    
    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        std::string measured_at = PQgetvalue(res, i, 0);
        uint32_t device_id_result = static_cast<uint32_t>(atoi(PQgetvalue(res, i, 1)));
        float power_consumption = static_cast<float>(atof(PQgetvalue(res, i, 2)));
        result.emplace_back(measured_at, device_id_result, power_consumption);
    }
    PQclear(res);
    return result;
}

std::vector<std::pair<int, std::string>> DbApi::get_devices() {
    std::vector<std::pair<int, std::string>> result;
    if (!connect()) return result;
    const char* query = "SELECT id, name FROM device ORDER BY id ASC";
    PGresult* res = PQexec(conn_, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::cerr << "SELECT failed: " << PQerrorMessage(conn_) << std::endl;
        PQclear(res);
        return result;
    }
    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        int id = atoi(PQgetvalue(res, i, 0));
        std::string name = PQgetvalue(res, i, 1);
        result.emplace_back(id, name);
    }
    PQclear(res);
    return result;
} 