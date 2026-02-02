#include "stress.h"
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <filesystem>
#include <memory>
#include <array>
#include <cstdlib>
#include <vector>

namespace AsusStress {
    static pid_t g_cpu_pid = -1;
    static pid_t g_gpu_pid = -1;

    static bool command_exists(const std::string& cmd) {
        std::string check = "command -v " + cmd + " > /dev/null 2>&1";
        return std::system(check.c_str()) == 0;
    }

    static pid_t launch_app(const std::string& bin_name, std::vector<std::string> args = {}) {
        pid_t pid = fork();
        if (pid == 0) {
            setsid(); 
            
            std::vector<char*> c_args;
            c_args.push_back(const_cast<char*>(bin_name.c_str()));
            for (const auto& arg : args) {
                c_args.push_back(const_cast<char*>(arg.c_str()));
            }
            c_args.push_back(nullptr);
            
            execvp(bin_name.c_str(), c_args.data());
            exit(1); 
        }
        return pid;
    }

    static void kill_process(pid_t& pid) {
        if (pid > 0) {
            kill(-pid, SIGTERM); 
            int status;
            waitpid(pid, &status, WNOHANG);
            pid = -1;
        }
    }

    bool has_cpu_burn() {
        return command_exists("stress-ng");
    }

    bool start_cpu_stress() {
        if (g_cpu_pid > 0) return false;
        if (!has_cpu_burn()) return false;
        g_cpu_pid = launch_app("stress-ng", {"--cpu", "0", "--cpu-method", "matrixprod", "--timeout", "31536000"});
        return g_cpu_pid > 0;
    }

    void stop_cpu_stress() {
        kill_process(g_cpu_pid);
    }

    bool is_cpu_stress_running() {
        if (g_cpu_pid <= 0) return false;
        int result = kill(g_cpu_pid, 0); 
        if (result == 0) return true;
        
        if (errno == ESRCH) {
            g_cpu_pid = -1;
            return false;
        }
        return true; 
    }


    bool has_gpu_burn() {
        return command_exists("gpu_burn");
    }

    bool start_gpu_stress() {
         if (g_gpu_pid > 0) return false;
         if (!has_gpu_burn()) return false;
         g_gpu_pid = launch_app("gpu_burn", {"31536000"});
         return g_gpu_pid > 0;
    }

    void stop_gpu_stress() {
        kill_process(g_gpu_pid);
    }

    bool is_gpu_stress_running() {
        if (g_gpu_pid <= 0) return false;
        int result = kill(g_gpu_pid, 0); 
        if (result == 0) return true;
        if (errno == ESRCH) {
             g_gpu_pid = -1;
             return false;
        }
        return true;
    }
}
