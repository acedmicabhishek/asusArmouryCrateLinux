#pragma once

#include <string>
#include <optional>

class SysfsWriter {
public:
    static bool write(const std::string& path, const std::string& value);
    static std::optional<std::string> read(const std::string& path);
};
