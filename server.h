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

class Device{
private:
	int send_message(Message * m);

public:
	std::string name;
	Api * api;
	uint8_t id;
	std::string ip;
    int32_t ctemp;
    int32_t dtemp;
    uint8_t fsm;
    uint8_t dstate;


	Device(std::string ip, std::string name, uint8_t id);
	Device(std::string ip, uint8_t id);
    bool parse_state(State *s);
	int request_ctemp();
	int request_state();
	int set_on();
	int set_off();
	int set_auto_mode(int mode_type);
	int set_temp(float temp);
	std::string toString();
	uint8_t get_id();
	int send_ack(uint16_t);
	void print();
    void print_state();
    int set_auto_mode() { return set_auto_mode(AUTO_TEMP); }
};

class Devices{
public:
	std::vector<Device*> devices;
	bool add_device(Device * d);
	bool remove_device(Device * d);
	bool remove_device(uint8_t id);
	void print_devices();
	void print_device(uint8_t id);
	Device * get_device(uint8_t id);
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
    void send(Message * m, Device * d);
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
	int process_message(Device *, Message *);
	Device * new_device(std::string ip, uint8_t id);
    std::pair<UnixSocketStatus, std::string> handle_unix_command(const std::string& cmd);
};

class Cli{
    Api *a;
public:
    void print_header();
    void print_table();
    void start();
    void print_devices();
    Cli(Api *a);
    bool run_command(Device*, std::string);
    std::string select_command();
    int select_device();
    void print_commands();


};

std::string code_to_str(uint8_t c);
int send_ack(Device * d, int16_t seq);
int new_device(unsigned short id, struct sockaddr_in * net_info, Device ** device);

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
    bool save_measurement(int device_id, double temperature, double target_temperature, bool is_active, const std::string& mode);
    // Vrátí historii měření pro zařízení v časovém intervalu (od, do)
    std::vector<std::tuple<std::string, double, double, bool, std::string>> get_measurements(int device_id, const std::string& from, const std::string& to);
    // Vrátí seznam zařízení (id, name)
    std::vector<std::pair<int, std::string>> get_devices();
private:
    std::string conninfo_;
    PGconn* conn_;
};

