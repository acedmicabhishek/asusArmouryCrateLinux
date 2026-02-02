#pragma once

#include <string>

namespace AsusMonitor {

    struct CpuMetrics {
        int freq_mhz;
        int ram_mt_s;
        int power_w;
    };

    struct GpuMetrics {
        int core_clock_mhz;
        int memory_clock_mhz; 
        int vram_used_mb;
        int vram_total_mb;
        int power_w;
        int power_limit_w;
    };

    void init();
    CpuMetrics get_cpu_metrics();
    GpuMetrics get_gpu_metrics();

    std::string format_freq(int mhz);
    std::string format_speed(int speed);
}
