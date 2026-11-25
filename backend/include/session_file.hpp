#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace freenamp::backend {

// Main-thread persistence: replace only after the complete new document is written.
inline bool write_session_file(const std::filesystem::path& target, const std::string& contents) {
    auto temporary = target;
    temporary += ".tmp";
    try {
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output) return false;
            output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
            output.flush();
            if (!output) return false;
            output.close();
            if (!output) return false;
        }
#ifdef _WIN32
        if (MoveFileExW(temporary.c_str(), target.c_str(),
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
#else
        std::error_code error;
        std::filesystem::rename(temporary, target, error);
        if (!error) return true;
#endif
    } catch (...) {}
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    return false;
}

} // namespace freenamp::backend
