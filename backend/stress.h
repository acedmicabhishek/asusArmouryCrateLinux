#pragma once

namespace AsusStress {
    bool start_cpu_stress();
    void stop_cpu_stress();
    bool is_cpu_stress_running();
    bool has_cpu_burn();

    bool start_gpu_stress();
    void stop_gpu_stress();
    bool is_gpu_stress_running();
    bool has_gpu_burn();
}
