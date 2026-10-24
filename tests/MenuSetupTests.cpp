#define TS_MENU_SETUP_TESTING
#include "../src/native/MenuSetup.cpp"
#include <iostream>
#include <stdexcept>
int main(){
    HKEY hive{};bool redirected=false;std::wstring path;
    try{
        GUID guid{};CoCreateGuid(&guid);wchar_t text[64]{};StringFromGUID2(guid,text,64);
        path=L"Software\\ThemeSwitcher.MenuSetupTests\\"+std::wstring(text);
        if(RegCreateKeyExW(HKEY_CURRENT_USER,path.c_str(),0,nullptr,0,KEY_ALL_ACCESS,nullptr,&hive,nullptr))throw std::runtime_error("Fixture creation");
        if(RegOverridePredefKey(HKEY_CURRENT_USER,hive))throw std::runtime_error("Process-local registry redirect");
        redirected=true;
        theme::deployment::Identity identity;
        identity.family=L"ThemeSwitcherTestFamily";
        identity.full=L"Nonexistent.Test.Package";
        HKEY key{};
        if(RegCreateKeyExW(HKEY_CURRENT_USER,PreferencePath(identity.family).c_str(),0,nullptr,0,KEY_ALL_ACCESS,nullptr,&key,nullptr))throw std::runtime_error("Preference fixture");
        for(DWORD choice:{0u,1u}){
            if(RegSetValueExW(key,L"Enabled",0,REG_DWORD,reinterpret_cast<const BYTE*>(&choice),sizeof(choice)))throw std::runtime_error("Write preference fixture");
            if(Initialize(identity)!=0)throw std::runtime_error("Existing choice must bypass package discovery and activation");
            DWORD actual{},bytes=sizeof(actual);
            if(RegQueryValueExW(key,L"Enabled",nullptr,nullptr,reinterpret_cast<BYTE*>(&actual),&bytes)||actual!=choice)throw std::runtime_error("An upgrade changed the user's explicit choice");
        }
        RegCloseKey(key);
        RegOverridePredefKey(HKEY_CURRENT_USER,nullptr);redirected=false;
        RegCloseKey(hive);hive=nullptr;
        RegDeleteTreeW(HKEY_CURRENT_USER,path.c_str());RegDeleteKeyW(HKEY_CURRENT_USER,L"Software\\ThemeSwitcher.MenuSetupTests");
        std::cout<<"PASS first-run preference: preserve both explicit off/on choices without activating the Shell settings service\n";
        return 0;
    }catch(const std::exception& error){
        if(redirected)RegOverridePredefKey(HKEY_CURRENT_USER,nullptr);
        if(hive)RegCloseKey(hive);
        if(!path.empty())RegDeleteTreeW(HKEY_CURRENT_USER,path.c_str());
        std::cerr<<"FAIL "<<error.what()<<"\n";return 1;
    }
}
