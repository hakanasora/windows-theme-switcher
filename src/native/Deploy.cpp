#include "ThemeCore.h"
#include "Version.h"
#include "PackageIdentity.h"
#include <appxpackaging.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wintrust.h>
#include <softpub.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Management.Deployment.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace winrt;
using namespace Windows::Management::Deployment;
namespace {
constexpr wchar_t LegacyKey[] = L"SOFTWARE\\Classes\\DesktopBackground\\Shell\\HakanaSora.ThemeSwitcher";
std::wstring ComKey() { return L"SOFTWARE\\Classes\\CLSID\\" + std::wstring(theme::CommandClsidString); }
void Win32(DWORD error) { if (error) throw_hresult(HRESULT_FROM_WIN32(error)); }
bool ValidVersion(const std::wstring& value) noexcept {
    // Exactly three nonempty decimal components; never accept '.' or '..'.
    unsigned parts = 1;
    bool digit = false;
    for (const auto ch : value) {
        if (ch >= L'0' && ch <= L'9') digit = true;
        else if (ch == L'.' && digit && parts < 3) { ++parts; digit = false; }
        else return false;
    }
    return digit && parts == 3;
}
std::wstring ReadIni(const fs::path& root, const wchar_t* name) {
    wchar_t value[32768]{};
    GetPrivateProfileStringW(L"Install",name,L"",value,32768,(root / L"install-state.ini").c_str());
    if (wcscmp(name,L"Version") == 0 && *value && !ValidVersion(value))
        throw hresult_error(E_INVALIDARG,L"安装状态中的版本号无效，已停止操作。");
    if (wcscmp(name,L"Backend") == 0 && *value && wcscmp(value,L"modern") != 0 && wcscmp(value,L"legacy") != 0)
        throw hresult_error(E_INVALIDARG,L"安装状态中的菜单类型无效，已停止操作。");
    return value;
}
void WriteIni(const fs::path& root, const wchar_t* name, const std::wstring& value) {
    if (!WritePrivateProfileStringW(L"Install",name,value.c_str(),(root / L"install-state.ini").c_str())) Win32(GetLastError());
}
std::wstring ReadRegistry(const std::wstring& path, const wchar_t* name = nullptr) {
    DWORD size{};
    auto error = RegGetValueW(HKEY_LOCAL_MACHINE,path.c_str(),name,RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY,nullptr,nullptr,&size);
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return {};
    Win32(error);
    std::wstring value(size / sizeof(wchar_t),L'\0');
    Win32(RegGetValueW(HKEY_LOCAL_MACHINE,path.c_str(),name,RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY,nullptr,value.data(),&size));
    value.resize(size / sizeof(wchar_t));
    while (!value.empty() && value.back() == L'\0') value.pop_back();
    return value;
}
void SetRegistry(const std::wstring& path,const wchar_t* name,const std::wstring& value) {
    HKEY key{};
    Win32(RegCreateKeyExW(HKEY_LOCAL_MACHINE,path.c_str(),0,nullptr,0,KEY_SET_VALUE | KEY_WOW64_64KEY,nullptr,&key,nullptr));
    const auto error = RegSetValueExW(key,name,0,REG_SZ,reinterpret_cast<const BYTE*>(value.c_str()),static_cast<DWORD>((value.size()+1)*sizeof(wchar_t)));
    RegCloseKey(key); Win32(error);
}
void BackupV1(const fs::path& root,const fs::path& destination,bool restore) {
    if (!restore) {
        const auto file=CreateFileW(destination.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if (file == INVALID_HANDLE_VALUE) Win32(GetLastError());
        const wchar_t bom=0xfeff; DWORD written{};
        const auto ok=WriteFile(file,&bom,sizeof(bom),&written,nullptr);
        const auto error=ok ? ERROR_SUCCESS : GetLastError(); CloseHandle(file); Win32(error);
    }
    const std::wstring prefix=L"SOFTWARE\\Classes\\DesktopBackground\\Shell\\";
    for (const auto& item : {std::pair{L"1_LightMode",L"light"},std::pair{L"2_DarkMode",L"dark"}}) {
        const auto expected=L"\""+(root/L"ThemeSwitcher.exe").wstring()+L"\" "+item.second;
        if (!restore && _wcsicmp(ReadRegistry(prefix+item.first+L"\\command").c_str(),expected.c_str()) != 0) continue;
        wchar_t command[32768]{};
        if (restore) {
            GetPrivateProfileStringW(item.first,L"command",L"",command,32768,destination.c_str());
            if (_wcsicmp(command,expected.c_str()) != 0) continue;
        }
        for (const auto* name : {L"title",L"icon",L"command"}) {
            if (!restore) {
                auto value = wcscmp(name,L"command")==0 ? ReadRegistry(prefix+item.first+L"\\command") :
                    ReadRegistry(prefix+item.first,wcscmp(name,L"title")==0?nullptr:L"Icon");
                if (!WritePrivateProfileStringW(item.first,name,value.c_str(),destination.c_str())) Win32(GetLastError());
            } else {
                wchar_t value[32768]{}; GetPrivateProfileStringW(item.first,name,L"",value,32768,destination.c_str());
                if (wcscmp(name,L"command")==0) SetRegistry(prefix+item.first+L"\\command",nullptr,value);
                else SetRegistry(prefix+item.first,wcscmp(name,L"title")==0?nullptr:L"Icon",value);
            }
        }
        if (restore) {
            for (const auto* separator : {L"0_SeparatorTop",L"3_SeparatorBottom"}) {
                HKEY key{}; Win32(RegCreateKeyExW(HKEY_LOCAL_MACHINE,(prefix+separator).c_str(),0,nullptr,0,KEY_SET_VALUE|KEY_WOW64_64KEY,nullptr,&key,nullptr));
                DWORD flags=8; auto error=RegSetValueExW(key,L"CommandFlags",0,REG_DWORD,reinterpret_cast<BYTE*>(&flags),sizeof(flags));
                RegCloseKey(key); Win32(error);
            }
        }
    }
}
void DeleteRegistry(const std::wstring& path) {
    HKEY classes{};
    Win32(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Classes",0,KEY_WRITE | KEY_READ | DELETE | KEY_WOW64_64KEY,&classes));
    auto relative = path.substr(std::wstring(L"SOFTWARE\\Classes\\").size());
    auto error = RegDeleteTreeW(classes,relative.c_str()); RegCloseKey(classes);
    if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) Win32(error);
}
void SetLegacy(const fs::path& dll) {
    SetRegistry(ComKey()+L"\\InprocServer32",nullptr,dll.wstring());
    SetRegistry(ComKey()+L"\\InprocServer32",L"ThreadingModel",L"Apartment");
    SetRegistry(LegacyKey,nullptr,L"切换颜色模式");
    SetRegistry(LegacyKey,L"ExplorerCommandHandler",theme::CommandClsidString);
}
void RemoveLegacyV2() { DeleteRegistry(LegacyKey); DeleteRegistry(ComKey()); }
void RemoveLegacyV1(const fs::path& root) {
    bool owned = false;
    const std::wstring prefix = L"SOFTWARE\\Classes\\DesktopBackground\\Shell\\";
    for (const auto& item : {std::pair{L"1_LightMode",L"light"},std::pair{L"2_DarkMode",L"dark"}}) {
        const auto path = prefix + item.first;
        const auto expected = L"\"" + (root / L"ThemeSwitcher.exe").wstring() + L"\" " + item.second;
        if (_wcsicmp(ReadRegistry(path+L"\\command").c_str(),expected.c_str()) == 0) { DeleteRegistry(path); owned = true; }
    }
    // Only remove the old generic-named separators when an adjacent command was
    // positively identified as this installation's command.
    if (owned) { DeleteRegistry(prefix+L"0_SeparatorTop"); DeleteRegistry(prefix+L"3_SeparatorBottom"); }
}
using theme::deployment::Identity;
using theme::deployment::ReadIdentity;
Windows::Foundation::Uri FileUri(const fs::path& path) {
    std::wstring uri(32768,L'\0'); DWORD length=static_cast<DWORD>(uri.size());
    check_hresult(UrlCreateFromPathW(fs::absolute(path).c_str(),uri.data(),&length,0)); uri.resize(length);
    return Windows::Foundation::Uri(uri);
}
void Deployment(const DeploymentResult& result) {
    const auto error = result.ExtendedErrorCode();
    if (error.value < 0) throw hresult_error(error,result.ErrorText());
}
bool Missing(hresult value) {
    return value == HRESULT_FROM_WIN32(ERROR_INSTALL_PACKAGE_NOT_FOUND) || value == HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}
void RemovePreference(const Identity& id){
    const auto preference=L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ContextMenu\\Preferences\\extensions\\"+id.family;
    const auto error=RegDeleteTreeW(HKEY_CURRENT_USER,preference.c_str());
    if(error!=ERROR_FILE_NOT_FOUND && error!=ERROR_PATH_NOT_FOUND)Win32(error);
}
void RemoveModern(const Identity& id) {
    PackageManager manager;
    if(!id.unsignedPrototype){
        try { Deployment(manager.DeprovisionPackageForAllUsersAsync(id.family).get()); }
        catch (const hresult_error& error) { if (!Missing(error.code())) throw; }
    }
    std::vector<std::wstring> packages;
    for (const auto& package : manager.FindPackages(id.name,id.publisher)) packages.emplace_back(package.Id().FullName());
    for (const auto& full : packages) {
        try { Deployment(manager.RemovePackageAsync(full,RemovalOptions::RemoveForAllUsers).get()); }
        catch (const hresult_error& error) { if (!Missing(error.code())) throw; }
    }
}
void AddModern(const fs::path& package,const fs::path& root,const Identity& id) {
    PackageManager manager;
    if(!id.unsignedPrototype){
        StagePackageOptions stage; stage.ExternalLocationUri(FileUri(root));
        Deployment(manager.StagePackageByUriAsync(FileUri(package),stage).get());
    }
    AddPackageOptions add; add.ExternalLocationUri(FileUri(root));
    add.AllowUnsigned(id.unsignedPrototype);
    Deployment(manager.AddPackageByUriAsync(FileUri(package),add).get());
    // Match the elevated Add-AppxPackage -AllowUnsigned experiment. Unsigned
    // executable packages use Windows' all-user deployment rules; do not add
    // an untested signed-package staging/provisioning step to this path.
    if(!id.unsignedPrototype)Deployment(manager.ProvisionPackageForAllUsersAsync(id.family).get());
}
void ModernInstall(const fs::path& package,const fs::path& root) {
    if (!fs::is_regular_file(root/L"ThemeSwitcher.exe") || !fs::is_regular_file(root/L"components"/TS_VERSION_W/L"ThemeSwitcherShell.dll"))
        throw hresult_error(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),L"安装文件不完整，无法注册菜单。");
    const auto id = ReadIdentity(package);
    PackageManager manager;
    bool alreadyInstalled = false;
    for (const auto& current : manager.FindPackages(id.name,id.publisher)) {
        if (current.Id().FullName() == id.full) alreadyInstalled = true;
    }
    const auto oldVersion = ReadIni(root,L"Version");
    const auto previous = root / L"components" / oldVersion / L"identity.msix";
    try {
        if (!alreadyInstalled) AddModern(package,root,id);
        WriteIni(root,L"Backend",L"modern");
        WriteIni(root,L"Version",TS_VERSION_W);
        RemoveLegacyV1(root);
        // Never keep both packaged and unpackaged v2 registrations.
        RemoveLegacyV2();
        if(!oldVersion.empty() && oldVersion!=TS_VERSION_W && fs::exists(previous)){
            const auto oldIdentity=ReadIdentity(previous);
            if(oldIdentity.family!=id.family)RemoveModern(oldIdentity);
        }
    } catch (...) {
        const auto original = std::current_exception();
        if (!alreadyInstalled) {
            try {
                // Remove only this newly introduced version; do not deprovision
                // the whole family or delete older versions during rollback.
                Deployment(manager.RemovePackageAsync(id.full,RemovalOptions::RemoveForAllUsers).get());
                if (!oldVersion.empty() && oldVersion != TS_VERSION_W && fs::exists(previous)) AddModern(previous,root,ReadIdentity(previous));
            } catch (...) { throw hresult_error(E_FAIL,L"菜单注册失败，且自动恢复未完成；请保留安装日志并重试旧版安装包。"); }
        }
        std::rethrow_exception(original);
    }
}
void LegacyInstall(const fs::path& root) {
    const auto dll = root / L"components" / TS_VERSION_W / L"ThemeSwitcherShell.dll";
    if (!fs::is_regular_file(root/L"ThemeSwitcher.exe") || !fs::is_regular_file(dll))
        throw hresult_error(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),L"安装文件不完整，无法注册菜单。");
    const auto previous = ReadRegistry(ComKey()+L"\\InprocServer32");
    try {
        SetLegacy(dll);
        WriteIni(root,L"Backend",L"legacy"); WriteIni(root,L"Version",TS_VERSION_W);
        RemoveLegacyV1(root);
    } catch (...) {
        const auto error = std::current_exception();
        try { RemoveLegacyV2(); if (!previous.empty()) SetLegacy(previous); }
        catch (...) { throw hresult_error(E_FAIL,L"菜单注册失败，且自动恢复未完成。"); }
        std::rethrow_exception(error);
    }
}
bool OwnedVersion(const fs::path& directory) {
    if (!fs::is_directory(directory) || fs::is_symlink(directory)) return false;
    const auto attributes = GetFileAttributesW(directory.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || attributes & FILE_ATTRIBUTE_REPARSE_POINT) return false;
    std::ifstream marker(directory / L"owner.txt"); std::string value; std::getline(marker,value);
    if (!value.empty() && value.back() == '\r') value.pop_back();
    return value == "HakanaSora.ThemeSwitcher";
}
fs::path Components(const fs::path& root) {
    const auto directory = root/L"components";
    const auto attributes = GetFileAttributesW(directory.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
        throw hresult_error(E_INVALIDARG,L"组件目录是重解析点，已停止操作。");
    return directory;
}
void RemoveOwnedVersion(const fs::path& directory) {
    if (!OwnedVersion(directory)) return;
    // Ownership marker is last, so a failed file deletion can be retried.
    for (const auto* file : {L"ThemeSwitcherShell.dll",L"ThemeSwitcher.Deploy.exe",L"ThemeSwitcher.MenuSetup.exe",L"identity.msix",L"owner.txt"}) {
        const auto path = directory/file;
        if (fs::exists(path) && !DeleteFileW(path.c_str())) Win32(GetLastError());
    }
    if (!RemoveDirectoryW(directory.c_str())) {
        const auto error = GetLastError();
        if (error != ERROR_DIR_NOT_EMPTY) Win32(error);
    }
}
void ProbeFile(const fs::path& path) {
    if (!fs::exists(path)) return;
    const auto handle = CreateFileW(path.c_str(),GENERIC_READ | GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if (handle == INVALID_HANDLE_VALUE) throw hresult_error(HRESULT_FROM_WIN32(GetLastError()),L"程序或菜单组件仍被占用。请关闭右键菜单；必要时手动刷新资源管理器，再重试。");
    CloseHandle(handle);
}
void Preflight(const fs::path& root) {
    if (!theme::IsNativeX64()) throw hresult_error(E_NOTIMPL,L"此版本仅支持原生 x64 Windows。");
    ProbeFile(root / L"ThemeSwitcher.exe");
    const auto components = Components(root);
    if (fs::exists(components)) for (const auto& directory : fs::directory_iterator(components)) {
        if (!OwnedVersion(directory.path())) continue;
        ProbeFile(directory.path()/L"ThemeSwitcherShell.dll");
        ProbeFile(directory.path()/L"ThemeSwitcher.Deploy.exe");
        ProbeFile(directory.path()/L"ThemeSwitcher.MenuSetup.exe");
    }
}
void CleanupOld(const fs::path& root) {
    const auto components = Components(root);
    if (!fs::exists(components)) return;
    for (const auto& entry : fs::directory_iterator(components)) {
        if (entry.path().filename() == TS_VERSION_W || !OwnedVersion(entry.path())) continue;
        RemoveOwnedVersion(entry.path());
    }
}
void Uninstall(const fs::path& root) {
    Preflight(root);
    if (ReadIni(root,L"Backend") == L"modern") {
        const auto version = ReadIni(root,L"Version");
        if (!ValidVersion(version)) throw_hresult(E_INVALIDARG);
        const auto package=root/L"components"/version/L"identity.msix";
        const auto id=ReadIdentity(package);
        try {
            RemoveModern(id);
            RemovePreference(id);
        }
        catch (...) {
            const auto original=std::current_exception();
            try { AddModern(package,root,id); }
            catch (...) { throw hresult_error(E_FAIL,L"移除菜单身份失败，自动恢复也未完成；程序文件已保留，请重试当前安装包。"); }
            std::rethrow_exception(original);
        }
    }
    RemoveLegacyV2(); RemoveLegacyV1(root);
}
void Rollback(const fs::path& root) {
    // Installer restores the previous state file and payload backups first.
    const auto oldBackend=ReadIni(root,L"Backend"), oldVersion=ReadIni(root,L"Version");
    const auto introduced=root/L"components"/TS_VERSION_W/L"identity.msix";
    if (theme::SupportsModernMenu() && fs::exists(introduced)) {
        const auto current=ReadIdentity(introduced);
        PackageManager manager;
        if (oldBackend != L"modern") {
            try { RemoveModern(current); } catch (const hresult_error& e) { if (!Missing(e.code())) throw; }
            RemovePreference(current);
        } else if (!oldVersion.empty() && oldVersion != TS_VERSION_W) {
            try { Deployment(manager.RemovePackageAsync(current.full,RemovalOptions::RemoveForAllUsers).get()); }
            catch (const hresult_error& e) { if (!Missing(e.code())) throw; }
            const auto oldPackage=root/L"components"/oldVersion/L"identity.msix";
            const auto previous=ReadIdentity(oldPackage);
            AddModern(oldPackage,root,previous);
            if(previous.family!=current.family)RemovePreference(current);
        }
    }
    if (oldBackend == L"legacy" && !oldVersion.empty()) SetLegacy(root/L"components"/oldVersion/L"ThemeSwitcherShell.dll");
    else RemoveLegacyV2();
}
void VerifySignature(const fs::path& package) {
    const auto identity=ReadIdentity(package); // Reject identities outside this product before any deployment.
    if(identity.unsignedPrototype){
        // Only this exact, isolated prototype publisher may use AllowUnsigned.
        // Never silently downgrade a signed identity when its trust check fails.
        com_ptr<IStream> stream;
        check_hresult(SHCreateStreamOnFileEx(package.c_str(),STGM_READ|STGM_SHARE_DENY_WRITE,0,FALSE,nullptr,stream.put()));
        auto factory=create_instance<IAppxFactory>(CLSID_AppxFactory);
        com_ptr<IAppxPackageReader> reader;check_hresult(factory->CreatePackageReader(stream.get(),reader.put()));
        com_ptr<IAppxFile> signature;
        if(SUCCEEDED(reader->GetFootprintFile(APPX_FOOTPRINT_FILE_TYPE_SIGNATURE,signature.put())))
            throw hresult_error(E_INVALIDARG,L"无签名验证包的身份与文件内容不一致。");
        return;
    }
    WINTRUST_FILE_INFO file{sizeof(file)}; file.pcwszFilePath=package.c_str();
    WINTRUST_DATA data{sizeof(data)};
    data.dwUIChoice=WTD_UI_NONE; data.fdwRevocationChecks=WTD_REVOKE_NONE;
    data.dwUnionChoice=WTD_CHOICE_FILE; data.pFile=&file;
    data.dwStateAction=WTD_STATEACTION_VERIFY; data.dwProvFlags=WTD_CACHE_ONLY_URL_RETRIEVAL;
    GUID action=WINTRUST_ACTION_GENERIC_VERIFY_V2;
    const auto result=WinVerifyTrust(nullptr,&action,&data);
    data.dwStateAction=WTD_STATEACTION_CLOSE; WinVerifyTrust(nullptr,&action,&data);
    if (result != ERROR_SUCCESS) throw hresult_error(result,L"菜单身份包的签名尚未受信任。开发验证版需先在测试环境手动信任所提供的公钥证书；安装程序不会自动修改证书信任。");
}
void DiscardVersion(const fs::path& root) {
    if (ReadIni(root,L"Version") == TS_VERSION_W) return;
    const auto components = Components(root);
    RemoveOwnedVersion(components/TS_VERSION_W);
    if (!RemoveDirectoryW(components.c_str())) {
        const auto error = GetLastError();
        if (error != ERROR_DIR_NOT_EMPTY && error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) Win32(error);
    }
}
void SaveError(const fs::path& file,const std::wstring& error) noexcept {
    if (file.empty()) return;
    const auto handle = CreateFileW(file.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (handle == INVALID_HANDLE_VALUE) return;
    DWORD written{}; const wchar_t bom=0xfeff;
    WriteFile(handle,&bom,sizeof(bom),&written,nullptr);
    CloseHandle(handle);
    WritePrivateProfileStringW(L"Error",L"Message",error.c_str(),file.c_str());
}
}
#ifndef TS_DEPLOY_TESTING
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    int count{}; auto args = CommandLineToArgvW(GetCommandLineW(),&count);
    if (!args) return 2;
    std::vector<std::wstring> values; for (int i=1;i<count;i++) values.emplace_back(args[i]); LocalFree(args);
    fs::path errorFile;
    if (values.size() >= 2 && values[values.size()-2] == L"--error-file") { errorFile=values.back(); values.resize(values.size()-2); }
    try {
        init_apartment(apartment_type::multi_threaded);
        if (values.size() == 1 && values[0] == L"supports-modern") return theme::SupportsModernMenu() ? 0 : 1;
        if (values.size() < 2) return 2;
        const fs::path root=fs::absolute(values[1]);
        if (values[0] == L"preflight" && values.size()==2) Preflight(root);
        else if (values[0] == L"verify-package" && values.size()==2) VerifySignature(root);
        else if (values[0] == L"backup-v1" && values.size()==3) BackupV1(root,values[2],false);
        else if (values[0] == L"restore-v1" && values.size()==3) BackupV1(root,values[2],true);
        else if (values[0] == L"rollback" && values.size()==2) Rollback(root);
        else if (values[0] == L"discard-version" && values.size()==2) DiscardVersion(root);
        else if (values[0] == L"install-legacy" && values.size()==2) LegacyInstall(root);
        else if (values[0] == L"install-modern" && values.size()==3 && theme::SupportsModernMenu()) ModernInstall(fs::absolute(values[2]),root);
        else if (values[0] == L"cleanup-old" && values.size()==2) CleanupOld(root);
        else if (values[0] == L"uninstall" && values.size()==2) Uninstall(root);
        else return 2;
        if (values[0] != L"preflight" && values[0] != L"verify-package" && values[0] != L"backup-v1")
            SHChangeNotify(SHCNE_ASSOCCHANGED,SHCNF_IDLIST,nullptr,nullptr);
        return 0;
    } catch (const hresult_error& error) { SaveError(errorFile,error.message().c_str()); return 10; }
      catch (const std::exception& error) { SaveError(errorFile,to_hstring(error.what()).c_str()); return 11; }
      catch (...) { SaveError(errorFile,L"部署失败，未能取得详细错误信息。"); return 12; }
}
#endif
