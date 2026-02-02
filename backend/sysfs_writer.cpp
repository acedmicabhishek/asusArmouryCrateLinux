#include "sysfs_writer.h"
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <format>

std::optional<std::string> SysfsWriter::read(const std::string& path) {
    std::ifstream file(path);
    if (file.is_open()) {
        std::string value;
        if (std::getline(file, value)) {
            return value;
        }
    }
    return std::nullopt;
}

bool SysfsWriter::write(const std::string& path, const std::string& value) {
    {
        std::ofstream file(path);
        if (file.is_open()) {
            file << value;
            if (file.good()) {
                return true;
            }
        }
    }
    std::string cmd = std::format("pkexec sh -c 'echo \"{}\" | tee {} > /dev/null'", value, path);
    int result = std::system(cmd.c_str());
    
    return result == 0;
}
