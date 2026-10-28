#pragma once
#include <windows.h>
#include <cstdint>

namespace theme {
inline constexpr wchar_t PersonalizeKey[] = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
inline constexpr wchar_t SystemValue[] = L"SystemUsesLightTheme";
inline constexpr wchar_t AppsValue[] = L"AppsUseLightTheme";
inline constexpr GUID CommandClsid{0x09a260c0, 0xdba6, 0x4b6d, {0x8d, 0x2d, 0xcf, 0x3a, 0x8c, 0x32, 0xba, 0x76}};
inline constexpr wchar_t CommandClsidString[] = L"{09A260C0-DBA6-4B6D-8D2D-CF3A8C32BA76}";
enum class Mode : std::uint8_t { Unknown = 0, Light = 1, Dark = 2 };
struct Snapshot {
    Mode current{Mode::Unknown};
    DWORD error{ERROR_NOT_READY};
    std::uint32_t generation{};
    bool Valid() const noexcept { return error == ERROR_SUCCESS && (current == Mode::Light || current == Mode::Dark); }
    Mode Target() const noexcept { return current == Mode::Light ? Mode::Dark : Mode::Light; }
    const wchar_t* Title() const noexcept {
        return !Valid() ? L"切换颜色模式（暂不可用）" :
            Target() == Mode::Dark ? L"切换到深色模式" : L"切换到浅色模式";
    }
    const wchar_t* Icon() const noexcept {
        return Valid() && Target() == Mode::Light ? L"shell32.dll,15" : L"imageres.dll,96";
    }
};
using RegistryWriter = decltype(&RegSetValueExW);
struct ApplyResult { DWORD error{}; DWORD rollbackError{}; bool Ok() const noexcept { return error == 0; } };
Snapshot ReadFromKey(HKEY key) noexcept;
Snapshot ReadCurrent() noexcept;
ApplyResult ApplyToKey(HKEY key, Mode target, RegistryWriter writer = &RegSetValueExW) noexcept;
ApplyResult ApplyCurrent(Mode target) noexcept;
DWORD NotifyShell() noexcept;
bool SupportsModernMenu() noexcept;
bool IsNativeX64() noexcept;
}
