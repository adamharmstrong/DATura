#include "stdafx.h"
#include "ffxi_dat_resolver.h"
#include "ffxi_file_io.h"
#include "noesis_rapi.h"
#include "model_ff11.h"
#include "ffxi_paths.h"
#include "ffxi_resource.h"
#include "zone_room_loader.h"
#include "bgw_player.h"
#include <winioctl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

namespace
{
namespace fs=std::filesystem;
int failures=0,checks=0;
void Check(bool condition,const char* message)
{ ++checks; if(!condition){++failures;std::cerr<<"FAIL: "<<message<<'\n';} }
void Put(const fs::path& path,const std::string& content)
{
    fs::create_directories(path.parent_path());
    std::ofstream file(path,std::ios::binary); file.write(content.data(),content.size());
    if(!file) throw std::runtime_error("Could not create fixture");
}
std::string Read(const fs::path& path)
{
    BYTE* raw=nullptr; DWORD size=0;
    if(!FFXIFileIO::ReadWholeFile(path.string().c_str(),&raw,&size)) return {};
    std::unique_ptr<BYTE[]> owner(raw);
    return std::string(reinterpret_cast<char*>(raw),size);
}
void Synthetic(const fs::path& base)
{
    const auto retail=base/"retail", replacement=base/"replacement";
    const auto logical=retail/"ROM/1/35.DAT", physical=replacement/"ROM/1/35.DAT";
    Put(logical,"retail"); Put(physical,"replacement");
    FFXIDatResolver::Configure({retail.string(),replacement.string(),false});
    Check(Read(logical)=="retail","disabled policy preserves retail bytes");
    FFXIDatResolver::Configure({retail.string(),replacement.string(),true});
    Check(Read(logical)=="replacement","replacement wins in shared scene/room reader");
    auto resolved=FFXIDatResolver::Resolve(logical.string().c_str());
    Check(resolved.logicalPath==logical.string() && fs::path(resolved.sourcePath)==physical,
        "logical identity and physical provenance stay separate");
    Check(FFXIPath::FindZoneIDByModelPath(retail.string().c_str(),resolved.logicalPath.c_str())==235,
        "replacement retains Bastok Markets zone identity");
    std::string mixed=(retail/"rom/1/../1/35.dat").string();
    std::replace(mixed.begin(),mixed.end(),'\\','/');
    Check(Read(mixed)=="replacement","case, slash and dot normalization select the same replacement");
    noeRAPI_t rapi(nullptr); rapi.SetCurrentFilePath(logical.string().c_str());
    int size=0; auto* data=rapi.Noesis_ReadFile(logical.string().c_str(),&size);
    Check(data && std::string(reinterpret_cast<char*>(data),size)=="replacement","DAT-set and companion RAPI reader shares policy");
    Check(rapi.GetCurrentFilePath()==logical.string(),"reading replacement does not rewrite parser context");
    FFXIResource::ParseResult parsed;
    FFXIResource::ParseFile(logical.string().c_str(),parsed);
    Check(parsed.logicalPath==logical.string() && fs::path(parsed.sourcePath)==physical,"resource parser reports the same selected source even on parse error");
    Put(retail/"ROM/1/36.DAT","fallback");
    Check(Read(retail/"ROM/1/36.DAT")=="fallback","absent replacement falls back to retail");
    Put(replacement/"ROM2/0/53.DAT","numbered");
    Check(Read(retail/"ROM2/0/53.DAT")=="numbered" && FFXIPath::FileExists((retail/"ROM2/0/53.DAT").string().c_str()),
        "numbered hive replacement loads even when retail asset is missing");
    Put(base/"retail-extra/ROM/1/35.DAT","outside");
    Check(Read(base/"retail-extra/ROM/1/35.DAT")=="outside","prefix siblings are outside installation scope");
    Put(base/"ROM/1/35.DAT","escaped");
    Check(Read(retail/"../ROM/1/35.DAT")=="escaped","parent traversal outside installation is never remapped");
    Put(retail/"config.txt","configuration"); Put(replacement/"config.txt","wrong");
    Check(Read(retail/"config.txt")=="configuration","configuration files are not replaced");
    Check(Read(physical)=="replacement" && FFXIDatResolver::Resolve(physical.string().c_str()).source==FFXIDatResolver::Source::Direct,
        "explicit external paths are direct reads, without recursive replacement");

    std::string vt(9,'\0'),ft(18,'\0'); vt[7]=1; ft[14]=static_cast<char>(163);
    Put(retail/"VTABLE.DAT",vt); Put(retail/"FTABLE.DAT",ft);
    vt[7]=0; vt[8]=2; ft[16]=53;
    Put(retail/"ROM2/VTABLE2.DAT",vt); Put(retail/"ROM2/FTABLE2.DAT",ft);
    Put(replacement/"VTABLE.DAT","bad table"); Put(replacement/"ROM2/VTABLE2.DAT","bad table");
    FFXIResource::ResolvedFile id;
    Check(FFXIResource::ResolveFileId(retail.string().c_str(),7,id) && id.replacement &&
        fs::path(id.fullPath)==logical && fs::path(id.sourcePath)==physical,"file-ID resolution keeps logical path and honors replacement");
    Check(FFXIResource::ResolveFileId(retail.string().c_str(),8,id) && id.hive==2 && id.replacement,
        "numbered FTABLE/VTABLE resolution stays retail while asset is replaced");
    Check(FFXIDatResolver::Resolve((retail/"ROM2/VTABLE2.DAT").string().c_str()).source==FFXIDatResolver::Source::Direct,
        "mapping tables are excluded from replacement policy");
    for(const char* invalid:{"ROM1/1/35.DAT","ROM20/1/35.DAT","ROM/1/35.DAT:stream","ROM/../config.txt","ROM/1/sub/35.DAT"})
        Check(FFXIDatResolver::Resolve((retail/invalid).string().c_str()).source==FFXIDatResolver::Source::Direct,
            "non-asset path cannot become a replacement key");

    Put(physical,"");
    Check(Read(logical).empty() && FFXIDatResolver::Resolve(logical.string().c_str()).source==FFXIDatResolver::Source::Replacement,
        "empty replacement fails instead of silently using retail");
    fs::remove(physical); fs::create_directory(physical);
    Check(Read(logical).empty() && !FFXIPath::FileExists(logical.string().c_str()),"directory replacement fails without retail fallback");
    fs::remove(physical); Put(physical,"locked");
    HANDLE exclusive=CreateFileA(physical.string().c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);
    Check(exclusive!=INVALID_HANDLE_VALUE,"create unreadable replacement fixture");
    Check(Read(logical).empty(),"sharing failure on replacement does not fall back");
    if(exclusive!=INVALID_HANDLE_VALUE) CloseHandle(exclusive);
    const auto history=FFXIDatResolver::RecentOpens();
    Check(!history.empty() && history.back().resolution.source==FFXIDatResolver::Source::Replacement && history.back().error!=0,
        "source history records selected path and actual open failure");
    fs::remove(physical);
    Check(Read(logical)=="retail","removing replacement restores fallback without negative caches");
    Put(physical,"changed"); Check(Read(logical)=="changed","next read sees newly added replacement");

    const auto huge=replacement/"ROM/1/37.DAT";
    HANDLE sparse=CreateFileA(huge.string().c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,0,nullptr);
    DWORD returned=0; LARGE_INTEGER hugeSize; hugeSize.QuadPart=(1ull<<32)+4;
    const bool madeSparse=sparse!=INVALID_HANDLE_VALUE && DeviceIoControl(sparse,FSCTL_SET_SPARSE,nullptr,0,nullptr,0,&returned,nullptr) &&
        SetFilePointerEx(sparse,hugeSize,nullptr,FILE_BEGIN) && SetEndOfFile(sparse);
    if(sparse!=INVALID_HANDLE_VALUE)CloseHandle(sparse);
    Check(madeSparse,"create oversized sparse DAT without allocating gigabytes");
    if(madeSparse)
    {
        const auto hugeLogical=(retail/"ROM/1/37.DAT").string();
        Check(Read(hugeLogical).empty(),"oversized DAT cannot wrap the shared reader length");
        size=99;
        Check(!rapi.Noesis_ReadFile(hugeLogical.c_str(),&size) && size==0,"oversized companion cannot wrap signed parser length");
    }
    fs::remove(huge);

    auto audio=[](char sample)
    {
        std::string result(50,'\0'); memcpy(result.data(),"SeWave",6);
        const auto write=[&](int at,unsigned value){for(int i=0;i<4;++i)result[at+i]=static_cast<char>(value>>(8*i));};
        write(8,50);write(12,1);write(20,1);write(28,22050);write(36,48);result[42]=1;result[48]=sample;
        return result;
    };
    const auto audioLogical=retail/"ROM/1/38.DAT";
    Put(audioLogical,audio(1));Put(replacement/"ROM/1/38.DAT",audio(2));
    char replacementWav[MAX_PATH]={},retailWav[MAX_PATH]={};
    FFXIAudioInfo info;
    Check(FFXIAudio_ReadInfo(audioLogical.string().c_str(),&info) && info.sampleRate==22050,"audio header reader accepts replacement DAT");
    Check(FFXIAudio_PrepareFile(audioLogical.string().c_str(),replacementWav,sizeof(replacementWav)),"prepare replacement audio without playback");
    FFXIDatResolver::Configure({retail.string(),replacement.string(),false});
    Check(FFXIAudio_PrepareFile(audioLogical.string().c_str(),retailWav,sizeof(retailWav)),"prepare retail audio without playback");
    const auto retailAudio=Read(retailWav),replacementAudio=Read(replacementWav);
    Check(strcmp(retailWav,replacementWav)!=0 && retailAudio!=replacementAudio && !retailAudio.empty() && !replacementAudio.empty(),
        "audio cache separates retail and replacement sources even at equal sizes");
    // Only remove the exact temporary outputs returned for these unique fixtures.
    const auto cacheRoot=fs::temp_directory_path()/"DATuraAudio";
    for(const char* wav:{replacementWav,retailWav})
        if(*wav && fs::path(wav).parent_path()==cacheRoot)fs::remove(wav);

    FFXIDatResolver::Configure({retail.string(),(base/"other").string(),true});
    Put(base/"other/ROM/1/35.DAT","other root");
    Check(Read(logical)=="other root","explicit reconfiguration clears prior root policy");
    FFXIDatResolver::Configure({retail.string(),replacement.string(),true});
    for(int i=0;i<140;++i) { auto h=FFXIDatResolver::OpenRead((retail/("ROM/5/"+std::to_string(i)+".DAT")).string().c_str()); if(h!=INVALID_HANDLE_VALUE) CloseHandle(h); }
    Check(FFXIDatResolver::RecentOpens().size()==128,"source history is bounded");
}
void Installed(const fs::path& root,const fs::path& base)
{
    const auto overrideRoot=base/"installed-replacements";
    for(const char* relative:{"ROM/1/35.DAT","ROM/1/61.DAT"})
    {
        const auto output=overrideRoot/relative; fs::create_directories(output.parent_path());
        fs::copy_file(root/relative,output,fs::copy_options::overwrite_existing);
    }
    FFXIDatResolver::Configure({root.string(),overrideRoot.string(),true});
    const auto logical=(root/"ROM/1/35.DAT").string();
    BYTE* raw=nullptr; DWORD size=0;
    Check(FFXIFileIO::ReadWholeFile(logical.c_str(),&raw,&size),"read installed replacement zone");
    if(!raw)return;
    std::unique_ptr<BYTE[]> bytes(raw);
    noeRAPI_t rapi(nullptr); rapi.SetCurrentFilePath(logical.c_str());
    ff11Opts_t options={}; options.collectCollision=true; options.renderWater=true;
    gpFF11Opts=&options; int count=0;
    auto* model=Model_FF11_LoadDAT(bytes.get(),static_cast<int>(size),count,&rapi);
    Check(model!=nullptr,"replacement zone parses through normal model pipeline");
    if(model) Check(ZoneRoomLoader::Append(model,&rapi,logical.c_str())==14,"logical zone identity still selects all fourteen companions");
    const auto history=FFXIDatResolver::RecentOpens();
    bool mainReplacement=false,roomReplacement=false,roomFallback=false;
    for(const auto& entry:history)
    {
        const auto& r=entry.resolution;
        mainReplacement |= r.relativePath=="ROM\\1\\35.DAT" && r.source==FFXIDatResolver::Source::Replacement;
        roomReplacement |= r.relativePath=="ROM\\1\\61.DAT" && r.source==FFXIDatResolver::Source::Replacement;
        roomFallback |= r.relativePath=="ROM\\1\\62.DAT" && r.source==FFXIDatResolver::Source::Retail;
    }
    Check(mainReplacement && roomReplacement && roomFallback,"real zone and rooms compose overrides with retail fallback independently");
    Check(FFXIPath::FindZoneIDByModelPath(root.string().c_str(),logical.c_str())==235,"real replacement retains zone resource identity");
    gpFF11Opts=nullptr;
    // Only our two copied test assets are removed; retail files remain untouched.
    fs::remove(overrideRoot/"ROM/1/35.DAT"); fs::remove(overrideRoot/"ROM/1/61.DAT");
}
}
int main(int argc,char** argv)
{
    const auto base=fs::absolute(fs::path("tests/bin/dat-resolver/fixtures")/std::to_string(GetCurrentProcessId()));
    try { Synthetic(base); if(argc>1)Installed(argv[1],base); }
    catch(const std::exception& error){Check(false,error.what());}
    FFXIDatResolver::Configure({});
    std::cout<<"DAT resolver: "<<checks<<" checks, "<<failures<<" failures\n";
    return failures?1:0;
}
