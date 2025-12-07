#include "collector.h"
#include "server.h"
#include <iostream>
#include <algorithm>
#include <exception>

Collector::Collector(Api* api_ref) : api(api_ref), running(false) {
    if (!api) {
        std::cerr << "Collector: Warning - null Api reference!" << std::endl;
    }
    std::cout << "Collector: Initialized with Api reference" << std::endl;
}

Collector::~Collector() {
    stop();  // Ensure thread is stopped
    std::cout << "Collector: Destroyed" << std::endl;
}

void Collector::start() {
    if (running) {
        std::cout << "Collector: Already running" << std::endl;
        return;
    }
    
    running = true;
    worker_thread = std::thread(&Collector::run, this);
    std::cout << "Collector: Started background thread" << std::endl;
}

void Collector::stop() {
    if (!running) {
        return;  // Not running
    }
    
    running = false;
    
    if (worker_thread.joinable()) {
        worker_thread.join();
    }
    
    std::cout << "Collector: Stopped background thread" << std::endl;
}

bool Collector::is_running() const {
    return running;
}

void Collector::run() {
    std::cout << "Collector: Thread started - calling get_state() on devices from Api" << std::endl;
    
    while (running) {
        if (api && !api->devs.devices.empty()) {
            std::cout << "Collector: Processing " << api->devs.devices.size() << " devices" << std::endl;
            
            // Process all devices from Api device list
            for (auto device : api->devs.devices) {
                if (device) {
                    try {
                        // Use UDP for PowerMeterDevice (fast, asynchronous)
                        // Use HTTP for ESP01_custom (full state data)
                        if (auto power_dev = dynamic_cast<PowerMeterDevice*>(device)) {
                            power_dev->request_power_udp();  // Fast UDP power request
                        } else {
                            device->request_state();         // HTTP state request for other devices
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "Collector: Error calling request method on device 0x" 
                                  << std::hex << device->id << std::dec << ": " << e.what() << std::endl;
                    }
                }
            }
        } else {
            std::cout << "Collector: No devices to process" << std::endl;
        }
        
        // Wait before next iteration
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    
    std::cout << "Collector: Thread finished" << std::endl;
}