#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <vector>

namespace anvil {

// A tiny cooperative cancellation token shared by the IPC server and LSP
// requests. Callbacks are invoked exactly once, either immediately when the
// token is already cancelled or from cancel().
class CancelToken {
public:
    void on_cancel(std::function<void()> callback);
    void cancel();
    [[nodiscard]] bool cancelled() const noexcept { return cancelled_.load(); }

private:
    std::atomic<bool> cancelled_{false};
    std::mutex mutex_;
    std::vector<std::function<void()>> callbacks_;
};

}  // namespace anvil
