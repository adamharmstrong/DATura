#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
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
public:
    explicit NewCopy(const std::wstring& path) {
        if(!CopyPathAllowed(path)) return;
        HANDLE h=CreateFileW(path.c_str(),GENERIC_WRITE|DELETE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(h!=INVALID_HANDLE_VALUE) handle=std::make_unique<Handle>(h);
    }
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
    bool Commit() {
        if(!handle || !FlushFileBuffers(handle->value)) return false;
        committed=true; return true;
    }
};
class Document {
    std::unique_ptr<Handle> source;
    std::map<uint64_t,unsigned char> patches;
    struct Change {uint64_t offset; Bytes before,after;};
    using Transaction=std::vector<Change>;
    std::vector<Transaction> undo,redo;
    uint64_t revision=0;
    bool Original(uint64_t offset,size_t count,Bytes& result) const {
        if(!source || offset>size || count>size-offset) return false;
        result.resize(count); LARGE_INTEGER position; position.QuadPart=offset;
        if(!SetFilePointerEx(source->value,position,nullptr,FILE_BEGIN)) return false;
        size_t read=0;
        while(read<count) {
            DWORD part=static_cast<DWORD>((std::min)(count-read,size_t(65536))),actual=0;
            if(!ReadFile(source->value,result.data()+read,part,&actual,nullptr)||actual!=part) return false;
            read+=actual;
        }
        return true;
    }
    bool Apply(const Transaction& changes,bool forward) {
        auto staged=patches;Bytes original;
        for(const auto& change:changes) {
            const auto& bytes=forward?change.after:change.before;
            if(!Original(change.offset,bytes.size(),original))return false;
            for(size_t i=0;i<bytes.size();++i){if(bytes[i]==original[i])staged.erase(change.offset+i);else staged[change.offset+i]=bytes[i];}
        }
        patches.swap(staged);++revision;return true;
    }
public:
    uint64_t size=0;
    std::wstring path;
    bool Open(const std::wstring& name) {
        auto next=std::make_unique<Handle>(CreateFileW(name.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
        LARGE_INTEGER length;
        if(next->value==INVALID_HANDLE_VALUE || !GetFileSizeEx(next->value,&length)) return false;
        ++revision;source=std::move(next); size=length.QuadPart; path=name; patches.clear(); undo.clear(); redo.clear(); return true;
    }
    bool Loaded() const {return bool(source);}
    bool Dirty() const {return !patches.empty();}
    bool Modified(uint64_t offset) const {return patches.find(offset)!=patches.end();}
    std::optional<uint64_t> ModifiedOffset(uint64_t current,bool previous=false) const {
        if(patches.empty())return {};
        if(previous){auto it=patches.lower_bound(current);if(it==patches.begin())return patches.rbegin()->first;return std::prev(it)->first;}
        auto it=patches.upper_bound(current);return (it==patches.end()?patches.begin():it)->first;
    }
    bool CanUndo() const {return !undo.empty();}
    bool CanRedo() const {return !redo.empty();}
    bool Read(uint64_t offset,size_t count,Bytes& result) const {
        if(!Original(offset,count,result)) return false;
        for(auto it=patches.lower_bound(offset);it!=patches.end() && it->first-offset<count;++it) result[static_cast<size_t>(it->first-offset)]=it->second;
        return true;
    }
    bool Overwrite(uint64_t offset,const Bytes& bytes) {
        Change change{offset,{},bytes};
        if(bytes.empty()||!Read(offset,bytes.size(),change.before))return false;
        if(change.before==bytes)return true;
        Transaction transaction;transaction.push_back(std::move(change));undo.reserve(undo.size()+1);
        if(!Apply(transaction,true))return false;
        undo.push_back(std::move(transaction));redo.clear();return true;
    }
    bool Undo() {
        if(undo.empty())return false;redo.reserve(redo.size()+1);
        if(!Apply(undo.back(),false))return false;
        redo.push_back(std::move(undo.back()));undo.pop_back();return true;
    }
    bool Redo() {
        if(redo.empty())return false;undo.reserve(undo.size()+1);
        if(!Apply(redo.back(),true))return false;
        undo.push_back(std::move(redo.back()));redo.pop_back();return true;
    }
    struct ReplacementPlan {
        const Document* owner=nullptr;uint64_t revision=0,count=0;
        std::map<uint64_t,unsigned char> patches;
        Transaction changes;
    };
    // Stage a complete non-overlapping replacement without mutating the document.
    // Cap history and sparse-map growth: exceeding either limit rejects the entire operation.
    template<class Cancel,class Progress>
    std::optional<ReplacementPlan> PrepareReplaceAll(const Bytes& pattern,const Bytes& replacement,Cancel cancel,Progress progress,bool& failed,bool& limit,uint64_t start=0,uint64_t length=UINT64_MAX) const {
        failed=limit=false;
        if(!Loaded()||pattern.empty()||pattern.size()!=replacement.size()||start>size||(length!=UINT64_MAX&&length>size-start)){failed=true;return {};}
        ReplacementPlan plan;plan.owner=this;plan.revision=revision;
        if(pattern==replacement)return plan;
        std::vector<uint64_t> offsets;uint64_t next=0;
        bool complete=ScanMatches(pattern,cancel,[&](uint64_t at){
            if(at<next)return true;
            if(offsets.size()>=100000||offsets.size()>=1048576/pattern.size()){limit=true;return false;}
            offsets.push_back(at);next=at+pattern.size();return true;
        },progress,failed,start,length);
        if(!complete||failed||limit||cancel())return {};
        plan.patches=patches;plan.changes.reserve(offsets.size());Bytes original;
        for(auto at:offsets) {
            if(cancel())return {};
            Change change{at,{},replacement};
            if(!Read(at,pattern.size(),change.before)||change.before!=pattern||!Original(at,pattern.size(),original)){failed=true;return {};}
            for(size_t i=0;i<replacement.size();++i){if(replacement[i]==original[i])plan.patches.erase(at+i);else plan.patches[at+i]=replacement[i];}
            plan.changes.push_back(std::move(change));
        }
        if(cancel())return {};
        plan.count=offsets.size();return plan;
    }
    bool CommitReplacement(ReplacementPlan&& plan) {
        if(plan.owner!=this||plan.revision!=revision)return false;
        if(plan.changes.empty())return true;
        undo.reserve(undo.size()+1);
        undo.push_back(std::move(plan.changes));patches.swap(plan.patches);redo.clear();++revision;return true;
    }
    bool SaveRangeCopy(const std::wstring& name,uint64_t start,uint64_t length) const {
        if(!Loaded() || start>size || length>size-start) return false;
        NewCopy output(name);
        if(!output.Valid()) return false;
        Bytes bytes;
        for(uint64_t offset=0;offset<length;) {
            size_t count=static_cast<size_t>((std::min)(uint64_t(65536),length-offset));
            if(!Read(start+offset,count,bytes)||!output.Write(bytes.data(),bytes.size())) return false;
            offset+=count;
        }
        return output.Commit();
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
