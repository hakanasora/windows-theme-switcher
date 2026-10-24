#include "ThemeCache.h"
#include <shobjidl.h>
#include <shlwapi.h>
#include <filesystem>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
HINSTANCE instance{};
std::atomic<long> objects{};
std::atomic<long> locks{};
std::mutex activation;
// Deliberately not a static Cache object: cleanup that waits for callbacks must
// happen in DllCanUnloadNow, never in a CRT destructor under the loader lock.
theme::Cache* cache{};

std::filesystem::path ModulePath() {
    std::wstring value(32768, L'\0');
    const auto count = GetModuleFileNameW(instance, value.data(), static_cast<DWORD>(value.size()));
    if (!count || count == value.size()) throw std::runtime_error("Cannot locate extension module.");
    value.resize(count);
    return value;
}
theme::Snapshot Capture() {
    std::lock_guard lock(activation);
    if (!cache) cache = new theme::Cache;
    if (!cache->Healthy()) cache->Start();
    return cache->Get();
}

class Command final : public IExplorerCommand {
    std::atomic<ULONG> references_{1};
    const theme::Snapshot snapshot_;
    const std::wstring executable_;
public:
    Command() : snapshot_(Capture()), executable_((ModulePath().parent_path().parent_path().parent_path() / L"ThemeSwitcher.exe").wstring()) { ++objects; }
    ~Command() { --objects; }
    IFACEMETHODIMP QueryInterface(REFIID id, void** result) override {
        if (!result) return E_POINTER;
        *result = nullptr;
        if (id == IID_IUnknown || id == __uuidof(IExplorerCommand)) *result = static_cast<IExplorerCommand*>(this);
        if (!*result) return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return ++references_; }
    IFACEMETHODIMP_(ULONG) Release() override { const auto count = --references_; if (!count) delete this; return count; }
    IFACEMETHODIMP GetTitle(IShellItemArray*, PWSTR* title) override { return SHStrDupW(snapshot_.Title(), title); }
    IFACEMETHODIMP GetIcon(IShellItemArray*, PWSTR* icon) override { return SHStrDupW(snapshot_.Icon(), icon); }
    IFACEMETHODIMP GetToolTip(IShellItemArray*, PWSTR* value) override {
        return SHStrDupW(snapshot_.Valid() ? L"切换 Windows 系统和应用的颜色模式" : L"暂时无法读取当前颜色模式", value);
    }
    IFACEMETHODIMP GetCanonicalName(GUID* value) override {
        if (!value) return E_POINTER;
        *value = theme::CommandClsid; return S_OK;
    }
    IFACEMETHODIMP GetState(IShellItemArray*, BOOL, EXPCMDSTATE* value) override {
        if (!value) return E_POINTER;
        *value = snapshot_.Valid() ? ECS_ENABLED : ECS_DISABLED; return S_OK;
    }
    IFACEMETHODIMP GetFlags(EXPCMDFLAGS* value) override {
        if (!value) return E_POINTER;
        *value = static_cast<EXPCMDFLAGS>(ECF_SEPARATORBEFORE | ECF_SEPARATORAFTER); return S_OK;
    }
    IFACEMETHODIMP EnumSubCommands(IEnumExplorerCommand** value) override {
        if (!value) return E_POINTER;
        *value = nullptr; return E_NOTIMPL;
    }
    IFACEMETHODIMP Invoke(IShellItemArray*, IBindCtx*) override {
        if (!snapshot_.Valid()) return HRESULT_FROM_WIN32(snapshot_.error);
        try {
            // Invoke the DISPLAYED target, rather than flipping a potentially
            // newer state while a menu is already open. No shell command parser.
            std::wstring commandLine = L"\"" + executable_ + L"\" " +
                (snapshot_.Target() == theme::Mode::Dark ? L"dark" : L"light");
            STARTUPINFOW startup{sizeof(startup)};
            PROCESS_INFORMATION process{};
            if (!CreateProcessW(executable_.c_str(), commandLine.data(), nullptr, nullptr, FALSE,
                CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) return HRESULT_FROM_WIN32(GetLastError());
            CloseHandle(process.hThread); CloseHandle(process.hProcess);
            return S_OK;
        } catch (...) { return E_OUTOFMEMORY; }
    }
};

class Factory final : public IClassFactory {
    std::atomic<ULONG> references_{1};
public:
    Factory() { ++objects; }
    ~Factory() { --objects; }
    IFACEMETHODIMP QueryInterface(REFIID id, void** result) override {
        if (!result) return E_POINTER;
        *result = nullptr;
        if (id == IID_IUnknown || id == IID_IClassFactory) *result = static_cast<IClassFactory*>(this);
        if (!*result) return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return ++references_; }
    IFACEMETHODIMP_(ULONG) Release() override { const auto count = --references_; if (!count) delete this; return count; }
    IFACEMETHODIMP CreateInstance(IUnknown* outer, REFIID id, void** result) override {
        if (!result) return E_POINTER;
        *result = nullptr;
        if (outer) return CLASS_E_NOAGGREGATION;
        try {
            auto command = new Command;
            const auto hr = command->QueryInterface(id, result);
            command->Release(); return hr;
        } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
          catch (...) { return E_FAIL; }
    }
    IFACEMETHODIMP LockServer(BOOL lock) override { if (lock) ++locks; else if (locks.load() > 0) --locks; return S_OK; }
};
}

extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID clsid, REFIID id, void** result) {
    if (!result) return E_POINTER;
    *result = nullptr;
    if (clsid != theme::CommandClsid) return CLASS_E_CLASSNOTAVAILABLE;
    std::lock_guard guard(activation);
    try {
        auto factory = new Factory;
        const auto hr = factory->QueryInterface(id, result);
        factory->Release(); return hr;
    } catch (...) { return E_OUTOFMEMORY; }
}
extern "C" HRESULT __stdcall DllCanUnloadNow() {
    std::lock_guard guard(activation);
    if (objects.load() || locks.load()) return S_FALSE;
    // No permanent module lock for the watcher. Cancel and drain before COM
    // unloads this DLL. A subsequent activation reconstructs the cache.
    if (cache) { cache->Stop(); delete cache; cache = nullptr; }
    return S_OK;
}
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, void*) {
    // Static CRT needs thread notifications; do not disable them in a /MT DLL.
    if (reason == DLL_PROCESS_ATTACH) instance = module;
    return TRUE;
}
