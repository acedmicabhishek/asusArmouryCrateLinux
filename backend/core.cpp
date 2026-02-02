#include "core.h"
#include "sysfs_writer.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <iostream>
#include <map>

namespace AsusCore {
    namespace fs = std::filesystem;

    struct SensorPaths {
        std::string cpu_fan;
        std::string gpu_fan;
        std::string cpu_temp;
        std::string gpu_temp;
        bool valid = false;
    };

    static SensorPaths g_sensors;

    static int read_int(const std::string& path) {
        if (path.empty()) return 0;
        auto val = SysfsWriter::read(path);
        if (val) {
            try { return std::stoi(*val); } catch (...) {}
        }
        return 0;
    }

    static void scan_sensors() {
        if (g_sensors.valid) return;

        g_sensors.cpu_fan = "";
        g_sensors.gpu_fan = "";
        g_sensors.cpu_temp = "";
        g_sensors.gpu_temp = "";

        if (fs::exists("/sys/class/hwmon")) {
            for (const auto& entry : fs::directory_iterator("/sys/class/hwmon")) {
                std::string path = entry.path().string();
                std::string name_path = path + "/name";
                
                auto name_opt = SysfsWriter::read(name_path);
                if (name_opt) {
                    std::string name = *name_opt;
                    if (name == "asus") {
                        if (fs::exists(path + "/fan1_input")) g_sensors.cpu_fan = path + "/fan1_input";
                        if (fs::exists(path + "/fan2_input")) g_sensors.gpu_fan = path + "/fan2_input";
                    }
                    if (name == "k10temp" || name == "coretemp") {
                         if (fs::exists(path + "/temp1_input")) g_sensors.cpu_temp = path + "/temp1_input";
                    }
                    if (name == "amdgpu") {
                        if (fs::exists(path + "/temp1_input")) {
                             g_sensors.gpu_temp = path + "/temp1_input";
                        }
                    }
                    if (name == "nvidia") {
                         if (fs::exists(path + "/temp1_input")) g_sensors.gpu_temp = path + "/temp1_input";
                    }
                }
            }
        }

        g_sensors.valid = true;
        
        std::cout << "[AsusCore] Scanned Sensors:" << std::endl;
        std::cout << "  CPU Fan: " << g_sensors.cpu_fan << std::endl;
        std::cout << "  GPU Fan: " << g_sensors.gpu_fan << std::endl;
        std::cout << "  CPU Temp: " << g_sensors.cpu_temp << std::endl;
        std::cout << "  GPU Temp: " << g_sensors.gpu_temp << std::endl;
    }

    FanMetrics get_metrics() {
        scan_sensors();
        FanMetrics m;
        m.cpu_rpm = read_int(g_sensors.cpu_fan);
        m.gpu_rpm = read_int(g_sensors.gpu_fan);
        
        m.cpu_temp = read_int(g_sensors.cpu_temp) / 1000;
        m.gpu_temp = read_int(g_sensors.gpu_temp) / 1000;
        
        return m;
    }

    bool is_supported() {
        scan_sensors();
        return !g_sensors.cpu_fan.empty(); 
    }

    // Hardware Info
    std::string get_cpu_name() {
        std::ifstream f("/proc/cpuinfo");
        std::string line;
        while (std::getline(f, line)) {
            if (line.find("model name") != std::string::npos) {
                auto pos = line.find(":");
                if (pos != std::string::npos) {
                    std::string name = line.substr(pos + 1);
                    size_t first = name.find_first_not_of(" \t");
                    if (first != std::string::npos) name = name.substr(first);
                    return name;
                }
            }
        }
        return "Unknown CPU";
    }

    std::string get_gpu_name() {
        // Simple heuristic: 
        // 1. Run lspci
        // 2. Look for "NVIDIA" -> Return description
        // 3. Else look for "Radeon" -> ?
        
        std::array<char, 256> buffer;
        std::string result;
        struct PcloseDeleter {
            void operator()(FILE* f) const { if (f) pclose(f); }
        };
        std::unique_ptr<FILE, PcloseDeleter> pipe(popen("lspci | grep -E \"VGA|3D\"", "r"));
        if (!pipe) return "GPU Unknown";
        
        std::string nvidia_card;
        
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            std::string line = buffer.data();
            if (line.find("NVIDIA") != std::string::npos) {
 auto start = line.find("[");
                auto end = line.find("]");
                if (start != std::string::npos && end != std::string::npos) {
                    nvidia_card = line.substr(start + 1, end - start - 1);
                    break;
                }
            }
        }
        
        if (!nvidia_card.empty()) return nvidia_card;

        return "dGPU Unavailable";
    }
}
