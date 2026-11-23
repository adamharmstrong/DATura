#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <cstring>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace DatHex {
using Bytes = std::vector<unsigned char>;
inline bool ParseNumber(const std::wstring& text, uint64_t& value, int base=16) {
    auto first=text.find_first_not_of(L" \t\r\n");
    if(first==std::wstring::npos || text[first]==L'-' || text[first]==L'+') return false;
    auto last=text.find_last_not_of(L" \t\r\n");
    std::wstring input=text.substr(first,last-first+1);
    if(input.size()>2 && input[0]==L'0' && (input[1]==L'x'||input[1]==L'X')) { base=16; input.erase(0,2); }
    if(input.empty()) return false;
    value=0;
    for(wchar_t c:input) {
        int digit=c>=L'0'&&c<=L'9'? c-L'0' : c>=L'a'&&c<=L'f'?c-L'a'+10:c>=L'A'&&c<=L'F'?c-L'A'+10:-1;
        if(digit<0 || digit>=base || value>(UINT64_MAX-digit)/base) return false;
        value=value*base+digit;
    }
    return true;
}
inline bool ParseHex(const std::wstring& text, Bytes& bytes) {
    bytes.clear(); int high=-1;
    for(wchar_t c:text) {
        if(c==L' '||c==L'\t'||c==L'\r'||c==L'\n') continue;
        int digit=c>=L'0'&&c<=L'9'?c-L'0':c>=L'a'&&c<=L'f'?c-L'a'+10:c>=L'A'&&c<=L'F'?c-L'A'+10:-1;
        if(digit<0) return false;
        if(high<0) high=digit; else {bytes.push_back(static_cast<unsigned char>(high*16+digit)); high=-1;}
    }
    return high<0 && !bytes.empty();
}
// Unsigned offsets are absolute; a leading + or - is relative to the current byte.
inline bool ResolveOffset(const std::wstring& text,uint64_t current,uint64_t size,int base,uint64_t& result) {
    auto first=text.find_first_not_of(L" \t\r\n");if(first==std::wstring::npos||current>size)return false;
    wchar_t sign=text[first];bool relative=sign==L'+'||sign==L'-';uint64_t magnitude=0;
    if(!ParseNumber(text.substr(first+(relative?1:0)),magnitude,base))return false;
    if(!relative)result=magnitude;
    else if(sign==L'-'){if(magnitude>current)return false;result=current-magnitude;}
    else {if(magnitude>size-current)return false;result=current+magnitude;}
    return result<=size;
}
struct Handle {
    HANDLE value=INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE h):value(h) {}
    ~Handle() {if(value!=INVALID_HANDLE_VALUE) CloseHandle(value);}
    Handle(const Handle&)=delete;
    Handle& operator=(const Handle&)=delete;
};
inline bool CopyPathAllowed(const std::wstring& path) {
    if(path.empty() || path.find(L'\0')!=std::wstring::npos) return false;
    std::error_code error;
    auto full=std::filesystem::absolute(std::filesystem::path(path),error);
    if(error) return false;
    auto name=full.wstring();
    std::replace(name.begin(),name.end(),L'/',L'\\');
    if(name.rfind(L"\\\\?\\",0)==0 || name.rfind(L"\\\\.\\",0)==0) return false;
    // A new alternate data stream still modifies an existing DAT. Allow only
    // the colon in a normal drive prefix, never ADS or device namespace paths.
    for(size_t i=0;i<name.size();++i) if(name[i]==L':' &&
        !(i==1 && ((name[0]>=L'A'&&name[0]<=L'Z')||(name[0]>=L'a'&&name[0]<=L'z')))) return false;
    for(const auto& part:full.relative_path()) {
        std::wstring component=part.wstring();
        component=component.substr(0,component.find(L'.'));
        while(!component.empty() && component.back()==L' ') component.pop_back();
        for(auto& c:component) if(c>=L'a'&&c<=L'z') c-=L'a'-L'A';
        if(component==L"CON"||component==L"PRN"||component==L"AUX"||component==L"NUL"||component==L"CONIN$"||component==L"CONOUT$") return false;
        if(component.size()==4 && (component.rfind(L"COM",0)==0||component.rfind(L"LPT",0)==0) &&
            ((component[3]>=L'1'&&component[3]<=L'9')||component[3]==L'\x00b9'||component[3]==L'\x00b2'||component[3]==L'\x00b3')) return false;
    }
    return true;
}
// CREATE_NEW is the non-negotiable save boundary: no existing file is ever
// opened for writing, even when the name is an alias or another process wins
// a race with the file picker. Failed outputs are removed using the owned handle.
class NewCopy {
    std::unique_ptr<Handle> handle;
    bool committed=false;
    struct CanonicalPath {};
    NewCopy(const std::wstring& path,CanonicalPath) {
        if(path.empty())return;
        HANDLE h=CreateFileW(path.c_str(),GENERIC_WRITE|DELETE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(h!=INVALID_HANDLE_VALUE) handle=std::make_unique<Handle>(h);
    }
public:
    explicit NewCopy(const std::wstring& path):NewCopy(CopyPathAllowed(path)?path:L"",CanonicalPath{}) {}
    ~NewCopy() {
        if(handle && !committed) {
            FILE_DISPOSITION_INFO info={TRUE};
            SetFileInformationByHandle(handle->value,FileDispositionInfo,&info,sizeof(info));
        }
    }
    // Used by a multi-file export to roll back only files it still owns.
    void Discard() {committed=false;}
    bool Valid() const {return bool(handle);}
    bool Write(const void* data, size_t length) {
        if(!handle) return false;
        auto p=static_cast<const unsigned char*>(data);
        while(length) {
            DWORD count=static_cast<DWORD>((std::min)(length,size_t(65536))),written=0;
            if(!WriteFile(handle->value,p,count,&written,nullptr) || written!=count) return false;
            p+=count; length-=count;
        }
        return true;
    }
private:
    friend class Document;
    bool Replace(const std::wstring& destination) {
        if(!handle)return false;
        const auto length=destination.size()*sizeof(wchar_t);
        const auto total=sizeof(FILE_RENAME_INFO)+length;
        std::vector<unsigned char> storage(total);
        auto info=reinterpret_cast<FILE_RENAME_INFO*>(storage.data());
        // POSIX replacement leaves the old read handle available to undo history.
        info->Flags=FILE_RENAME_FLAG_REPLACE_IF_EXISTS|FILE_RENAME_FLAG_POSIX_SEMANTICS;
        info->FileNameLength=static_cast<DWORD>(length);
        std::memcpy(info->FileName,destination.c_str(),length+sizeof(wchar_t));
        if(!SetFileInformationByHandle(handle->value,FileRenameInfoEx,info,static_cast<DWORD>(total)))return false;
        committed=true;return true;
    }
    bool Identity(BY_HANDLE_FILE_INFORMATION& identity) const {
        return handle&&GetFileInformationByHandle(handle->value,&identity);
    }
public:
    bool Commit() {
        if(!handle || !FlushFileBuffers(handle->value)) return false;
        committed=true; return true;
    }
};
inline std::wstring FinalLocalPath(HANDLE handle) {
    constexpr DWORD flags=FILE_NAME_NORMALIZED|VOLUME_NAME_GUID;
    DWORD count=GetFinalPathNameByHandleW(handle,nullptr,0,flags);
    if(!count)return {};
    std::wstring path(count,L'\0');
    DWORD written=GetFinalPathNameByHandleW(handle,path.data(),count,flags);
    if(!written||written>=count)return {};
    path.resize(written);
    if(path.rfind(L"\\\\?\\Volume{",0)!=0)return {};
    while(path.back()==L'\\')path.pop_back();
    return path;
}
inline bool Within(const std::wstring& path,const std::wstring& directory) {
    return path.size()>=directory.size() &&
        CompareStringOrdinal(path.data(),static_cast<int>(directory.size()),directory.data(),
            static_cast<int>(directory.size()),TRUE)==CSTR_EQUAL &&
        (path.size()==directory.size()||path[directory.size()]==L'\\');
}
inline bool SameFile(const BY_HANDLE_FILE_INFORMATION& left,const BY_HANDLE_FILE_INFORMATION& right) {
    return left.dwVolumeSerialNumber==right.dwVolumeSerialNumber&&
        left.nFileIndexHigh==right.nFileIndexHigh&&left.nFileIndexLow==right.nFileIndexLow;
}
inline bool ReadInstallRoots(std::vector<std::wstring>& roots) {
    for(auto view:{KEY_WOW64_32KEY,KEY_WOW64_64KEY}) {
        for(auto key:{L"SOFTWARE\\PlayOnlineUS\\InstallFolder",L"SOFTWARE\\PlayOnlineEU\\InstallFolder",
                      L"SOFTWARE\\PlayOnline\\InstallFolder"}) {
            wchar_t value[32768];DWORD length=sizeof(value);
            auto status=RegGetValueW(HKEY_LOCAL_MACHINE,key,L"0001",RRF_RT_REG_SZ|RRF_RT_REG_EXPAND_SZ|
                (view==KEY_WOW64_32KEY?RRF_SUBKEY_WOW6432KEY:RRF_SUBKEY_WOW6464KEY),nullptr,value,&length);
            if(status==ERROR_FILE_NOT_FOUND||status==ERROR_PATH_NOT_FOUND)continue;
            if(status!=ERROR_SUCCESS)return false;
            if(*value)roots.emplace_back(value);
        }
    }
    return true;
}
class Document {
    std::unique_ptr<Handle> source;
    BY_HANDLE_FILE_INFORMATION saveIdentity{};
    std::wstring openedPath;
    std::vector<std::wstring> openRoots;
    bool protectedAtOpen=false,memoryDocument=false;

    static bool OutsideInstall(const std::wstring& destination,const std::wstring& name) {
        Handle directory(CreateFileW(name.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,
            nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr));
        if(directory.value==INVALID_HANDLE_VALUE) {
            auto error=GetLastError();
            return error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND;
        }
        auto resolved=FinalLocalPath(directory.value);
        if(resolved.empty()||Within(destination,resolved))return false;

        // ROM volumes can be relocated with a junction. Protect the resolved
        // backing directory even when the user opens it directly.
        WIN32_FIND_DATAW info{};
        HANDLE search=FindFirstFileW((name+L"\\ROM*").c_str(),&info);
        if(search==INVALID_HANDLE_VALUE)return GetLastError()==ERROR_FILE_NOT_FOUND;
        bool outside=true;
        do {
            const DWORD redirected=FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT;
            if((info.dwFileAttributes&redirected)!=redirected)continue;
            const auto child=name+L"\\"+info.cFileName;
            Handle rom(CreateFileW(child.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,
                nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr));
            if(rom.value==INVALID_HANDLE_VALUE){outside=false;break;}
            auto target=FinalLocalPath(rom.value);
            if(target.empty()||Within(destination,target)){outside=false;break;}
        } while(FindNextFileW(search,&info));
        if(outside)outside=GetLastError()==ERROR_NO_MORE_FILES;
        FindClose(search);
        return outside;
    }

    // Resolve the target from its handle, then pin its canonical directory chain.
    // Refuse ambiguous destinations, including multiply linked files and streams.
    std::wstring WorkingDestination(const std::vector<std::wstring>& configuredRoots,
                                    std::vector<std::unique_ptr<Handle>>& directories,
                                    std::unique_ptr<Handle>& target) const {
        if(!Loaded()||protectedAtOpen||!CopyPathAllowed(openedPath))return {};
        auto candidate=std::make_unique<Handle>(CreateFileW(openedPath.c_str(),FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
        BY_HANDLE_FILE_INFORMATION identity{};
        if(candidate->value==INVALID_HANDLE_VALUE||!GetFileInformationByHandle(candidate->value,&identity)||
           !SameFile(identity,saveIdentity)||identity.nNumberOfLinks!=1||
           (identity.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))return {};
        auto destination=FinalLocalPath(candidate->value);
        if(destination.empty())return {};
        auto volumeEnd=destination.find(L'}');
        if(volumeEnd==std::wstring::npos)return {};
        auto roots=openRoots;
        roots.insert(roots.end(),configuredRoots.begin(),configuredRoots.end());
        roots.emplace_back(L"C:\\Program Files (x86)\\PlayOnline\\SquareEnix\\FINAL FANTASY XI");
        if(!ReadInstallRoots(roots))return {};
        for(const auto& root:roots) {
            if(root.empty())continue;
            std::error_code errorCode;
            auto name=std::filesystem::absolute(root,errorCode).lexically_normal().make_preferred().wstring();
            if(errorCode)return {};
            while(name.size()>3&&(name.back()==L'\\'||name.back()==L'/'))name.pop_back();
            if(Within(std::filesystem::path(openedPath).lexically_normal().make_preferred().wstring(),name)||
               !OutsideInstall(destination,name))return {};
        }
        auto slash=destination.find(L'\\',volumeEnd);
        while(slash!=std::wstring::npos) {
            auto parent=destination.substr(0,slash==volumeEnd+1?slash+1:slash);
            auto directory=std::make_unique<Handle>(CreateFileW(parent.c_str(),FILE_LIST_DIRECTORY|FILE_READ_ATTRIBUTES,
                FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
            BY_HANDLE_FILE_INFORMATION info{};
            if(directory->value==INVALID_HANDLE_VALUE||!GetFileInformationByHandle(directory->value,&info)||
               !(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)||
               !Within(destination,FinalLocalPath(directory->value)))return {};
            if(parent.back()!=L'\\')parent+=L'\\';
            // Recognize an installation tree even if it is absent from the registry.
            for(auto marker:{L"FTABLE.DAT",L"VTABLE.DAT"}) {
                if(GetFileAttributesW((parent+marker).c_str())!=INVALID_FILE_ATTRIBUTES)return {};
                auto error=GetLastError();
                if(error!=ERROR_FILE_NOT_FOUND&&error!=ERROR_PATH_NOT_FOUND)return {};
            }
            directories.push_back(std::move(directory));
            slash=destination.find(L'\\',slash+1);
        }
        auto pinned=std::make_unique<Handle>(CreateFileW(destination.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,
            nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
        if(pinned->value==INVALID_HANDLE_VALUE||!GetFileInformationByHandle(pinned->value,&identity)||
           !SameFile(identity,saveIdentity)||identity.nNumberOfLinks!=1||
           (identity.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))return {};
        target=std::move(pinned);
        return destination;
    }
    struct Piece {
        // A null buffer refers to an unchanged span in the open source file.
        std::shared_ptr<const Bytes> bytes;
        uint64_t start,length,end=0;
    };
    struct Version {
        std::vector<Piece> pieces;
        uint64_t size=0,id=0;
    };
    Version current;
    // Each change records the untouched gap before it. The same gap works
    // in both directions even when earlier changes have shifted offsets.
    struct Change {
        uint64_t gap=0,beforeLength=0,afterLength=0;
        size_t firstRemoved=0,removedCount=0;
    };
    struct Edit {
        std::vector<Change> changes;
        std::vector<Piece> removed;
        std::shared_ptr<const Bytes> inserted;
        uint64_t beforeId=0,afterId=0,beforeSize=0,afterSize=0;
    };
    std::vector<Edit> undo,redo;
    uint64_t revision=0,saved=0;
    static uint64_t PieceOffset(const Piece& piece,uint64_t within) {
        const auto position=piece.start+within;
        return piece.bytes?position%piece.bytes->size():position;
    }
    static void Append(std::vector<Piece>& pieces,const Piece& piece,size_t first=0) {
        if(pieces.size()>first&&pieces.back().bytes==piece.bytes&&
           PieceOffset(pieces.back(),pieces.back().length)==piece.start) {
            pieces.back().length+=piece.length;
            pieces.back().end+=piece.length;
        } else if(piece.length) {
            const auto end=pieces.empty()?0:pieces.back().end;
            pieces.push_back({piece.bytes,piece.start,piece.length,end+piece.length});
        }
    }
    // A cursor consumes spans in ascending order, so replace-all walks the
    // piece list once even when it has thousands of matches.
    struct Cursor {
        const std::vector<Piece>& pieces;
        size_t index=0;
        uint64_t within=0;
        void Advance(uint64_t count,std::vector<Piece>* output=nullptr,size_t first=0) {
            while(count) {
                const auto& piece=pieces[index];
                auto take=(std::min)(count,piece.length-within);
                if(output) Append(*output,{piece.bytes,PieceOffset(piece,within),take},first);
                within+=take;
                if(within==piece.length){++index;within=0;}
                count-=take;
            }
        }
    };
    Cursor At(uint64_t offset) const {
        auto it=std::upper_bound(current.pieces.begin(),current.pieces.end(),offset,
            [](uint64_t value,const Piece& piece){return value<piece.end;});
        const size_t index=static_cast<size_t>(it-current.pieces.begin());
        const uint64_t start=index?current.pieces[index-1].end:0;
        return {current.pieces,index,offset-start};
    }
    bool ReadOriginal(uint64_t offset,size_t count,unsigned char* result) const {
        LARGE_INTEGER position;position.QuadPart=offset;
        if(!SetFilePointerEx(source->value,position,nullptr,FILE_BEGIN))return false;
        while(count) {
            DWORD part=static_cast<DWORD>((std::min)(count,size_t(65536))),actual=0;
            if(!ReadFile(source->value,result,part,&actual,nullptr)||actual!=part)return false;
            result+=actual;count-=actual;
        }
        return true;
    }
    static void Record(Edit& edit,Cursor& cursor,uint64_t gap,uint64_t removed,uint64_t inserted) {
        if(gap||edit.changes.empty())edit.changes.push_back({gap,0,0,edit.removed.size(),0});
        auto& change=edit.changes.back();
        cursor.Advance(removed,&edit.removed,change.firstRemoved);
        change.beforeLength+=removed;
        change.afterLength+=inserted;
        change.removedCount=edit.removed.size()-change.firstRemoved;
    }
    void Apply(Version&& next,Edit&& edit) {
        edit.beforeId=current.id;edit.afterId=revision+1;
        edit.beforeSize=current.size;edit.afterSize=next.size;
        undo.push_back(std::move(edit));
        next.id=++revision;
        current=std::move(next);size=current.size;redo.clear();
    }
    bool Restore(std::vector<Edit>& from,std::vector<Edit>& to,bool forward) {
        if(from.empty())return false;
        const auto& edit=from.back();
        Version next;
        next.size=forward?edit.afterSize:edit.beforeSize;
        next.id=forward?edit.afterId:edit.beforeId;
        Cursor cursor{current.pieces};
        uint64_t remaining=size;
        for(const auto& change:edit.changes) {
            cursor.Advance(change.gap,&next.pieces);
            const auto removed=forward?change.beforeLength:change.afterLength;
            cursor.Advance(removed);remaining-=change.gap+removed;
            if(forward) {
                if(change.afterLength)Append(next.pieces,{edit.inserted,0,change.afterLength});
            } else {
                for(size_t i=0;i<change.removedCount;++i)
                    Append(next.pieces,edit.removed[change.firstRemoved+i]);
            }
        }
        cursor.Advance(remaining,&next.pieces);
        to.push_back(std::move(from.back()));from.pop_back();
        current=std::move(next);size=current.size;++revision;return true;
    }
public:
    uint64_t size=0;
    std::wstring path;
    bool Open(const std::wstring& name,const std::vector<std::wstring>& roots={}) {
        auto next=std::make_unique<Handle>(CreateFileW(name.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
        LARGE_INTEGER length;
        if(next->value==INVALID_HANDLE_VALUE || GetFileType(next->value)!=FILE_TYPE_DISK ||
           !GetFileSizeEx(next->value,&length)||!GetFileInformationByHandle(next->value,&saveIdentity))return false;
        Version original;original.size=length.QuadPart;original.id=++revision;
        Append(original.pieces,{nullptr,0,original.size});
        source=std::move(next);memoryDocument=false;path=name;size=original.size;saved=original.id;
        current=std::move(original);undo.clear();redo.clear();
        std::error_code error;
        openedPath=std::filesystem::absolute(name,error).wstring();
        if(error||!CopyPathAllowed(name))openedPath.clear();
        protectedAtOpen=false;openRoots=roots;
        // Readers used only for analysis/export do not need an installation probe.
        if(!roots.empty())protectedAtOpen=!CanSave({});
        return true;
    }
    void New() {
        source.reset();memoryDocument=true;protectedAtOpen=false;
        openedPath.clear();openRoots.clear();path.clear();
        current=Version{};current.id=++revision;saved=current.id;size=0;
        undo.clear();redo.clear();
    }
    bool Loaded() const {return source||memoryDocument;}
    bool Untitled() const {return memoryDocument&&path.empty();}
    bool Dirty() const {return current.id!=saved;}
    bool Modified(uint64_t offset) const {
        auto cursor=At(offset);
        return cursor.index<current.pieces.size()&&bool(current.pieces[cursor.index].bytes);
    }
    std::optional<uint64_t> ModifiedOffset(uint64_t currentOffset,bool previous=false) const {
        uint64_t start=0;
        std::optional<uint64_t> first,last,prior;
        for(const auto& piece:current.pieces) {
            const uint64_t end=start+piece.length;
            if(piece.bytes) {
                if(!first)first=start;
                last=end-1;
                if(!previous&&currentOffset<end-1)return currentOffset<start?start:currentOffset+1;
                if(previous&&currentOffset>start)prior=(std::min)(currentOffset-1,end-1);
            }
            start=end;
        }
        return previous?(prior?prior:last):first;
    }
    bool CanUndo() const {return !undo.empty();}
    bool CanRedo() const {return !redo.empty();}
    bool Read(uint64_t offset,size_t count,Bytes& result) const {
        if(!Loaded()||offset>size||count>size-offset)return false;
        result.resize(count);
        auto cursor=At(offset);
        size_t read=0;
        while(read<count) {
            const auto& piece=cursor.pieces[cursor.index];
            auto take=static_cast<size_t>((std::min)(uint64_t(count-read),piece.length-cursor.within));
            uint64_t start=PieceOffset(piece,cursor.within);
            if(piece.bytes) {
                const auto period=piece.bytes->size();
                const auto tail=(std::min)(take,period-static_cast<size_t>(start));
                std::copy_n(piece.bytes->data()+start,tail,result.data()+read);
                const auto head=(std::min)(take-tail,static_cast<size_t>(start));
                std::copy_n(piece.bytes->data(),head,result.data()+read+tail);
                for(size_t copied=tail+head;copied<take;) {
                    const auto repeat=(std::min)(copied,take-copied);
                    std::copy_n(result.data()+read,repeat,result.data()+read+copied);
                    copied+=repeat;
                }
            }
            else if(!ReadOriginal(start,take,result.data()+read))return false;
            cursor.Advance(take);read+=take;
        }
        return true;
    }
    bool Splice(uint64_t offset,uint64_t removed,const Bytes& bytes) {
        if(!Loaded()||offset>size||removed>size-offset||bytes.size()>INT64_MAX-(size-removed))return false;
        if(removed==bytes.size()) {
            Bytes before;
            if(!Read(offset,bytes.size(),before))return false;
            if(before==bytes)return true;
        }
        Version next;next.size=size-removed+bytes.size();
        Cursor cursor{current.pieces};cursor.Advance(offset,&next.pieces);
        Edit edit;
        if(!bytes.empty())edit.inserted=std::make_shared<const Bytes>(bytes);
        Record(edit,cursor,offset,removed,bytes.size());
        if(edit.inserted)Append(next.pieces,{edit.inserted,0,bytes.size()});
        cursor.Advance(size-offset-removed,&next.pieces);
        Apply(std::move(next),std::move(edit));return true;
    }
    bool Overwrite(uint64_t offset,const Bytes& bytes) {
        return !bytes.empty()&&Splice(offset,bytes.size(),bytes);
    }
    bool Undo() {return Restore(undo,redo,false);}
    bool Redo() {return Restore(redo,undo,true);}
    class ReplacementPlan {
        friend class Document;
        const Document* owner=nullptr;
        uint64_t revision=0;
        Version next;
        Edit edit;
    public:
        uint64_t count=0;
    };
    template<class Cancel,class Progress>
    std::optional<ReplacementPlan> PrepareReplaceAll(const Bytes& pattern,const Bytes& replacement,Cancel cancel,Progress progress,bool& failed,bool& limit,uint64_t start=0,uint64_t length=UINT64_MAX) const {
        failed=limit=false;
        if(!Loaded()||pattern.empty()||start>size||(length!=UINT64_MAX&&length>size-start)){failed=true;return {};}
        ReplacementPlan plan;plan.owner=this;plan.revision=revision;plan.next.size=size;
        if(pattern==replacement)return plan;
        Cursor cursor{current.pieces};uint64_t next=0;
        if(!replacement.empty())plan.edit.inserted=std::make_shared<const Bytes>(replacement);
        bool complete=ScanMatches(pattern,cancel,[&](uint64_t at){
            if(at<next)return true;
            if(replacement.size()>INT64_MAX-(plan.next.size-pattern.size())){
                limit=true;return false;
            }
            cursor.Advance(at-next,&plan.next.pieces);
            Record(plan.edit,cursor,at-next,pattern.size(),replacement.size());
            if(plan.edit.inserted)Append(plan.next.pieces,{plan.edit.inserted,0,replacement.size()});
            next=at+pattern.size();++plan.count;
            plan.next.size=plan.next.size-pattern.size()+replacement.size();
            return true;
        },progress,failed,start,length);
        if(!complete||failed||limit||cancel())return {};
        cursor.Advance(size-next,&plan.next.pieces);return plan;
    }
    bool CommitReplacement(ReplacementPlan&& plan) {
        if(plan.owner!=this||plan.revision!=revision)return false;
        if(plan.count)Apply(std::move(plan.next),std::move(plan.edit));
        return true;
    }
    bool CanSave(const std::vector<std::wstring>& roots) const {
        std::vector<std::unique_ptr<Handle>> directories;
        std::unique_ptr<Handle> target;
        return !WorkingDestination(roots,directories,target).empty();
    }
    enum class SaveMode { Working, Copy, Untitled };
    class PendingSave {
        friend class Document;
        const Document* owner=nullptr;
        uint64_t revision=0;
        SaveMode mode=SaveMode::Copy;
        std::wstring destination;
        std::vector<std::unique_ptr<Handle>> directories;
        std::unique_ptr<Handle> target;
        std::unique_ptr<NewCopy> output;
    };
    template<class Cancel,class Progress>
    std::unique_ptr<PendingSave> PrepareSave(SaveMode mode,const std::wstring& name,
        const std::vector<std::wstring>& roots,Cancel cancel,Progress progress) const {
        if(!Loaded()||cancel()||(mode==SaveMode::Untitled&&!Untitled()))return {};
        auto pending=std::make_unique<PendingSave>();
        pending->owner=this;pending->revision=revision;pending->mode=mode;
        if(mode==SaveMode::Working) {
            pending->destination=WorkingDestination(roots,pending->directories,pending->target);
            if(pending->destination.empty())return {};
            const auto parent=pending->destination.substr(0,pending->destination.find_last_of(L'\\')+1);
            static std::atomic<uint64_t> serial{0};
            auto temp=parent+L".datura-save-"+std::to_wstring(GetCurrentProcessId())+L"-"+
                std::to_wstring(GetTickCount64())+L"-"+std::to_wstring(++serial)+L".tmp";
            pending->output.reset(new NewCopy(temp,NewCopy::CanonicalPath{}));
        } else {
            if(!CopyPathAllowed(name))return {};
            std::error_code error;
            pending->destination=std::filesystem::absolute(name,error).wstring();
            if(error)return {};
            pending->output=std::make_unique<NewCopy>(pending->destination);
        }
        auto& output=*pending->output;
        if(!output.Valid()||!WriteRange(output,0,size,cancel,progress)||
           !FlushFileBuffers(output.handle->value)||cancel())return {};
        return pending;
    }
    bool CommitSave(std::unique_ptr<PendingSave> pending,const std::vector<std::wstring>& roots) {
        if(!pending||pending->owner!=this||pending->revision!=revision)return false;
        auto& output=*pending->output;
        if(pending->mode==SaveMode::Copy){output.committed=true;return true;}
        BY_HANDLE_FILE_INFORMATION identity{};
        if(!output.Identity(identity))return false;
        if(pending->mode==SaveMode::Working) {
            if(WorkingDestination(roots,pending->directories,pending->target)!=pending->destination||
               !output.Replace(pending->destination))return false;
        } else {
            auto nameForDisplay=pending->destination;
            auto protectedRoots=roots;
            output.committed=true;pending->output.reset();
            openedPath=std::move(pending->destination);path=std::move(nameForDisplay);
            openRoots=std::move(protectedRoots);
        }
        saveIdentity=identity;saved=current.id;++revision;
        if(pending->mode==SaveMode::Untitled)protectedAtOpen=!CanSave({});
        return true;
    }
    bool SaveWorkingCopy(const std::vector<std::wstring>& roots) {
        return CommitSave(PrepareSave(SaveMode::Working,{},roots,[]{return false;},[](uint64_t){}),roots);
    }
    bool SaveUntitled(const std::wstring& name,const std::vector<std::wstring>& roots) {
        return CommitSave(PrepareSave(SaveMode::Untitled,name,roots,[]{return false;},[](uint64_t){}),roots);
    }
private:
    template<class Cancel,class Progress>
    bool WriteRange(NewCopy& output,uint64_t start,uint64_t length,Cancel cancel,Progress progress) const {
        Bytes bytes;
        for(uint64_t offset=0;offset<length;) {
            if(cancel())return false;
            size_t count=static_cast<size_t>((std::min)(uint64_t(65536),length-offset));
            if(!Read(start+offset,count,bytes)||!output.Write(bytes.data(),bytes.size()))return false;
            offset+=count;progress(offset);
        }
        return !cancel();
    }
public:
    bool SaveRangeCopy(const std::wstring& name,uint64_t start,uint64_t length) const {
        if(!Loaded() || start>size || length>size-start) return false;
        NewCopy output(name);
        return output.Valid()&&WriteRange(output,start,length,[]{return false;},[](uint64_t){})&&output.Commit();
    }
    bool SaveCopy(const std::wstring& name) const {return SaveRangeCopy(name,0,size);}
    // Scan every overlapping match with bounded memory; callbacks may stop early.
    template<class Cancel,class Match,class Progress>
    bool ScanMatches(const Bytes& pattern,Cancel cancel,Match match,Progress progress,bool& failed,uint64_t start=0,uint64_t length=UINT64_MAX) const {
        failed=false;
        if(start>size||(length!=UINT64_MAX&&length>size-start)){failed=true;return false;}
        if(length==UINT64_MAX)length=size-start;
        uint64_t end=start+length;
        if(pattern.empty())return true;
        std::vector<size_t> prefix(pattern.size());
        for(size_t i=1,j=0;i<pattern.size();++i){while(j&&pattern[i]!=pattern[j])j=prefix[j-1];if(pattern[i]==pattern[j])++j;prefix[i]=j;}
        Bytes bytes;size_t matched=0;
        for(uint64_t offset=start;offset<end;) {
            if(cancel())return false;
            size_t count=static_cast<size_t>((std::min)(uint64_t(65536),end-offset));
            if(!Read(offset,count,bytes)){failed=true;return false;}
            for(size_t i=0;i<count;++i) {
                while(matched&&bytes[i]!=pattern[matched])matched=prefix[matched-1];
                if(bytes[i]==pattern[matched])++matched;
                if(matched==pattern.size()) {if(!match(offset+i+1-pattern.size()))return true;matched=prefix[matched-1];}
            }
            offset+=count;progress(offset-start);
        }
        return !cancel();
    }
    // Streaming KMP: bounded memory, including matches across read boundaries.
    template<class Cancel> std::optional<uint64_t> Find(const Bytes& pattern,uint64_t start,Cancel cancel,bool& failed,uint64_t end=UINT64_MAX) const {
        failed=false;if(end==UINT64_MAX)end=size;
        if(end>size||start>end){failed=true;return {};}
        if(pattern.empty()||start>size) return {};
        std::vector<size_t> prefix(pattern.size());
        for(size_t i=1,j=0;i<pattern.size();++i) {while(j && pattern[i]!=pattern[j]) j=prefix[j-1]; if(pattern[i]==pattern[j]) ++j; prefix[i]=j;}
        Bytes bytes; size_t matched=0;
        for(uint64_t offset=start;offset<end;) {
            if(cancel()) return {};
            size_t count=static_cast<size_t>((std::min)(uint64_t(65536),end-offset));
            if(!Read(offset,count,bytes)) {failed=true; return {};}
            for(size_t i=0;i<count;++i) {
                while(matched && bytes[i]!=pattern[matched]) matched=prefix[matched-1];
                if(bytes[i]==pattern[matched]) ++matched;
                if(matched==pattern.size()) return offset+i+1-pattern.size();
            }
            offset+=count;
        }
        return {};
    }
};
inline uint32_t Crc32(const Bytes& bytes) {
    uint32_t crc=0xffffffff;
    for(unsigned char b:bytes) {crc^=b; for(int i=0;i<8;++i) crc=(crc>>1)^((crc&1)?0xedb88320:0);}
    return crc^0xffffffff;
}
inline uint64_t UnsignedValue(const Bytes& bytes,size_t count,bool bigEndian) {
    uint64_t result=0;
    for(size_t i=0;i<count;++i) result=(result<<8)|bytes[bigEndian?i:count-i-1];
    return result;
}
}
