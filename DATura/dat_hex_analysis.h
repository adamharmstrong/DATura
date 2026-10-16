#pragma once
#include "dat_hex_document.h"
#include <bcrypt.h>
#include <array>
#include <cmath>
#include <sstream>
#include <iomanip>
#pragma comment(lib,"bcrypt.lib")

namespace DatHex {
inline uint32_t UpdateCrc32(uint32_t crc,const Bytes& bytes) {
    static const auto table=[] {
        std::array<uint32_t,256> result={};
        for(uint32_t i=0;i<256;++i) {
            uint32_t c=i;
            for(int bit=0;bit<8;++bit)c=(c>>1)^((c&1)?0xedb88320:0);
            result[i]=c;
        }
        return result;
    }();
    for(unsigned char b:bytes)crc=(crc>>8)^table[(crc^b)&255];
    return crc;
}
struct StatisticsResult {std::array<uint64_t,256> counts={};uint64_t total=0;double entropy=0;};
template<class Cancel,class Progress>
std::optional<StatisticsResult> Statistics(const Document& document,Cancel cancel,Progress progress,bool& failed,uint64_t start=0,uint64_t length=UINT64_MAX) {
    failed=false;
    if(!document.Loaded()||start>document.size){failed=true;return {};}
    if(length==UINT64_MAX)length=document.size-start;
    if(length>document.size-start){failed=true;return {};}
    StatisticsResult result;Bytes bytes;
    for(uint64_t offset=0;offset<length;) {
        if(cancel())return {};
        size_t count=static_cast<size_t>((std::min)(uint64_t(65536),length-offset));
        if(!document.Read(start+offset,count,bytes)){failed=true;return {};}
        for(auto b:bytes)++result.counts[b];offset+=count;progress(offset);
    }
    if(cancel())return {};
    result.total=length;
    if(result.total)for(auto count:result.counts)if(count){double probability=double(count)/double(result.total);result.entropy-=probability*std::log2(probability);}
    return result;
}
enum class HashAlgorithm { Crc32Sha256,Crc32,Adler32,Md5,Sha1,Sha256,Sha512,Sum8,Sum16,Sum32,Sum64,Crc16Ccitt,Crc16X25,Crc64Ecma,Crc32C,Crc32Bzip2,Crc16Arc,Crc16Modbus };
inline const wchar_t* HashName(HashAlgorithm algorithm) {
    switch(algorithm){case HashAlgorithm::Crc32Sha256:return L"CRC-32 and SHA-256";case HashAlgorithm::Crc32:return L"CRC-32";
    case HashAlgorithm::Adler32:return L"Adler-32";case HashAlgorithm::Md5:return L"MD5";case HashAlgorithm::Sha1:return L"SHA-1";
    case HashAlgorithm::Sum8:return L"Checksum-8";case HashAlgorithm::Sum16:return L"Checksum-16";case HashAlgorithm::Sum32:return L"Checksum-32";case HashAlgorithm::Sum64:return L"Checksum-64";
    case HashAlgorithm::Crc16Ccitt:return L"CRC-16/CCITT-FALSE";case HashAlgorithm::Crc16X25:return L"CRC-16/X-25";case HashAlgorithm::Crc64Ecma:return L"CRC-64/ECMA-182";
    case HashAlgorithm::Crc32C:return L"CRC-32C (Castagnoli)";case HashAlgorithm::Crc32Bzip2:return L"CRC-32/BZIP2";case HashAlgorithm::Crc16Arc:return L"CRC-16/ARC";case HashAlgorithm::Crc16Modbus:return L"CRC-16/MODBUS";
    case HashAlgorithm::Sha512:return L"SHA-512";default:return L"SHA-256";}
}
inline bool NormalizeChecksum(const std::wstring& input,HashAlgorithm algorithm,std::wstring& result) {
    size_t digits=algorithm==HashAlgorithm::Sum8?2:algorithm==HashAlgorithm::Sum16||algorithm==HashAlgorithm::Crc16Ccitt||algorithm==HashAlgorithm::Crc16X25||algorithm==HashAlgorithm::Crc16Arc||algorithm==HashAlgorithm::Crc16Modbus?4:algorithm==HashAlgorithm::Sum32?8:algorithm==HashAlgorithm::Sum64||algorithm==HashAlgorithm::Crc64Ecma?16:algorithm==HashAlgorithm::Crc32||algorithm==HashAlgorithm::Adler32||algorithm==HashAlgorithm::Crc32C||algorithm==HashAlgorithm::Crc32Bzip2?8:algorithm==HashAlgorithm::Md5?32:algorithm==HashAlgorithm::Sha1?40:algorithm==HashAlgorithm::Sha512?128:64;
    result.clear();
    for(wchar_t c:input){if(c==L' '||c==L'\t'||c==L'\r'||c==L'\n')continue;result+=c;}
    if(result.size()>2&&result[0]==L'0'&&(result[1]==L'x'||result[1]==L'X'))result.erase(0,2);
    if(result.size()!=digits)return false;
    for(auto& c:result){if(c>=L'a'&&c<=L'f')c-=L'a'-L'A';if(!((c>=L'0'&&c<=L'9')||(c>=L'A'&&c<=L'F')))return false;}
    return true;
}
inline uint64_t UpdatePresetCrc(uint64_t crc,const Bytes& bytes,unsigned width,uint64_t polynomial,bool reflected) {
    uint64_t mask=width==64?UINT64_MAX:(uint64_t(1)<<width)-1;
    for(auto byte:bytes) {
        crc^=reflected?uint64_t(byte):uint64_t(byte)<<(width-8);
        for(int bit=0;bit<8;++bit)crc=reflected?((crc>>1)^((crc&1)?polynomial:0)):((crc<<1)^((crc&(uint64_t(1)<<(width-1)))?polynomial:0));
        crc&=mask;
    }
    return crc;
}
struct PresetCrc {unsigned width;uint64_t polynomial,initial,xorOut;bool reflected;};
inline std::optional<PresetCrc> CrcPreset(HashAlgorithm algorithm) {
    switch(algorithm){
    case HashAlgorithm::Crc16Ccitt:return PresetCrc{16,0x1021,0xffff,0,false};
    case HashAlgorithm::Crc16X25:return PresetCrc{16,0x8408,0xffff,0xffff,true};
    case HashAlgorithm::Crc64Ecma:return PresetCrc{64,0x42f0e1eba9ea3693ULL,0,0,false};
    case HashAlgorithm::Crc32C:return PresetCrc{32,0x82f63b78,0xffffffff,0xffffffff,true};
    case HashAlgorithm::Crc32Bzip2:return PresetCrc{32,0x04c11db7,0xffffffff,0xffffffff,false};
    case HashAlgorithm::Crc16Arc:return PresetCrc{16,0xa001,0,0,true};
    case HashAlgorithm::Crc16Modbus:return PresetCrc{16,0xa001,0xffff,0,true};
    default:return {};
    }
}
struct HashResult {uint32_t crc32=0;std::wstring sha256;std::wstring digest;};
struct CryptoHash {
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    BCRYPT_HASH_HANDLE hash=nullptr;
    std::vector<unsigned char> object;
    DWORD digestLength=0;
    explicit CryptoHash(const wchar_t* name) {
        DWORD size=0,actual=0;
        if(BCryptOpenAlgorithmProvider(&algorithm,name,nullptr,0)<0)return;
        if(BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&actual,0)<0)return;
        if(BCryptGetProperty(algorithm,BCRYPT_HASH_LENGTH,reinterpret_cast<PUCHAR>(&digestLength),sizeof(digestLength),&actual,0)<0)return;
        object.resize(size);
        BCryptCreateHash(algorithm,&hash,object.data(),size,nullptr,0,0);
    }
    ~CryptoHash() {if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);}
    CryptoHash(const CryptoHash&)=delete;
    CryptoHash& operator=(const CryptoHash&)=delete;
};
template<class Cancel,class Progress>
std::optional<HashResult> HashDocument(const Document& document,Cancel cancel,Progress progress,bool& failed,
    HashAlgorithm algorithm=HashAlgorithm::Crc32Sha256,uint64_t start=0,uint64_t length=UINT64_MAX) {
    failed=false;
    if(!document.Loaded()||start>document.size){failed=true;return {};}
    if(length==UINT64_MAX)length=document.size-start;
    if(length>document.size-start){failed=true;return {};}
    bool crcRequired=algorithm==HashAlgorithm::Crc32Sha256||algorithm==HashAlgorithm::Crc32;
    bool adlerRequired=algorithm==HashAlgorithm::Adler32;
    std::unique_ptr<CryptoHash> hash;
    if(algorithm==HashAlgorithm::Crc32Sha256||algorithm==HashAlgorithm::Md5||algorithm==HashAlgorithm::Sha1||algorithm==HashAlgorithm::Sha256||algorithm==HashAlgorithm::Sha512) {
        const wchar_t* name=algorithm==HashAlgorithm::Md5?BCRYPT_MD5_ALGORITHM:algorithm==HashAlgorithm::Sha1?BCRYPT_SHA1_ALGORITHM:
            algorithm==HashAlgorithm::Sha512?BCRYPT_SHA512_ALGORITHM:BCRYPT_SHA256_ALGORITHM;
        hash=std::make_unique<CryptoHash>(name);
        if(!hash->hash){failed=true;return {};}
    }
    auto preset=CrcPreset(algorithm);
    Bytes bytes;uint32_t crc=0xffffffff,adlerA=1,adlerB=0;uint64_t sum=0,presetCrc=preset?preset->initial:0;
    for(uint64_t offset=0;offset<length;) {
        if(cancel())return {};
        auto count=static_cast<size_t>((std::min)(uint64_t(65536),length-offset));
        if(!document.Read(start+offset,count,bytes)||(hash&&BCryptHashData(hash->hash,bytes.data(),static_cast<ULONG>(bytes.size()),0)<0)) {failed=true;return {};}
        if(crcRequired)crc=UpdateCrc32(crc,bytes);
        if(adlerRequired)for(auto byte:bytes){adlerA=(adlerA+byte)%65521;adlerB=(adlerB+adlerA)%65521;}
        if(algorithm>=HashAlgorithm::Sum8&&algorithm<=HashAlgorithm::Sum64)for(auto byte:bytes)sum+=byte;
        if(preset)presetCrc=UpdatePresetCrc(presetCrc,bytes,preset->width,preset->polynomial,preset->reflected);
        offset+=count;progress(offset);
    }
    if(cancel())return {};
    HashResult result;result.crc32=crc^0xffffffff;
    std::wostringstream text;text<<std::hex<<std::uppercase<<std::setfill(L'0');
    if(hash) {
        Bytes digest(hash->digestLength);
        if(BCryptFinishHash(hash->hash,digest.data(),static_cast<ULONG>(digest.size()),0)<0){failed=true;return {};}
        for(auto b:digest)text<<std::setw(2)<<unsigned(b);
        result.digest=text.str();
        if(algorithm==HashAlgorithm::Sha256||algorithm==HashAlgorithm::Crc32Sha256)result.sha256=result.digest;
    }else {
        if(algorithm>=HashAlgorithm::Sum8&&algorithm<=HashAlgorithm::Sum64) {
            unsigned bits=8u<<(static_cast<unsigned>(algorithm)-static_cast<unsigned>(HashAlgorithm::Sum8));
            uint64_t mask=bits==64?UINT64_MAX:(uint64_t(1)<<bits)-1;text<<std::setw(bits/4)<<(sum&mask);
        }else if(preset)text<<std::setw(preset->width/4)<<(presetCrc^preset->xorOut);
        else text<<std::setw(8)<<(adlerRequired?(adlerB<<16)|adlerA:result.crc32);
        result.digest=text.str();
    }
    return result;
}
struct Difference {uint64_t offset=0;std::optional<unsigned char> left,right;};
template<class Cancel,class Progress>
std::optional<Difference> CompareDocuments(const Document& left,const Document& right,uint64_t start,
    Cancel cancel,Progress progress,bool& failed) {
    failed=false;
    if(!left.Loaded()||!right.Loaded()){failed=true;return {};}
    Bytes a,b;uint64_t common=(std::min)(left.size,right.size);
    for(uint64_t offset=start;offset<common;) {
        if(cancel())return {};
        auto count=static_cast<size_t>((std::min)(uint64_t(65536),common-offset));
        if(!left.Read(offset,count,a)||!right.Read(offset,count,b)){failed=true;return {};}
        for(size_t i=0;i<count;++i)if(a[i]!=b[i])return Difference{offset+i,a[i],b[i]};
        offset+=count;progress(offset);
    }
    if(cancel())return {};
    uint64_t longest=(std::max)(left.size,right.size),offset=(std::max)(start,common);
    if(left.size!=right.size&&offset<longest) {
        Difference diff;diff.offset=offset;
        if(offset<left.size) {if(!left.Read(offset,1,a)){failed=true;return {};}diff.left=a[0];}
        if(offset<right.size){if(!right.Read(offset,1,b)){failed=true;return {};}diff.right=b[0];}
        return diff;
    }
    return {};
}
}
