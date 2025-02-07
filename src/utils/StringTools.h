#pragma once

#include <memory>
#include <string>
#include <vector>

template<typename... Args>
std::string string_format(const std::string &format, Args... args) {
    int size_s = std::snprintf(nullptr, 0, format.c_str(), args...) + 1; // Extra space for '\0'
    auto size  = static_cast<size_t>(size_s);
    auto buf   = std::make_unique<char[]>(size);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

static inline bool starts_with_case_insensitive(const std::string_view str, const std::string_view prefix) {
    if (str.size() < prefix.size())
        return false;

    return std::equal(prefix.begin(), prefix.end(), str.begin(),
                      [](const char a, const char b) {
                          return std::tolower(a) == std::tolower(b);
                      });
}
