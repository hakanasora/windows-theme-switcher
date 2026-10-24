#pragma once
#include "ThemeCore.h"
#include <atomic>
#include <mutex>

namespace theme {
// One cache per loaded module. It owns no polling thread and no independent process.
class Cache final {
public:
    Cache() = default;
    Cache(const Cache&) = delete;
    Cache& operator=(const Cache&) = delete;
    ~Cache() { Stop(); }
    DWORD Start(HKEY root = HKEY_CURRENT_USER, const wchar_t* path = PersonalizeKey) noexcept;
    void Stop() noexcept;
    Snapshot Get() const noexcept;
    bool Healthy() const noexcept { return healthy_.load(std::memory_order_acquire); }
    std::uint64_t Reads() const noexcept { return reads_.load(); }
    std::uint64_t Notifications() const noexcept { return notifications_.load(); }
private:
    static void CALLBACK Changed(PTP_CALLBACK_INSTANCE, void*, PTP_WAIT, TP_WAIT_RESULT) noexcept;
    void OnChanged() noexcept;
    DWORD Arm() noexcept;
    void Publish(Snapshot value) noexcept;
    void Read() noexcept;
    void StopLocked() noexcept;
    HKEY key_{};
    HANDLE event_{};
    PTP_WAIT wait_{};
    std::mutex lifecycle_;
    std::mutex callback_;
    std::atomic<bool> stopping_{true};
    std::atomic<bool> healthy_{false};
    std::atomic<std::uint64_t> packed_{static_cast<std::uint64_t>(ERROR_NOT_READY) << 2};
    std::atomic<std::uint64_t> reads_{};
    std::atomic<std::uint64_t> notifications_{};
    std::uint32_t generation_{};
};
}
