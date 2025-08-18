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
    if (!connect()) return false;
    char query[512];
    snprintf(query, sizeof(query),
        "INSERT INTO device_measurement (device_id, temperature, target_temperature, is_active, mode) VALUES (%d, %f, %f, %s, '%s')",
        device_id, temperature, target_temperature, is_active ? "true" : "false", mode.c_str());
    PGresult* res = PQexec(conn_, query);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::cerr << "INSERT failed: " << PQerrorMessage(conn_) << std::endl;
        PQclear(res);
        return false;
    }
    PQclear(res);
    return true;
}

std::vector<std::tuple<std::string, double, double, bool, std::string>> DbApi::get_measurements(int device_id, const std::string& from, const std::string& to) {
    std::vector<std::tuple<std::string, double, double, bool, std::string>> result;
    if (!connect()) return result;
    char query[512];
    snprintf(query, sizeof(query),
        "SELECT measured_at, temperature, target_temperature, is_active, mode FROM device_measurement WHERE device_id = %d AND measured_at BETWEEN '%s' AND '%s' ORDER BY measured_at ASC",
        device_id, from.c_str(), to.c_str());
    PGresult* res = PQexec(conn_, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::cerr << "SELECT failed: " << PQerrorMessage(conn_) << std::endl;
        PQclear(res);
        return result;
    }
    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        std::string measured_at = PQgetvalue(res, i, 0);
        double temperature = atof(PQgetvalue(res, i, 1));
        double target_temperature = atof(PQgetvalue(res, i, 2));
        bool is_active = strcmp(PQgetvalue(res, i, 3), "t") == 0 || strcmp(PQgetvalue(res, i, 3), "true") == 0;
        std::string mode = PQgetvalue(res, i, 4);
        result.emplace_back(measured_at, temperature, target_temperature, is_active, mode);
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