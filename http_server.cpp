#include "server.h"
#include "httplib.h" // stáhni z https://github.com/yhirose/cpp-httplib a přidej do projektu
#include <thread>
#include <sstream>
#include "json.hpp" // pro JSON parsing (přidej do projektu: https://github.com/nlohmann/json)

HttpServer::HttpServer(Api* api, int port) : api(api), port(port) {}

void HttpServer::start() {
    std::thread([this]() {
        httplib::Server svr;

        svr.Get("/devices", [this](const httplib::Request&, httplib::Response& res) {
            std::ostringstream out;
            out << "[";
            bool first = true;
            for (auto d : api->devs.devices) {
                if (!first) out << ",";
                out << "{\"id\":" << (int)d->id << ",\"name\":\"" << d->name << "\",\"state\":\"" << code_to_str(d->dstate) << "\",\"ctemp\":" << d->ctemp/100.0 << ",\"dtemp\":" << d->dtemp/100.0 << "}";
                first = false;
            }
            out << "]";
            res.set_content(out.str(), "application/json");
        });

        svr.Post(R"(/device/(\d+)/on)", [this](const httplib::Request& req, httplib::Response& res) {
            int id = std::stoi(req.matches[1]);
            Device* d = api->devs.get_device(id);
            if (d) {
                d->set_on();
                res.set_content("{\"result\":\"ok\"}", "application/json");
            } else {
                res.status = 404;
                res.set_content("{\"error\":\"not found\"}", "application/json");
            }
        });

        svr.Post(R"(/device/(\d+)/off)", [this](const httplib::Request& req, httplib::Response& res) {
            int id = std::stoi(req.matches[1]);
            Device* d = api->devs.get_device(id);
            if (d) {
                d->set_off();
                res.set_content("{\"result\":\"ok\"}", "application/json");
            } else {
                res.status = 404;
                res.set_content("{\"error\":\"not found\"}", "application/json");
            }
        });

        svr.Post(R"(/device/(\d+)/auto)", [this](const httplib::Request& req, httplib::Response& res) {
            int id = std::stoi(req.matches[1]);
            Device* d = api->devs.get_device(id);
            int mode_type = AUTO_TEMP;
            try {
                if (req.has_param("type")) {
                    std::string t = req.get_param_value("type");
                    if (t == "timer") mode_type = AUTO_TIMER;
                } else if (!req.body.empty()) {
                    auto j = nlohmann::json::parse(req.body);
                    if (j.contains("type") && j["type"] == "timer") mode_type = AUTO_TIMER;
                }
            } catch (...) {}
            if (d) {
                d->set_auto_mode(mode_type);
                res.set_content("{\"result\":\"ok\"}", "application/json");
            } else {
                res.status = 404;
                res.set_content("{\"error\":\"not found\"}", "application/json");
            }
        });

        svr.Get(R"(/device/(\d+)/state)", [this](const httplib::Request& req, httplib::Response& res) {
            int id = std::stoi(req.matches[1]);
            Device* d = api->devs.get_device(id);
            if (d) {
                std::ostringstream out;
                out << "{\"id\":" << (int)d->id << ",\"name\":\"" << d->name << "\",\"state\":\"" << code_to_str(d->dstate) << "\",\"ctemp\":" << d->ctemp/100.0 << ",\"dtemp\":" << d->dtemp/100.0 << "}";
                res.set_content(out.str(), "application/json");
            } else {
                res.status = 404;
                res.set_content("{\"error\":\"not found\"}", "application/json");
            }
        });

        svr.Post(R"(/device/(\d+)/set_temp)", [this](const httplib::Request& req, httplib::Response& res) {
            int id = std::stoi(req.matches[1]);
            Device* d = api->devs.get_device(id);
            if (d) {
                try {
                    float temp = std::stof(req.body);
                    d->set_temp(temp);
                    res.set_content("{\"result\":\"ok\"}", "application/json");
                } catch (...) {
                    res.status = 400;
                    res.set_content("{\"error\":\"invalid temp\"}", "application/json");
                }
            } else {
                res.status = 404;
                res.set_content("{\"error\":\"not found\"}", "application/json");
            }
        });

        svr.Post(R"(/device/(\d+)/schedule)", [this](const httplib::Request& req, httplib::Response& res) {
            int id = std::stoi(req.matches[1]);
            Device* d = api->devs.get_device(id);
            if (!d) {
                res.status = 404;
                res.set_content("{\"error\":\"not found\"}", "application/json");
                return;
            }
            try {
                auto j = nlohmann::json::parse(req.body);
                if (!j.contains("schedule") || !j["schedule"].is_array()) {
                    res.status = 400;
                    res.set_content("{\"error\":\"missing schedule\"}", "application/json");
                    return;
                }
                std::vector<std::pair<std::string, std::string>> schedule;
                for (const auto& item : j["schedule"]) {
                    if (!item.is_array() || item.size() != 2) continue;
                    schedule.emplace_back(item[0].get<std::string>(), item[1].get<std::string>());
                }
                Watcher::instance().add_api(api);
                Watcher::instance().set_schedule(id, schedule);
                res.set_content("{\"result\":\"ok\"}", "application/json");
            } catch (...) {
                res.status = 400;
                res.set_content("{\"error\":\"invalid json\"}", "application/json");
            }
        });

        svr.Get("/", [this](const httplib::Request&, httplib::Response& res) {
            std::ostringstream out;
            out << "<html><head><title>Device Overview</title><style>body{font-family:sans-serif;background:#23272e;color:#fff;}table{border-collapse:collapse;width:80%;margin:2rem auto;background:#262b33;border-radius:12px;box-shadow:0 2px 12px 0 rgba(0,0,0,0.25);}th,td{padding:0.7rem 1rem;text-align:center;}th{background:#ffb300;color:#23272e;}tr:nth-child(even){background:#23272e;}tr:nth-child(odd){background:#262b33;}tr:hover{background:#31363f;}caption{font-size:2rem;font-weight:700;margin-bottom:1rem;color:#ffb300;}h1{text-align:center;margin-top:2rem;}</style></head><body>";
            out << "<h1>Device Overview</h1>";
            out << "<table><caption>Seznam zařízení</caption><tr><th>ID</th><th>Název</th><th>Stav</th><th>Aktuální teplota</th><th>Cílová teplota</th></tr>";
            for (auto d : api->devs.devices) {
                out << "<tr>";
                out << "<td>" << (int)d->id << "</td>";
                out << "<td>" << d->name << "</td>";
                out << "<td>" << code_to_str(d->dstate) << "</td>";
                out << "<td>" << d->ctemp/100.0 << " °C</td>";
                out << "<td>" << d->dtemp/100.0 << " °C</td>";
                out << "</tr>";
            }
            out << "</table></body></html>";
            res.set_content(out.str(), "text/html; charset=utf-8");
        });

        // Povolit CORS pro vývoj s Reactem
        svr.set_pre_routing_handler([](const httplib::Request &req, httplib::Response &res) {
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            res.set_header("Access-Control-Allow-Headers", "Content-Type");
            if (req.method == "OPTIONS") {
                res.status = 204;
                return httplib::Server::HandlerResponse::Handled;
            }
            return httplib::Server::HandlerResponse::Unhandled;
        });

        std::cout << "[HttpServer] Listening on port " << port << std::endl;
        svr.listen("0.0.0.0", port);
    }).detach();
} 