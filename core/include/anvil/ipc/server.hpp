#pragma once

#include <filesystem>
#include <memory>

namespace anvil {

struct ServerOptions {
    std::filesystem::path config_dir;
    std::filesystem::path cache_dir;
    bool dev_mode = false;
};

class Server {
public:
    explicit Server(ServerOptions options);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    // Reads newline-delimited JSON-RPC from stdin and writes responses and
    // notifications to stdout. Returns 0 on a clean EOF/shutdown.
    int run();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace anvil
