#pragma once

#include <windows.h>
#include <algorithm>
#include <cctype>
#include <mutex>
#include <string>
#include <vector>

// One process-wide policy for DAT reads. Logical paths always remain rooted in
// the retail installation so zone identity, FTABLE and companion lookup agree.
// Header-only because several independent parser/test targets embed the readers.
namespace FFXIDatResolver
{
struct Settings
{
    std::string installRoot;
    std::string replacementRoot;
    bool enabled = false;
};
enum class Source { Direct, Retail, Replacement };
struct Resolution
{
    std::string logicalPath;
    std::string sourcePath;
    std::string relativePath;
    Source source = Source::Direct;
};
struct OpenRecord
{
    Resolution resolution;
    DWORD error = ERROR_SUCCESS; // OS open result, not parser validation.
};
namespace Detail
{
inline std::mutex mutex;
inline Settings settings;
inline std::vector<OpenRecord> recent;

inline std::string Absolute(const char* path)
{
    if (!path || !*path) return {};
    const DWORD length = GetFullPathNameA(path,0,nullptr,nullptr);
    if (!length) return {};
    std::string result(length,'\0');
    const DWORD copied = GetFullPathNameA(path,length,result.data(),nullptr);
    if (!copied || copied >= length) return {};
    result.resize(copied);
    std::replace(result.begin(),result.end(),'/','\\');
    while (result.size()>3 && result.back()=='\\') result.pop_back();
    return result;
}
inline bool Equal(const std::string& a,const std::string& b)
{ return _stricmp(a.c_str(),b.c_str())==0; }
inline bool Digits(const std::string& value)
{
    return !value.empty() && std::all_of(value.begin(),value.end(),
        [](unsigned char c){return c>='0' && c<='9';});
}
inline bool AssetKey(const std::string& value)
{
    const auto first=value.find('\\'), second=value.find('\\',first==std::string::npos?0:first+1);
    if(first==std::string::npos || second==std::string::npos || value.find('\\',second+1)!=std::string::npos) return false;
    const auto rom=value.substr(0,first), folder=value.substr(first+1,second-first-1), file=value.substr(second+1);
    if(_strnicmp(rom.c_str(),"ROM",3)!=0 || (rom.size()!=3 && !Digits(rom.substr(3)))) return false;
    // Retail numbered hives are ROM2..ROM19. Tables themselves are never replaced.
    if(rom.size()!=3 && (rom.size()>5 || rom[3]=='0' || std::stoi(rom.substr(3))<2 || std::stoi(rom.substr(3))>19)) return false;
    return Digits(folder) && file.size()>4 && _stricmp(file.c_str()+file.size()-4,".DAT")==0 && Digits(file.substr(0,file.size()-4));
}
}

inline Settings CurrentSettings()
{ std::lock_guard<std::mutex> lock(Detail::mutex); return Detail::settings; }

inline void Configure(Settings settings)
{
    settings.installRoot=Detail::Absolute(settings.installRoot.c_str());
    settings.replacementRoot=Detail::Absolute(settings.replacementRoot.c_str());
    std::lock_guard<std::mutex> lock(Detail::mutex);
    Detail::settings=std::move(settings);
    Detail::recent.clear();
}
inline void SetInstallRoot(const char* root)
{
    auto settings=CurrentSettings(); settings.installRoot=root?root:""; Configure(std::move(settings));
}

inline Resolution Resolve(const char* path)
{
    Resolution result;
    result.logicalPath=path?path:"";
    result.sourcePath=result.logicalPath;
    const auto settings=CurrentSettings();
    const auto absolute=Detail::Absolute(path);
    std::string prefix=settings.installRoot;
    if(prefix.empty() || absolute.empty()) return result;
    if(prefix.back()!='\\') prefix+='\\';
    if(absolute.size()<=prefix.size() || _strnicmp(absolute.c_str(),prefix.c_str(),prefix.size())!=0) return result;
    const auto relative=absolute.substr(prefix.size());
    if(!Detail::AssetKey(relative)) return result;
    result.relativePath=relative;
    result.source=Source::Retail;
    if(!settings.enabled || settings.replacementRoot.empty()) return result;
    const std::string candidate=settings.replacementRoot+"\\"+relative;
    const DWORD attributes=GetFileAttributesA(candidate.c_str());
    const DWORD error=attributes==INVALID_FILE_ATTRIBUTES?GetLastError():ERROR_SUCCESS;
    // Only absence permits retail fallback. Access failures and directories
    // remain selected candidates and fail at the reader instead of hiding edits.
    if(attributes==INVALID_FILE_ATTRIBUTES && (error==ERROR_FILE_NOT_FOUND || error==ERROR_PATH_NOT_FOUND)) return result;
    result.sourcePath=candidate;
    result.source=Source::Replacement;
    return result;
}

inline bool FileExists(const char* path)
{
    const auto resolved=Resolve(path);
    if(resolved.sourcePath.empty()) return false;
    const DWORD attributes=GetFileAttributesA(resolved.sourcePath.c_str());
    return attributes!=INVALID_FILE_ATTRIBUTES && !(attributes&FILE_ATTRIBUTE_DIRECTORY);
}

inline HANDLE OpenRead(const char* path, Resolution* selected=nullptr)
{
    const auto resolved=Resolve(path);
    HANDLE file=CreateFileA(resolved.sourcePath.c_str(),GENERIC_READ,FILE_SHARE_READ,
        nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    const DWORD error=file==INVALID_HANDLE_VALUE?GetLastError():ERROR_SUCCESS;
    if(selected) *selected=resolved;
    if(!resolved.relativePath.empty())
    {
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 26110 26117)
#endif
        std::lock_guard<std::mutex> lock(Detail::mutex);
        auto& history=Detail::recent;
        for (auto it = history.begin(); it != history.end(); )
        {
            if (Detail::Equal(it->resolution.logicalPath, resolved.logicalPath))
                it = history.erase(it);
            else
                ++it;
        }
        if(history.size()>=128) history.erase(history.begin());
        history.push_back({resolved,error});
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
    }
    if(file==INVALID_HANDLE_VALUE) SetLastError(error);
    return file;
}
inline std::vector<OpenRecord> RecentOpens()
{ std::lock_guard<std::mutex> lock(Detail::mutex); return Detail::recent; }

inline bool ReadBytes(const char* path, std::vector<unsigned char>& bytes,
                      size_t maximum=512u*1024u*1024u, Resolution* selected=nullptr)
{
    bytes.clear();
    struct File { HANDLE handle; ~File(){if(handle!=INVALID_HANDLE_VALUE) CloseHandle(handle);} } file{OpenRead(path,selected)};
    if(file.handle==INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size={};
    if(!GetFileSizeEx(file.handle,&size) || size.QuadPart<=0 ||
        static_cast<unsigned long long>(size.QuadPart)>maximum || size.QuadPart>MAXDWORD) return false;
    bytes.resize(static_cast<size_t>(size.QuadPart));
    DWORD count=0;
    if(!ReadFile(file.handle,bytes.data(),static_cast<DWORD>(bytes.size()),&count,nullptr) || count!=bytes.size())
    { bytes.clear(); return false; }
    return true;
}
}
