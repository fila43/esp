#include "server.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <thread>
#include <sstream>
#include <algorithm>
#include <cctype>

UnixSocketServer::UnixSocketServer(Api* api, const std::string& socket_path)
    : api(api), socket_path(socket_path), server_fd(-1), running(false) {}

void UnixSocketServer::start() {
    running = true;
    std::thread([this]() {
        // Smaž starý socket, pokud existuje
        unlink(socket_path.c_str());
        server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (server_fd < 0) {
            std::cerr << "[UnixSocketServer] Cannot create socket!\n";
            return;
        }
        sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);
        if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cerr << "[UnixSocketServer] Bind failed!\n";
            close(server_fd);
            return;
        }
        listen(server_fd, 5);
        std::cout << "[UnixSocketServer] Listening on " << socket_path << std::endl;
        while (running) {
            int client_fd = accept(server_fd, nullptr, nullptr);
            if (client_fd < 0) continue;
            handle_client(client_fd);
            close(client_fd);
        }
        close(server_fd);
        unlink(socket_path.c_str());
    }).detach();
}

void UnixSocketServer::stop() {
    running = false;
    if (server_fd >= 0) close(server_fd);
    unlink(socket_path.c_str());
}

void UnixSocketServer::handle_client(int client_fd) {
    char buf[512];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    if (n <= 0) return;
    buf[n] = '\0';
    std::string cmd(buf);
    // Odstraň případné koncové znaky nového řádku
    cmd.erase(cmd.find_last_not_of("\r\n") + 1);
    auto [status, response] = api->handle_unix_command(cmd);
    std::string reply;
    switch (status) {
        case UnixSocketStatus::OK:
            reply = "OK: " + response + "\n";
            break;
        case UnixSocketStatus::INVALID_COMMAND:
            reply = "ERROR: Invalid command\n";
            break;
        case UnixSocketStatus::DEVICE_NOT_FOUND:
            reply = "ERROR: Device not found\n";
            break;
        case UnixSocketStatus::INVALID_ARGUMENT:
            reply = "ERROR: Invalid argument\n";
            break;
        case UnixSocketStatus::INTERNAL_ERROR:
            reply = "ERROR: Internal error\n";
            break;
        default:
            reply = "ERROR: Unknown error\n";
    }
    if (!response.empty() && status != UnixSocketStatus::OK) {
        reply += response + "\n";
    }
    write(client_fd, reply.c_str(), reply.size());
}

std::pair<UnixSocketStatus, std::string> Api::handle_unix_command(const std::string& cmd) {
    std::istringstream iss(cmd);
    std::string action;
    iss >> action;
    // převod na velká písmena pro robustnost
    std::transform(action.begin(), action.end(), action.begin(), ::toupper);
    if (action == "LIST") {
        std::ostringstream out;
        for (auto d : devs.devices) {
            out << "ID: " << (int)d->id << ", Name: " << d->name << ", IP: " << d->ip;
            if (auto esp_dev = dynamic_cast<ESP01_custom*>(d)) {
                out << ", State: " << code_to_str(esp_dev->dstate)
                    << ", CTemp: " << esp_dev->ctemp/100.0 << ", DTemp: " << esp_dev->dtemp/100.0;
            }
            out << "\n";
        }
        return {UnixSocketStatus::OK, out.str()};
    } else if (action == "ON" || action == "OFF" || action == "AUTO" || action == "STATE") {
        int id;
        if (!(iss >> id)) return {UnixSocketStatus::INVALID_ARGUMENT, "Missing or invalid device ID"};
        AbstractDevice* d = devs.get_device(id);
        if (!d) return {UnixSocketStatus::DEVICE_NOT_FOUND, ""};
        if (auto esp_dev = dynamic_cast<ESP01_custom*>(d)) {
            if (action == "ON") {
                esp_dev->set_on();
                return {UnixSocketStatus::OK, "Device ON"};
            } else if (action == "OFF") {
                esp_dev->set_off();
                return {UnixSocketStatus::OK, "Device OFF"};
            } else if (action == "STATE") {
                std::ostringstream out;
                out << "ID: " << (int)d->id << ", Name: " << d->name << ", IP: " << d->ip
                    << ", State: " << code_to_str(esp_dev->dstate)
                    << ", CTemp: " << esp_dev->ctemp/100.0 << ", DTemp: " << esp_dev->dtemp/100.0;
                return {UnixSocketStatus::OK, out.str()};
            } else if (action == "AUTO") {
                esp_dev->set_auto_mode();
                return {UnixSocketStatus::OK, "Device AUTO"};
            }
        } else {
            return {UnixSocketStatus::INVALID_ARGUMENT, "Device does not support ESP01 commands"};
        }
    } else if (action == "SET_TEMP") {
        int id;
        float temp;
        if (!(iss >> id >> temp)) return {UnixSocketStatus::INVALID_ARGUMENT, "Usage: SET_TEMP <id> <temp>"};
        AbstractDevice* d = devs.get_device(id);
        if (!d) return {UnixSocketStatus::DEVICE_NOT_FOUND, ""};
        if (auto esp_dev = dynamic_cast<ESP01_custom*>(d)) {
            esp_dev->set_temp(temp);
            return {UnixSocketStatus::OK, "Temperature set"};
        } else {
            return {UnixSocketStatus::INVALID_ARGUMENT, "Device does not support temperature setting"};
        }
    } else if (action == "HELP") {
        std::string help =
            "Supported commands:\n"
            "LIST\n"
            "ON <id>\n"
            "OFF <id>\n"
            "AUTO_TEMP <id>\n"
            "AUTO_TIMER <id>\n"
            "STATE <id>\n"
            "SET_TEMP <id> <temp>\n"
            "HELP\n";
        return {UnixSocketStatus::OK, help};
    } else if (action == "AUTO_TEMP") {
        int id;
        if (!(iss >> id)) return {UnixSocketStatus::INVALID_ARGUMENT, "Missing device ID"};
        AbstractDevice* d = devs.get_device(id);
        if (!d) return {UnixSocketStatus::DEVICE_NOT_FOUND, ""};
        if (auto esp_dev = dynamic_cast<ESP01_custom*>(d)) {
            esp_dev->set_auto_mode(AUTO_TEMP);
            return {UnixSocketStatus::OK, "Device AUTO_TEMP"};
        } else {
            return {UnixSocketStatus::INVALID_ARGUMENT, "Device does not support AUTO_TEMP"};
        }
    } else if (action == "AUTO_TIMER") {
        int id;
        if (!(iss >> id)) return {UnixSocketStatus::INVALID_ARGUMENT, "Missing device ID"};
        AbstractDevice* d = devs.get_device(id);
        if (!d) return {UnixSocketStatus::DEVICE_NOT_FOUND, ""};
        if (auto esp_dev = dynamic_cast<ESP01_custom*>(d)) {
            esp_dev->set_auto_mode(AUTO_TIMER);
            return {UnixSocketStatus::OK, "Device AUTO_TIMER"};
        } else {
            return {UnixSocketStatus::INVALID_ARGUMENT, "Device does not support AUTO_TIMER"};
        }
    }
    return {UnixSocketStatus::INVALID_COMMAND, "Unknown command"};
} 