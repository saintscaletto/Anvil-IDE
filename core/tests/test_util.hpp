#pragma once

#include <unistd.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

namespace anvil::test {

class TempDir {
public:
    TempDir() {
        std::string pattern = "/tmp/anvil-test-XXXXXX";
        const char* created = ::mkdtemp(pattern.data());
        if (created == nullptr) {
            throw std::runtime_error("mkdtemp failed");
        }
        path_ = created;
    }

    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

inline void write_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << content;
}

inline std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

// A small one-shot notification gate used by watcher/LSP integration tests.
class EventGate {
public:
    void set() {
        {
            std::lock_guard lock(mutex_);
            ready_ = true;
        }
        condition_.notify_all();
    }

    bool wait(std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
        std::unique_lock lock(mutex_);
        return condition_.wait_for(lock, timeout, [&] { return ready_; });
    }

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    bool ready_ = false;
};

}  // namespace anvil::test
