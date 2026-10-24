#define TS_DEPLOY_TESTING
#include "../src/native/Deploy.cpp"
#include <iostream>

namespace {
void Require(bool condition,const char* message) { if (!condition) throw std::runtime_error(message); }
struct Environment {
    HKEY hive{};
    std::wstring registryPath;
    fs::path directory;
    Environment() {
        GUID guid{}; CoCreateGuid(&guid); wchar_t text[64]{}; StringFromGUID2(guid,text,64);
        registryPath=L"Software\\ThemeSwitcher.DeployTests\\"+std::wstring(text);
        directory=fs::temp_directory_path()/(L"ThemeSwitcher-DeployTests-"+std::wstring(text));
        fs::create_directories(directory);
        Win32(RegCreateKeyExW(HKEY_CURRENT_USER,registryPath.c_str(),0,nullptr,0,KEY_ALL_ACCESS,nullptr,&hive,nullptr));
        // This changes only the predefined handle mapping INSIDE THIS PROCESS.
        // All HKLM writes below go into our own disposable HKCU subtree.
        Win32(RegOverridePredefKey(HKEY_LOCAL_MACHINE,hive));
    }
    ~Environment() {
        RegOverridePredefKey(HKEY_LOCAL_MACHINE,nullptr);
        RegCloseKey(hive);
        RegDeleteTreeW(HKEY_CURRENT_USER,registryPath.c_str());
        RegDeleteKeyW(HKEY_CURRENT_USER,L"Software\\ThemeSwitcher.DeployTests");
        // Only remove the freshly created GUID directory under the exact temp
        // parent; never recursively remove a computed installation directory.
        std::error_code error;
        if (directory.parent_path() == fs::temp_directory_path() && directory.filename().wstring().starts_with(L"ThemeSwitcher-DeployTests-"))
            fs::remove_all(directory,error);
    }
};
void Put(const fs::path& path,const char* value="test") { fs::create_directories(path.parent_path()); std::ofstream file(path,std::ios::binary); file<<value; }
void Tests() {
    Environment environment;
    const auto root=environment.directory/L"installation";
    const auto components=root/L"components"/TS_VERSION_W;
    Put(root/L"ThemeSwitcher.exe","original"); Put(components/L"ThemeSwitcherShell.dll"); Put(components/L"owner.txt","HakanaSora.ThemeSwitcher\r\n");
    const std::wstring prefix=L"SOFTWARE\\Classes\\DesktopBackground\\Shell\\";
    for (const auto& item : {std::pair{L"1_LightMode",L"light"},std::pair{L"2_DarkMode",L"dark"}}) {
        SetRegistry(prefix+item.first,nullptr,L"原来的中文菜单");
        SetRegistry(prefix+item.first,L"Icon",L"system-icon");
        SetRegistry(prefix+item.first+L"\\command",nullptr,L"\""+(root/L"ThemeSwitcher.exe").wstring()+L"\" "+item.second);
    }
    SetRegistry(prefix+L"OtherApp\\command",nullptr,L"unrelated-command");
    const auto backup=environment.directory/L"old-menu.ini";
    BackupV1(root,backup,false);
    LegacyInstall(root);
    Require(ReadRegistry(LegacyKey,L"ExplorerCommandHandler") == theme::CommandClsidString,"Legacy dynamic command registration");
    Require(ReadRegistry(ComKey()+L"\\InprocServer32") == (components/L"ThemeSwitcherShell.dll").wstring(),"64-bit COM points to versioned component");
    Require(ReadRegistry(prefix+L"1_LightMode\\command").empty() && ReadRegistry(prefix+L"2_DarkMode\\command").empty(),"Upgrade removes old dual menu");
    Require(ReadRegistry(prefix+L"OtherApp\\command") == L"unrelated-command","Never touch unrelated menu registration");
    Require(ReadIni(root,L"Backend") == L"legacy" && ReadIni(root,L"Version") == TS_VERSION_W,"Installation journal");
    RemoveLegacyV2(); BackupV1(root,backup,true);
    Require(ReadRegistry(prefix+L"1_LightMode") == L"原来的中文菜单","Rollback retains Unicode menu text");
    Require(!ReadRegistry(prefix+L"2_DarkMode\\command").empty(),"Rollback restores previous command");
    SetRegistry(prefix+L"1_LightMode\\command",nullptr,L"another-program.exe");
    DeleteRegistry(prefix+L"2_DarkMode");
    RemoveLegacyV1(root);
    Require(ReadRegistry(prefix+L"1_LightMode\\command") == L"another-program.exe","Old generic names are removed only when command ownership matches");
    const auto locked=CreateFileW((root/L"ThemeSwitcher.exe").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    Require(locked != INVALID_HANDLE_VALUE,"Open locked fixture");
    bool blocked=false; try { Preflight(root); } catch (const hresult_error&) { blocked=true; }
    CloseHandle(locked); Require(blocked,"Occupied program fails preflight without changing registrations");
    const auto old=root/L"components"/L"1.9.0";
    Put(old/L"owner.txt","HakanaSora.ThemeSwitcher\r\n"); Put(old/L"ThemeSwitcherShell.dll"); Put(old/L"user-note.txt","keep");
    const auto unknown=root/L"components"/L"unrelated"; Put(unknown/L"ThemeSwitcherShell.dll","keep");
    CleanupOld(root);
    Require(fs::exists(old/L"user-note.txt") && !fs::exists(old/L"ThemeSwitcherShell.dll"),"Cleanup deletes only owned component files");
    Require(fs::exists(unknown/L"ThemeSwitcherShell.dll") && fs::exists(components/L"ThemeSwitcherShell.dll"),"Cleanup preserves unknown and active directories");
    fs::remove(root/L"install-state.ini");
    // There is no MSIX in this fixture: rollback cannot call package deployment.
    Rollback(root); DiscardVersion(root);
    Require(!fs::exists(components),"Failed new version cleanup");
    Require(fs::exists(old/L"user-note.txt"),"Rollback never removes unrelated content");
    std::cout<<"PASS deployment: process-local registry redirection, legacy upgrade/restore, Unicode, ownership, locks and safe cleanup\n";
}
}
int main() {
    try { Tests(); return 0; }
    catch (const winrt::hresult_error& e) { std::cerr<<"FAIL HRESULT "<<std::hex<<e.code().value<<"\n"; return 1; }
    catch (const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<"\n"; return 1; }
}
