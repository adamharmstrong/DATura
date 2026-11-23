#include "../DATura/dat_hex_reader.cpp"
#include <iostream>
#include <stdexcept>
#define CHECK(x) do {if(!(x)) throw std::runtime_error(#x);} while(0)
static int pickerCalls=0;
struct RtfInput {std::string text;size_t position=0;};
static DWORD CALLBACK ReadRtf(DWORD_PTR cookie,LPBYTE buffer,LONG count,LONG* actual) {
    auto& input=*reinterpret_cast<RtfInput*>(cookie);size_t take=(std::min)(size_t(count),input.text.size()-input.position);
    std::memcpy(buffer,input.text.data()+input.position,take);input.position+=take;*actual=static_cast<LONG>(take);return 0;
}

static std::wstring expectedNativeFolder;
static DWORD nativePickerThread=0;
static bool nativePickerConfigured=false,nativePickerSta=false;
static thread_local IFileOpenDialog* autoCancelPicker=nullptr;
static bool realPickerPumped=false;
static void CALLBACK CancelRealPicker(HWND,UINT,UINT_PTR timer,DWORD) {
    if(autoCancelPicker&&SUCCEEDED(autoCancelPicker->Close(HRESULT_FROM_WIN32(ERROR_CANCELLED)))){KillTimer(nullptr,timer);realPickerPumped=true;}
}
static HRESULT ShowAndCancelRealPicker(IFileOpenDialog* picker,HWND owner) {
    autoCancelPicker=picker;UINT_PTR timer=SetTimer(nullptr,0,250,CancelRealPicker);
    if(!timer){autoCancelPicker=nullptr;return E_FAIL;}
    HRESULT status=picker->Show(owner);KillTimer(nullptr,timer);autoCancelPicker=nullptr;return status;
}
static HRESULT CancelConfiguredPicker(IFileOpenDialog* picker,HWND) {
    nativePickerThread=GetCurrentThreadId();APTTYPE type;APTTYPEQUALIFIER qualifier;
    nativePickerSta=SUCCEEDED(CoGetApartmentType(&type,&qualifier))&&(type==APTTYPE_STA||type==APTTYPE_MAINSTA);
    Microsoft::WRL::ComPtr<IShellItem> folder;FILEOPENDIALOGOPTIONS options={};
    PWSTR name=nullptr;
    if(SUCCEEDED(picker->GetOptions(&options))&&SUCCEEDED(picker->GetFolder(folder.GetAddressOf()))&&SUCCEEDED(folder->GetDisplayName(SIGDN_FILESYSPATH,&name))) {
        std::error_code error;
        nativePickerConfigured=std::filesystem::equivalent(name,expectedNativeFolder,error)&&!error&&(options&FOS_FILEMUSTEXIST)&&(options&FOS_FORCEFILESYSTEM);
    }
    CoTaskMemFree(name);return HRESULT_FROM_WIN32(ERROR_CANCELLED);
}

static DatHex::Bytes ReadFileBytes(const std::filesystem::path& path) {
    DatHex::Document d; CHECK(d.Open(path.wstring())); DatHex::Bytes bytes;
    CHECK(d.Read(0,static_cast<size_t>(d.size),bytes)); return bytes;
}
int main() {
    std::filesystem::path dir=std::filesystem::current_path()/L"artifacts"/L"dat-hex-tests"/
        (std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
    std::filesystem::create_directories(dir);
    try {
        using namespace DatHex;
        uint64_t value; Bytes bytes;
        CHECK(ParseNumber(L" 0xFFFFFFFFFFFFFFFF ",value)&&value==UINT64_MAX);
        CHECK(!ParseNumber(L"10000000000000000",value)); CHECK(!ParseNumber(L"-1",value));
        CHECK(ParseNumber(L"123",value,10)&&value==123); CHECK(!ParseNumber(L"12z",value));
        CHECK(ResolveOffset(L"+20",100,1000,16,value)&&value==132);
        CHECK(ResolveOffset(L"-0x10",100,1000,10,value)&&value==84);
        CHECK(ResolveOffset(L"  +900  ",100,1000,10,value)&&value==1000);
        CHECK(ResolveOffset(L"0",100,1000,16,value)&&value==0);
        CHECK(!ResolveOffset(L"-101",100,1000,10,value));CHECK(!ResolveOffset(L"+901",100,1000,10,value));
        CHECK(!ResolveOffset(L"1001",0,1000,10,value));CHECK(!ResolveOffset(L"+",0,1000,16,value));
        CHECK(!ResolveOffset(L"+-1",0,1000,16,value));CHECK(!ResolveOffset(L"+FFFFFFFFFFFFFFFF",1,UINT64_MAX,16,value));
        CHECK(!CopyPathAllowed(L"file.DAT:edited"));CHECK(!CopyPathAllowed(L"NUL.DAT"));
        CHECK(!CopyPathAllowed(L"\\\\.\\C:"));CHECK(!CopyPathAllowed(L"\\\\?\\C:\\test.DAT"));
        CHECK(ParseHex(L"82 A0\nff",bytes)&&bytes==Bytes({0x82,0xa0,0xff}));
        CHECK(!ParseHex(L"ABC",bytes)); CHECK(!ParseHex(L"GG",bytes));
        CHECK(Crc32(Bytes({'1','2','3','4','5','6','7','8','9'}))==0xcbf43926);
        CHECK(UnsignedValue(Bytes({1,2,3,4}),4,false)==0x04030201);
        CHECK(UnsignedValue(Bytes({1,2,3,4}),4,true)==0x01020304);
        auto source=dir/L"original.DAT",copy=dir/L"copy.DAT",existing=dir/L"existing.DAT";
        Bytes original(65540,0);original[100]=0x82;original[101]=0xa0;
        original[65535]=0xaa;original[65536]=0xbb;original[65537]=0xcc;
        {NewCopy output(source.wstring());CHECK(output.Valid());CHECK(output.Write(original.data(),original.size()));CHECK(output.Commit());}
        auto alias=dir/L"alias.DAT";CHECK(CreateHardLinkW(alias.c_str(),source.c_str(),nullptr));
        Document doc;CHECK(doc.Open(source.wstring()));CHECK(!doc.Dirty());
        CHECK(doc.Overwrite(0,Bytes({0x42,0x43})));CHECK(doc.Dirty());CHECK(doc.CanUndo());
        CHECK(!doc.Overwrite(doc.size,Bytes({1})));CHECK(!doc.Overwrite(UINT64_MAX,Bytes({1})));
        CHECK(doc.Undo());CHECK(!doc.Dirty());CHECK(doc.Redo());CHECK(doc.Dirty());
        CHECK(doc.Read(0,2,bytes)&&bytes==Bytes({0x42,0x43}));
        CHECK(doc.ModifiedOffset(0)==1);CHECK(doc.ModifiedOffset(1)==0);
        CHECK(doc.ModifiedOffset(0,true)==1);CHECK(doc.ModifiedOffset(1,true)==0);
        CHECK(doc.Undo());CHECK(!doc.ModifiedOffset(0));CHECK(doc.Redo());

        bool failed;auto match=doc.Find(Bytes({0xaa,0xbb,0xcc}),0,[]{return false;},failed);
        CHECK(!failed&&match&&*match==65535);
        CHECK(doc.Find(Bytes({0x42,0x43}),0,[]{return false;},failed)==0);
        CHECK(!doc.Find(Bytes({0xaa,0xbb}),0,[]{return true;},failed));
        std::vector<uint64_t> offsets;uint64_t scanned=0;
        CHECK(doc.ScanMatches(Bytes({0xaa,0xbb,0xcc}),[]{return false;},[&](uint64_t at){offsets.push_back(at);return true;},[&](uint64_t n){scanned=n;},failed));
        CHECK(!failed&&offsets==std::vector<uint64_t>({65535})&&scanned==doc.size);
        offsets.clear();
        CHECK(doc.ScanMatches(Bytes({0,0,0}),[]{return false;},[&](uint64_t at){offsets.push_back(at);return offsets.size()<3;},[](uint64_t){},failed));
        CHECK(offsets==std::vector<uint64_t>({2,3,4}));
        bool called=false;
        CHECK(!doc.ScanMatches(Bytes({0}),[]{return true;},[&](uint64_t){called=true;return true;},[](uint64_t){},failed));CHECK(!called&&!failed);
        auto stats=Statistics(doc,[]{return false;},[](uint64_t){},failed);
        CHECK(stats&&!failed&&stats->total==doc.size&&stats->counts[0]==doc.size-7);
        CHECK(stats->counts[0x42]==1&&stats->counts[0xbb]==1&&stats->entropy>0&&stats->entropy<1);
        CHECK(!Statistics(doc,[]{return true;},[](uint64_t){},failed)&&!failed);
        Document batch;CHECK(batch.Open(source.wstring()));bool limit=false;
        auto plan=batch.PrepareReplaceAll(Bytes({0xaa,0xbb,0xcc}),Bytes({1,2,3}),[]{return false;},[](uint64_t){},failed,limit);
        CHECK(plan&&!failed&&!limit&&plan->count==1&&!batch.Dirty());CHECK(batch.CommitReplacement(std::move(*plan)));
        CHECK(batch.Read(65535,3,bytes)&&bytes==Bytes({1,2,3}));CHECK(batch.Undo()&&!batch.Dirty());
        CHECK(batch.Redo());CHECK(batch.Read(65535,3,bytes)&&bytes==Bytes({1,2,3}));CHECK(batch.Undo());
        auto many=batch.PrepareReplaceAll(Bytes({0,0}),Bytes({0x12,0x34}),[]{return false;},[](uint64_t){},failed,limit);
        CHECK(many&&!failed&&!limit&&many->count>30000);CHECK(batch.CommitReplacement(std::move(*many)));
        CHECK(batch.Read(0,4,bytes)&&bytes==Bytes({0x12,0x34,0x12,0x34}));CHECK(batch.Undo()&&!batch.Dirty());
        auto cancelled=batch.PrepareReplaceAll(Bytes({0}),Bytes({1}),[]{return true;},[](uint64_t){},failed,limit);
        CHECK(!cancelled&&!failed&&!limit&&!batch.Dirty());
        auto stale=batch.PrepareReplaceAll(Bytes({0xaa,0xbb,0xcc}),Bytes({1,2,3}),[]{return false;},[](uint64_t){},failed,limit);
        CHECK(stale);CHECK(batch.Overwrite(0,Bytes({9})));CHECK(!batch.CommitReplacement(std::move(*stale)));CHECK(batch.Undo()&&!batch.Dirty());
        CHECK(!batch.PrepareReplaceAll(Bytes({1}),Bytes({1,2}),[]{return false;},[](uint64_t){},failed,limit)&&failed);
        auto unchanged=batch.PrepareReplaceAll(Bytes({0}),Bytes({0}),[]{return false;},[](uint64_t){},failed,limit);
        CHECK(unchanged&&unchanged->count==0);CHECK(batch.CommitReplacement(std::move(*unchanged))&&!batch.Dirty());
        auto largePath=dir/L"large.DAT";{Bytes zeros(1048592,0);NewCopy output(largePath.wstring());CHECK(output.Write(zeros.data(),zeros.size()));CHECK(output.Commit());}
        Document large;CHECK(large.Open(largePath.wstring()));
        CHECK(!large.PrepareReplaceAll(Bytes({0}),Bytes({1}),[]{return false;},[](uint64_t){},failed,limit)&&limit&&!failed&&!large.Dirty());
        CHECK(!large.PrepareReplaceAll(Bytes(16,0),Bytes(16,1),[]{return false;},[](uint64_t){},failed,limit)&&limit&&!failed&&!large.Dirty());
        CHECK(!doc.Find(Bytes({0xaa,0xbb,0xcc}),65534,[]{return false;},failed,65536)&&!failed);
        CHECK(doc.Find(Bytes({0xaa,0xbb,0xcc}),65534,[]{return false;},failed,65539)==65535);
        offsets.clear();CHECK(doc.ScanMatches(Bytes({0,0}),[]{return false;},[&](uint64_t at){offsets.push_back(at);return true;},[](uint64_t){},failed,3,5));
        CHECK(offsets==std::vector<uint64_t>({3,4,5,6}));
        CHECK(!doc.ScanMatches(Bytes({0}),[]{return false;},[](uint64_t){return true;},[](uint64_t){},failed,doc.size,1)&&failed);
        auto bounded=batch.PrepareReplaceAll(Bytes({0,0}),Bytes({9,8}),[]{return false;},[](uint64_t){},failed,limit,3,5);
        CHECK(bounded&&!failed&&bounded->count==2);CHECK(batch.CommitReplacement(std::move(*bounded)));
        CHECK(batch.Read(2,7,bytes)&&bytes==Bytes({0,9,8,9,8,0,0}));CHECK(batch.Undo()&&!batch.Dirty());
        // Every save must be a new file, even with an unmodified document.
        CHECK(!doc.SaveCopy(source.wstring()));CHECK(ReadFileBytes(source)==original);
        Handle writer(CreateFileW(source.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr));
        CHECK(writer.value==INVALID_HANDLE_VALUE);
        CHECK(doc.SaveCopy(copy.wstring()));auto edited=original;edited[0]=0x42;edited[1]=0x43;
        CHECK(ReadFileBytes(copy)==edited);CHECK(!doc.SaveCopy(copy.wstring()));CHECK(ReadFileBytes(copy)==edited);
        {NewCopy output(existing.wstring());CHECK(output.Write("KEEP",4));CHECK(output.Commit());}
        CHECK(!doc.SaveCopy(existing.wstring()));CHECK(ReadFileBytes(existing)==Bytes({'K','E','E','P'}));
        CHECK(!doc.SaveCopy(existing.wstring()+L":edited"));
        CHECK(ReadFileBytes(existing)==Bytes({'K','E','E','P'}));
        CHECK(!doc.SaveCopy(alias.wstring()));CHECK(ReadFileBytes(source)==original);
        CHECK(!doc.SaveCopy((dir/L"ORIGINAL.dat").wstring()));CHECK(ReadFileBytes(source)==original);
        auto block=dir/L"block.bin";
        CHECK(doc.SaveRangeCopy(block.wstring(),65535,3));CHECK(ReadFileBytes(block)==Bytes({0xaa,0xbb,0xcc}));
        CHECK(!doc.SaveRangeCopy(block.wstring(),0,1));CHECK(!doc.SaveRangeCopy(source.wstring(),0,1));
        CHECK(!doc.SaveRangeCopy((dir/L"invalid.bin").wstring(),UINT64_MAX,1));
        CHECK(!std::filesystem::exists(dir/L"invalid.bin"));
        Document baseline;CHECK(baseline.Open(source.wstring()));
        auto diff=CompareDocuments(doc,baseline,0,[]{return false;},[](uint64_t){},failed);
        CHECK(!failed&&diff&&diff->offset==0&&diff->left==0x42&&diff->right==0);
        CHECK(!CompareDocuments(doc,baseline,2,[]{return false;},[](uint64_t){},failed));
        auto shorter=dir/L"shorter.DAT";CHECK(baseline.SaveRangeCopy(shorter.wstring(),0,65538));
        Document shortDoc;CHECK(shortDoc.Open(shorter.wstring()));
        diff=CompareDocuments(baseline,shortDoc,0,[]{return false;},[](uint64_t){},failed);
        CHECK(diff&&diff->offset==65538&&diff->left==0&&!diff->right);
        diff=CompareDocuments(shortDoc,baseline,0,[]{return false;},[](uint64_t){},failed);
        CHECK(diff&&diff->offset==65538&&!diff->left&&diff->right==0);
        CHECK(!CompareDocuments(doc,baseline,0,[]{return true;},[](uint64_t){},failed));
        auto hashInput=dir/L"abc.DAT";
        {NewCopy output(hashInput.wstring());CHECK(output.Write("abc",3));CHECK(output.Commit());}
        Document abc;CHECK(abc.Open(hashInput.wstring()));
        auto hash=HashDocument(abc,[]{return false;},[](uint64_t){},failed);
        CHECK(!failed&&hash&&hash->crc32==0x352441c2);
        CHECK(hash->sha256==L"BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD");
        for(const auto& example:std::vector<std::pair<HashAlgorithm,std::wstring>>{
            {HashAlgorithm::Md5,L"900150983CD24FB0D6963F7D28E17F72"},
            {HashAlgorithm::Sha1,L"A9993E364706816ABA3E25717850C26C9CD0D89D"},
            {HashAlgorithm::Sha512,L"DDAF35A193617ABACC417349AE20413112E6FA4E89A97EA20A9EEEE64B55D39A2192992A274FC1A836BA3C23A3FEEBBD454D4423643CE80E2A9AC94FA54CA49F"},
            {HashAlgorithm::Adler32,L"024D0127"},{HashAlgorithm::Crc32,L"352441C2"}}) {
            auto computed=HashDocument(abc,[]{return false;},[](uint64_t){},failed,example.first);
            CHECK(computed&&!failed&&computed->digest==example.second);
        }
        auto rangeHash=HashDocument(doc,[]{return false;},[](uint64_t){},failed,HashAlgorithm::Crc32,65535,3);
        CHECK(rangeHash&&!failed&&rangeHash->crc32==Crc32(Bytes({0xaa,0xbb,0xcc})));
        CHECK(!HashDocument(doc,[]{return false;},[](uint64_t){},failed,HashAlgorithm::Md5,doc.size,1)&&failed);
        auto rangeStats=Statistics(doc,[]{return false;},[](uint64_t){},failed,65535,3);
        CHECK(rangeStats&&!failed&&rangeStats->total==3&&rangeStats->counts[0xaa]==1&&rangeStats->counts[0]==0);
        CHECK(std::abs(rangeStats->entropy-std::log2(3.0))<0.00001);
        CHECK(!Statistics(doc,[]{return false;},[](uint64_t){},failed,doc.size+1,0)&&failed);
        std::wstring normalized;
        CHECK(NormalizeChecksum(L" 0x35 24 41 c2 ",HashAlgorithm::Crc32,normalized)&&normalized==L"352441C2");
        CHECK(!NormalizeChecksum(L"123",HashAlgorithm::Crc32,normalized));CHECK(!NormalizeChecksum(L"GG2441C2",HashAlgorithm::Crc32,normalized));
        CHECK(NormalizeChecksum(std::wstring(64,L'a'),HashAlgorithm::Crc32Sha256,normalized)&&normalized==std::wstring(64,L'A'));
        auto checkPath=dir/L"checksum-vector.DAT";{NewCopy output(checkPath.wstring());CHECK(output.Write("123456789",9));CHECK(output.Commit());}
        Document checkDocument;CHECK(checkDocument.Open(checkPath.wstring()));
        for(const auto& example:std::vector<std::pair<HashAlgorithm,std::wstring>>{
            {HashAlgorithm::Sum8,L"DD"},{HashAlgorithm::Sum16,L"01DD"},{HashAlgorithm::Sum32,L"000001DD"},{HashAlgorithm::Sum64,L"00000000000001DD"},
            {HashAlgorithm::Crc16Ccitt,L"29B1"},{HashAlgorithm::Crc16X25,L"906E"},{HashAlgorithm::Crc64Ecma,L"6C40DF5F0B497347"},{HashAlgorithm::Crc32C,L"E3069283"},{HashAlgorithm::Crc32Bzip2,L"FC891918"},{HashAlgorithm::Crc16Arc,L"BB3D"},{HashAlgorithm::Crc16Modbus,L"4B37"}}) {
            auto computed=HashDocument(checkDocument,[]{return false;},[](uint64_t){},failed,example.first);
            CHECK(computed&&!failed&&computed->digest==example.second);CHECK(NormalizeChecksum(example.second,example.first,normalized));
            CHECK(!HashDocument(checkDocument,[]{return true;},[](uint64_t){},failed,example.first)&&!failed);
        }
        for(auto algorithm:{HashAlgorithm::Crc32C,HashAlgorithm::Crc32Bzip2,HashAlgorithm::Crc16Arc,HashAlgorithm::Crc16Modbus}) {
            auto preset=CrcPreset(algorithm);CHECK(preset);Bytes vector={'1','2','3','4','5','6','7','8','9'};
            auto full=UpdatePresetCrc(preset->initial,vector,preset->width,preset->polynomial,preset->reflected);
            auto segmented=UpdatePresetCrc(preset->initial,Bytes(vector.begin(),vector.begin()+4),preset->width,preset->polynomial,preset->reflected);
            segmented=UpdatePresetCrc(segmented,Bytes(vector.begin()+4,vector.end()),preset->width,preset->polynomial,preset->reflected);CHECK(full==segmented);
            auto empty=HashDocument(checkDocument,[]{return false;},[](uint64_t){},failed,algorithm,0,0);CHECK(empty&&NormalizeChecksum(empty->digest,algorithm,normalized));
            CHECK(!NormalizeChecksum(L"0",algorithm,normalized));
        }
        auto originalHash=HashDocument(baseline,[]{return false;},[](uint64_t){},failed);
        CHECK(originalHash&&originalHash->crc32==Crc32(original));
        auto editedHash=HashDocument(doc,[]{return false;},[](uint64_t){},failed);
        CHECK(editedHash&&editedHash->sha256!=originalHash->sha256&&editedHash->crc32==Crc32(edited));
        CHECK(!HashDocument(doc,[]{return true;},[](uint64_t){},failed));
        auto partial=dir/L"partial.DAT";{NewCopy output(partial.wstring());CHECK(output.Valid());CHECK(output.Write("partial",7));}
        CHECK(!std::filesystem::exists(partial));
        auto failedCopy=dir/L"failed.DAT";
        {NewCopy output(failedCopy.wstring());CHECK(output.Valid());CHECK(!output.Write(nullptr,10));}
        CHECK(!std::filesystem::exists(failedCopy));
        auto raced=dir/L"race.DAT";std::atomic<int> winners=0;
        auto race=[&] {NewCopy output(raced.wstring());if(output.Valid()){CHECK(output.Write("new",3));CHECK(output.Commit());++winners;}};
        std::thread a(race),b(race);a.join();b.join();CHECK(winners==1);CHECK(ReadFileBytes(raced)==Bytes({'n','e','w'}));
        auto empty=dir/L"empty.DAT";{NewCopy output(empty.wstring());CHECK(output.Commit());}
        Document zero;CHECK(zero.Open(empty.wstring()));CHECK(zero.SaveCopy((dir/L"empty-copy.DAT").wstring()));CHECK(!zero.SaveCopy(empty.wstring()));
        hash=HashDocument(zero,[]{return false;},[](uint64_t){},failed);
        CHECK(hash&&hash->crc32==0&&hash->sha256==L"E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855");
        auto emptyStats=Statistics(zero,[]{return false;},[](uint64_t){},failed);CHECK(emptyStats&&emptyStats->total==0&&emptyStats->entropy==0);
        auto emptyAdler=HashDocument(zero,[]{return false;},[](uint64_t){},failed,HashAlgorithm::Adler32);CHECK(emptyAdler&&emptyAdler->digest==L"00000001");
        using namespace DatHexReader;
        CHECK(ParseBinary(L"0b10101010 10111011",bytes)&&bytes==Bytes({0xaa,0xbb}));
        CHECK(ParseBinary(L" 00000000\n11111111 ",bytes)&&bytes==Bytes({0,0xff}));
        CHECK(!ParseBinary(L"101",bytes));CHECK(!ParseBinary(L"00000002",bytes));CHECK(!ParseBinary(L"0b",bytes));
        Bytes guidBytes;for(unsigned i=0;i<16;++i)guidBytes.push_back(static_cast<unsigned char>(i));
        CHECK(GuidText(guidBytes,true)==L"03020100-0504-0706-0809-0A0B0C0D0E0F");
        CHECK(GuidText(guidBytes,false)==L"00010203-0405-0607-0809-0A0B0C0D0E0F");
        CHECK(Leb128Text(Bytes({0xe5,0x8e,0x26}),false)==L"624485 (3 bytes)");
        CHECK(Leb128Text(Bytes({0x9b,0xf1,0x59}),true)==L"-624485 (3 bytes)");
        CHECK(Leb128Text(Bytes({0x7f}),true)==L"-1 (1 bytes)");
        CHECK(EncodeLeb128(624485,false)==Bytes({0xe5,0x8e,0x26}));CHECK(EncodeLeb128(static_cast<uint64_t>(-624485),true)==Bytes({0x9b,0xf1,0x59}));
        for(uint64_t value:{uint64_t(0),uint64_t(63),uint64_t(64),uint64_t(127),uint64_t(128),UINT64_MAX,uint64_t(1)<<63})for(bool sign:{false,true}){auto encoded=EncodeLeb128(value,sign);auto decoded=ReadLeb128(encoded,sign);CHECK(decoded&&decoded->first==value&&decoded->second==encoded.size());}
        Bytes maxLeb(10,255);maxLeb.back()=1;CHECK(Leb128Text(maxLeb,false)==L"18446744073709551615 (10 bytes)");
        CHECK(!ReadLeb128(Bytes({0x80}),false));maxLeb.back()=2;CHECK(!ReadLeb128(maxLeb,false)&&!ReadLeb128(maxLeb,true));
        Bytes minLeb(10,0x80);minLeb.back()=0x7f;CHECK(Leb128Text(minLeb,true)==L"-9223372036854775808 (10 bytes)");
        CHECK(DosDateText(Bytes({0,0,0x21,0}))==L"1980-01-01 00:00:00 (timezone unspecified)");
        CHECK(DosDateText(Bytes({255,255,0,0}))==L"invalid DOS date/time");
        CHECK(FileTimeText(Bytes({0,0x80,0x3e,0xd5,0xde,0xb1,0x9d,1}),false)==L"1970-01-01 00:00:00.000 UTC");
        CHECK(FileTimeText(Bytes({1,0x9d,0xb1,0xde,0xd5,0x3e,0x80,0}),true)==L"1970-01-01 00:00:00.000 UTC");
        auto chartArea=ChartArea(820,500);
        CHECK(ChartByte(chartArea,chartArea.left,chartArea.top)==0);
        CHECK(ChartByte(chartArea,chartArea.right-1,chartArea.bottom-1)==255);
        CHECK(ChartByte(chartArea,chartArea.right,chartArea.top)==-1);
        CHECK(ChartHeight(5,10,100)==50&&ChartHeight(10,10,100)==100&&ChartHeight(0,0,100)==0);
        DistributionView chartView;chartView.stats.total=10;chartView.stats.counts[0]=10;
        Win32ToolWindow::WideSpec chartSpec{DistributionProcedure,L"DatHexDistributionTest",L"Chart test",820,500};chartSpec.createParameter=&chartView;
        HWND chart=Win32ToolWindow::Create(nullptr,chartSpec);CHECK(chart);
        RECT chartBounds;GetClientRect(chart,&chartBounds);auto actualArea=ChartArea(chartBounds.right,chartBounds.bottom);
        SendMessageW(chart,WM_MOUSEMOVE,0,MAKELPARAM(actualArea.left+1,actualArea.top+1));CHECK(chartView.hovered==0);
        SendMessageW(chart,WM_MOUSEMOVE,0,MAKELPARAM(actualArea.right-1,actualArea.top+1));CHECK(chartView.hovered==255);
        SendMessageW(chart,WM_MOUSELEAVE,0,0);CHECK(chartView.hovered==-1);
        SendMessageW(chart,WM_PAINT,0,0);DestroyWindow(chart);CHECK(!IsWindow(chart));
        CHECK(FormatBytes(Bytes({0,0x80,0xff}),ByteFormat::Pascal)==L"const\r\n  Data: array[0..2] of Byte = (\r\n    $00, $80, $FF\r\n  );");
        CHECK(FormatBytes(Bytes({0,0x80,0xff}),ByteFormat::Java)==L"byte[] data = new byte[] {\r\n    (byte)0x00, (byte)0x80, (byte)0xFF\r\n};");
        CHECK(FormatBytes(Bytes({0,0x80,0xff}),ByteFormat::CSharp)==L"byte[] data = new byte[] {\r\n    0x00, 0x80, 0xFF\r\n};");
        CHECK(FormatBytes(Bytes({0,0x80,0xff}),ByteFormat::VB)==L"Dim data As Byte() = {\r\n    &H00, &H80, &HFF\r\n}");
        CHECK(FormatBytes(Bytes(17,0xff),ByteFormat::VB).find(L", _\r\n    &HFF")!=std::wstring::npos);
        for(auto format:{ByteFormat::Pascal,ByteFormat::Java,ByteFormat::CSharp,ByteFormat::VB}) {
            auto codePath=dir/(L"source-"+std::to_wstring(static_cast<int>(format))+L".txt");
            CHECK(SaveFormattedSelection(doc,codePath.wstring(),65535,3,format));CHECK(!SaveFormattedSelection(doc,codePath.wstring(),0,1,format));
            auto saved=ReadFileBytes(codePath);auto formatted=FormatBytes(Bytes({0xaa,0xbb,0xcc}),format);
            CHECK(std::wstring(saved.begin(),saved.end())==formatted);
        }
        CHECK(FormatBytes(Bytes({'f'}),ByteFormat::Base64)==L"Zg==");
        CHECK(FormatBytes(Bytes({'f','o'}),ByteFormat::Base64)==L"Zm8=");
        CHECK(FormatBytes(Bytes({'f','o','o'}),ByteFormat::Base64)==L"Zm9v");
        CHECK(FormatBytes(Bytes({'f','o','o','b','a','r'}),ByteFormat::Base64)==L"Zm9vYmFy");
        CHECK(FormatBytes(Bytes({0,0xff,0x80}),ByteFormat::Base64)==L"AP+A");
        CHECK(FormatBytes(Bytes(),ByteFormat::Base64).empty());
        CHECK(FormatBytes(Bytes({0,0xff,0x41}),ByteFormat::Escaped)==L"\\x00\\xFF\\x41");
        CHECK(FormatBytes(Bytes({0,0xff}),ByteFormat::CArray)==L"const unsigned char data[] = {\r\n    0x00, 0xFF\r\n};");
        CHECK(FormatBytes(Bytes(17,0xab),ByteFormat::CArray).find(L",\r\n    0xAB")!=std::wstring::npos);
        CHECK(FormatBytes(Bytes({0,0xff}),ByteFormat::Hex)==L"00 FF");
        CHECK(RepeatedPattern(Bytes({0xaa,0xbb}),5)==Bytes({0xaa,0xbb,0xaa,0xbb,0xaa}));
        CHECK(RepeatedPattern(Bytes({1,2,3}),2)==Bytes({1,2}));
        CHECK(RepeatedPattern(Bytes(),5).empty());
        bytes={0x00,0x55,0xff};CHECK(TransformBytes(bytes,Transform::Invert)&&bytes==Bytes({0xff,0xaa,0}));
        bytes={0xab,0xcd,0xef};CHECK(TransformBytes(bytes,Transform::And,Bytes({0x0f,0xf0}))&&bytes==Bytes({0x0b,0xc0,0x0f}));
        bytes={0x01,0x02,0x03};CHECK(TransformBytes(bytes,Transform::Or,Bytes({0x80}))&&bytes==Bytes({0x81,0x82,0x83}));
        bytes={0xaa,0x55,0};CHECK(TransformBytes(bytes,Transform::Xor,Bytes({0xff,0x0f}))&&bytes==Bytes({0x55,0x5a,0xff}));
        bytes={1,2,3};CHECK(TransformBytes(bytes,Transform::Reverse)&&bytes==Bytes({3,2,1}));
        bytes={1,2,3,4};CHECK(TransformBytes(bytes,Transform::Swap16)&&bytes==Bytes({2,1,4,3}));
        bytes={1,2,3,4,5,6,7,8};CHECK(TransformBytes(bytes,Transform::Swap32)&&bytes==Bytes({4,3,2,1,8,7,6,5}));
        CHECK(TransformBytes(bytes,Transform::Swap64)&&bytes==Bytes({5,6,7,8,1,2,3,4}));
        bytes={1,2,3};CHECK(!TransformBytes(bytes,Transform::Swap16)&&bytes==Bytes({1,2,3}));
        CHECK(!TransformBytes(bytes,Transform::Xor)&&bytes==Bytes({1,2,3}));
        CHECK(IntelHexRecord(1,0,{})==":00000001FF\r\n");
        CHECK(IntelHexRecord(4,0,Bytes({0,1}))==":020000040001F9\r\n");
        auto hexPath=dir/L"selection.hex";
        CHECK(SaveIntelHex(doc,hexPath.wstring(),65535,3));
        auto hexBytes=ReadFileBytes(hexPath);std::string hexText(hexBytes.begin(),hexBytes.end());
        CHECK(hexText==":020000040000FA\r\n:01FFFF00AA57\r\n:020000040001F9\r\n:02000000BBCC77\r\n:00000001FF\r\n");
        CHECK(!SaveIntelHex(doc,hexPath.wstring(),0,2));CHECK(!SaveIntelHex(doc,source.wstring(),0,2));
        CHECK(ReadFileBytes(hexPath)==hexBytes&&ReadFileBytes(source)==original);
        auto invalidHex=dir/L"invalid.hex";CHECK(!SaveIntelHex(doc,invalidHex.wstring(),doc.size,1));CHECK(!std::filesystem::exists(invalidHex));
        auto base64Path=dir/L"selection-base64.txt",arrayPath=dir/L"selection.h",escapedPath=dir/L"selection-escaped.txt";
        CHECK(SaveFormattedSelection(doc,base64Path.wstring(),65535,3,ByteFormat::Base64));
        CHECK(ReadFileBytes(base64Path)==Bytes({'q','r','v','M'}));
        CHECK(SaveFormattedSelection(doc,arrayPath.wstring(),65535,3,ByteFormat::CArray));
        auto arrayBytes=ReadFileBytes(arrayPath);CHECK(std::string(arrayBytes.begin(),arrayBytes.end())=="const unsigned char data[] = {\r\n    0xAA, 0xBB, 0xCC\r\n};");
        CHECK(SaveFormattedSelection(doc,escapedPath.wstring(),65535,3,ByteFormat::Escaped));
        auto escapedBytes=ReadFileBytes(escapedPath);CHECK(std::string(escapedBytes.begin(),escapedBytes.end())=="\\xAA\\xBB\\xCC");
        CHECK(!SaveFormattedSelection(doc,base64Path.wstring(),0,1,ByteFormat::Base64));
        CHECK(!SaveFormattedSelection(doc,source.wstring(),0,1,ByteFormat::CArray));CHECK(ReadFileBytes(source)==original);
        CHECK(!SaveFormattedSelection(doc,(dir/L"empty-format.txt").wstring(),0,0,ByteFormat::Base64));
        CHECK(!SaveFormattedSelection(doc,(dir/L"invalid-format.txt").wstring(),doc.size,1,ByteFormat::Base64));
        CHECK(SRecord(3,65535,4,Bytes({0xaa,0xbb,0xcc}))==L"S3080000FFFFAABBCCC8\r\n");
        CHECK(SRecord(7,0,4,{})==L"S70500000000FA\r\n");
        CHECK(HtmlEscape(L"<&\"test> ")==L"&lt;&amp;&quot;test&gt; ");
        auto srecPath=dir/L"selection.srec";
        CHECK(SaveDocumentSelection(doc,srecPath.wstring(),65535,3,false,0));
        auto recordBytes=ReadFileBytes(srecPath);CHECK(std::string(recordBytes.begin(),recordBytes.end())=="S0090000444154757261D5\r\nS3080000FFFFAABBCCC8\r\nS70500000000FA\r\n");
        auto html=FormattedDocumentSelection(doc,100,2,true,0);CHECK(html&&html->find(L"<td>64</td><td>82 A0</td><td>\x3042</td>")!=std::wstring::npos);
        auto htmlPath=dir/L"selection.html";CHECK(SaveDocumentSelection(doc,htmlPath.wstring(),100,2,true,0));
        CHECK(!SaveDocumentSelection(doc,srecPath.wstring(),0,1,false,0));CHECK(!SaveDocumentSelection(doc,source.wstring(),0,1,true,0));CHECK(ReadFileBytes(source)==original);
        CHECK(!FormattedDocumentSelection(doc,doc.size,1,true,0));
        auto plain=PlainSelection(doc,100,2,0);CHECK(plain&&plain->find(L"0000000000000064  82 A0")!=std::wstring::npos&&plain->find(L"\x3042")!=std::wstring::npos);
        CHECK(!PlainSelection(doc,0,0,0)&&!PlainSelection(doc,0,1048577,0)&&!PlainSelection(doc,doc.size,1,0));
        auto plainPath=dir/L"selection-table.txt";CHECK(SavePlainSelection(doc,plainPath.wstring(),100,2,0));
        auto plainBytes=ReadFileBytes(plainPath);Bytes utf8Needle={0xe3,0x81,0x82};CHECK(std::search(plainBytes.begin(),plainBytes.end(),utf8Needle.begin(),utf8Needle.end())!=plainBytes.end());
        CHECK(!SavePlainSelection(doc,plainPath.wstring(),0,1,0)&&ReadFileBytes(plainPath)==plainBytes);
        CHECK(!SavePlainSelection(doc,source.wstring(),0,1,0)&&ReadFileBytes(source)==original);
        auto halfway=MeasureScan(500,1000,2);CHECK(halfway.percent==50&&halfway.bytesPerSecond==250&&halfway.remainingSeconds==2);
        CHECK(!MeasureScan(0,1000,2).remainingSeconds&&!MeasureScan(500,1000,0.5).remainingSeconds);
        CHECK(MeasureScan(UINT64_MAX,UINT64_MAX,2).percent==100&&MeasureScan(2000,1000,2).percent==100);
        SearchJob progressJob;progressJob.kind=SearchJob::Kind::Statistics;progressJob.rangeLength=1000;progressJob.processed=500;
        CHECK(ScanStatus(progressJob,2).find(L"50%")!=std::wstring::npos&&ScanStatus(progressJob,2).find(L"2 s remaining")!=std::wstring::npos);
        progressJob.kind=SearchJob::Kind::Search;CHECK(ScanStatus(progressJob,2).find(L"2 s elapsed")!=std::wstring::npos);
        progressJob.cancel=true;CHECK(ScanStatus(progressJob,2)==L"Cancelling operation...");
        auto joinedPath=dir/L"joined.bin";uint64_t joinedProgress=0;
        CHECK(SaveJoinedCopy(doc,doc,joinedPath.wstring(),[]{return false;},[&](uint64_t n){joinedProgress=n;}));
        auto joined=ReadFileBytes(joinedPath);Bytes current;CHECK(doc.Read(0,static_cast<size_t>(doc.size),current));Bytes expectedJoined=current;expectedJoined.insert(expectedJoined.end(),current.begin(),current.end());CHECK(joined==expectedJoined&&joinedProgress==doc.size*2);
        CHECK(!SaveJoinedCopy(doc,doc,joinedPath.wstring(),[]{return false;},[](uint64_t){}));CHECK(ReadFileBytes(joinedPath)==joined);
        CHECK(!SaveJoinedCopy(doc,doc,source.wstring(),[]{return false;},[](uint64_t){}));CHECK(ReadFileBytes(source)==original);
        Document joinOther;CHECK(joinOther.Open(existing.wstring()));
        auto beforePath=dir/L"join-before.bin",afterPath=dir/L"join-after.bin";
        CHECK(SaveJoinedCopy(joinOther,doc,beforePath.wstring(),[]{return false;},[](uint64_t){}));
        CHECK(SaveJoinedCopy(doc,joinOther,afterPath.wstring(),[]{return false;},[](uint64_t){}));
        Bytes beforeExpected={'K','E','E','P'};beforeExpected.insert(beforeExpected.end(),current.begin(),current.end());
        Bytes afterExpected=current;afterExpected.insert(afterExpected.end(),{'K','E','E','P'});
        CHECK(ReadFileBytes(beforePath)==beforeExpected&&ReadFileBytes(afterPath)==afterExpected&&ReadFileBytes(existing)==Bytes({'K','E','E','P'}));
        auto splitBase=(dir/L"split.bin").wstring();CHECK(SaveSplitCopies(doc,splitBase,65536,[]{return false;},[](uint64_t){}));
        Bytes rejoined;for(uint64_t i=0;i<1+(doc.size-1)/65536;++i){auto part=ReadFileBytes(SplitPath(splitBase,i));CHECK(part.size()==(std::min)(uint64_t(65536),doc.size-i*65536));rejoined.insert(rejoined.end(),part.begin(),part.end());}CHECK(rejoined==current);
        CHECK(!SaveSplitCopies(doc,splitBase,65536,[]{return false;},[](uint64_t){}));
        auto collideBase=(dir/L"collide.bin").wstring();{NewCopy keep(SplitPath(collideBase,1));CHECK(keep.Write("KEEP",4)&&keep.Commit());}
        CHECK(!SaveSplitCopies(doc,collideBase,65536,[]{return false;},[](uint64_t){}));CHECK(!std::filesystem::exists(SplitPath(collideBase,0))&&ReadFileBytes(SplitPath(collideBase,1))==Bytes({'K','E','E','P'}));
        auto splitCancelBase=(dir/L"cancel-split.bin").wstring();bool splitCancel=false;
        CHECK(!SaveSplitCopies(doc,splitCancelBase,65536,[&]{return splitCancel;},[&](uint64_t){splitCancel=true;}));CHECK(!std::filesystem::exists(SplitPath(splitCancelBase,0))&&!std::filesystem::exists(SplitPath(splitCancelBase,1)));
        CHECK(!SaveSplitCopies(doc,(dir/L"too-many.bin").wstring(),1,[]{return false;},[](uint64_t){}));CHECK(ReadFileBytes(source)==original);
        auto cancelledJoin=dir/L"cancelled-join.bin";bool cancelJoin=false;
        CHECK(!SaveJoinedCopy(doc,doc,cancelledJoin.wstring(),[&]{return cancelJoin;},[&](uint64_t){cancelJoin=true;}));CHECK(!std::filesystem::exists(cancelledJoin));
        Bytes imported;CHECK(DecodePaste(L"\\x00 \\xFF",PasteFormat::Escaped,0,imported)&&imported==Bytes({0,255}));
        CHECK(!DecodePaste(L"\\x0G",PasteFormat::Escaped,0,imported));CHECK(!DecodePaste(L"\\x0",PasteFormat::Escaped,0,imported));
        CHECK(FormatBytes(Bytes({0,255}),ByteFormat::Binary)==L"00000000 11111111");
        CHECK(DecodePaste(L"00000000 11111111",PasteFormat::Binary,0,imported)&&imported==Bytes({0,255}));CHECK(!DecodePaste(L"101",PasteFormat::Binary,0,imported));
        Bytes pasteDecoded;
        CHECK(DecodePaste(L" AAE=\r\n",PasteFormat::Base64,0,pasteDecoded)&&pasteDecoded==Bytes({0,1}));
        CHECK(DecodePaste(L"/w==",PasteFormat::Base64,0,pasteDecoded)&&pasteDecoded==Bytes({255}));
        CHECK(DecodePaste(L"AQID",PasteFormat::Base64,0,pasteDecoded)&&pasteDecoded==Bytes({1,2,3}));
        for(auto invalid:{L"A",L"A===",L"AB==",L"AAF=",L"AA==AAAA",L"!!!!"})CHECK(!DecodePaste(invalid,PasteFormat::Base64,0,pasteDecoded)&&pasteDecoded.empty());
        CHECK(DecodePaste(L"\x3042",PasteFormat::Decoded,0,pasteDecoded)&&pasteDecoded==Bytes({0x82,0xa0}));
        CHECK(DecodePaste(L"\x3042",PasteFormat::Utf8,0,pasteDecoded)&&pasteDecoded==Bytes({0xe3,0x81,0x82}));
        CHECK(!DecodePaste(L"\x3042",PasteFormat::Decoded,1,pasteDecoded)&&!DecodePaste(L"\xd800",PasteFormat::Utf8,0,pasteDecoded));
        CHECK(!DecodePaste(std::wstring(65537,L'A'),PasteFormat::Decoded,1,pasteDecoded));
        CHECK(PointerDestination(Bytes({0x34,0x12}),2,false,false,0,65536)==0x1234);
        CHECK(PointerDestination(Bytes({0x12,0x34}),2,true,false,0,65536)==0x1234);
        CHECK(PointerDestination(Bytes({0xfe,0xff}),2,false,true,10,20)==8);
        CHECK(PointerDestination(Bytes({0x0a,0}),2,false,true,10,20)==20);
        CHECK(!PointerDestination(Bytes({0xff,0xff}),2,false,true,0,20));
        CHECK(!PointerDestination(Bytes({0xff,0xff}),2,false,false,0,20));
        CHECK(!PointerDestination(Bytes({1}),2,false,false,0,20));
        CHECK(!PointerDestination(Bytes({0,0,0,0,0,0,0,0x80}),8,false,true,10,20));
        CHECK(PointerDestination(Bytes({0xff,0xff,0xff}),3,false,true,10,20)==9);
        CHECK(ValueStep(0,32,8,true)==8&&ValueStep(8,32,8,false)==0);
        CHECK(ValueStep(32,32,8,false)==24);CHECK(!ValueStep(0,32,8,false)&&!ValueStep(24,32,8,true));
        CHECK(!ValueStep(UINT64_MAX-1,UINT64_MAX,8,true)&&!ValueStep(0,4,8,true));
        CHECK(InspectorInteger(255,1,true,true)==L"-0x1"&&InspectorInteger(255,1,false,true)==L"0xFF");
        CHECK(InspectorInteger(uint64_t(1)<<63,8,true,true)==L"-0x8000000000000000");
        CHECK(InspectorInteger(uint64_t(1)<<63,8,true,false)==L"-9223372036854775808");CHECK(InspectorInteger(UINT64_MAX,8,false,false)==L"18446744073709551615");
        CHECK(RtfEscape(L"{\\\x3042}\r\n")==L"\\{\\\\\\u12354?\\}\\par\r\n");
        auto rich=RichSelection(doc,100,2,true,0);CHECK(rich&&rich->find(L"\\u12354?")!=std::wstring::npos);
        auto tex=RichSelection(doc,65535,3,false,0);CHECK(tex&&tex->find(L"000000000000FFFF  AA BB CC")!=std::wstring::npos);
        auto rtfPath=dir/L"selection.rtf",texPath=dir/L"selection.tex";
        CHECK(SaveRichSelection(doc,rtfPath.wstring(),100,2,true,0));CHECK(SaveRichSelection(doc,texPath.wstring(),65535,3,false,0));
        CHECK(!SaveRichSelection(doc,rtfPath.wstring(),0,1,true,0));CHECK(!SaveRichSelection(doc,source.wstring(),0,1,false,0));CHECK(ReadFileBytes(source)==original);
        auto reportPath=dir/L"analysis.txt";
        CHECK(SaveUtf8Copy(reportPath.wstring(),L"Report: \x3042\r\n"));
        CHECK(ReadFileBytes(reportPath)==Bytes({'R','e','p','o','r','t',':',' ',0xe3,0x81,0x82,'\r','\n'}));
        CHECK(!SaveUtf8Copy(reportPath.wstring(),L"replace"));CHECK(!SaveUtf8Copy(source.wstring(),L"replace"));
        CHECK(SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)));
        {
            Microsoft::WRL::ComPtr<IFileOpenDialog> picker;
            CHECK(SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(picker.GetAddressOf()))));
            CHECK(SUCCEEDED(ConfigureDatPicker(picker.Get(),dir.wstring(),L"Open DAT")));
            FILEOPENDIALOGOPTIONS options={};CHECK(SUCCEEDED(picker->GetOptions(&options)));
            CHECK((options&FOS_FILEMUSTEXIST)&&(options&FOS_FORCEFILESYSTEM));
            Microsoft::WRL::ComPtr<IShellItem> folder;CHECK(SUCCEEDED(picker->GetFolder(folder.GetAddressOf())));
            PWSTR name=nullptr;CHECK(SUCCEEDED(folder->GetDisplayName(SIGDN_FILESYSPATH,&name)));
            CHECK(std::filesystem::equivalent(std::filesystem::path(name),dir));CoTaskMemFree(name);
            picker.Reset();
            CHECK(SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(picker.GetAddressOf()))));
            CHECK(SUCCEEDED(ConfigureDatPicker(picker.Get(),(dir/L"missing-folder").wstring(),L"Open DAT")));
        }
        expectedNativeFolder=dir.wstring();std::wstring pickerResult;
        CHECK(!PickDatFileWithShow(nullptr,dir.wstring(),L"Open DAT",pickerResult,CancelConfiguredPicker));
        CHECK(nativePickerSta&&nativePickerConfigured&&nativePickerThread==GetCurrentThreadId()&&pickerResult.empty());
        CoUninitialize();
        // Reproduce the real application's audio-initialized COM apartment.
        CHECK(SUCCEEDED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)));
        nativePickerSta=nativePickerConfigured=false;
        CHECK(!PickDatFileWithShow(nullptr,dir.wstring(),L"Open DAT",pickerResult,CancelConfiguredPicker));
        CHECK(nativePickerSta&&nativePickerConfigured&&nativePickerThread!=GetCurrentThreadId()&&pickerResult.empty());
        CHECK(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)==RPC_E_CHANGED_MODE);CoUninitialize();
        CHECK(BrowseRoot(nullptr)==L"C:\\Program Files (x86)\\PlayOnline\\SquareEnix\\FINAL FANTASY XI\\");
        CHECK(Decode(Bytes({0x82,0xa0,0xa6}).data(),3,true)==L"\x3042\xff66");
        CHECK(DecodeEncoding(Bytes({0x80,0xe9}).data(),2,2)==L"\x20ac\x00e9");
        CHECK(DecodeEncoding(Bytes({0x82,0xb3}).data(),2,3)==L"\x00e9\x2502");
        CHECK(DecodeEncoding(Bytes({0,1,0x41}).data(),3,2)==L"_.A");
        if(IsValidCodePage(500))CHECK(DecodeEncoding(Bytes({0xc1,0x40,0xc2}).data(),3,4)==L"A B");
        if(IsValidCodePage(10000))CHECK(DecodeEncoding(Bytes({0x80}).data(),1,5)==L"\x00c4");

        auto s=std::make_unique<State>();CHECK(s->document->Open(source.wstring()));s->root=BrowseRoot(nullptr);
        Win32ToolWindow::WideSpec spec{Procedure,L"DatHexEditorTest",L"Test",1180,760};spec.createParameter=s.get();
        HWND h=Win32ToolWindow::Create(nullptr,spec);CHECK(h);Refresh(h,*s);
        CHECK(SUCCEEDED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)));
        CHECK(IsWindowEnabled(h));
        CHECK(!PickDatFileWithShow(h,dir.wstring(),L"Open DAT",pickerResult,CancelConfiguredPicker));
        CHECK(IsWindowEnabled(h)&&nativePickerSta&&nativePickerConfigured);
        // Exercise the real shell Show/modal loop; its own timer cancels it.
        if(GetEnvironmentVariableW(L"DATURA_TEST_NATIVE_PICKER",nullptr,0)) {
        HRESULT shown=OnPickerSta(h,[&] {
            Microsoft::WRL::ComPtr<IFileOpenDialog> picker;
            HRESULT status=CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(picker.GetAddressOf()));
            if(SUCCEEDED(status))status=ConfigureDatPicker(picker.Get(),dir.wstring(),L"DAT picker regression test (automatically closes)");
            if(SUCCEEDED(status))status=ShowAndCancelRealPicker(picker.Get(),h);return status;
        });
        CHECK(shown==HRESULT_FROM_WIN32(ERROR_CANCELLED)&&realPickerPumped&&IsWindowEnabled(h));
        }
        CoUninitialize();
        CHECK(TabCtrl_GetItemCount(GetDlgItem(h,ToolTabs))==5);
        for(int tab=0;tab<5;++tab){ActivateToolTab(h,*s,tab);for(const auto& item:ToolControls)CHECK(bool(GetWindowLongPtrW(GetDlgItem(h,item.id),GWL_STYLE)&WS_VISIBLE)==(item.tab==tab));}
        CHECK(GetWindowLongPtrW(GetDlgItem(h,Open),GWL_STYLE)&WS_VISIBLE);CHECK(GetWindowLongPtrW(GetDlgItem(h,SaveCopy),GWL_STYLE)&WS_VISIBLE);
        SetDlgItemTextW(h,SearchText,L"AA BB");ActivateToolTab(h,*s,0);ActivateToolTab(h,*s,2);CHECK(Text(h,SearchText)==L"AA BB");SetDlgItemTextW(h,SearchText,L"");ActivateToolTab(h,*s,0);
        HWND rtfReader=CreateWindowExW(0,MSFTEDIT_CLASS,L"",WS_POPUP|ES_MULTILINE,0,0,600,400,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);CHECK(rtfReader);
        RtfInput rtfInput;for(wchar_t c:*rich){CHECK(c<=127);rtfInput.text.push_back(static_cast<char>(c));}EDITSTREAM rtfStream={};rtfStream.dwCookie=reinterpret_cast<DWORD_PTR>(&rtfInput);rtfStream.pfnCallback=ReadRtf;
        SendMessageW(rtfReader,EM_STREAMIN,SF_RTF,reinterpret_cast<LPARAM>(&rtfStream));CHECK(rtfStream.dwError==0);
        std::wstring decodedRtf(GetWindowTextLengthW(rtfReader)+1,L'\0');GetWindowTextW(rtfReader,decodedRtf.data(),static_cast<int>(decodedRtf.size()));CHECK(decodedRtf.find(L"\x3042")!=std::wstring::npos);DestroyWindow(rtfReader);

        // Exercise the actual button dispatch without making the user interact
        // with a modal picker. The real native picker is configured above.
        s->root=dir.wstring();
        s->pickDatFile=[](HWND,const std::wstring& root,const wchar_t* title,std::wstring& result) {
            ++pickerCalls;CHECK(std::wstring(title)==L"Open DAT");
            result=(std::filesystem::path(root)/L"original.DAT").wstring();return true;
        };
        s->picking=true;SendDlgItemMessageW(h,Open,BM_CLICK,0,0);CHECK(pickerCalls==0);SendMessageW(h,WM_CLOSE,0,0);CHECK(IsWindow(h));s->picking=false;
        auto beforeOpen=s->document;SendDlgItemMessageW(h,Open,BM_CLICK,0,0);
        CHECK(pickerCalls==1&&s->document!=beforeOpen&&s->document->path==source.wstring());
        s->pickDatFile=[](HWND,const std::wstring&,const wchar_t*,std::wstring& result){result.clear();return false;};
        auto beforeCancel=s->document;SendDlgItemMessageW(h,Open,BM_CLICK,0,0);CHECK(s->document==beforeCancel);
        auto colorAt=[&](size_t position){SendDlgItemMessageW(h,Dump,EM_SETSEL,position,position+1);CHARFORMAT2W format={};format.cbSize=sizeof(format);SendDlgItemMessageW(h,Dump,EM_GETCHARFORMAT,SCF_SELECTION,reinterpret_cast<LPARAM>(&format));return format.crTextColor;};
        CHECK(colorAt(0)==RGB(45,45,210));size_t byteZero=0;while(s->positions[byteZero]!=0)++byteZero;CHECK(colorAt(byteZero)==RGB(0,0,0));
        size_t byteOne=byteZero;while(s->positions[byteOne]!=1)++byteOne;CHECK(colorAt(byteOne)==RGB(145,75,55));
        CHECK(colorAt(s->text.find(L'\r')+1)==RGB(45,45,210));Select(h,*s,0,1);
        CHECK(!IsWindowEnabled(GetDlgItem(h,Overwrite)));CHECK(IsWindowEnabled(GetDlgItem(h,SaveCopy)));
        auto japanesePosition=s->text.find(L"\x3042");CHECK(japanesePosition!=std::wstring::npos);
        SendDlgItemMessageW(h,Dump,EM_SETSEL,japanesePosition,japanesePosition);MouseSelection(h,*s);
        CHECK(s->selection==100&&s->length==2);
        for(int id:{Group1,Group2,Group4,Group8,Group16}) {
            Command(h,*s,id);CHECK(s->groupBytes==(1u<<(id-Group1)));
            CHECK(s->selection==100&&s->length==2);
            size_t at=0;while(at<s->positions.size()&&s->positions[at]!=1)++at;CHECK(at<s->positions.size());
            SendDlgItemMessageW(h,Dump,EM_SETSEL,at,at+2);MouseSelection(h,*s);CHECK(s->selection==1&&s->length==1);Select(h,*s,100,2);
        }
        Command(h,*s,ViewHex);CHECK(s->displayMode==1&&s->text.find(L"Decoded text")==std::wstring::npos&&s->text.find(L"\x3042")==std::wstring::npos);
        Command(h,*s,ViewText);CHECK(s->displayMode==2&&s->text.find(L"\x3042")!=std::wstring::npos);
        Select(h,*s,101,1);DWORD glyphStart=0,glyphEnd=0;
        SendDlgItemMessageW(h,Dump,EM_GETSEL,reinterpret_cast<WPARAM>(&glyphStart),reinterpret_cast<LPARAM>(&glyphEnd));
        CHECK(glyphEnd>glyphStart&&s->text[glyphStart]==L'\x3042');
        Command(h,*s,ViewBoth);Command(h,*s,Group1);Select(h,*s,100,2);
        Select(h,*s,65535,3);CHECK(s->selection==65535&&s->length==3);
        DWORD start=0,end=0;SendDlgItemMessageW(h,Dump,EM_GETSEL,reinterpret_cast<WPARAM>(&start),reinterpret_cast<LPARAM>(&end));
        CHECK(end>start);
        CHECK(Text(h,Inspector).find(L"UInt8")!=std::wstring::npos);
        CHECK(s->text.find(L"AA")!=std::wstring::npos);
        s->offset=65536;Refresh(h,*s);CHECK(!IsWindowEnabled(GetDlgItem(h,Next)));
        CHECK(IsWindowEnabled(GetDlgItem(h,Previous)));
        Select(h,*s,0,1);SetDlgItemTextW(h,SearchText,L"AA BB CC");Command(h,*s,Find);
        CHECK(s->job);CHECK(!IsWindowEnabled(GetDlgItem(h,Overwrite)));
        for(int i=0;i<500&&!s->job->done.load();++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        CHECK(s->job->done.load());SendMessageW(h,WM_TIMER,1,0);
        CHECK(!s->job&&s->selection==65535&&s->length==3);
        SendDlgItemMessageW(h,EditMode,BM_SETCHECK,BST_CHECKED,0);Command(h,*s,EditMode);
        CHECK(IsWindowEnabled(GetDlgItem(h,Overwrite)));
        Select(h,*s,0,2);SetDlgItemTextW(h,PatchText,L"FE FF");Command(h,*s,Overwrite);
        CHECK(s->document->Read(0,2,bytes)&&bytes==Bytes({0xfe,0xff}));
        Command(h,*s,Undo);CHECK(!s->document->Dirty());Command(h,*s,Redo);CHECK(s->document->Dirty());
        Select(h,*s,100,2);Command(h,*s,AddBookmark);CHECK(s->bookmarks.size()==1);
        Select(h,*s,65536,1);Command(h,*s,GoBookmark);CHECK(s->selection==100&&s->length==2);
        Command(h,*s,RemoveBookmark);CHECK(s->bookmarks.empty());CHECK(!IsWindowEnabled(GetDlgItem(h,GoBookmark)));
        CHECK(BookmarkShortcut('0',true,true)==SetNumberedBookmark);CHECK(BookmarkShortcut('9',true,false)==GoNumberedBookmark+9);
        CHECK(BookmarkShortcut('9',false,false)==0&&BookmarkShortcut('A',true,true)==0);
        Select(h,*s,100,2);Command(h,*s,SetNumberedBookmark+5);CHECK(s->bookmarks.size()==1&&s->bookmarks[0].slot==5);
        CHECK(Text(h,BookmarkList).find(L"[5]")!=std::wstring::npos);
        Select(h,*s,65535,3);Command(h,*s,GoNumberedBookmark+5);CHECK(s->selection==100&&s->length==2);
        Select(h,*s,65535,3);Command(h,*s,SetNumberedBookmark+5);CHECK(s->bookmarks.size()==1);
        Select(h,*s,0,1);Command(h,*s,GoNumberedBookmark+5);CHECK(s->selection==65535&&s->length==3);
        Command(h,*s,RemoveBookmark);CHECK(s->bookmarks.empty());Command(h,*s,GoNumberedBookmark+5);
        CHECK(Text(h,Status)==L"This numbered bookmark has not been set.");

        Select(h,*s,100,1);CHECK(s->document->Overwrite(100,Bytes({1,0,0x80,0x3f})));Inspect(h,*s);CHECK(Text(h,Inspector).find(L"Float32  1.00000012")!=std::wstring::npos);Command(h,*s,Undo);
        CHECK(s->document->Overwrite(100,Bytes({1,0,0,0,0,0,0xf0,0x3f})));Inspect(h,*s);CHECK(Text(h,Inspector).find(L"Float64  1.0000000000000002")!=std::wstring::npos);Command(h,*s,Undo);
        Select(h,*s,100,2);CHECK(PasteBytes(h,*s,Bytes({0,255})));CHECK(s->document->Read(100,2,bytes)&&bytes==Bytes({0,255}));Command(h,*s,Undo);CHECK(s->document->Read(100,2,bytes)&&bytes==Bytes({0x82,0xa0}));
        Select(h,*s,65535,1);MoveNibble(h,*s,true);CHECK(s->selection==65535&&s->activeNibble==1);
        TypeHex(h,*s,L'5');CHECK(s->document->Read(65535,1,bytes)&&bytes==Bytes({0xa5})&&s->selection==65536&&s->activeNibble==0);
        TypeHex(h,*s,L'1');CHECK(s->document->Read(65536,1,bytes)&&bytes==Bytes({0x1b})&&s->activeNibble==1);
        Command(h,*s,Undo);CHECK(s->document->Read(65536,1,bytes)&&bytes==Bytes({0xbb}));Command(h,*s,Undo);CHECK(s->document->Read(65535,1,bytes)&&bytes==Bytes({0xaa}));
        Select(h,*s,0,1);MoveNibble(h,*s,false);CHECK(s->selection==0&&s->activeNibble==0);
        SelectNibble(h,*s,s->document->size-1,1);MoveNibble(h,*s,true);CHECK(s->selection==s->document->size-1&&s->activeNibble==1);
        s->edit=false;DatHex::Bytes nibbleBefore;CHECK(s->document->Read(s->selection,1,nibbleBefore));TypeHex(h,*s,L'0');CHECK(s->document->Read(s->selection,1,bytes)&&bytes==nibbleBefore);s->edit=true;
        SendDlgItemMessageW(h,Dump,WM_KEYDOWN,VK_ESCAPE,0);CHECK(s->activeNibble==-1);
        Select(h,*s,0,1);SendDlgItemMessageW(h,Dump,WM_CHAR,L'A',0);
        CHECK(s->highNibble==10);CHECK(s->document->Read(0,1,bytes)&&bytes[0]==0xfe);
        SendDlgItemMessageW(h,Dump,WM_CHAR,L'B',0);
        CHECK(s->highNibble<0&&s->selection==1);CHECK(s->document->Read(0,1,bytes)&&bytes[0]==0xab);
        Command(h,*s,Undo);CHECK(s->document->Read(0,1,bytes)&&bytes[0]==0xfe);
        Select(h,*s,0,2);CHECK(PasteHex(h,*s,L"12 34"));CHECK(s->document->Read(0,2,bytes)&&bytes==Bytes({0x12,0x34}));
        Command(h,*s,Undo);CHECK(s->document->Read(0,2,bytes)&&bytes==Bytes({0xfe,0xff}));
        Select(h,*s,65535,1);SendDlgItemMessageW(h,Dump,WM_KEYDOWN,VK_RIGHT,0);
        CHECK(s->selection==65536&&s->offset==65536);
        SendDlgItemMessageW(h,Dump,WM_KEYDOWN,VK_UP,0);CHECK(s->selection==65536-s->rowBytes&&s->offset==0);
        auto complete=[&] {
            CHECK(s->job);
            for(int i=0;i<500&&!s->job->done.load();++i)std::this_thread::sleep_for(std::chrono::milliseconds(10));
            CHECK(s->job->done.load());SendMessageW(h,WM_TIMER,1,0);CHECK(!s->job);
        };
        SendDlgItemMessageW(h,FindMode,CB_SETCURSEL,14,0);
        CHECK(SearchPattern(h,L"A\x3042\xd83d\xde00",bytes)&&bytes==Bytes({0x41,0xe3,0x81,0x82,0xf0,0x9f,0x98,0x80}));
        CHECK(!SearchPattern(h,L"\xd800",bytes));CHECK(!SearchPattern(h,L"\xdc00",bytes));
        SendDlgItemMessageW(h,FindMode,CB_SETCURSEL,15,0);
        CHECK(SearchPattern(h,L"A\x3042",bytes)&&bytes==Bytes({0x41,0,0x42,0x30}));
        SendDlgItemMessageW(h,FindMode,CB_SETCURSEL,16,0);
        CHECK(SearchPattern(h,L"A\x3042",bytes)&&bytes==Bytes({0,0x41,0x30,0x42}));
        CHECK(!SearchPattern(h,L"",bytes));
        Select(h,*s,65535,1);SetDlgItemTextW(h,GoText,L"+1");Command(h,*s,Go);
        CHECK(s->selection==65536&&s->offset==65536);
        SetDlgItemTextW(h,GoText,L"-0x10");Command(h,*s,Go);CHECK(s->selection==65520&&s->offset==0);
        Command(h,*s,SelectToEnd);CHECK(s->selection==65520&&s->length==20);
        Select(h,*s,100,1);Command(h,*s,SelectToStart);CHECK(s->selection==0&&s->length==100);
        Command(h,*s,FileEnd);CHECK(s->selection==s->document->size&&s->length==0);
        Command(h,*s,FileStart);CHECK(s->selection==0&&s->length==1);
        Command(h,*s,NextModified);CHECK(s->selection==1);Command(h,*s,NextModified);CHECK(s->selection==0);
        Command(h,*s,PreviousModified);CHECK(s->selection==1);
        SendDlgItemMessageW(h,FindMode,CB_SETCURSEL,17,0);SetDlgItemTextW(h,SearchText,L"10101010 10111011 11001100");
        Command(h,*s,Find);complete();CHECK(s->selection==65535&&s->length==3);
        SetDlgItemTextW(h,ReplacementText,L"00000001 00000010 00000011");Command(h,*s,ReplaceMatch);
        CHECK(s->document->Read(65535,3,bytes)&&bytes==Bytes({1,2,3}));Command(h,*s,Undo);
        SendDlgItemMessageW(h,FindMode,CB_SETCURSEL,1,0);SendDlgItemMessageW(h,Encoding,CB_SETCURSEL,2,0);Command(h,*s,Encoding);
        CHECK(s->encodingIndex==2);CHECK(SearchPattern(h,L"\x20ac\x00e9",bytes)&&bytes==Bytes({0x80,0xe9}));
        CHECK(!SearchPattern(h,L"\x3042",bytes));
        SendDlgItemMessageW(h,Encoding,CB_SETCURSEL,3,0);Command(h,*s,Encoding);
        CHECK(SearchPattern(h,L"\x00e9",bytes)&&bytes==Bytes({0x82}));
        SendDlgItemMessageW(h,Encoding,CB_SETCURSEL,0,0);Command(h,*s,Encoding);
        SendDlgItemMessageW(h,FindMode,CB_SETCURSEL,18,0);CHECK(SearchPattern(h,L"624485",bytes)&&bytes==Bytes({0xe5,0x8e,0x26}));CHECK(!SearchPattern(h,L"-1",bytes));
        CHECK(SearchPattern(h,L"18446744073709551615",bytes)&&bytes==EncodeLeb128(UINT64_MAX,false));CHECK(!SearchPattern(h,L"18446744073709551616",bytes));
        SendDlgItemMessageW(h,FindMode,CB_SETCURSEL,19,0);CHECK(SearchPattern(h,L"-624485",bytes)&&bytes==Bytes({0x9b,0xf1,0x59}));CHECK(SearchPattern(h,L"-9223372036854775808",bytes)&&bytes==EncodeLeb128(uint64_t(1)<<63,true));CHECK(!SearchPattern(h,L"9223372036854775808",bytes));
        Select(h,*s,0,1);CHECK(s->document->Overwrite(0,Bytes({100,0})));Command(h,*s,Pointer16);CHECK(s->selection==100);Command(h,*s,Undo);
        Select(h,*s,65532,1);Command(h,*s,NextValue4);CHECK(s->selection==65536&&s->length==4);Command(h,*s,PreviousValue4);CHECK(s->selection==65532&&s->length==4);
        Select(h,*s,0,1);Command(h,*s,PreviousValue8);CHECK(s->selection==0&&s->length==1);
        // Typed searches honor width, signedness, range, and endian.
        SendDlgItemMessageW(h,FindMode,CB_SETCURSEL,6,0); // UInt24
        CHECK(SearchPattern(h,L"0x123456",bytes)&&bytes==Bytes({0x56,0x34,0x12}));
        CHECK(!SearchPattern(h,L"16777216",bytes));
        SendDlgItemMessageW(h,Endian,CB_SETCURSEL,1,0);
        CHECK(SearchPattern(h,L"1193046",bytes)&&bytes==Bytes({0x12,0x34,0x56}));
        SendDlgItemMessageW(h,FindMode,CB_SETCURSEL,7,0); // Int24
        CHECK(SearchPattern(h,L"-1",bytes)&&bytes==Bytes({0xff,0xff,0xff}));
        CHECK(!SearchPattern(h,L"-8388609",bytes));
        SendDlgItemMessageW(h,Endian,CB_SETCURSEL,0,0);
        SendDlgItemMessageW(h,FindMode,CB_SETCURSEL,12,0);
        CHECK(SearchPattern(h,L"1.5",bytes)&&bytes==Bytes({0,0,0xc0,0x3f}));
        CHECK(!SearchPattern(h,L"nan",bytes));CHECK(!SearchPattern(h,L"1.5 trailing",bytes));
        SendDlgItemMessageW(h,FindMode,CB_SETCURSEL,10,0);
        CHECK(SearchPattern(h,L"18446744073709551615",bytes)&&bytes==Bytes(8,0xff));
        CHECK(!SearchPattern(h,L"18446744073709551616",bytes));
        SendDlgItemMessageW(h,FindMode,CB_SETCURSEL,0,0);
        SetDlgItemTextW(h,SearchText,L"FE FF");SetDlgItemTextW(h,ReplacementText,L"12 34");
        Select(h,*s,0,2);Command(h,*s,Find);complete();CHECK(s->selection==0&&s->length==2);
        Command(h,*s,ReplaceMatch);CHECK(s->document->Read(0,2,bytes)&&bytes==Bytes({0x12,0x34}));
        CHECK(s->document->Modified(0));
        // Modified bytes receive a distinct foreground color in both columns.
        size_t colorPosition=0;while(colorPosition<s->positions.size()&&s->positions[colorPosition]!=0)++colorPosition;
        SendDlgItemMessageW(h,Dump,EM_SETSEL,colorPosition,colorPosition+1);
        CHARFORMAT2W color={};color.cbSize=sizeof(color);
        SendDlgItemMessageW(h,Dump,EM_GETCHARFORMAT,SCF_SELECTION,reinterpret_cast<LPARAM>(&color));
        CHECK(color.crTextColor==RGB(190,45,25));
        Command(h,*s,Undo);CHECK(s->document->Read(0,2,bytes)&&bytes==Bytes({0xfe,0xff}));
        Select(h,*s,0,16);CHECK(Text(h,Inspector).find(L"UInt24")!=std::wstring::npos);
        CHECK(Text(h,Inspector).find(L"GUID (Windows layout)")!=std::wstring::npos);
        SetDlgItemTextW(h,SearchText,L"AA BB CC");SetDlgItemTextW(h,ReplacementText,L"01 02 03");
        Command(h,*s,ReplaceAll);CHECK(!IsWindowEnabled(GetDlgItem(h,ReplaceAll)));complete();
        CHECK(s->document->Read(65535,3,bytes)&&bytes==Bytes({1,2,3}));
        CHECK(Text(h,AnalysisOutput).find(L"1 non-overlapping matches replaced")!=std::wstring::npos);
        Command(h,*s,Undo);CHECK(s->document->Read(65535,3,bytes)&&bytes==Bytes({0xaa,0xbb,0xcc}));
        Command(h,*s,ReplaceAll);Command(h,*s,Cancel);complete();
        CHECK(s->document->Read(65535,3,bytes)&&bytes==Bytes({0xaa,0xbb,0xcc}));
        CHECK(Text(h,Status)==L"Operation cancelled.");
        SendDlgItemMessageW(h,SearchScope,CB_SETCURSEL,1,0);Select(h,*s,65534,5);
        SetDlgItemTextW(h,SearchText,L"AA BB CC");Command(h,*s,Find);complete();CHECK(s->selection==65535&&s->length==3);
        CHECK(s->lastSearchStart==65534&&s->lastSearchLength==5);
        Command(h,*s,FindNext);complete();CHECK(s->selection==65535);
        Select(h,*s,0,1);Command(h,*s,FindNext);complete();CHECK(s->selection==65535);
        Command(h,*s,FindPrevious);complete();CHECK(s->selection==65535);
        Select(h,*s,3,5);SetDlgItemTextW(h,SearchText,L"00 00");Command(h,*s,FindAll);complete();
        CHECK(s->matches==std::vector<uint64_t>({3,4,5,6}));
        SetDlgItemTextW(h,ReplacementText,L"09 08");Select(h,*s,3,5);Command(h,*s,ReplaceAll);complete();
        CHECK(s->document->Read(2,7,bytes)&&bytes==Bytes({0,9,8,9,8,0,0}));Command(h,*s,Undo);
        SendDlgItemMessageW(h,SearchScope,CB_SETCURSEL,0,0);
        SetDlgItemTextW(h,SearchText,L"AA BB CC");Command(h,*s,FindAll);complete();
        CHECK(s->matches==std::vector<uint64_t>({65535})&&s->matchLength==3);
        CHECK(IsWindowEnabled(GetDlgItem(h,GoMatch)));Command(h,*s,GoMatch);CHECK(s->selection==65535&&s->length==3);
        Select(h,*s,0,1);Command(h,*s,FindPrevious);complete();CHECK(s->selection==65535); // wrap
        Select(h,*s,65538,1);Command(h,*s,FindPrevious);complete();CHECK(s->selection==65535);
        SetDlgItemTextW(h,SearchText,L"00 00");Command(h,*s,FindAll);complete();
        CHECK(s->matches.size()==10000&&s->matches[0]==2&&s->matches[1]==3);
        CHECK(Text(h,AnalysisOutput).find(L"first 10,000")!=std::wstring::npos);
        // Cancelled jobs publish no incomplete results and leave navigation available.
        Command(h,*s,FindAll);Command(h,*s,Cancel);complete();
        CHECK(Text(h,Status)==L"Operation cancelled.");CHECK(s->matches.size()==10000);
        Select(h,*s,0,2);CHECK(PasteHex(h,*s,L"10 20"));CHECK(s->matches.empty());
        CHECK(!IsWindowEnabled(GetDlgItem(h,GoMatch)));Command(h,*s,Undo);
        Command(h,*s,ByteStatistics);CHECK(!IsWindowEnabled(GetDlgItem(h,FindAll)));complete();
        CHECK(Text(h,AnalysisOutput).find(L"Entropy:")!=std::wstring::npos);
        CHECK(s->chartStatistics&&s->chartStatistics->total==s->document->size&&IsWindowEnabled(GetDlgItem(h,DistributionChart)));
        CHECK(Text(h,AnalysisOutput).find(L"FF")!=std::wstring::npos);
        Select(h,*s,65534,5);SetDlgItemTextW(h,PatchText,L"12 34");Command(h,*s,Fill);
        CHECK(s->document->Read(65534,5,bytes)&&bytes==Bytes({0x12,0x34,0x12,0x34,0x12}));
        CHECK(s->document->size==original.size());Command(h,*s,Undo);
        CHECK(s->document->Read(65534,5,bytes)&&bytes==Bytes({0,0xaa,0xbb,0xcc,0}));
        Command(h,*s,Redo);CHECK(s->document->Read(65534,5,bytes)&&bytes==Bytes({0x12,0x34,0x12,0x34,0x12}));Command(h,*s,Undo);
        Select(h,*s,65534,5);SendDlgItemMessageW(h,ByteOperation,CB_SETCURSEL,0,0);Command(h,*s,ApplyOperation);
        CHECK(s->document->Read(65534,5,bytes)&&bytes==Bytes({0xff,0x55,0x44,0x33,0xff}));Command(h,*s,Undo);
        SetDlgItemTextW(h,PatchText,L"FF 0F");SendDlgItemMessageW(h,ByteOperation,CB_SETCURSEL,3,0);Command(h,*s,ApplyOperation);
        CHECK(s->document->Read(65534,5,bytes)&&bytes==Bytes({0xff,0xa5,0x44,0xc3,0xff}));Command(h,*s,Undo);
        SendDlgItemMessageW(h,ByteOperation,CB_SETCURSEL,4,0);Command(h,*s,ApplyOperation);
        CHECK(s->document->Read(65534,5,bytes)&&bytes==Bytes({0,0xcc,0xbb,0xaa,0}));
        Command(h,*s,Undo);Command(h,*s,Redo);CHECK(s->document->Read(65534,5,bytes)&&bytes==Bytes({0,0xcc,0xbb,0xaa,0}));Command(h,*s,Undo);
        SendDlgItemMessageW(h,EditMode,BM_SETCHECK,BST_UNCHECKED,0);Command(h,*s,EditMode);
        CHECK(!IsWindowEnabled(GetDlgItem(h,ApplyOperation)));Command(h,*s,ApplyOperation);
        CHECK(s->document->Read(65534,5,bytes)&&bytes==Bytes({0,0xaa,0xbb,0xcc,0}));
        SendDlgItemMessageW(h,EditMode,BM_SETCHECK,BST_CHECKED,0);Command(h,*s,EditMode);
        CHECK(IsWindowEnabled(GetDlgItem(h,CopyFormatted)));
        Command(h,*s,Checksums);CHECK(!IsWindowEnabled(GetDlgItem(h,PasteOverwrite)));CHECK(!IsWindowEnabled(GetDlgItem(h,CopyFormatted)));complete();
        CHECK(Text(h,AnalysisOutput).find(L"SHA-256")!=std::wstring::npos);
        SendDlgItemMessageW(h,ChecksumType,CB_SETCURSEL,6,0);SendDlgItemMessageW(h,AnalysisScope,CB_SETCURSEL,1,0);
        Select(h,*s,65535,3);Command(h,*s,Checksums);
        CHECK(s->job->rangeStart==65535&&s->job->rangeLength==3&&s->job->hashAlgorithm==HashAlgorithm::Sha512);
        CHECK(!IsWindowEnabled(GetDlgItem(h,AnalysisScope)));complete();
        CHECK(Text(h,AnalysisOutput).find(L"Range: 0xFFFF + 3 bytes")!=std::wstring::npos);
        CHECK(Text(h,AnalysisOutput).find(L"SHA-512")!=std::wstring::npos);
        Command(h,*s,ByteStatistics);complete();CHECK(Text(h,AnalysisOutput).find(L"Bytes: 3")!=std::wstring::npos);CHECK(s->chartStatistics&&s->chartStatistics->total==3&&s->chartStart==65535);
        Select(h,*s,s->document->size,0);Command(h,*s,ByteStatistics);complete();
        CHECK(Text(h,AnalysisOutput).find(L"Entropy: 0.0000")!=std::wstring::npos);
        SendDlgItemMessageW(h,AnalysisScope,CB_SETCURSEL,0,0);SendDlgItemMessageW(h,ChecksumType,CB_SETCURSEL,0,0);
        SendDlgItemMessageW(h,ChecksumType,CB_SETCURSEL,1,0);SendDlgItemMessageW(h,AnalysisScope,CB_SETCURSEL,1,0);
        Select(h,*s,0,2);SetDlgItemTextW(h,ExpectedChecksum,L"00000000");Command(h,*s,VerifyChecksum);complete();
        CHECK(Text(h,AnalysisOutput).find(L"MISMATCH")!=std::wstring::npos);
        wchar_t expected[9];swprintf_s(expected,L"%08X",Crc32(Bytes({0xfe,0xff})));SetDlgItemTextW(h,ExpectedChecksum,expected);
        Command(h,*s,VerifyChecksum);CHECK(!IsWindowEnabled(GetDlgItem(h,ExpectedChecksum)));complete();
        CHECK(Text(h,Status)==L"Checksum matches the expected value.");
        CHECK(s->lastChecksum==expected&&s->reportReady);CHECK(IsWindowEnabled(GetDlgItem(h,CopyChecksum)));CHECK(IsWindowEnabled(GetDlgItem(h,ExportReport)));
        Changed(h,*s);Buttons(h,*s);CHECK(s->lastChecksum.empty()&&!s->reportReady&&!IsWindowEnabled(GetDlgItem(h,ExportReport)));CHECK(!s->chartStatistics&&!IsWindowEnabled(GetDlgItem(h,DistributionChart)));
        SendDlgItemMessageW(h,AnalysisScope,CB_SETCURSEL,0,0);SendDlgItemMessageW(h,ChecksumType,CB_SETCURSEL,0,0);
        s->comparison=std::make_shared<Document>();CHECK(s->comparison->Open(source.wstring()));s->compareStart=0;
        Command(h,*s,NextDifference);complete();CHECK(s->selection==0&&s->compareStart==1);
        CHECK(Text(h,AnalysisOutput).find(L"Difference at 0x0")!=std::wstring::npos);
        Command(h,*s,NextDifference);complete();CHECK(s->selection==1&&s->compareStart==2);
        Command(h,*s,NextDifference);complete();CHECK(s->compareStart==0);
        CHECK(Text(h,AnalysisOutput).find(L"No further differences")!=std::wstring::npos);
        CHECK(ReadFileBytes(source)==original);
        DestroyWindow(h);s.reset();
        std::cout<<"DAT hex editor: parsing, paging, search, inspector, edits, direct typing, paste, bookmarks, CRC-32/SHA-256, comparison, binary exports, undo/redo, copy-only saves, aliases, failed-save cleanup, and creation race passed.\n";
        // Test artifacts are retained for inspection; every fixture lives in a new folder.
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<"\n";return 1;}
}

