#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <stop_token>
#include <thread>
#include <vector>

namespace anvil {

// A bounded worker pool used so a slow filesystem operation cannot stall the
// JSON-RPC input loop. Blocking work is explicitly enqueued.
class ThreadPool {
public:
    explicit ThreadPool(std::size_t worker_count, std::size_t max_queue = 1024);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Returns false when the queue is full or the pool is stopping.
    bool enqueue(std::function<void()> task);
    void stop();

private:
    void worker_loop(const std::stop_token& stop_token);

    std::mutex mutex_;
    std::condition_variable_any condition_;
    std::queue<std::function<void()>> tasks_;
    std::vector<std::jthread> workers_;
    std::size_t max_queue_;
    bool stopping_ = false;
};

}  // namespace anvil
