#include "PackageIdentity.h"
#include <roapi.h>
#include <inspectable.h>
#include <shellapi.h>
#include <winrt/Windows.Foundation.h>
#include <vector>

namespace {
// Undocumented interfaces verified against the local Windows Beta and public
// Microsoft PDBs. Always QueryInterface; never call private functions by RVA.
struct __declspec(uuid("80ed7316-dd8a-5f02-8f7a-224314683792")) Settings2 : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE ExtensionSettings(IUnknown** result)=0;
};
struct ExtensionVector : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE GetAt(UINT32 index,IUnknown** result)=0;
    virtual HRESULT STDMETHODCALLTYPE Size(UINT32* result)=0;
};
struct __declspec(uuid("bd5cdd78-d7e0-528d-8a83-002cb9c6309a")) ExtensionSetting : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE DisplayName(HSTRING* result)=0;
    virtual HRESULT STDMETHODCALLTYPE PackageFullName(HSTRING* result)=0;
    virtual HRESULT STDMETHODCALLTYPE Logo(IInspectable** result)=0;
    virtual HRESULT STDMETHODCALLTYPE Enabled(BYTE* result)=0;
    virtual HRESULT STDMETHODCALLTYPE SetEnabled(BYTE value)=0;
};
std::wstring PreferencePath(const std::wstring& family){
    return L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ContextMenu\\Preferences\\extensions\\"+family;
}
bool ExplicitChoice(const std::wstring& family){
    DWORD size{},type{};
    return RegGetValueW(HKEY_CURRENT_USER,PreferencePath(family).c_str(),L"Enabled",RRF_RT_ANY,&type,nullptr,&size)==ERROR_SUCCESS;
}
int Initialize(const theme::deployment::Identity& identity){
    // Never overwrite an existing off/on choice during an upgrade.
    if(ExplicitChoice(identity.family))return 0;
    winrt::hstring name=L"Windows.Internal.Shell.FileExplorer.ContextMenu.FileExplorerContextMenuSettings";
    winrt::com_ptr<IInspectable> instance;
    auto hr=RoActivateInstance(reinterpret_cast<HSTRING>(winrt::get_abi(name)),instance.put());
    if(FAILED(hr))return 20; // This settings page/API is absent on older Windows.
    auto settings=instance.try_as<Settings2>();if(!settings)return 20;
    winrt::com_ptr<IUnknown> rawList;winrt::check_hresult(settings->ExtensionSettings(rawList.put()));
    auto* list=reinterpret_cast<ExtensionVector*>(rawList.get());
    UINT32 count{};winrt::check_hresult(list->Size(&count));
    if(count>10000)winrt::throw_hresult(E_UNEXPECTED);
    for(UINT32 i=0;i<count;++i){
        winrt::com_ptr<IUnknown> raw;winrt::check_hresult(list->GetAt(i,raw.put()));
        auto item=raw.try_as<ExtensionSetting>();if(!item)continue;
        winrt::hstring full;winrt::check_hresult(item->PackageFullName(reinterpret_cast<HSTRING*>(winrt::put_abi(full))));
        if(full!=identity.full)continue;
        BYTE before{};winrt::check_hresult(item->Enabled(&before));
        if(!before)winrt::check_hresult(item->SetEnabled(TRUE));
        BYTE after{};winrt::check_hresult(item->Enabled(&after));
        return after?0:21;
    }
    return 22; // Package was not registered for this interactive user.
}
}
#ifndef TS_MENU_SETUP_TESTING
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    int count{};auto argv=CommandLineToArgvW(GetCommandLineW(),&count);if(!argv)return 2;
    std::vector<std::wstring> args;for(int i=1;i<count;++i)args.emplace_back(argv[i]);LocalFree(argv);
    try{
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        if(args.size()!=2 || args[0]!=L"initialize")return 2;
        return Initialize(theme::deployment::ReadIdentity(std::filesystem::absolute(args[1])));
    }catch(...){return 21;}
}
#endif
