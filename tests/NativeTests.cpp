#include "ThemeCache.h"
#include <shobjidl.h>
#include <shellapi.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
struct TestKey {
    std::wstring path;
    HKEY key{};
    TestKey() {
        GUID id{}; CoCreateGuid(&id); wchar_t guid[64]{}; StringFromGUID2(id, guid, 64);
        path = L"Software\\ThemeSwitcher.NativeTests\\" + std::wstring(guid);
        Check(RegCreateKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &key, nullptr) == 0, "Create test key");
    }
    ~TestKey() { RegCloseKey(key); RegDeleteTreeW(HKEY_CURRENT_USER, path.c_str()); RegDeleteKeyW(HKEY_CURRENT_USER,L"Software\\ThemeSwitcher.NativeTests"); }
    void Set(const wchar_t* name, DWORD value) {
        Check(RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<BYTE*>(&value), sizeof(value)) == 0, "Write test key");
    }
    DWORD Value(const wchar_t* name) { DWORD value{}, size = sizeof(value); Check(RegGetValueW(key, nullptr, name, RRF_RT_REG_DWORD, nullptr, &value, &size) == 0, "Read test value"); return value; }
};
bool WaitFor(theme::Cache& cache, theme::Mode mode, DWORD timeout = 3000) {
    const auto end = GetTickCount64() + timeout;
    do { if (cache.Get().Valid() && cache.Get().current == mode) return true; Sleep(1); } while (GetTickCount64() < end);
    return false;
}
LSTATUS WINAPI FailApps(HKEY key, LPCWSTR name, DWORD reserved, DWORD type, const BYTE* value, DWORD bytes) {
    return wcscmp(name, theme::AppsValue) == 0 ? ERROR_ACCESS_DENIED : RegSetValueExW(key,name,reserved,type,value,bytes);
}
int faultWrites{};
LSTATUS WINAPI FailWriteAndRollback(HKEY key,LPCWSTR name,DWORD reserved,DWORD type,const BYTE* value,DWORD bytes) {
    if (++faultWrites >= 2) return ERROR_ACCESS_DENIED;
    return RegSetValueExW(key,name,reserved,type,value,bytes);
}
void CoreTests() {
    TestKey key;
    Check(!theme::Snapshot{static_cast<theme::Mode>(3),ERROR_SUCCESS}.Valid(),"Reject out-of-range snapshot mode");
    Check(theme::ApplyToKey(key.key,theme::Mode::Light,nullptr).error == ERROR_INVALID_PARAMETER,"Reject null writer without touching registry");
    Check(!theme::ReadFromKey(key.key).Valid(), "Missing state must be unknown");
    key.Set(theme::SystemValue, 1); key.Set(theme::AppsValue, 0);
    auto state = theme::ReadFromKey(key.key);
    Check(state.Valid() && state.current == theme::Mode::Light && state.Target() == theme::Mode::Dark, "Mixed mode follows system");
    Check(wcscmp(state.Title(), L"切换到深色模式") == 0 && wcscmp(state.Icon(), L"imageres.dll,96") == 0, "Dark title/icon pair");
    Check(theme::ApplyToKey(key.key, theme::Mode::Dark).Ok(), "Apply dark");
    Check(key.Value(theme::SystemValue) == 0 && key.Value(theme::AppsValue) == 0, "Dark sets both values");
    state = theme::ReadFromKey(key.key);
    Check(wcscmp(state.Title(), L"切换到浅色模式") == 0 && wcscmp(state.Icon(), L"shell32.dll,15") == 0, "Light title/icon pair");
    auto failed = theme::ApplyToKey(key.key, theme::Mode::Light, FailApps);
    Check(failed.error == ERROR_ACCESS_DENIED && failed.rollbackError == 0, "Partial write failure reported");
    Check(key.Value(theme::SystemValue) == 0 && key.Value(theme::AppsValue) == 0, "Partial write rolls back system value");
    faultWrites=0;
    failed=theme::ApplyToKey(key.key,theme::Mode::Light,FailWriteAndRollback);
    Check(failed.error == ERROR_ACCESS_DENIED && failed.rollbackError == ERROR_ACCESS_DENIED,"Rollback failure must not be reported as success");
    RegDeleteValueW(key.key,theme::SystemValue);
    failed=theme::ApplyToKey(key.key,theme::Mode::Light,FailApps);
    Check(failed.error == ERROR_ACCESS_DENIED && failed.rollbackError == 0 && !theme::ReadFromKey(key.key).Valid(),"Rollback preserves an originally absent value");
    key.Set(theme::SystemValue, 7); Check(!theme::ReadFromKey(key.key).Valid(), "Invalid state must not be guessed");
    Check(theme::ApplyToKey(key.key, theme::Mode::Unknown).error == ERROR_INVALID_PARAMETER, "Reject invalid target");
    std::cout << "PASS core: modes, mixed state, icon/title, transactional write failure\n";
}
void CacheTests() {
    TestKey key; key.Set(theme::SystemValue, 1); key.Set(theme::AppsValue, 1);
    theme::Cache cache;
    Check(cache.Start(HKEY_CURRENT_USER, key.path.c_str()) == 0, "Start watcher");
    Check(WaitFor(cache, theme::Mode::Light), "Initial snapshot");
    const auto reads = cache.Reads();
    for (int i=0;i<100000;i++) Check(cache.Get().current == theme::Mode::Light, "Cached snapshot");
    Check(cache.Reads() == reads, "Cache hits must never query the registry");
    const auto notifications = cache.Notifications();
    Sleep(100); Check(cache.Reads() == reads && cache.Notifications() == notifications, "Idle watcher must not poll");
    for (int i=0;i<20;i++) { const auto light = (i%2) != 0; key.Set(theme::SystemValue, light ? 1 : 0); Check(WaitFor(cache,light?theme::Mode::Light:theme::Mode::Dark), "External change notification"); }
    DWORD baseline{}; GetProcessHandleCount(GetCurrentProcess(), &baseline);
    cache.Stop();
    Check(!cache.Healthy() && !cache.Get().Valid(),"Stop invalidates cached theme");
    cache.Stop();
    Check(!cache.Get().Valid(),"Repeated stop is safe");
    for (int i=0;i<100;i++) {
        Check(cache.Start(HKEY_CURRENT_USER,key.path.c_str()) == 0, "Restart watcher");
        key.Set(theme::SystemValue, static_cast<DWORD>(i%2));
        cache.Stop(); // Race a pending/running notification against cleanup.
    }
    DWORD after{}; GetProcessHandleCount(GetCurrentProcess(), &after);
    Check(after <= baseline + 4, "Watcher restart must not leak handles");
    Check(cache.Start(HKEY_CURRENT_USER,L"Software\\ThemeSwitcher.NativeTests\\Missing") != 0, "Missing key start fails safely");
    Check(!cache.Get().Valid(), "Failed start invalidates snapshot");
    Check(cache.Start(HKEY_CURRENT_USER,key.path.c_str()) == 0, "Recover from missing key");
    key.Set(theme::SystemValue,9);
    const auto invalidEnd=GetTickCount64()+3000;
    while (cache.Get().Valid() && GetTickCount64()<invalidEnd) Sleep(1);
    Check(!cache.Get().Valid(),"External invalid state disables the command");
    key.Set(theme::SystemValue,1); Check(WaitFor(cache,theme::Mode::Light),"Recover after invalid state");
    std::cout << "PASS cache: no registry reads on hits, no polling, external changes, restart/race cleanup\n";
}
DWORD Run(const wchar_t* executable, const wchar_t* argument) {
    std::wstring command = L"\"" + std::wstring(executable) + L"\" " + argument;
    STARTUPINFOW startup{sizeof(startup)}; PROCESS_INFORMATION process{};
    Check(CreateProcessW(executable,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process), "Start no-op CLI");
    Check(WaitForSingleObject(process.hProcess,5000) == WAIT_OBJECT_0,"CLI exits promptly");
    DWORD result{}; GetExitCodeProcess(process.hProcess,&result); CloseHandle(process.hThread); CloseHandle(process.hProcess); return result;
}
void ShellTests(const wchar_t* path) {
    const auto module = LoadLibraryW(path); Check(module != nullptr,"Load shell DLL without registration");
    using GetFactory = HRESULT(__stdcall*)(REFCLSID,REFIID,void**);
    using CanUnload = HRESULT(__stdcall*)();
    const auto getFactory = reinterpret_cast<GetFactory>(GetProcAddress(module,"DllGetClassObject"));
    const auto canUnload = reinterpret_cast<CanUnload>(GetProcAddress(module,"DllCanUnloadNow"));
    Check(getFactory && canUnload,"COM exports");
    IClassFactory* factory{};
    Check(SUCCEEDED(getFactory(theme::CommandClsid,IID_IClassFactory,reinterpret_cast<void**>(&factory))),"Create factory");
    IExplorerCommand* command{};
    Check(SUCCEEDED(factory->CreateInstance(nullptr,__uuidof(IExplorerCommand),reinterpret_cast<void**>(&command))),"Create command");
    Check(canUnload() == S_FALSE,"Never unload live objects");
    Check(command->GetTitle(nullptr,nullptr) == E_POINTER && command->GetIcon(nullptr,nullptr) == E_POINTER &&
        command->GetToolTip(nullptr,nullptr) == E_POINTER,"COM null outputs are rejected");
    factory->LockServer(TRUE);
    std::vector<std::thread> unlockers;
    for (int i=0;i<8;++i) unlockers.emplace_back([factory]{ for(int n=0;n<1000;++n) factory->LockServer(FALSE); });
    for(auto& thread:unlockers)thread.join();
    IEnumExplorerCommand* children{};
    Check(command->EnumSubCommands(&children) == E_NOTIMPL && children == nullptr,"No submenu");
    EXPCMDFLAGS flags{}; command->GetFlags(&flags);
    Check((flags & ECF_SEPARATORBEFORE) && (flags & ECF_SEPARATORAFTER),"Legacy grouping flags");
    std::vector<double> times; times.reserve(10000);
    for (int i=0;i<10000;i++) {
        const auto start = std::chrono::steady_clock::now();
        PWSTR title{}, icon{}; EXPCMDSTATE state{};
        Check(SUCCEEDED(command->GetIcon(nullptr,&icon)) && SUCCEEDED(command->GetTitle(nullptr,&title)) && SUCCEEDED(command->GetState(nullptr,FALSE,&state)),"Menu callbacks");
        if (state == ECS_ENABLED) {
            const auto dark = wcscmp(title,L"切换到深色模式") == 0;
            Check(wcscmp(icon,dark?L"imageres.dll,96":L"shell32.dll,15") == 0,"Consistent command snapshot");
        }
        CoTaskMemFree(title); CoTaskMemFree(icon);
        times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
    }
    std::sort(times.begin(),times.end());
    std::cout << "Menu callback group P95: " << times[9500] << " ms; P99: " << times[9900] << " ms\n";
    Check(times[9500] <= 1.0,"Cached menu callback P95 exceeds 1 ms");
    command->Release(); factory->Release(); Check(canUnload() == S_OK,"Watcher does not pin the DLL");
    Check(FreeLibrary(module),"Unload shell DLL");
    for (int i=0;i<30;i++) {
        const auto again = LoadLibraryW(path); Check(again != nullptr,"Reload DLL");
        auto create = reinterpret_cast<GetFactory>(GetProcAddress(again,"DllGetClassObject"));
        auto unload = reinterpret_cast<CanUnload>(GetProcAddress(again,"DllCanUnloadNow"));
        IClassFactory* f{}; IExplorerCommand* c{};
        Check(SUCCEEDED(create(theme::CommandClsid,IID_IClassFactory,reinterpret_cast<void**>(&f))),"Reload factory");
        Check(SUCCEEDED(f->CreateInstance(nullptr,__uuidof(IExplorerCommand),reinterpret_cast<void**>(&c))),"Reload command");
        c->Release(); f->Release(); Check(unload() == S_OK,"Reload cleanup"); FreeLibrary(again);
    }
    std::cout << "PASS shell: COM lifecycle, consistent snapshot, no submenu, callback budget\n";
}
}
int wmain(int argc,wchar_t** argv) {
    try {
        Check(argc == 3,"Pass shell DLL and CLI paths");
        CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        CoreTests(); CacheTests();
        Check(Run(argv[2],L"") == 0,"No-argument CLI is a no-op");
        Check(Run(argv[2],L"invalid") == 2,"Invalid CLI is rejected without changing settings");
        Check(Run(argv[2],L"light extra") == 2,"Extra arguments must not switch the real theme");
        ShellTests(argv[1]);
        CoUninitialize(); std::cout << "All native checks passed. Real Windows theme values were not modified.\n"; return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
