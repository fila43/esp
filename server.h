#include <netinet/in.h>
#include "client.h"
#include <string>
#include <vector>
#include <iostream>
#include <thread>
#include <iomanip>
#include <libpq-fe.h>
#include <tuple>
#include <utility>
#include <map>
#include <chrono>
#include <memory>

// Forward declarations to avoid circular dependency
struct DeviceMeasurement;
class Collector;

class Api;
class UnixSocketServer;

// Definice možných stavů pro unix socket komunikaci
enum class UnixSocketStatus {
    OK,
    ERROR,
    INVALID_COMMAND,
    DEVICE_NOT_FOUND,
    INVALID_ARGUMENT,
    INTERNAL_ERROR
};

// Abstract Device class - contains only common elements
class AbstractDevice{
protected:
	int send_message(Message * m);

public:
	std::string name;
	Api * api;
	uint32_t id;      // Changed from uint8_t to uint32_t for ESP Chip ID
	std::string ip;

	AbstractDevice(std::string ip, std::string name, uint32_t id);
	AbstractDevice(std::string ip, uint32_t id);
	virtual ~AbstractDevice() = default;
	
    // Common methods
	std::string toString();
	uint32_t get_id();
	int send_ack(uint16_t);
	virtual void print();
    virtual void print_state() {}
    virtual void update_from_measurement(const DeviceMeasurement& measurement);
    virtual void request_state() = 0;  // Polymorphic method - each device type implements differently
    
    // ESP01 compatibility methods (default empty implementation for non-ESP01 devices)
    virtual bool parse_state(State *s) { return false; }
    virtual int request_ctemp() { return 0; }
    virtual int set_on() { return 0; }
    virtual int set_off() { return 0; }
    virtual int set_auto_mode(int mode_type) { return 0; }
    virtual int set_temp(float temp) { return 0; }
    virtual int set_auto_mode() { return set_auto_mode(AUTO_TEMP); }
};

// ESP01 Custom - temperature and state functionality
class ESP01_custom : public AbstractDevice {
public:
    int32_t ctemp;
    int32_t dtemp;
    uint8_t fsm;
    uint8_t dstate;

	ESP01_custom(std::string ip, std::string name, uint32_t id);
	ESP01_custom(std::string ip, uint32_t id);
	
    // Temperature and state methods
    bool parse_state(State *s);
	int request_ctemp();
	void request_state() override;  // ESP01 specific state retrieval (polymorphic)
	int set_on();
	int set_off();
	int set_auto_mode(int mode_type);
	int set_temp(float temp);
    void print_state() override;
    int set_auto_mode() { return set_auto_mode(AUTO_TEMP); }
};

// Power Meter Device - power monitoring functionality
class PowerMeterDevice : public AbstractDevice {
public:
    // Power monitoring fields
    float power;        // active power in W
    float energy_total; // total energy in kWh
    float energy_today; // today's energy in kWh
    std::chrono::system_clock::time_point last_power_update;
    
    PowerMeterDevice(std::string ip, std::string name, uint32_t id);
    PowerMeterDevice(std::string ip, uint32_t id);
    
    void print_state() override;
    void update_from_measurement(const DeviceMeasurement& measurement) override;
    void request_state() override;          // Power meter HTTP API state retrieval
    void request_power_udp();               // Request power data via UDP (fast, asynchronous)
    
    // ESP01 compatibility methods (do nothing - PowerMeterDevice doesn't support these)
    bool parse_state(State *s);        // Does nothing - PowerMeterDevice uses HTTP not UDP state
    int request_ctemp();               // Does nothing - PowerMeterDevice doesn't have temperature sensor
    int set_on();                      // Does nothing - PowerMeterDevice doesn't control relay
    int set_off();                     // Does nothing - PowerMeterDevice doesn't control relay  
    int set_auto_mode(int mode_type);  // Does nothing - PowerMeterDevice doesn't have auto mode
    int set_temp(float temp);          // Does nothing - PowerMeterDevice doesn't control temperature
    int set_auto_mode() { return set_auto_mode(AUTO_TEMP); } // Does nothing - compatibility wrapper
};

class Devices{
public:
	std::vector<AbstractDevice*> devices;
	bool add_device(AbstractDevice * d);
	bool remove_device(AbstractDevice * d);
	bool remove_device(uint32_t id);
	void print_devices();
	void print_device(uint32_t id);
	AbstractDevice * get_device(uint32_t id);
};

class Connection{
protected:
    struct sockaddr_in serv;
    socklen_t len;
    Api * api;

public:
    int fd;
    /**
     *
     * @param port
     */
    Connection(in_port_t port, Api * a = NULL);
    Connection();
    /**
     *
     */
    void bindPort();
    /**
     *
     * @param data
     * @param ip
     * @param port
     */
    void send(std::string data,std::string ip,in_port_t port);
    void send(Message * m, AbstractDevice * d);
    void listen();
    std::thread start_listen();
    bool broadcast_hello_message();

};

class Watcher{
    Api * a;
    unsigned delay;
    bool stop;
    std::map<int, std::thread> schedule_threads;
    std::map<int, bool> stop_flags;
public:
    Watcher();
    void send_stop();
    void add_api(Api * a);
    void set_delay( unsigned d);
    void watch();
    std::thread start_watch();
    std::thread start_periodic_state_query(unsigned period_sec = 20);
    void set_schedule(int device_id, const std::vector<std::pair<std::string, std::string>>& schedule);
    static Watcher& instance();
    void stop_schedule(int device_id);
};


class Event{
	static unsigned int id;
	Message *m;
};

class Events{
	std::vector<Event *> events;
public:
	void add_event(Event *e);
	void remove_event(uint8_t seq);
	void remove_event(Event *e);
	Event * get_event(unsigned int id);
	Event * get_event(uint8_t seq);
};

class Api{
public:
	Devices devs;
	Events evs;
	Connection c;
	std::unique_ptr<Collector> collector;
	
	Api();
	~Api();
	
	int process_message(AbstractDevice *, Message *);
	AbstractDevice * new_device(std::string ip, uint32_t id, uint8_t device_type);
    std::pair<UnixSocketStatus, std::string> handle_unix_command(const std::string& cmd);
    
    // Collector management methods
    void setup_collector();
    
    // Broadcast management
    void start_periodic_broadcast();
    void stop_periodic_broadcast();
    
private:
    std::thread broadcast_thread;
    bool broadcast_running = false;
    void periodic_broadcast_loop();
};

class Cli{
    Api *a;
public:
    void print_header();
    void print_table();
    void start();
    void print_devices();
    Cli(Api *a);
    bool run_command(AbstractDevice*, std::string);
    std::string select_command();
    int select_device();
    void print_commands();


};

std::string code_to_str(uint8_t c);
int send_ack(AbstractDevice * d, int16_t seq);
int new_device(unsigned short id, struct sockaddr_in * net_info, AbstractDevice ** device);

// Třída pro obsluhu unix socketu
class UnixSocketServer {
public:
    UnixSocketServer(Api* api, const std::string& socket_path = "/tmp/server.sock");
    void start(); // spustí server v samostatném vlákně
    void stop();
    bool is_running() const { return running; }
private:
    Api* api;
    std::string socket_path;
    int server_fd = -1;
    bool running = false;
    void handle_client(int client_fd);
};

class HttpServer {
public:
    HttpServer(Api* api, int port = 8080);
    void start();
private:
    Api* api;
    int port;
};

// Deklarace třídy pro přístup k databázi TimescaleDB přes libpq
class DbApi {
public:
    DbApi(const std::string& conninfo);
    ~DbApi();
    bool connect();
    void disconnect();
    // Legacy method (backward compatibility)
    bool save_measurement(int device_id, double temperature, double target_temperature, bool is_active, const std::string& mode);
    // New power measurement methods
    bool save_power_measurement(uint32_t device_id, float power_watts);
    std::vector<std::tuple<std::string, uint32_t, float>> get_power_measurements(uint32_t device_id, const std::string& from, const std::string& to);
    std::vector<std::tuple<std::string, uint32_t, float>> get_recent_power_measurements(int hours = 24);
    // Legacy methods
    std::vector<std::tuple<std::string, double, double, bool, std::string>> get_measurements(int device_id, const std::string& from, const std::string& to);
    std::vector<std::pair<int, std::string>> get_devices();
private:
    std::string conninfo_;
    PGconn* conn_;
};

