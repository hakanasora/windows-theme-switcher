#include "ThemeCore.h"
#include <winternl.h>

namespace theme {
Snapshot ReadFromKey(HKEY key) noexcept {
    DWORD value{}, size = sizeof(value);
    DWORD error = RegGetValueW(key, nullptr, SystemValue, RRF_RT_REG_DWORD, nullptr, &value, &size);
    if (error != ERROR_SUCCESS) return {Mode::Unknown, error};
    if (value > 1) return {Mode::Unknown, ERROR_INVALID_DATA};
    return {value == 1 ? Mode::Light : Mode::Dark, ERROR_SUCCESS};
}
Snapshot ReadCurrent() noexcept {
    HKEY key{};
    DWORD error = RegOpenKeyExW(HKEY_CURRENT_USER, PersonalizeKey, 0, KEY_QUERY_VALUE, &key);
    if (error) return {Mode::Unknown, error};
    auto result = ReadFromKey(key);
    RegCloseKey(key);
    return result;
}
namespace {
struct Saved { DWORD value{}; bool exists{}; };
DWORD Save(HKEY key, const wchar_t* name, Saved& saved) noexcept {
    DWORD size = sizeof(DWORD);
    auto error = RegGetValueW(key, nullptr, name, RRF_RT_REG_DWORD, nullptr, &saved.value, &size);
    if (error == ERROR_FILE_NOT_FOUND) return ERROR_SUCCESS;
    if (!error) saved.exists = true;
    return error;
}
DWORD Write(HKEY key, const wchar_t* name, DWORD value, RegistryWriter writer) noexcept {
    return writer(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
}
}
ApplyResult ApplyToKey(HKEY key, Mode target, RegistryWriter writer) noexcept {
    if (target != Mode::Light && target != Mode::Dark) return {ERROR_INVALID_PARAMETER};
    Saved system{}, apps{};
    if (auto error = Save(key, SystemValue, system)) return {error};
    if (auto error = Save(key, AppsValue, apps)) return {error};
    const DWORD value = target == Mode::Light ? 1 : 0;
    if (auto error = Write(key, SystemValue, value, writer)) return {error};
    if (auto error = Write(key, AppsValue, value, writer)) {
        const DWORD rollback = system.exists ? Write(key, SystemValue, system.value, writer) : RegDeleteValueW(key, SystemValue);
        return {error, rollback};
    }
    return {};
}
ApplyResult ApplyCurrent(Mode target) noexcept {
    HKEY key{};
    const DWORD error = RegOpenKeyExW(HKEY_CURRENT_USER, PersonalizeKey, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &key);
    if (error) return {error};
    const auto result = ApplyToKey(key, target);
    RegCloseKey(key);
    return result;
}
DWORD NotifyShell() noexcept {
    DWORD_PTR result{};
    SetLastError(ERROR_SUCCESS);
    if (SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
        reinterpret_cast<LPARAM>(L"ImmersiveColorSet"), SMTO_ABORTIFHUNG, 5000, &result)) return ERROR_SUCCESS;
    const auto error = GetLastError();
    return error ? error : ERROR_TIMEOUT;
}
bool SupportsModernMenu() noexcept {
    using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    const auto fn = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
    RTL_OSVERSIONINFOW info{}; info.dwOSVersionInfoSize = sizeof(info);
    return fn && fn(&info) == 0 && info.dwBuildNumber >= 22621;
}
bool IsNativeX64() noexcept {
    SYSTEM_INFO info{}; GetNativeSystemInfo(&info);
    return info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64;
}
}
