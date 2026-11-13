#include "anvil/ipc/cancel.hpp"

#include <utility>

namespace anvil {

void CancelToken::on_cancel(std::function<void()> callback) {
    {
        std::lock_guard lock(mutex_);
        if (!cancelled_.load()) {
            callbacks_.push_back(std::move(callback));
            return;
        }
    }
    callback();
}

void CancelToken::cancel() {
    std::vector<std::function<void()>> callbacks;
    {
        std::lock_guard lock(mutex_);
        if (cancelled_.exchange(true)) return;
        callbacks.swap(callbacks_);
    }
    for (auto& callback : callbacks) {
        if (callback) callback();
    }
}

}  // namespace anvil
