#include "ThemeCache.h"

namespace theme {
void Cache::Publish(Snapshot value) noexcept {
    ++generation_;
    const auto packed = (static_cast<std::uint64_t>(generation_ & 0x3fffffff) << 34) |
        (static_cast<std::uint64_t>(value.error) << 2) | static_cast<unsigned>(value.current);
    packed_.store(packed, std::memory_order_release);
}
Snapshot Cache::Get() const noexcept {
    const auto value = packed_.load(std::memory_order_acquire);
    return {static_cast<Mode>(value & 3), static_cast<DWORD>((value >> 2) & 0xffffffff), static_cast<std::uint32_t>(value >> 34)};
}
void Cache::Read() noexcept { ++reads_; Publish(ReadFromKey(key_)); }
DWORD Cache::Arm() noexcept {
    return RegNotifyChangeKeyValue(key_, FALSE, REG_NOTIFY_CHANGE_LAST_SET | REG_NOTIFY_THREAD_AGNOSTIC, event_, TRUE);
}
DWORD Cache::Start(HKEY root, const wchar_t* path) noexcept {
    std::lock_guard lock(lifecycle_);
    if (healthy_.load()) return ERROR_SUCCESS;
    StopLocked();
    DWORD error = RegOpenKeyExW(root, path, 0, KEY_QUERY_VALUE | KEY_NOTIFY, &key_);
    if (error) { Publish({Mode::Unknown, error}); return error; }
    event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!event_) error = GetLastError();
    if (!error) { wait_ = CreateThreadpoolWait(Changed, this, nullptr); if (!wait_) error = GetLastError(); }
    if (!error) error = Arm();
    if (error) { StopLocked(); Publish({Mode::Unknown, error}); return error; }
    // Subscribe BEFORE reading: a concurrent change is either included in this
    // read or leaves the event signaled for the wait callback (or both).
    Read();
    stopping_.store(false, std::memory_order_release);
    healthy_.store(true, std::memory_order_release);
    SetThreadpoolWait(wait_, event_, nullptr);
    return ERROR_SUCCESS;
}
void CALLBACK Cache::Changed(PTP_CALLBACK_INSTANCE, void* context, PTP_WAIT, TP_WAIT_RESULT) noexcept {
    static_cast<Cache*>(context)->OnChanged();
}
void Cache::OnChanged() noexcept {
    std::lock_guard lock(callback_);
    if (stopping_.load(std::memory_order_acquire)) return;
    ++notifications_;
    ResetEvent(event_);
    const auto error = Arm();
    if (error) { healthy_.store(false); Publish({Mode::Unknown, error}); return; }
    Read();
    if (!stopping_.load(std::memory_order_acquire)) SetThreadpoolWait(wait_, event_, nullptr);
}
void Cache::StopLocked() noexcept {
    stopping_.store(true, std::memory_order_release);
    healthy_.store(false, std::memory_order_release);
    if (wait_) {
        // Serialize disarming with the callback's final rearm; never close a
        // handle while a queued/running callback may still touch it.
        { std::lock_guard lock(callback_); SetThreadpoolWait(wait_, nullptr, nullptr); }
        WaitForThreadpoolWaitCallbacks(wait_, TRUE);
        CloseThreadpoolWait(wait_); wait_ = nullptr;
    }
    if (key_) { RegCloseKey(key_); key_ = nullptr; }
    if (event_) { CloseHandle(event_); event_ = nullptr; }
}
void Cache::Stop() noexcept { std::lock_guard lock(lifecycle_); StopLocked(); }
}
