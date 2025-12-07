#ifndef COLLECTOR_H
#define COLLECTOR_H

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include <map>
#include <functional>
#include <curl/curl.h>
#include "json.hpp"


// Forward declarations
class Api;
class AbstractDevice;

// Data structure for device measurements
struct DeviceMeasurement {
    uint8_t device_id;
    std::string device_name;
    std::string ip_address;
    std::chrono::system_clock::time_point timestamp;
    
    // Power data
    float power;        // W
    float energy_total; // kWh
    float energy_today; // kWh
    
    // Device state
    bool is_online;
};

// Abstract base class for data sources
class DataSource {
public:
    virtual ~DataSource() = default;
    virtual bool get_measurement(const std::string& device_identifier, DeviceMeasurement& measurement) = 0;
    virtual bool is_available(const std::string& device_identifier) = 0;
    virtual std::string get_source_type() const = 0;
};

// Tasmota HTTP API data source
class TasmotaAPISource : public DataSource {
private:
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
    std::string make_http_request(const std::string& url);
    nlohmann::json parse_tasmota_response(const std::string& response);
    
public:
    TasmotaAPISource();
    ~TasmotaAPISource();
    
    bool get_measurement(const std::string& device_identifier, DeviceMeasurement& measurement) override;
    bool is_available(const std::string& device_identifier) override;
    std::string get_source_type() const override { return "TasmotaAPI"; }
};

// UDP protocol data source (existing devices)
class UDPSource : public DataSource {
private:
    Api* api_ref;
    
public:
    UDPSource(Api* api);
    
    bool get_measurement(const std::string& device_identifier, DeviceMeasurement& measurement) override;
    bool is_available(const std::string& device_identifier) override;
    std::string get_source_type() const override { return "UDP"; }
};

// Forward declaration of classes
class AbstractDevice;

// Simple Collector class - uses device list from Api
class Collector {
private:
    Api* api;                              // Reference to Api class with device list
    std::thread worker_thread;             // Background thread
    bool running;                          // Thread control flag
    
    void run();  // Thread main function

public:
    Collector(Api* api_ref);
    ~Collector();
    
    // Thread control
    void start();  // Start collector thread
    void stop();   // Stop collector thread
    
    // Status
    bool is_running() const;
};

#endif // COLLECTOR_H
