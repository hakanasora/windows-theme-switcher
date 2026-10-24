#include "ThemeCore.h"
#include <shellapi.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int count{};
    auto args = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!args) return 2;
    if (count == 1) { LocalFree(args); return 0; }
    theme::Mode target = theme::Mode::Unknown;
    if (count == 2 && _wcsicmp(args[1], L"light") == 0) target = theme::Mode::Light;
    if (count == 2 && _wcsicmp(args[1], L"dark") == 0) target = theme::Mode::Dark;
    if (count == 2 && _wcsicmp(args[1], L"toggle") == 0) {
        const auto current = theme::ReadCurrent();
        if (!current.Valid()) { LocalFree(args); return 3; }
        target = current.Target();
    }
    LocalFree(args);
    if (target == theme::Mode::Unknown) return 2;
    const auto result = theme::ApplyCurrent(target);
    if (result.rollbackError) return 5;
    if (!result.Ok()) return 4;
    return theme::NotifyShell() == ERROR_SUCCESS ? 0 : 6;
}
