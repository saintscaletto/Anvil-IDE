#include "anvil/util/thread_pool.hpp"

#include <algorithm>
#include <utility>

namespace anvil {

ThreadPool::ThreadPool(std::size_t worker_count, std::size_t max_queue)
    : max_queue_(max_queue) {
    worker_count = std::max<std::size_t>(1, worker_count);
    workers_.reserve(worker_count);
    for (std::size_t i = 0; i < worker_count; ++i) {
        workers_.emplace_back([this](std::stop_token token) { worker_loop(token); });
    }
}

ThreadPool::~ThreadPool() { stop(); }

bool ThreadPool::enqueue(std::function<void()> task) {
    {
        std::lock_guard lock(mutex_);
        if (stopping_ || tasks_.size() >= max_queue_) return false;
        tasks_.push(std::move(task));
    }
    condition_.notify_one();
    return true;
}

void ThreadPool::stop() {
    {
        std::lock_guard lock(mutex_);
        if (stopping_) return;
        stopping_ = true;
    }
    for (auto& worker : workers_) worker.request_stop();
    condition_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) worker.join();
    }
    std::lock_guard lock(mutex_);
    std::queue<std::function<void()>> empty;
    tasks_.swap(empty);
}

void ThreadPool::worker_loop(const std::stop_token& stop_token) {
    while (!stop_token.stop_requested()) {
        std::function<void()> task;
        {
            std::unique_lock lock(mutex_);
            condition_.wait(lock, [&] {
                return stop_token.stop_requested() || stopping_ || !tasks_.empty();
            });
            if (stop_token.stop_requested() || stopping_) return;
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        try {
            task();
        } catch (...) {
            // Tasks are responsible for reporting their own failures to the
            // caller. A stray exception must never take down a worker.
        }
    }
}

}  // namespace anvil
