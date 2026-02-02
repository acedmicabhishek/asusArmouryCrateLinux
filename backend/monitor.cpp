#include "monitor.h"
#include <fstream>
#include <array>
#include <cstdio>
#include <memory>
#include <iostream>
#include <algorithm>
#include <vector>
#include <chrono>

namespace AsusMonitor {

    static int cached_ram_speed = 0;
    static long long last_energy_uj = 0;
    static std::chrono::steady_clock::time_point last_energy_time;

    struct PcloseDeleter {
        void operator()(FILE* f) const { if (f) pclose(f); }
    };
    static std::string run_cmd(const char* cmd) {
        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, PcloseDeleter> pipe(popen(cmd, "r"));
        if (!pipe) {
            return "";
        }
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        return result;
    }

    void init() {
        std::string output = run_cmd("pkexec dmidecode -t memory 2>/dev/null | grep 'Speed:' | head -n 1");
        if (output.empty()) {
             output = run_cmd("dmidecode -t memory 2>/dev/null | grep 'Speed:' | head -n 1");
        }
        
        if (!output.empty()) {
            size_t colon = output.find(":");
            if (colon != std::string::npos) {
                std::string val = output.substr(colon + 1);
                std::string num_str;
                for (char c : val) {
                    if (isdigit(c)) num_str += c;
                    else if (!num_str.empty()) break;
                }
                if (!num_str.empty()) {
                    cached_ram_speed = std::stoi(num_str);
                }
            }
        }
    }

    CpuMetrics get_cpu_metrics() {
        CpuMetrics m = {0, cached_ram_speed, 0};
        
        std::ifstream f("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
        if (f.is_open()) {
            int khz = 0;
            f >> khz;
            m.freq_mhz = khz / 1000;
        }

        std::ifstream f_energy("/sys/class/powercap/intel-rapl:0/energy_uj");
        if (f_energy.is_open()) {
            long long uj = 0;
            f_energy >> uj;
            
            auto now = std::chrono::steady_clock::now();
            if (last_energy_uj > 0) {
                 auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_energy_time).count();
                 if (duration_ms > 0) {
                     long long delta_uj = uj - last_energy_uj;
                     if (delta_uj < 0) delta_uj += 262143328850LL;

                     if (delta_uj > 0) {
                        m.power_w = (int)((delta_uj / 1000) / duration_ms);
                     }
                 }
            }
            last_energy_uj = uj;
            last_energy_time = now;
        }

        return m;
    }

    GpuMetrics get_gpu_metrics() {
        GpuMetrics m = {0, 0, 0, 0, 0, 0};

        std::string out = run_cmd("nvidia-smi --query-gpu=clocks.gr,clocks.mem,memory.used,memory.total,power.draw,power.limit,enforced.power.limit --format=csv,noheader,nounits 2>/dev/null");
        
        if (!out.empty()) {
            std::vector<std::string> parts;
            std::string current;
            for (char c : out) {
                if (c == ',') {
                    parts.push_back(current);
                    current.clear();
                } else if (c != ' ' && c != '\n' && c != '\r') {
                    current += c;
                }
            }
            parts.push_back(current);

            if (parts.size() >= 7) {
                try {
                    m.core_clock_mhz = std::stoi(parts[0]);
                    m.memory_clock_mhz = std::stoi(parts[1]);
                    m.vram_used_mb = std::stoi(parts[2]);
                    m.vram_total_mb = std::stoi(parts[3]);
                    
                    try { m.power_w = (int)std::stof(parts[4]); } catch(...) {}
                    
                    int p_limit = 0;
                    try { p_limit = (int)std::stof(parts[5]); } catch(...) {}
                    
                    if (p_limit == 0) {
                        try { p_limit = (int)std::stof(parts[6]); } catch(...) {}
                    }
                    m.power_limit_w = p_limit;
                    
                } catch(...) {}
            }
        }

        return m;
    }

    std::string format_freq(int mhz) {
        if (mhz >= 1000) {
            float ghz = mhz / 1000.0f;
            char buf[32];
            snprintf(buf, sizeof(buf), "%.1f GHz", ghz);
            return std::string(buf);
        }
        return std::to_string(mhz) + " MHz";
    }

    std::string format_speed(int speed) {
        return std::to_string(speed) + " MT/s";
    }

}
