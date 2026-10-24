#pragma once
#include <appxpackaging.h>
#include <shlwapi.h>
#include <winrt/base.h>
#include <filesystem>
#include <string>
namespace theme::deployment {
inline constexpr wchar_t UnsignedName[]=L"HakanaSora.ThemeSwitcher.UnsignedDevelopment";
inline constexpr wchar_t UnsignedPublisher[]=L"CN=Hakana Sora ThemeSwitcher Unsigned Development, OID.2.25.311729368913984317654407730594956997722=1";
struct Identity { std::wstring name,publisher,family,full; bool unsignedPrototype{}; };
inline bool ValidIdentity(const std::wstring& name,const std::wstring& publisher) {
    if(name==UnsignedName)return publisher==UnsignedPublisher;
    return (name==L"HakanaSora.ThemeSwitcher" || name==L"HakanaSora.ThemeSwitcher.Development") &&
        publisher.find(L"OID.2.25.311729368913984317654407730594956997722")==std::wstring::npos;
}
inline Identity ReadIdentity(const std::filesystem::path& package) {
    using namespace winrt;
    com_ptr<IStream> stream;
    check_hresult(SHCreateStreamOnFileEx(package.c_str(),STGM_READ|STGM_SHARE_DENY_WRITE,0,FALSE,nullptr,stream.put()));
    auto factory=create_instance<IAppxFactory>(CLSID_AppxFactory);
    com_ptr<IAppxPackageReader> reader;check_hresult(factory->CreatePackageReader(stream.get(),reader.put()));
    com_ptr<IAppxManifestReader> manifest;check_hresult(reader->GetManifest(manifest.put()));
    com_ptr<IAppxManifestPackageId> id;check_hresult(manifest->GetPackageId(id.put()));
    Identity value;
    const auto take=[](LPWSTR pointer){std::wstring result(pointer);CoTaskMemFree(pointer);return result;};
    LPWSTR text{};
    check_hresult(id->GetName(&text));value.name=take(text);
    check_hresult(id->GetPublisher(&text));value.publisher=take(text);
    check_hresult(id->GetPackageFamilyName(&text));value.family=take(text);
    check_hresult(id->GetPackageFullName(&text));value.full=take(text);
    if(!ValidIdentity(value.name,value.publisher))throw_hresult(E_INVALIDARG);
    value.unsignedPrototype=value.name==UnsignedName;
    return value;
}
}
