#pragma once

#include <chrono>
#include <filesystem>
#include <string>

namespace visual_editor {

class FileWatcher {
public:
    explicit FileWatcher(std::string path) : path_(std::move(path)) {}

    bool poll() {
        namespace fs = std::filesystem;
        std::error_code error;
        const auto current = fs::last_write_time(path_, error);
        if (error) {
            return false;
        }
        if (!initialized_) {
            initialized_ = true;
            lastWrite_ = current;
            return false;
        }
        if (current != lastWrite_) {
            lastWrite_ = current;
            return true;
        }
        return false;
    }

private:
    std::string path_;
    bool initialized_ = false;
    std::filesystem::file_time_type lastWrite_{};
};

} // namespace visual_editor
