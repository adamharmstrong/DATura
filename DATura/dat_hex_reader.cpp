#include "stdafx.h"
#include "dat_hex_reader.h"
#include "dat_hex_document.h"
#include "dat_hex_analysis.h"
#include "win32_tool_window.h"
#include <commdlg.h>
#include <commctrl.h>
#pragma comment(lib,"comctl32.lib")
#include <shobjidl.h>
#include <wrl/client.h>
#include <atomic>
#include <thread>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <richedit.h>
#include <cmath>
#include <cerrno>
#include <chrono>
#include <limits>
#pragma comment(lib,"ole32.lib")
#pragma comment(lib,"shell32.lib")
#pragma comment(lib,"uuid.lib")

namespace DatHexReader {
namespace {
constexpr uint64_t PageSize=65536;
constexpr uint64_t NoByte=UINT64_MAX;
enum {Open=101,Previous,Next,Encoding,Export,Info,Dump,SaveCopy,Undo,Redo,EditMode,Rows,OffsetBase,
    GoText,Go,SearchText,FindMode,Find,FindNext,Cancel,StartText,LengthText,SelectBlock,CopyHex,CopyText,
    PatchText,Overwrite,Fill,Endian,Inspector,Status,PasteOverwrite,BookmarkList,AddBookmark,
    GoBookmark,RemoveBookmark,CompareFile,NextDifference,Checksums,ExportBlock,AnalysisOutput,ReplacementText,ReplaceMatch,FindPrevious,FindAll,MatchList,GoMatch,ByteStatistics,ChecksumType,AnalysisScope,CopyFormat,CopyFormatted,CopyCArray,CopyEscaped,CopyBase64,ByteOperation,ApplyOperation,ReplaceAll,Navigation,NextModified,PreviousModified,SelectToEnd,SelectToStart,CopyOffset,FileStart,FileEnd,ExpectedChecksum,VerifyChecksum,CopyChecksum,ExportReport,ExportIntelHex,ExportSelectionMenu,ExportCArray,ExportEscaped,ExportBase64,SearchScope,ViewMenu,Group1,Group2,Group4,Group8,Group16,ViewBoth,ViewHex,ViewText,DistributionChart,ExportPascal,ExportJava,ExportCSharp,ExportVB,CopyPascal,CopyJava,CopyCSharp,CopyVB,ExportSRecord,ExportHtml,ExportRtf,ExportTex,ExportPlainTable,CopyPlainTable,JoinBefore,JoinAfter,Split64K,Split1M,Split16M,PasteDecoded,PasteUtf8,PasteBase64,PasteEscaped,PasteBinary,CopyBinary,CopyInspector,ExportInspector,Pointer16,Pointer24,Pointer32,Pointer64,Relative16,Relative24,Relative32,Relative64,NextValue1,NextValue2,NextValue3,NextValue4,NextValue8,NextValue16,PreviousValue1,PreviousValue2,PreviousValue3,PreviousValue4,PreviousValue8,PreviousValue16,InspectorHex,ToolTabs,StartLabel,LengthLabel,ExpectedLabel,SetNumberedBookmark=2000,GoNumberedBookmark=2010};
HWND window=nullptr;
HRESULT ConfigureDatPicker(IFileOpenDialog* picker,const std::wstring& root,const wchar_t* title) {
    const COMDLG_FILTERSPEC filters[]={{L"FFXI DAT files",L"*.dat"},{L"All files",L"*.*"}};
    FILEOPENDIALOGOPTIONS options={};
    HRESULT hr=picker->GetOptions(&options);
    if(SUCCEEDED(hr))hr=picker->SetOptions(options|FOS_FILEMUSTEXIST|FOS_PATHMUSTEXIST|FOS_FORCEFILESYSTEM|FOS_NOCHANGEDIR);
    if(SUCCEEDED(hr))hr=picker->SetFileTypes(2,filters);
    if(SUCCEEDED(hr))hr=picker->SetTitle(title);
    if(FAILED(hr))return hr;
    Microsoft::WRL::ComPtr<IShellItem> folder;
    hr=SHCreateItemFromParsingName(root.c_str(),nullptr,IID_PPV_ARGS(folder.GetAddressOf()));
    // A missing installation folder should not prevent choosing another DAT.
    if(FAILED(hr))return S_OK;
    // SetFolder overrides Windows' remembered directory without treating the
    // directory as a filename (which can reject the legacy open dialog).
    return picker->SetFolder(folder.Get());
}
// Audio may already have initialized the UI thread as an MTA. Shell dialogs
// require an STA; never ignore RPC_E_CHANGED_MODE and create one on that MTA.
template<class Action> HRESULT OnPickerSta(HWND owner,Action action) {
    HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(SUCCEEDED(initialized)) {
        struct Apartment{~Apartment(){CoUninitialize();}} apartment;
        try{return action();}catch(...){return E_FAIL;}
    }
    if(initialized!=RPC_E_CHANGED_MODE)return initialized;
    DatHex::Handle completed(CreateEventW(nullptr,TRUE,FALSE,nullptr));
    if(!completed.value)return HRESULT_FROM_WIN32(GetLastError());
    bool restore=owner&&IsWindowEnabled(owner);if(restore)EnableWindow(owner,FALSE);
    struct OwnerState{HWND owner;bool restore;~OwnerState(){if(restore&&IsWindow(owner))EnableWindow(owner,TRUE);}} ownerState{owner,restore};
    HRESULT result=E_FAIL;std::thread worker;
    try {
        worker=std::thread([&] {
            HRESULT ready=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
            if(SUCCEEDED(ready)) {
                try{result=action();}catch(...){result=E_FAIL;}
                CoUninitialize();
            }else result=ready;
            SetEvent(completed.value);
        });
    }catch(...){return E_FAIL;}
    bool quit=false;WPARAM exitCode=0;
    // Shell modal dialogs send messages to their owner. Pump those messages
    // instead of joining a worker while the UI thread is blocked.
    for(;;) {
        DWORD wait=MsgWaitForMultipleObjectsEx(1,&completed.value,INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
        if(wait==WAIT_OBJECT_0)break;
        if(wait==WAIT_FAILED){worker.join();return HRESULT_FROM_WIN32(GetLastError());}
        MSG message={};
        while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
            if(message.message==WM_QUIT){quit=true;exitCode=message.wParam;continue;}
            TranslateMessage(&message);DispatchMessageW(&message);
        }
    }
    worker.join();if(quit)PostQuitMessage(static_cast<int>(exitCode));return result;
}
HRESULT ShowDatPicker(IFileOpenDialog* picker,HWND owner){return picker->Show(owner);}
using PickerShow=HRESULT (*)(IFileOpenDialog*,HWND);
bool PickDatFileWithShow(HWND owner,const std::wstring& root,const wchar_t* title,std::wstring& path,PickerShow show) {
    path.clear();
    HRESULT hr=OnPickerSta(owner,[&] {
        Microsoft::WRL::ComPtr<IFileOpenDialog> picker;
        HRESULT status=CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(picker.GetAddressOf()));
        if(SUCCEEDED(status))status=ConfigureDatPicker(picker.Get(),root,title);
        if(SUCCEEDED(status))status=show(picker.Get(),owner);
        Microsoft::WRL::ComPtr<IShellItem> item;
        if(SUCCEEDED(status))status=picker->GetResult(item.GetAddressOf());
        PWSTR selected=nullptr;
        if(SUCCEEDED(status))status=item->GetDisplayName(SIGDN_FILESYSPATH,&selected);
        if(SUCCEEDED(status))path=selected;
        CoTaskMemFree(selected);return status;
    });
    if(hr==HRESULT_FROM_WIN32(ERROR_CANCELLED))return false;
    if(FAILED(hr)) {
        wchar_t message[160]={};
        swprintf_s(message,L"The DAT file picker could not open or return a file (error 0x%08X).",static_cast<unsigned>(hr));
        MessageBoxW(owner,message,L"DAT Hex Reader",MB_OK|MB_ICONERROR);
        return false;
    }
    return !path.empty();
}
bool PickDatFile(HWND owner,const std::wstring& root,const wchar_t* title,std::wstring& path) {
    return PickDatFileWithShow(owner,root,title,path,ShowDatPicker);
}
struct SearchJob {
    enum class Kind { Search,Compare,Hashes,AllMatches,PreviousMatch,Statistics,ReplaceAll,Join,Split } kind=Kind::Search;
    std::shared_ptr<DatHex::Document> document;
    std::shared_ptr<DatHex::Document> comparison;
    DatHex::Bytes pattern,replacement;
    std::optional<DatHex::Document::ReplacementPlan> replacementPlan;
    bool replacementLimit=false;
    std::chrono::steady_clock::time_point started=std::chrono::steady_clock::now();
    uint64_t partCount=0;
    uint64_t start=0;
    std::optional<uint64_t> result;
    std::optional<DatHex::Difference> difference;
    std::optional<DatHex::HashResult> hashes;
    std::wstring expectedChecksum,outputPath;
    std::optional<DatHex::StatisticsResult> statistics;
    std::vector<uint64_t> matches;
    uint64_t matchCount=0,rangeStart=0,rangeLength=0;
    DatHex::HashAlgorithm hashAlgorithm=DatHex::HashAlgorithm::Crc32Sha256;
    bool failed=false;
    std::atomic<bool> done=false,cancel=false;
    std::atomic<uint64_t> processed=0;
};
struct ScanProgress {unsigned percent=0;double bytesPerSecond=0;std::optional<uint64_t> remainingSeconds;};
ScanProgress MeasureScan(uint64_t processed,uint64_t total,double elapsed) {
    ScanProgress result;if(!total)return result;
    processed=(std::min)(processed,total);
    result.percent=static_cast<unsigned>(static_cast<long double>(processed)*100/total);
    if(elapsed>=1&&std::isfinite(elapsed)&&processed){
        result.bytesPerSecond=static_cast<double>(processed)/elapsed;
        double remaining=static_cast<double>(total-processed)/result.bytesPerSecond;
        if(std::isfinite(remaining)&&remaining<static_cast<double>(UINT64_MAX))result.remainingSeconds=static_cast<uint64_t>(std::ceil(remaining));
    }
    return result;
}
std::wstring ScanStatus(const SearchJob& job,double elapsed) {
    if(job.cancel.load())return L"Cancelling operation...";
    const wchar_t* name=job.kind==SearchJob::Kind::Split?L"Split file":job.kind==SearchJob::Kind::Join?L"Join files":job.kind==SearchJob::Kind::Hashes?L"Checksum":job.kind==SearchJob::Kind::Statistics?L"Statistics":job.kind==SearchJob::Kind::Compare?L"Compare":job.kind==SearchJob::Kind::ReplaceAll?L"Replace all":L"Search";
    bool measured=job.kind!=SearchJob::Kind::Search;
    uint64_t total=job.rangeLength,processed=job.processed.load();
    if(job.kind==SearchJob::Kind::Compare){uint64_t end=job.comparison?(std::min)(job.document->size,job.comparison->size):0;total=end>job.start?end-job.start:0;processed=processed>job.start?processed-job.start:0;}
    if(!measured||!total)return std::wstring(name)+L" running - "+std::to_wstring(static_cast<uint64_t>((std::max)(0.0,elapsed)))+L" s elapsed. Cancel stops the operation.";
    auto progress=MeasureScan(processed,total,elapsed);std::wostringstream text;
    text<<name<<L": "<<progress.percent<<L"% - "<<(std::min)(processed,total)<<L" / "<<total<<L" bytes";
    if(progress.remainingSeconds)text<<L" - "<<std::fixed<<std::setprecision(1)<<progress.bytesPerSecond/1048576<<L" MiB/s - about "<<*progress.remainingSeconds<<L" s remaining";
    else text<<L" - estimating time remaining";
    text<<L". Cancel stops the operation.";return text.str();
}
struct State {
    decltype(&PickDatFile) pickDatFile=PickDatFile;
    std::wstring root,text,lastChecksum;
    bool reportReady=false;
    std::optional<DatHex::StatisticsResult> chartStatistics;
    uint64_t chartStart=0;
    std::shared_ptr<DatHex::Document> document=std::make_shared<DatHex::Document>();
    std::shared_ptr<SearchJob> job;
    uint64_t offset=0,selection=0,length=0;
    unsigned rowBytes=32,groupBytes=1;
    int displayMode=0,encodingIndex=0;
    bool decimal=false,ownedByWindow=false,settingText=false,edit=false,changesSaved=false,picking=false;
    HFONT font=nullptr;
    WNDPROC originalEdit=nullptr,originalInput=nullptr;
    // Character positions in either the hex or decoded-text columns map to bytes.
    std::vector<uint64_t> positions;
    std::vector<uint64_t> positionEnds;
    DWORD lastStart=0,lastEnd=0;
    DatHex::Bytes lastPattern;
    uint64_t lastSearchStart=0,lastSearchLength=0;
    std::vector<uint64_t> matches;
    size_t matchLength=0;
    struct Bookmark {uint64_t offset,length;int slot=-1;};
    std::vector<Bookmark> bookmarks;
    std::shared_ptr<DatHex::Document> comparison;
    uint64_t compareStart=0;
    bool inspectorHex=false;
    int toolTab=0;
    int activeNibble=-1; // -1: byte mode, 0: high digit, 1: low digit
    int highNibble=-1;
    uint64_t nibbleOffset=0;
};
std::wstring BrowseRoot(const char* root) {
    if(!root||!*root) return L"C:\\Program Files (x86)\\PlayOnline\\SquareEnix\\FINAL FANTASY XI\\";
    int n=MultiByteToWideChar(CP_ACP,0,root,-1,nullptr,0); if(!n) return {};
    std::wstring result(n,L'\0'); MultiByteToWideChar(CP_ACP,0,root,-1,result.data(),n); result.resize(n-1); return result;
}
std::wstring Text(HWND h,int id) {
    HWND c=GetDlgItem(h,id); int n=GetWindowTextLengthW(c); std::wstring text(n+1,L'\0');
    GetWindowTextW(c,text.data(),n+1); text.resize(n); return text;
}
std::wstring Number(uint64_t value,bool decimal=false) {
    std::wostringstream out; if(!decimal) out<<std::uppercase<<std::hex; out<<value; return out.str();
}
void Error(HWND h,const wchar_t* text) {MessageBoxW(h,text,L"DAT Hex Reader",MB_OK|MB_ICONINFORMATION);}
HWND Control(HWND h,const wchar_t* type,const wchar_t* text,int id,int x,int y,int width,DWORD style=0,int height=26) {
    HWND c=CreateWindowExW(type==std::wstring(L"EDIT")?WS_EX_CLIENTEDGE:0,type,text,WS_CHILD|WS_VISIBLE|WS_TABSTOP|style,
        x,y,width,height,h,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
    SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE); return c;
}
void Combo(HWND h,int id,int x,int y,int width,std::initializer_list<const wchar_t*> options) {
    Control(h,L"COMBOBOX",L"",id,x,y,width,CBS_DROPDOWNLIST|WS_VSCROLL,200);
    for(auto option:options) SendDlgItemMessageW(h,id,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(option));
    SendDlgItemMessageW(h,id,CB_SETCURSEL,0,0);
}
struct TabControlLayout {int id,tab,x,y,width;};
const TabControlLayout ToolControls[]={
    {Encoding,0,20,84,190},{Rows,0,220,84,130},{OffsetBase,0,360,84,150},{ViewMenu,0,520,84,100},{Export,0,630,84,130},
    {CopyHex,0,20,120,110},{CopyText,0,140,120,110},{CopyFormat,0,260,120,220},{CopyFormatted,0,490,120,150},
    {StartLabel,1,20,84,40},{StartText,1,65,84,148},{LengthLabel,1,225,84,46},{LengthText,1,275,84,100},{SelectBlock,1,385,84,120},
    {ByteOperation,1,520,84,240},{ApplyOperation,1,770,84,90},{PatchText,1,20,120,400},{Overwrite,1,430,120,155},{Fill,1,595,120,130},{PasteOverwrite,1,735,120,155},
    {SearchText,2,20,84,260},{FindMode,2,290,84,200},{SearchScope,2,500,84,185},{Find,2,695,84,65},{FindNext,2,770,84,100},{FindPrevious,2,880,84,120},
    {ReplacementText,2,20,120,260},{ReplaceMatch,2,290,120,125},{ReplaceAll,2,425,120,110},{FindAll,2,545,120,100},{MatchList,2,655,120,230},{GoMatch,2,895,120,110},
    {Previous,3,20,84,120},{Next,3,150,84,110},{GoText,3,270,84,170},{Go,3,450,84,125},{Navigation,3,585,84,180},
    {AddBookmark,3,20,120,115},{BookmarkList,3,145,120,350},{GoBookmark,3,505,120,80},{RemoveBookmark,3,595,120,100},
    {ChecksumType,4,20,84,235},{AnalysisScope,4,265,84,220},{Checksums,4,495,84,110},{ByteStatistics,4,615,84,130},{CompareFile,4,755,84,130},{NextDifference,4,895,84,140},
    {ExpectedLabel,4,20,122,160},{ExpectedChecksum,4,185,120,320},{VerifyChecksum,4,515,120,145},{CopyChecksum,4,670,120,130},{ExportReport,4,810,120,130}
};
void LayoutToolTabs(HWND h,State& state) {
    for(const auto& item:ToolControls){HWND control=GetDlgItem(h,item.id);RECT bounds={};GetWindowRect(control,&bounds);
        wchar_t className[32]={};GetClassNameW(control,className,32);int height=wcscmp(className,L"ComboBox")==0?200:bounds.bottom-bounds.top;
        SetWindowPos(control,nullptr,item.x,item.y,item.width,height,SWP_NOZORDER|SWP_NOACTIVATE);
        ShowWindow(control,item.tab==state.toolTab?SW_SHOW:SW_HIDE);
    }
    const TabControlLayout common[]={{Open,0,10,10,100},{SaveCopy,0,120,10,110},{Undo,0,240,10,62},{Redo,0,312,10,62},{EditMode,0,384,10,126},{Endian,0,520,10,150},{Cancel,0,680,10,90},{ExportSelectionMenu,0,780,10,240}};
    for(const auto& item:common)SetWindowPos(GetDlgItem(h,item.id),nullptr,item.x,item.y,item.width,item.id==Endian?200:26,SWP_NOZORDER|SWP_NOACTIVATE);
}
void ActivateToolTab(HWND h,State& state,int index) {state.toolTab=index;TabCtrl_SetCurSel(GetDlgItem(h,ToolTabs),index);LayoutToolTabs(h,state);}
std::wstring Decode(const unsigned char* bytes,size_t count,bool japanese) {
    std::wstring text;
    for(size_t i=0;i<count;) {
        unsigned char c=bytes[i];
        if(japanese && ((c>=0x81&&c<=0x9f)||(c>=0xe0&&c<=0xfc)) && i+1<count &&
            ((bytes[i+1]>=0x40&&bytes[i+1]<=0x7e)||(bytes[i+1]>=0x80&&bytes[i+1]<=0xfc))) {
            wchar_t decoded[2]={}; int n=MultiByteToWideChar(932,MB_ERR_INVALID_CHARS,reinterpret_cast<const char*>(bytes+i),2,decoded,2);
            if(n) {text.append(decoded,n);i+=2;continue;}
        }
        if(c>=0x20&&c<=0x7e) text+=wchar_t(c);
        else if(japanese&&c>=0xa1&&c<=0xdf) text+=wchar_t(0xff61+c-0xa1);
        else text+=c==0?L'_':L'.';
        ++i;
    }
    return text;
}
UINT EncodingPage(int index) {
    const UINT pages[]={932,20127,1252,437,500,10000};
    return index>=0&&index<6?pages[index]:0;
}
std::wstring DecodeEncoding(const unsigned char* bytes,size_t count,int index) {
    if(index==0||index==1)return Decode(bytes,count,index==0);
    UINT page=EncodingPage(index);std::wstring text; text.reserve(count);
    for(size_t i=0;i<count;++i) {
        wchar_t decoded=0;
        int n=page?MultiByteToWideChar(page,0,reinterpret_cast<const char*>(bytes+i),1,&decoded,1):0;
        if(n&&decoded>=32&&!(decoded>=0x7f&&decoded<=0x9f)&&decoded!=0xfffd)text+=decoded;
        else text+=bytes[i]==0?L'_':L'.';
    }
    return text;
}
void Buttons(HWND h,State& s) {
    bool loaded=s.document->Loaded(),busy=bool(s.job);
    for(int id:{Open,Encoding,Rows,OffsetBase,EditMode,Endian,ChecksumType,AnalysisScope,ViewMenu}) EnableWindow(GetDlgItem(h,id),!busy);
    for(int id:{Export,SaveCopy,Go,GoText,Find,SearchText,FindMode,StartText,LengthText,SelectBlock,CopyHex,CopyText,CopyFormat,CopyFormatted,
        AddBookmark,CompareFile,Checksums,ExportBlock,ExportSelectionMenu,FindAll,ByteStatistics,Navigation,ExpectedChecksum,VerifyChecksum,SearchScope})
        EnableWindow(GetDlgItem(h,id),loaded&&!busy);
    EnableWindow(GetDlgItem(h,FindPrevious),loaded&&!busy&&!s.lastPattern.empty());
    for(int id:{MatchList,GoMatch})EnableWindow(GetDlgItem(h,id),loaded&&!busy&&!s.matches.empty());
    EnableWindow(GetDlgItem(h,FindNext),loaded&&!busy&&!s.lastPattern.empty());
    EnableWindow(GetDlgItem(h,Previous),loaded&&!busy&&s.offset>0);
    EnableWindow(GetDlgItem(h,Next),loaded&&!busy&&s.document->size-s.offset>PageSize);
    EnableWindow(GetDlgItem(h,Undo),s.edit&&!busy&&s.document->CanUndo());
    EnableWindow(GetDlgItem(h,Redo),s.edit&&!busy&&s.document->CanRedo());
    for(int id:{PatchText,Overwrite,Fill,PasteOverwrite,ReplacementText,ReplaceMatch,ByteOperation,ApplyOperation,ReplaceAll}) EnableWindow(GetDlgItem(h,id),loaded&&s.edit&&!busy);
    for(int id:{BookmarkList,GoBookmark,RemoveBookmark})EnableWindow(GetDlgItem(h,id),loaded&&!busy&&!s.bookmarks.empty());
    EnableWindow(GetDlgItem(h,NextDifference),loaded&&!busy&&bool(s.comparison));
    EnableWindow(GetDlgItem(h,DistributionChart),loaded&&!busy&&bool(s.chartStatistics));
    EnableWindow(GetDlgItem(h,CopyChecksum),loaded&&!busy&&!s.lastChecksum.empty());
    EnableWindow(GetDlgItem(h,ExportReport),loaded&&!busy&&s.reportReady);
    EnableWindow(GetDlgItem(h,Cancel),busy);
}
bool ParseBinary(const std::wstring& input,DatHex::Bytes& bytes) {
    bytes.clear();auto first=input.find_first_not_of(L" \t\r\n");if(first==std::wstring::npos)return false;
    size_t start=first;if(input.size()>start+1&&input[start]==L'0'&&(input[start+1]==L'b'||input[start+1]==L'B'))start+=2;
    unsigned byte=0,bits=0;
    for(size_t i=start;i<input.size();++i){wchar_t c=input[i];if(c==L' '||c==L'\t'||c==L'\r'||c==L'\n')continue;
        if(c!=L'0'&&c!=L'1')return false;byte=(byte<<1)|(c-L'0');
        if(++bits==8){if(bytes.size()>=PageSize)return false;bytes.push_back(static_cast<unsigned char>(byte));bits=byte=0;}
    }
    return !bytes.empty()&&!bits;
}
std::wstring GuidText(const DatHex::Bytes& bytes,bool windowsLayout) {
    if(bytes.size()<16)return {};
    const size_t windowsOrder[]={3,2,1,0,5,4,7,6,8,9,10,11,12,13,14,15};
    std::wostringstream out;out<<std::hex<<std::uppercase<<std::setfill(L'0');
    for(size_t i=0;i<16;++i){out<<std::setw(2)<<unsigned(bytes[windowsLayout?windowsOrder[i]:i]);if(i==3||i==5||i==7||i==9)out<<L'-';}
    return out.str();
}
std::wstring FileTimeText(const DatHex::Bytes& bytes,bool big) {
    if(bytes.size()<8)return {};
    uint64_t ticks=DatHex::UnsignedValue(bytes,8,big);FILETIME ft{static_cast<DWORD>(ticks),static_cast<DWORD>(ticks>>32)};SYSTEMTIME date={};
    if(!FileTimeToSystemTime(&ft,&date))return L"outside supported range";
    wchar_t formatted[80];swprintf_s(formatted,L"%04u-%02u-%02u %02u:%02u:%02u.%03u UTC",date.wYear,date.wMonth,date.wDay,date.wHour,date.wMinute,date.wSecond,date.wMilliseconds);return formatted;
}
std::optional<std::pair<uint64_t,size_t>> ReadLeb128(const DatHex::Bytes& bytes,bool signedValue) {
    uint64_t value=0;
    for(size_t i=0;i<bytes.size()&&i<10;++i){unsigned byte=bytes[i],payload=byte&127;
        if(i==9&&(signedValue?(payload!=0&&payload!=127):payload>1))return {};
        value|=uint64_t(payload)<<(i*7);
        if(!(byte&128)){unsigned bits=static_cast<unsigned>((i+1)*7);
            if(signedValue&&(byte&64)&&bits<64)value|=UINT64_MAX<<bits;
            return std::make_pair(value,i+1);
        }
    }
    return {};
}
DatHex::Bytes EncodeLeb128(uint64_t bits,bool signedValue) {
    DatHex::Bytes bytes;bool negative=signedValue&&(bits>>63);
    for(;;){unsigned char byte=static_cast<unsigned char>(bits&127);bits>>=7;if(negative)bits|=UINT64_MAX<<57;
        bool end=signedValue?((bits==0&&!(byte&64))||(bits==UINT64_MAX&&(byte&64))):bits==0;
        bytes.push_back(static_cast<unsigned char>(byte|(end?0:128)));if(end)return bytes;
    }
}
std::wstring Leb128Text(const DatHex::Bytes& bytes,bool signedValue) {
    auto decoded=ReadLeb128(bytes,signedValue);if(!decoded)return L"incomplete or outside 64-bit range";
    std::wstring value;if(signedValue){int64_t number;std::memcpy(&number,&decoded->first,8);value=std::to_wstring(number);}else value=std::to_wstring(decoded->first);
    return value+L" ("+std::to_wstring(decoded->second)+L" bytes)";
}
std::wstring DosDateText(const DatHex::Bytes& bytes) {
    if(bytes.size()<4)return L"incomplete";
    unsigned time=bytes[0]|unsigned(bytes[1])<<8,date=bytes[2]|unsigned(bytes[3])<<8;
    SYSTEMTIME value={};value.wYear=static_cast<WORD>(1980+(date>>9));value.wMonth=static_cast<WORD>((date>>5)&15);value.wDay=static_cast<WORD>(date&31);
    value.wHour=static_cast<WORD>(time>>11);value.wMinute=static_cast<WORD>((time>>5)&63);value.wSecond=static_cast<WORD>((time&31)*2);
    FILETIME checked;if(!SystemTimeToFileTime(&value,&checked))return L"invalid DOS date/time";
    wchar_t text[80];swprintf_s(text,L"%04u-%02u-%02u %02u:%02u:%02u (timezone unspecified)",value.wYear,value.wMonth,value.wDay,value.wHour,value.wMinute,value.wSecond);return text;
}
std::wstring InspectorInteger(uint64_t value,size_t width,bool signedValue,bool hex) {
    uint64_t mask=width==8?UINT64_MAX:(uint64_t(1)<<(width*8))-1;value&=mask;
    bool negative=signedValue&&(value&(uint64_t(1)<<(width*8-1)));
    uint64_t magnitude=negative?((~value)&mask)+1:value;
    if(hex){std::wostringstream out;out<<(negative?L"-0x":L"0x")<<std::hex<<std::uppercase<<magnitude;return out.str();}
    return (negative?L"-":L"")+std::to_wstring(magnitude);
}
void Inspect(HWND h,State& s) {
    SetDlgItemTextW(h,StartText,Number(s.selection,s.decimal).c_str());
    SetDlgItemTextW(h,LengthText,Number(s.length,s.decimal).c_str());
    std::wstring status=L"Offset("+std::wstring(s.decimal?L"d":L"h")+L"): "+Number(s.selection,s.decimal)+
        L"   Length: "+Number(s.length,s.decimal)+L" bytes   "+(s.edit?L"Edit buffer (overwrite)":L"Read only")+
        (s.document->Dirty()?L"   Modified":L"")+L"   Saves always create a new copy";
    SetDlgItemTextW(h,Status,status.c_str());
    DatHex::Bytes bytes; size_t count=static_cast<size_t>((std::min)(uint64_t(16),s.document->size-s.selection));
    if(!s.document->Loaded()||!count||!s.document->Read(s.selection,count,bytes)) {SetDlgItemTextW(h,Inspector,L"Select bytes to inspect their values.");return;}
    bool big=SendDlgItemMessageW(h,Endian,CB_GETCURSEL,0,0)==1;
    std::wostringstream out; out<<L"DATA INSPECTOR\r\n\r\nAt offset "<<Number(s.selection,s.decimal)<<L"\r\n";
    out<<L"Binary (8 bit)  "; for(int i=7;i>=0;--i) out<<((bytes[0]>>i)&1); out<<L"\r\n\r\n";
    for(size_t n:{size_t(1),size_t(2),size_t(3),size_t(4),size_t(8)}) {
        if(count<n) continue;
        uint64_t u=DatHex::UnsignedValue(bytes,n,big);
        out<<L"UInt"<<n*8<<L"  "<<InspectorInteger(u,n,false,s.inspectorHex)<<L"\r\nInt"<<n*8<<L"   "<<InspectorInteger(u,n,true,s.inspectorHex)<<L"\r\nHex     "<<Number(u)<<L"\r\n\r\n";
        if(n==4) {uint32_t bits=static_cast<uint32_t>(u);float f;std::memcpy(&f,&bits,4);out<<L"Float32  "<<std::setprecision(std::numeric_limits<float>::max_digits10)<<f<<L"\r\n\r\n";}
        if(n==8) {double d;std::memcpy(&d,&u,8);out<<L"Float64  "<<std::setprecision(std::numeric_limits<double>::max_digits10)<<d<<L"\r\n\r\n";}
    }
    out<<L"ULEB128 (unsigned)\r\n"<<Leb128Text(bytes,false)<<L"\r\nLEB128 (signed)\r\n"<<Leb128Text(bytes,true)<<L"\r\n\r\n";
    if(count>=4)out<<L"DOS time/date (little endian)\r\n"<<DosDateText(bytes)<<L"\r\n\r\n";
    if(count>=16)out<<L"GUID (Windows layout)\r\n"<<GuidText(bytes,true)<<L"\r\nGUID (network byte order)\r\n"<<GuidText(bytes,false)<<L"\r\n\r\n";
    if(count>=8)out<<L"FILETIME (little endian)\r\n"<<FileTimeText(bytes,false)<<L"\r\nFILETIME (big endian)\r\n"<<FileTimeText(bytes,true)<<L"\r\n\r\n";
    if(s.length && s.length<=PageSize) {
        DatHex::Bytes block;
        if(s.document->Read(s.selection,static_cast<size_t>(s.length),block)) out<<L"Selection CRC-32\r\n"<<Number(DatHex::Crc32(block))<<L"\r\n";
    } else if(s.length>PageSize) out<<L"CRC-32: select up to 64 KB.\r\n";
    SetDlgItemTextW(h,Inspector,out.str().c_str());
}
void Highlight(HWND h,State& s) {
    size_t first=s.positions.size();
    for(size_t i=0;i<s.positions.size();++i) if(s.positions[i]!=NoByte && s.positionEnds[i]>=s.selection && s.positions[i]<s.selection+s.length) {
        first=i;break;
    }
    // Native edit selections are continuous. Highlight from the first to the last
    // selected hex byte, including intervening row separators for multi-row blocks.
    if(first<s.positions.size()) {
        size_t end=first;
        uint64_t target=(std::min)(s.selection+s.length-1,
            s.offset+(std::min)(PageSize,s.document->size-s.offset)-1);
        for(size_t i=first;i<s.positions.size();++i) if(s.positions[i]!=NoByte&&s.positions[i]<=target&&s.positionEnds[i]>=target) {end=i+1; if(i+1==s.positions.size()||s.positions[i+1]!=target) break;}
        s.lastStart=static_cast<DWORD>(first);s.lastEnd=static_cast<DWORD>(end);
        SendDlgItemMessageW(h,Dump,EM_SETSEL,s.lastStart,s.lastEnd);SendDlgItemMessageW(h,Dump,EM_SCROLLCARET,0,0);
    } else {s.lastStart=s.lastEnd=0;SendDlgItemMessageW(h,Dump,EM_SETSEL,0,0);}
}
std::wstring RtfEscape(const std::wstring& input);
struct DumpRtfStream {std::string text;size_t offset=0;};
DWORD CALLBACK StreamDumpRtf(DWORD_PTR cookie,LPBYTE buffer,LONG count,LONG* read) {
    auto& stream=*reinterpret_cast<DumpRtfStream*>(cookie);size_t take=(std::min)(size_t(count),stream.text.size()-stream.offset);
    std::memcpy(buffer,stream.text.data()+stream.offset,take);stream.offset+=take;*read=static_cast<LONG>(take);return 0;
}
void Refresh(HWND h,State& s) {
    if(s.job) return;
    Buttons(h,s);
    if(!s.document->Loaded()) {Inspect(h,s);return;}
    DatHex::Bytes bytes;
    size_t count=static_cast<size_t>((std::min)(PageSize,s.document->size-s.offset));
    if(!s.document->Read(s.offset,count,bytes)) {Error(h,L"Could not read the DAT.");return;}
    s.text.clear();s.positions.clear();s.positionEnds.clear();
    std::wstring colored;bool header=true;
    auto append=[&](const std::wstring& t,uint64_t at=NoByte,size_t span=1,int color=0) {
        bool modified=false;if(at!=NoByte)for(size_t i=0;i<span;++i)if(s.document->Modified(at+i)){modified=true;break;}
        colored+=L"{\\cf"+std::to_wstring(modified?3:header?1:color)+(modified?L"\\b ":L"\\b0 ")+RtfEscape(t)+L"}";
        s.text+=t;s.positions.insert(s.positions.end(),t.size(),at);
        s.positionEnds.insert(s.positionEnds.end(),t.size(),at==NoByte?NoByte:at+span-1);
    };
    append(s.decimal?L"Offset(d)             ":L"Offset(h)         ");
    auto gap=[&](unsigned j){if((j+1)%s.groupBytes==0||j+1==s.rowBytes)append(L" ");if(s.groupBytes<=8&&j%8==7)append(L" ");};
    if(s.displayMode!=2)for(unsigned j=0;j<s.rowBytes;++j){std::wostringstream t;t<<std::hex<<std::uppercase<<std::setw(2)<<std::setfill(L'0')<<j;append(t.str());gap(j);}
    if(s.displayMode!=1)append(s.displayMode==0?L" Decoded text":L"Decoded text");append(L"\r");header=false;
    int encoding=static_cast<int>(SendDlgItemMessageW(h,Encoding,CB_GETCURSEL,0,0));bool japanese=encoding==0;
    int offsetWidth=s.decimal?20:16;
    for(size_t i=0;i<bytes.size();i+=s.rowBytes) {
        std::wostringstream t;t<<std::setfill(L'0')<<std::setw(offsetWidth);if(!s.decimal)t<<std::hex<<std::uppercase;t<<s.offset+i<<L"  ";append(t.str(),NoByte,1,1);
        size_t n=(std::min)(size_t(s.rowBytes),bytes.size()-i);
        if(s.displayMode!=2)for(unsigned j=0;j<s.rowBytes;++j) {
            if(j<n) {std::wostringstream b;b<<std::hex<<std::uppercase<<std::setfill(L'0')<<std::setw(2)<<unsigned(bytes[i+j]);append(b.str(),s.offset+i+j,1,(j/s.groupBytes)%2?2:0);}
            else append(L"  ");
            gap(j);
        }
        if(s.displayMode==0)append(L" ");
        if(s.displayMode!=1)for(size_t j=0;j<n;) {
            size_t take=1;unsigned char c=bytes[i+j];
            if(japanese && ((c>=0x81&&c<=0x9f)||(c>=0xe0&&c<=0xfc)) && j+1<n) {
                wchar_t decoded[2];
                if(MultiByteToWideChar(932,MB_ERR_INVALID_CHARS,reinterpret_cast<const char*>(bytes.data()+i+j),2,decoded,2)>0) take=2;
            }
            append(DecodeEncoding(bytes.data()+i+j,take,encoding),s.offset+i+j,take);j+=take;
        }
        append(L"\r");
    }
    std::wstring rtf=LR"({\rtf1\ansi\uc1\deff0{\fonttbl{\f0\fmodern MS Gothic;}}{\colortbl\red0\green0\blue0;\red45\green45\blue210;\red145\green75\blue55;\red190\green45\blue25;}\f0\fs22 )"+colored+L"}";
    DumpRtfStream input;input.text.reserve(rtf.size());for(wchar_t c:rtf)input.text.push_back(static_cast<char>(c));
    EDITSTREAM stream={};stream.dwCookie=reinterpret_cast<DWORD_PTR>(&input);stream.pfnCallback=StreamDumpRtf;
    s.settingText=true;SendDlgItemMessageW(h,Dump,WM_SETREDRAW,FALSE,0);SendDlgItemMessageW(h,Dump,EM_STREAMIN,SF_RTF,reinterpret_cast<LPARAM>(&stream));SendDlgItemMessageW(h,Dump,WM_SETREDRAW,TRUE,0);s.settingText=false;
    InvalidateRect(GetDlgItem(h,Dump),nullptr,TRUE);
    std::wstring info=s.document->path+L" | "+std::to_wstring(s.document->size)+L" bytes | Page "+std::to_wstring(s.offset/PageSize+1)+L" / "+std::to_wstring((std::max)(uint64_t(1),(s.document->size+PageSize-1)/PageSize));
    SetDlgItemTextW(h,Info,info.c_str());Inspect(h,s);Highlight(h,s);
}
void Select(HWND h,State& s,uint64_t offset,uint64_t length) {
    if(offset>s.document->size || length>s.document->size-offset) {Error(h,L"The selection is outside this file.");return;}
    s.highNibble=-1;s.activeNibble=-1;s.selection=offset;s.length=length;uint64_t page=offset/PageSize*PageSize;
    if(page==s.document->size&&page) page-=PageSize;
    if(s.offset!=page) {s.offset=page;Refresh(h,s);} else {Inspect(h,s);Highlight(h,s);}
}
void MouseSelection(HWND h,State& s) {
    if(s.job||s.settingText||s.positions.empty()) return;
    DWORD start=0,end=0;SendDlgItemMessageW(h,Dump,EM_GETSEL,reinterpret_cast<WPARAM>(&start),reinterpret_cast<LPARAM>(&end));
    if(start==s.lastStart&&end==s.lastEnd) return;
    s.lastStart=start;s.lastEnd=end;
    uint64_t low=NoByte,high=0;
    if(start==end) {if(start<s.positions.size()&&s.positions[start]!=NoByte) {low=s.positions[start];high=s.positionEnds[start];}}
    else for(size_t i=start;i<end&&i<s.positions.size();++i) if(s.positions[i]!=NoByte) {low=(std::min)(low,s.positions[i]);high=(std::max)(high,s.positionEnds[i]);}
    if(low==NoByte) return;
    s.highNibble=-1;s.activeNibble=-1;s.selection=low;s.length=high-low+1;Inspect(h,s);
}
bool Discard(HWND h,State& s) {
    return !s.document->Dirty()||s.changesSaved||MessageBoxW(h,L"Discard unsaved buffer changes? Use Save Copy to keep a separate DAT.",L"Unsaved changes",MB_OKCANCEL|MB_ICONQUESTION)==IDOK;
}
void Browse(HWND h,State& s) {
    std::wstring path;
    s.picking=true;bool picked=s.pickDatFile(h,s.root,L"Open DAT",path);s.picking=false;
    if(!picked)return;
    auto document=std::make_shared<DatHex::Document>();if(!document->Open(path)){Error(h,L"Cannot open the selected file.");return;}
    if(!Discard(h,s)) return;
    s.lastChecksum.clear();s.reportReady=false;s.chartStatistics.reset();
    s.document=std::move(document);s.offset=s.selection=0;s.length=s.document->size?1:0;s.lastPattern.clear();s.changesSaved=false;
    s.matches.clear();s.matchLength=0;SendDlgItemMessageW(h,MatchList,CB_RESETCONTENT,0,0);
    s.highNibble=-1;s.activeNibble=-1;s.bookmarks.clear();s.comparison.reset();s.compareStart=0;
    SendDlgItemMessageW(h,BookmarkList,CB_RESETCONTENT,0,0);SetDlgItemTextW(h,AnalysisOutput,L"");Refresh(h,s);
}
void Save(HWND h,State& s,bool binary) {
    std::wstring name=binary?std::filesystem::path(s.document->path).stem().wstring()+L"-copy.DAT":L"dat-hex-page.txt";
    wchar_t path[32768]={};if(name.size()<32768)std::copy(name.begin(),name.end(),path);
    OPENFILENAMEW d={};d.lStructSize=sizeof(d);d.hwndOwner=h;d.lpstrFile=path;d.nMaxFile=32768;
    d.lpstrFilter=binary?L"DAT copy\0*.dat\0\0":L"UTF-8 text\0*.txt\0\0";d.lpstrDefExt=binary?L"dat":L"txt";
    d.lpstrTitle=binary?L"Save a new DAT copy (existing files cannot be replaced)":L"Export page to a new text file";
    d.Flags=OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetSaveFileNameW(&d))return;
    bool success=false;
    if(binary) success=s.document->SaveCopy(path);
    else {
        std::wstring report=s.document->path+L"\r\nPage offset: "+Number(s.offset)+L"\r\n"+s.text;
        int n=WideCharToMultiByte(CP_UTF8,0,report.data(),static_cast<int>(report.size()),nullptr,0,nullptr,nullptr);
        std::string utf8(n,'\0');WideCharToMultiByte(CP_UTF8,0,report.data(),static_cast<int>(report.size()),utf8.data(),n,nullptr,nullptr);
        DatHex::NewCopy output(path);success=output.Valid()&&output.Write(utf8.data(),utf8.size())&&output.Commit();
    }
    if(!success) Error(h,L"Could not create the copy. Choose a new filename in a writable folder. Existing files, including original DATs, can never be overwritten.");
    else {
        if(binary)s.changesSaved=true;
        SetDlgItemTextW(h,Status,(L"Created copy: "+std::wstring(path)+L". The source DAT remains unchanged.").c_str());
    }
}
bool Clipboard(HWND h,const std::wstring& text) {
    if(!OpenClipboard(h))return false;
    bool copied=false;
    HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,(text.size()+1)*sizeof(wchar_t));
    if(memory) {
        void* p=GlobalLock(memory);
        if(p) {std::memcpy(p,text.c_str(),(text.size()+1)*sizeof(wchar_t));GlobalUnlock(memory);if(EmptyClipboard()&&SetClipboardData(CF_UNICODETEXT,memory)){memory=nullptr;copied=true;}}
        if(memory)GlobalFree(memory);
    }
    CloseClipboard();return copied;
}
enum class ByteFormat { Hex,CArray,Escaped,Base64,Pascal,Java,CSharp,VB,Binary };
std::wstring FormatBytes(const DatHex::Bytes& bytes,ByteFormat format) {
    if(format>=ByteFormat::Pascal&&format<=ByteFormat::VB) {
        if(bytes.empty())return {};
        std::wostringstream out;
        if(format==ByteFormat::Pascal)out<<L"const\r\n  Data: array[0.."<<bytes.size()-1<<L"] of Byte = (\r\n    ";
        else if(format==ByteFormat::Java)out<<L"byte[] data = new byte[] {\r\n    ";
        else if(format==ByteFormat::CSharp)out<<L"byte[] data = new byte[] {\r\n    ";
        else out<<L"Dim data As Byte() = {\r\n    ";
        out<<std::hex<<std::uppercase<<std::setfill(L'0');
        for(size_t i=0;i<bytes.size();++i) {
            if(i)out<<(i%16==0?(format==ByteFormat::VB?L", _\r\n    ":L",\r\n    "):L", ");
            out<<(format==ByteFormat::Pascal?L"$":format==ByteFormat::VB?L"&H":format==ByteFormat::Java?L"(byte)0x":L"0x")<<std::setw(2)<<unsigned(bytes[i]);
        }
        out<<(format==ByteFormat::Pascal?L"\r\n  );":format==ByteFormat::VB?L"\r\n}":L"\r\n};");return out.str();
    }
    if(format==ByteFormat::Binary){std::wstring out;for(auto byte:bytes){if(!out.empty())out+=L' ';for(int bit=7;bit>=0;--bit)out+=((byte>>bit)&1)?L'1':L'0';}return out;}
    if(format==ByteFormat::Base64) {
        const wchar_t* alphabet=L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::wstring out;out.reserve((bytes.size()+2)/3*4);
        for(size_t i=0;i<bytes.size();i+=3) {
            size_t count=(std::min)(size_t(3),bytes.size()-i);
            uint32_t group=uint32_t(bytes[i])<<16;if(count>1)group|=uint32_t(bytes[i+1])<<8;if(count>2)group|=bytes[i+2];
            out+=alphabet[(group>>18)&63];out+=alphabet[(group>>12)&63];out+=count>1?alphabet[(group>>6)&63]:L'=';out+=count>2?alphabet[group&63]:L'=';
        }
        return out;
    }
    std::wostringstream out;out<<std::hex<<std::uppercase<<std::setfill(L'0');
    if(format==ByteFormat::CArray)out<<L"const unsigned char data[] = {\r\n    ";
    for(size_t i=0;i<bytes.size();++i) {
        if(format==ByteFormat::Hex){if(i)out<<L' ';}
        else if(format==ByteFormat::CArray){if(i)out<<(i%16==0?L",\r\n    ":L", ");out<<L"0x";}
        else out<<L"\\x";
        out<<std::setw(2)<<unsigned(bytes[i]);
    }
    if(format==ByteFormat::CArray)out<<L"\r\n};";
    return out.str();
}
void Copy(HWND h,State& s,bool hex,ByteFormat format=ByteFormat::Hex) {
    if(!s.length||s.length>1024*1024) {Error(h,L"Select between 1 byte and 1 MB to copy.");return;}
    DatHex::Bytes bytes;if(!s.document->Read(s.selection,static_cast<size_t>(s.length),bytes)){Error(h,L"Cannot read this selection.");return;}
    auto text=hex?FormatBytes(bytes,format):DecodeEncoding(bytes.data(),bytes.size(),static_cast<int>(SendDlgItemMessageW(h,Encoding,CB_GETCURSEL,0,0)));
    if(!Clipboard(h,text))Error(h,L"The selection could not be copied. Try again when the clipboard is available.");
    else SetDlgItemTextW(h,Status,(L"Copied "+std::to_wstring(s.length)+L" bytes as "+(hex?(format==ByteFormat::CArray?L"a C array":format==ByteFormat::Escaped?L"escaped bytes":format==ByteFormat::Base64?L"Base64":format>=ByteFormat::Pascal?L"source code":L"hex"):L"decoded text")+L".").c_str());
}
DatHex::Bytes RepeatedPattern(const DatHex::Bytes& pattern,size_t length) {
    if(pattern.empty())return {};
    DatHex::Bytes bytes(length);for(size_t i=0;i<length;++i)bytes[i]=pattern[i%pattern.size()];return bytes;
}
void Changed(HWND h,State& s) {
    s.lastChecksum.clear();s.reportReady=false;s.chartStatistics.reset();
    s.matches.clear();s.matchLength=0;SendDlgItemMessageW(h,MatchList,CB_RESETCONTENT,0,0);
    s.changesSaved=false;s.highNibble=-1;s.compareStart=0;
    SetDlgItemTextW(h,AnalysisOutput,L"Buffer changed. Recalculate checksums or compare again for current results.");
}
enum class PasteFormat { Hex,Decoded,Utf8,Base64,Escaped,Binary };
bool DecodePaste(const std::wstring& text,PasteFormat format,int encoding,DatHex::Bytes& output) {
    output.clear();if(text.empty()||text.size()>PageSize*9)return false;
    DatHex::Bytes bytes;
    if(format==PasteFormat::Hex){if(!DatHex::ParseHex(text,bytes))return false;}
    else if(format==PasteFormat::Binary){if(!ParseBinary(text,bytes))return false;}
    else if(format==PasteFormat::Escaped){
        std::wstring compact;for(wchar_t c:text)if(c!=L' '&&c!=L'\t'&&c!=L'\r'&&c!=L'\n')compact+=c;
        if(compact.empty()||compact.size()%4)return false;
        std::wstring hex;for(size_t i=0;i<compact.size();i+=4){if(compact[i]!=L'\\'||compact[i+1]!=L'x')return false;hex+=compact.substr(i+2,2);hex+=L' ';}
        if(!DatHex::ParseHex(hex,bytes))return false;
    }
    else if(format==PasteFormat::Base64){
        std::wstring compact;for(wchar_t c:text)if(c!=L' '&&c!=L'\t'&&c!=L'\r'&&c!=L'\n')compact+=c;
        if(compact.empty()||compact.size()%4)return false;
        auto digit=[](wchar_t c)->int{if(c>=L'A'&&c<=L'Z')return c-L'A';if(c>=L'a'&&c<=L'z')return c-L'a'+26;if(c>=L'0'&&c<=L'9')return c-L'0'+52;return c==L'+'?62:c==L'/'?63:-1;};
        for(size_t i=0;i<compact.size();i+=4){int a=digit(compact[i]),b=digit(compact[i+1]),c=digit(compact[i+2]),d=digit(compact[i+3]);
            bool pad2=compact[i+2]==L'=',pad1=compact[i+3]==L'=';
            if(a<0||b<0||(!pad2&&c<0)||(!pad1&&d<0)||(pad2&&!pad1)||((pad1||pad2)&&i+4!=compact.size())||(pad2&&(b&15))||(pad1&&!pad2&&(c&3)))return false;
            bytes.push_back(static_cast<unsigned char>((a<<2)|(b>>4)));
            if(!pad2)bytes.push_back(static_cast<unsigned char>((b<<4)|(c>>2)));
            if(!pad1)bytes.push_back(static_cast<unsigned char>((c<<6)|d));
        }
    }else{
        // Validate UTF-16 even when converting to a legacy encoding.
        for(size_t i=0;i<text.size();++i){unsigned c=text[i];if(c>=0xd800&&c<=0xdbff){if(++i>=text.size()||text[i]<0xdc00||text[i]>0xdfff)return false;}else if(c>=0xdc00&&c<=0xdfff)return false;}
        UINT page=format==PasteFormat::Utf8?CP_UTF8:EncodingPage(encoding);if(!page||!IsValidCodePage(page))return false;
        BOOL substituted=FALSE;DWORD flags=page==CP_UTF8?WC_ERR_INVALID_CHARS:WC_NO_BEST_FIT_CHARS;
        int count=WideCharToMultiByte(page,flags,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,page==CP_UTF8?nullptr:&substituted);
        if(!count||count>PageSize||substituted)return false;bytes.resize(count);
        if(WideCharToMultiByte(page,flags,text.data(),static_cast<int>(text.size()),reinterpret_cast<char*>(bytes.data()),count,nullptr,page==CP_UTF8?nullptr:&substituted)!=count||substituted)return false;
    }
    if(bytes.empty()||bytes.size()>PageSize)return false;output=std::move(bytes);return true;
}
bool PasteBytes(HWND h,State& s,const DatHex::Bytes& bytes) {
    if(!s.edit||s.job||!s.document->Loaded()||bytes.empty()||bytes.size()>PageSize)return false;
    if(s.selection>s.document->size||bytes.size()>s.document->size-s.selection) {Error(h,L"Pasted bytes would extend past the end of this DAT. File size must stay unchanged.");return false;}
    if(!s.document->Overwrite(s.selection,bytes)){Error(h,L"Could not paste these bytes.");return false;}
    s.activeNibble=-1;Changed(h,s);s.length=bytes.size();Refresh(h,s);return true;
}
bool PasteHex(HWND h,State& s,const std::wstring& text) {
    if(!s.edit||s.job||!s.document->Loaded())return false;
    DatHex::Bytes bytes;if(!DecodePaste(text,PasteFormat::Hex,0,bytes)){Error(h,L"Paste Overwrite needs complete hex byte pairs (up to 64 KB), for example: 01 FF 82 A0.");return false;}
    return PasteBytes(h,s,bytes);
}
void Paste(HWND h,State& s,PasteFormat format=PasteFormat::Hex) {
    if(!OpenClipboard(h))return;std::wstring text;HANDLE data=GetClipboardData(CF_UNICODETEXT);
    if(data){size_t length=GlobalSize(data)/sizeof(wchar_t);auto p=static_cast<const wchar_t*>(GlobalLock(data));
        if(p&&length&&length<=PageSize*9+1){const auto end=std::find(p,p+length,L'\0');if(end!=p+length)text.assign(p,end);}if(p)GlobalUnlock(data);
    }
    CloseClipboard();DatHex::Bytes bytes;
    if(!DecodePaste(text,format,static_cast<int>(SendDlgItemMessageW(h,Encoding,CB_GETCURSEL,0,0)),bytes)){Error(h,L"Clipboard data is invalid for this paste format, cannot be represented in the chosen encoding, or exceeds 64 KB. No bytes were changed.");return;}
    PasteBytes(h,s,bytes);
}
void SelectNibble(HWND h,State& s,uint64_t offset,int digit) {
    Select(h,s,offset,1);s.activeNibble=digit;
    // The first occurrence is the hex column; decoded text also maps to bytes.
    for(size_t i=0;i+1<s.positions.size();++i)if(s.positions[i]==offset&&s.positions[i+1]==offset){
        s.lastStart=static_cast<DWORD>(i+digit);s.lastEnd=s.lastStart+1;
        SendDlgItemMessageW(h,Dump,EM_SETSEL,s.lastStart,s.lastEnd);SendDlgItemMessageW(h,Dump,EM_SCROLLCARET,0,0);break;
    }
    SetDlgItemTextW(h,Status,(L"Offset 0x"+Number(offset)+L" - "+(digit?L"low":L"high")+L" hex digit. Ctrl+Left/Right moves by digit; Esc returns to byte mode.").c_str());
}
void MoveNibble(HWND h,State& s,bool right) {
    if(s.job||!s.document->Loaded()||!s.document->size)return;
    if(s.displayMode==2){SetDlgItemTextW(h,Status,L"Choose a view with hex bytes to navigate individual digits.");return;}
    uint64_t offset=(std::min)(s.selection,s.document->size-1);int digit=s.activeNibble<0?0:s.activeNibble;
    if(right){if(!digit)digit=1;else if(offset<s.document->size-1){++offset;digit=0;}}
    else {if(digit)digit=0;else if(offset){--offset;digit=1;}}
    SelectNibble(h,s,offset,digit);
}
void TypeHex(HWND h,State& s,wchar_t c) {
    if(!s.edit||s.job||!s.document->Loaded()||s.selection>=s.document->size)return;
    int digit=c>=L'0'&&c<=L'9'?c-L'0':c>=L'a'&&c<=L'f'?c-L'a'+10:c>=L'A'&&c<=L'F'?c-L'A'+10:-1;
    if(digit<0)return;
    if(s.activeNibble>=0){
        DatHex::Bytes old;if(!s.document->Read(s.selection,1,old))return;
        int nibble=s.activeNibble;uint64_t offset=s.selection;
        unsigned char value=static_cast<unsigned char>(nibble?(old[0]&0xf0)|digit:(old[0]&0x0f)|(digit<<4));
        if(!s.document->Overwrite(offset,DatHex::Bytes{value})){Error(h,L"Could not overwrite this hex digit.");return;}
        Changed(h,s);Refresh(h,s);SelectNibble(h,s,offset,nibble);MoveNibble(h,s,true);return;
    }
    if(s.highNibble<0 || s.nibbleOffset!=s.selection) {
        s.highNibble=digit;s.nibbleOffset=s.selection;
        SetDlgItemTextW(h,Status,(L"Pending hex byte: "+Number(digit)+L"_   Type its second digit, or Esc to cancel.").c_str());return;
    }
    unsigned char value=static_cast<unsigned char>(s.highNibble*16+digit);
    uint64_t at=s.selection;
    if(!s.document->Overwrite(at,DatHex::Bytes{value})){Error(h,L"Could not overwrite this byte.");return;}
    Changed(h,s);s.length=1;Refresh(h,s);
    Select(h,s,(std::min)(at+1,s.document->size-1),1);
}
void Bookmarks(HWND h,State& s) {
    SendDlgItemMessageW(h,BookmarkList,CB_RESETCONTENT,0,0);
    for(const auto& mark:s.bookmarks) {
        std::wstring label=(mark.slot>=0?L"["+std::to_wstring(mark.slot)+L"] ":L"")+std::wstring(L"Offset ")+Number(mark.offset,s.decimal)+L" / "+Number(mark.length,s.decimal)+L" bytes";
        SendDlgItemMessageW(h,BookmarkList,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
    }
    if(!s.bookmarks.empty())SendDlgItemMessageW(h,BookmarkList,CB_SETCURSEL,static_cast<WPARAM>(s.bookmarks.size()-1),0);
    Buttons(h,s);
}
int BookmarkShortcut(WPARAM key,bool control,bool shift) {
    if(!control||key<'0'||key>'9')return 0;
    return (shift?SetNumberedBookmark:GoNumberedBookmark)+static_cast<int>(key-'0');
}
void NumberedBookmarkMenus(HMENU menu,const State& state) {
    HMENU set=CreatePopupMenu(),go=CreatePopupMenu();
    for(int slot=0;slot<10;++slot) {
        auto label=std::to_wstring(slot);auto found=std::find_if(state.bookmarks.begin(),state.bookmarks.end(),[&](const State::Bookmark& bookmark){return bookmark.slot==slot;});
        auto setLabel=label+L"\tCtrl+Shift+"+label;
        AppendMenuW(set,MF_STRING,SetNumberedBookmark+slot,setLabel.c_str());
        auto goLabel=label+(found==state.bookmarks.end()?L" (unset)":L" - offset "+Number(found->offset,state.decimal))+L"\tCtrl+"+label;
        AppendMenuW(go,MF_STRING|(found==state.bookmarks.end()?MF_GRAYED:0),GoNumberedBookmark+slot,goLabel.c_str());
    }
    AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(set),L"Set numbered bookmark");AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(go),L"Go to numbered bookmark");
}
void SaveBlock(HWND h,State& s) {
    if(!s.length){Error(h,L"Select bytes to export as a new binary file.");return;}
    wchar_t path[32768]=L"dat-selection.bin";
    OPENFILENAMEW d={};d.lStructSize=sizeof(d);d.hwndOwner=h;d.lpstrFile=path;d.nMaxFile=32768;
    d.lpstrFilter=L"Binary selection\0*.bin\0All files\0*.*\0\0";d.lpstrDefExt=L"bin";
    d.lpstrTitle=L"Export selection to a new file";d.Flags=OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetSaveFileNameW(&d))return;
    if(!s.document->SaveRangeCopy(path,s.selection,s.length))Error(h,L"Could not create the selection copy. Choose a new filename; existing files cannot be replaced.");
    else SetDlgItemTextW(h,Status,(L"Created binary selection copy: "+std::wstring(path)).c_str());
}
// Intel HEX-386: 16-byte data records, extended linear addresses, and EOF.
std::string IntelHexRecord(unsigned type,unsigned address,const DatHex::Bytes& bytes) {
    std::ostringstream out;out<<':'<<std::hex<<std::uppercase<<std::setfill('0')<<std::setw(2)<<bytes.size()<<std::setw(4)<<address<<std::setw(2)<<type;
    unsigned sum=static_cast<unsigned>(bytes.size())+(address>>8)+(address&255)+type;
    for(auto byte:bytes){out<<std::setw(2)<<unsigned(byte);sum+=byte;}
    out<<std::setw(2)<<((0u-sum)&255)<<"\r\n";return out.str();
}
bool SaveIntelHex(const DatHex::Document& document,const std::wstring& path,uint64_t start,uint64_t length) {
    if(!document.Loaded()||start>document.size||length>document.size-start||start>UINT32_MAX||(length&&length-1>UINT32_MAX-start))return false;
    DatHex::NewCopy output(path);if(!output.Valid())return false;
    std::string text;DatHex::Bytes bytes;uint64_t consumed=0;uint64_t lastUpper=UINT64_MAX;
    while(consumed<length) {
        uint64_t at=start+consumed,upper=at>>16;
        if(upper!=lastUpper){text+=IntelHexRecord(4,0,DatHex::Bytes({static_cast<unsigned char>(upper>>8),static_cast<unsigned char>(upper)}));lastUpper=upper;}
        size_t count=static_cast<size_t>((std::min)(length-consumed,65536-(at&65535)));
        if(!document.Read(at,count,bytes))return false;
        for(size_t i=0;i<count;i+=16){size_t part=(std::min)(size_t(16),count-i);
            text+=IntelHexRecord(0,static_cast<unsigned>((at+i)&65535),DatHex::Bytes(bytes.begin()+i,bytes.begin()+i+part));
            if(text.size()>=65536){if(!output.Write(text.data(),text.size()))return false;text.clear();}
        }
        consumed+=count;
    }
    text+=IntelHexRecord(1,0,{});return output.Write(text.data(),text.size())&&output.Commit();
}
void ExportIntelSelection(HWND h,State& s) {
    if(!s.length||s.length>1048576){Error(h,L"Select between 1 byte and 1 MB to export as Intel HEX.");return;}
    if(s.selection>UINT32_MAX||s.length-1>UINT32_MAX-s.selection){Error(h,L"Intel HEX export requires offsets within the 32-bit address range.");return;}
    wchar_t path[32768]=L"dat-selection.hex";OPENFILENAMEW dialog={};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=h;
    dialog.lpstrFile=path;dialog.nMaxFile=32768;dialog.lpstrFilter=L"Intel HEX\0*.hex\0\0";dialog.lpstrDefExt=L"hex";
    dialog.lpstrTitle=L"Export selection as a new Intel HEX file";dialog.Flags=OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetSaveFileNameW(&dialog))return;
    if(!SaveIntelHex(*s.document,path,s.selection,s.length))Error(h,L"Could not create Intel HEX output. Choose a new filename; existing files cannot be replaced.");
    else SetDlgItemTextW(h,Status,L"Created Intel HEX selection copy with original file offsets.");
}
bool SaveUtf8Copy(const std::wstring& path,const std::wstring& text) {
    if(text.empty())return false;
    int count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);
    if(!count)return false;
    std::string bytes(count,'\0');
    if(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),bytes.data(),count,nullptr,nullptr)!=count)return false;
    DatHex::NewCopy output(path);return output.Valid()&&output.Write(bytes.data(),bytes.size())&&output.Commit();
}
bool SaveFormattedSelection(const DatHex::Document& document,const std::wstring& path,uint64_t start,uint64_t length,ByteFormat format) {
    if(!length||length>1048576||start>document.size||length>document.size-start)return false;
    DatHex::Bytes bytes;if(!document.Read(start,static_cast<size_t>(length),bytes))return false;
    return SaveUtf8Copy(path,FormatBytes(bytes,format));
}
void ExportFormattedSelection(HWND h,State& s,ByteFormat format) {
    if(!s.length||s.length>1048576){Error(h,L"Select between 1 byte and 1 MB for a formatted export.");return;}
    bool array=format==ByteFormat::CArray;wchar_t path[32768]={};
    const wchar_t* extension=array?L"h":format==ByteFormat::Pascal?L"pas":format==ByteFormat::Java?L"java":format==ByteFormat::CSharp?L"cs":format==ByteFormat::VB?L"vb":L"txt";
    swprintf_s(path,L"dat-selection.%s",extension);
    OPENFILENAMEW dialog={};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=h;dialog.lpstrFile=path;dialog.nMaxFile=32768;
    dialog.lpstrFilter=format>=ByteFormat::Pascal?L"Source code\0*.*\0\0":array?L"C header\0*.h\0\0":L"UTF-8 text\0*.txt\0\0";dialog.lpstrDefExt=extension;
    dialog.lpstrTitle=L"Export selection to a new formatted file";dialog.Flags=OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetSaveFileNameW(&dialog))return;
    if(!SaveFormattedSelection(*s.document,path,s.selection,s.length,format))Error(h,L"Could not create formatted output. Choose a new filename; existing files cannot be replaced.");
    else SetDlgItemTextW(h,Status,L"Created formatted selection copy.");
}
std::optional<uint64_t> PointerDestination(const DatHex::Bytes& bytes,size_t width,bool big,bool relative,uint64_t current,uint64_t size) {
    if((width!=2&&width!=3&&width!=4&&width!=8)||bytes.size()<width||current>size)return {};
    uint64_t value=DatHex::UnsignedValue(bytes,width,big);
    if(!relative)return value<=size?std::optional<uint64_t>(value):std::nullopt;
    bool negative=(value&(uint64_t(1)<<(width*8-1)))!=0;
    if(negative){uint64_t mask=width==8?UINT64_MAX:(uint64_t(1)<<(width*8))-1;uint64_t distance=((~value)&mask)+1;return distance<=current?std::optional<uint64_t>(current-distance):std::nullopt;}
    return value<=size-current?std::optional<uint64_t>(current+value):std::nullopt;
}
std::optional<uint64_t> ValueStep(uint64_t current,uint64_t size,size_t width,bool next) {
    if(!width||current>size||width>size)return {};
    if(next){if(width>size-current)return {};uint64_t destination=current+width;if(width>size-destination)return {};return destination;}
    if(width>current)return {};uint64_t destination=current-width;if(width>size-destination)return {};return destination;
}
void ValueMenus(HMENU menu) {
    HMENU next=CreatePopupMenu(),previous=CreatePopupMenu();const wchar_t* labels[]={L"1 byte (8-bit)",L"2 bytes (16-bit)",L"3 bytes (24-bit)",L"4 bytes (32-bit / float)",L"8 bytes (64-bit / double)",L"16 bytes (GUID)"};
    for(int i=0;i<6;++i){AppendMenuW(next,MF_STRING,NextValue1+i,labels[i]);AppendMenuW(previous,MF_STRING,PreviousValue1+i,labels[i]);}
    AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(next),L"Next value by size");AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(previous),L"Previous value by size");
}
void StepValue(HWND h,State& state,int id) {
    bool next=id<PreviousValue1;size_t widths[]={1,2,3,4,8,16};size_t width=widths[id-(next?NextValue1:PreviousValue1)];
    auto destination=ValueStep(state.selection,state.document->size,width,next);
    if(!destination){SetDlgItemTextW(h,Status,L"No complete value of that size in this direction. Selection kept.");return;}
    Select(h,state,*destination,width);
}
void PointerMenus(HMENU menu) {
    HMENU absolute=CreatePopupMenu(),relative=CreatePopupMenu();const wchar_t* labels[]={L"16-bit value",L"24-bit value",L"32-bit value",L"64-bit value"};
    for(int i=0;i<4;++i){AppendMenuW(absolute,MF_STRING,Pointer16+i,labels[i]);AppendMenuW(relative,MF_STRING,Relative16+i,labels[i]);}
    AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(absolute),L"Follow unsigned pointer (absolute offset)");
    AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(relative),L"Follow signed pointer (relative to current byte)");
}
void FollowPointer(HWND h,State& state,int id) {
    bool relative=id>=Relative16;size_t widths[]={2,3,4,8};size_t width=widths[id-(relative?Relative16:Pointer16)];
    DatHex::Bytes bytes;
    if(state.selection>state.document->size||width>state.document->size-state.selection||!state.document->Read(state.selection,width,bytes)){Error(h,L"There are not enough bytes at this offset for that pointer width.");return;}
    bool big=SendDlgItemMessageW(h,Endian,CB_GETCURSEL,0,0)==1;
    auto destination=PointerDestination(bytes,width,big,relative,state.selection,state.document->size);
    if(!destination){Error(h,L"This pointer leads outside the file. The current selection was kept.");return;}
    Select(h,state,*destination,*destination<state.document->size?1:0);
}
void InspectorReport(HWND h,State& state,bool copy) {
    if(!state.document->Loaded()||state.selection>=state.document->size){Error(h,L"Select a byte before copying or exporting its values.");return;}
    Inspect(h,state);
    auto report=L"DATura data inspector\r\nSource: "+state.document->path+L"\r\nBuffer: "+(state.document->Dirty()?std::wstring(L"modified"):std::wstring(L"unmodified"))+L"\r\nByte order: "+(SendDlgItemMessageW(h,Endian,CB_GETCURSEL,0,0)==1?L"big endian":L"little endian")+L"\r\nInteger display: "+(state.inspectorHex?L"hexadecimal":L"decimal")+L"\r\nSelection length: "+std::to_wstring(state.length)+L" bytes\r\n\r\n"+Text(h,Inspector);
    if(copy){if(!Clipboard(h,report))Error(h,L"The inspector readout could not be copied.");else SetDlgItemTextW(h,Status,L"Copied the complete data inspector readout.");return;}
    wchar_t path[32768]=L"dat-inspector.txt";OPENFILENAMEW dialog={};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=h;
    dialog.lpstrFile=path;dialog.nMaxFile=32768;dialog.lpstrFilter=L"UTF-8 inspector report\0*.txt\0\0";dialog.lpstrDefExt=L"txt";
    dialog.lpstrTitle=L"Export inspector readout to a new file";dialog.Flags=OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetSaveFileNameW(&dialog))return;
    if(!SaveUtf8Copy(path,report))Error(h,L"Could not create the inspector report. Choose a new filename; existing files cannot be replaced.");
    else SetDlgItemTextW(h,Status,L"Created a new inspector report.");
}
void AddFormattedExports(HMENU menu) {
    AppendMenuW(menu,MF_STRING,ExportInspector,L"Data inspector report (.txt)...");
    AppendMenuW(menu,MF_STRING,JoinBefore,L"Join file before buffer (new copy)...");AppendMenuW(menu,MF_STRING,JoinAfter,L"Join file after buffer (new copy)...");
    HMENU split=CreatePopupMenu();AppendMenuW(split,MF_STRING,Split64K,L"64 KB parts...");AppendMenuW(split,MF_STRING,Split1M,L"1 MB parts...");AppendMenuW(split,MF_STRING,Split16M,L"16 MB parts...");AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(split),L"Split buffer into new copies");
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
    AppendMenuW(menu,MF_STRING,ExportPlainTable,L"Plain text hex table (.txt)...");
    AppendMenuW(menu,MF_STRING,ExportRtf,L"Rich Text hex dump (.rtf)...");AppendMenuW(menu,MF_STRING,ExportTex,L"LaTeX hex table (.tex)...");
    AppendMenuW(menu,MF_STRING,ExportSRecord,L"Motorola S-record (.srec)...");AppendMenuW(menu,MF_STRING,ExportHtml,L"HTML hex table (.html)...");
    AppendMenuW(menu,MF_STRING,ExportPascal,L"Pascal byte array (.pas)...");AppendMenuW(menu,MF_STRING,ExportJava,L"Java byte array (.java)...");
    AppendMenuW(menu,MF_STRING,ExportCSharp,L"C# byte array (.cs)...");AppendMenuW(menu,MF_STRING,ExportVB,L"VB.NET byte array (.vb)...");
    AppendMenuW(menu,MF_STRING,ExportCArray,L"C byte array (.h)...");AppendMenuW(menu,MF_STRING,ExportEscaped,L"Escaped bytes (.txt)...");AppendMenuW(menu,MF_STRING,ExportBase64,L"Base64 (.txt)...");
}
std::wstring SRecord(unsigned type,unsigned address,unsigned addressBytes,const DatHex::Bytes& bytes) {
    unsigned count=addressBytes+static_cast<unsigned>(bytes.size())+1,sum=count;
    std::wostringstream out;out<<L'S'<<type<<std::hex<<std::uppercase<<std::setfill(L'0')<<std::setw(2)<<count;
    for(unsigned i=addressBytes;i>0;--i){unsigned byte=(address>>((i-1)*8))&255;sum+=byte;out<<std::setw(2)<<byte;}
    for(auto byte:bytes){sum+=byte;out<<std::setw(2)<<unsigned(byte);}
    out<<std::setw(2)<<((~sum)&255)<<L"\r\n";return out.str();
}
std::wstring HtmlEscape(const std::wstring& text) {
    std::wstring result;for(auto c:text){if(c==L'&')result+=L"&amp;";else if(c==L'<')result+=L"&lt;";else if(c==L'>')result+=L"&gt;";else if(c==L'\"')result+=L"&quot;";else result+=c;}return result;
}
std::optional<std::wstring> FormattedDocumentSelection(const DatHex::Document& document,uint64_t start,uint64_t length,bool html,int encoding) {
    if(!length||length>1048576||start>document.size||length>document.size-start||(!html&&(start>UINT32_MAX||length-1>UINT32_MAX-start)))return {};
    DatHex::Bytes bytes;if(!document.Read(start,static_cast<size_t>(length),bytes))return {};
    std::wstring output;
    if(html)output=L"<!doctype html>\r\n<html><head><meta charset=\"utf-8\"><title>DATura hex export</title>"
        L"<style>body{font-family:system-ui;margin:2em}table{border-collapse:collapse;font-family:monospace}th,td{padding:.3em .8em;border:1px solid #ccc;text-align:left;white-space:pre}th{background:#eee}</style>"
        L"</head><body><h1>DATura hex export</h1><p>Source: "+HtmlEscape(document.path)+L"</p><p>Buffer snapshot: offset 0x"+Number(start)+L", "+std::to_wstring(length)+L" bytes</p><table><thead><tr><th>Offset (hex)</th><th>Hex bytes</th><th>Decoded text</th></tr></thead><tbody>\r\n";
    else output=SRecord(0,0,2,DatHex::Bytes({'D','A','T','u','r','a'}));
    for(size_t i=0;i<bytes.size();i+=16) {
        size_t count=(std::min)(size_t(16),bytes.size()-i);DatHex::Bytes row(bytes.begin()+i,bytes.begin()+i+count);
        if(html)output+=L"<tr><td>"+Number(start+i)+L"</td><td>"+FormatBytes(row,ByteFormat::Hex)+L"</td><td>"+HtmlEscape(DecodeEncoding(row.data(),row.size(),encoding))+L"</td></tr>\r\n";
        else output+=SRecord(3,static_cast<unsigned>(start+i),4,row);
    }
    output+=html?L"</tbody></table></body></html>\r\n":SRecord(7,0,4,{});return output;
}
bool SaveDocumentSelection(const DatHex::Document& document,const std::wstring& path,uint64_t start,uint64_t length,bool html,int encoding) {
    auto output=FormattedDocumentSelection(document,start,length,html,encoding);return output&&SaveUtf8Copy(path,*output);
}
void ExportDocumentSelection(HWND h,State& state,bool html) {
    if(!state.length||state.length>1048576){Error(h,L"Select between 1 byte and 1 MB to export.");return;}
    if(!html&&(state.selection>UINT32_MAX||state.length-1>UINT32_MAX-state.selection)){Error(h,L"Motorola S-record requires offsets within the 32-bit address range.");return;}
    wchar_t path[32768];wcscpy_s(path,html?L"dat-selection.html":L"dat-selection.srec");OPENFILENAMEW dialog={};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=h;
    dialog.lpstrFile=path;dialog.nMaxFile=32768;dialog.lpstrFilter=html?L"HTML hex table\0*.html\0\0":L"Motorola S-record\0*.srec\0\0";dialog.lpstrDefExt=html?L"html":L"srec";
    dialog.lpstrTitle=L"Export selection to a new file";dialog.Flags=OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetSaveFileNameW(&dialog))return;
    if(!SaveDocumentSelection(*state.document,path,state.selection,state.length,html,static_cast<int>(SendDlgItemMessageW(h,Encoding,CB_GETCURSEL,0,0))))Error(h,L"Could not create output. Choose a new filename; existing files cannot be replaced.");
    else SetDlgItemTextW(h,Status,L"Created selection export with original byte offsets.");
}
std::optional<std::wstring> PlainSelection(const DatHex::Document& document,uint64_t start,uint64_t length,int encoding) {
    if(!length||length>1048576||start>document.size||length>document.size-start)return {};
    DatHex::Bytes bytes;if(!document.Read(start,static_cast<size_t>(length),bytes))return {};
    std::wstring output=L"DATura hex export\r\nSource: "+document.path+L"\r\nBuffer snapshot: offset 0x"+Number(start)+L", "+std::to_wstring(length)+L" bytes\r\n\r\nOffset (hex)       Hex bytes                                          Decoded text\r\n";
    for(size_t i=0;i<bytes.size();i+=16){size_t count=(std::min)(size_t(16),bytes.size()-i);
        DatHex::Bytes row(bytes.begin()+i,bytes.begin()+i+count);auto hex=FormatBytes(row,ByteFormat::Hex);
        std::wostringstream offset;offset<<std::hex<<std::uppercase<<std::setw(16)<<std::setfill(L'0')<<start+i;
        output+=offset.str()+L"  "+hex+std::wstring(47-hex.size(),L' ')+L"  "+DecodeEncoding(row.data(),row.size(),encoding)+L"\r\n";
    }
    return output;
}
bool SavePlainSelection(const DatHex::Document& document,const std::wstring& path,uint64_t start,uint64_t length,int encoding) {
    auto output=PlainSelection(document,start,length,encoding);return output&&SaveUtf8Copy(path,*output);
}
void PlainTable(HWND h,State& state,bool copy) {
    auto text=PlainSelection(*state.document,state.selection,state.length,static_cast<int>(SendDlgItemMessageW(h,Encoding,CB_GETCURSEL,0,0)));
    if(!text){Error(h,L"Select between 1 byte and 1 MB to create a hex table.");return;}
    if(copy){if(!Clipboard(h,*text))Error(h,L"The hex table could not be copied. Try again when the clipboard is available.");else SetDlgItemTextW(h,Status,L"Copied selection as a hex table with offsets and decoded text.");return;}
    wchar_t path[32768]=L"dat-selection-table.txt";OPENFILENAMEW dialog={};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=h;
    dialog.lpstrFile=path;dialog.nMaxFile=32768;dialog.lpstrFilter=L"UTF-8 hex table\0*.txt\0\0";dialog.lpstrDefExt=L"txt";
    dialog.lpstrTitle=L"Export selection to a new hex table";dialog.Flags=OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetSaveFileNameW(&dialog))return;
    if(!SaveUtf8Copy(path,*text))Error(h,L"Could not create the hex table. Choose a new filename; existing files cannot be replaced.");
    else SetDlgItemTextW(h,Status,L"Created UTF-8 selection hex table with original byte offsets.");
}
std::wstring RtfEscape(const std::wstring& input) {
    std::wstring output;
    for(size_t i=0;i<input.size();++i){wchar_t c=input[i];
        if(c==L'\r'||c==L'\n'){if(c==L'\r'&&i+1<input.size()&&input[i+1]==L'\n')++i;output+=L"\\par\r\n";}
        else if(c==L'\\'||c==L'{'||c==L'}'){output+=L'\\';output+=c;}
        else if(c>=32&&c<=126)output+=c;
        else output+=L"\\u"+std::to_wstring(static_cast<int16_t>(c))+L"?";
    }
    return output;
}
std::optional<std::wstring> RichSelection(const DatHex::Document& document,uint64_t start,uint64_t length,bool rtf,int encoding) {
    if(!length||length>1048576||start>document.size||length>document.size-start)return {};
    DatHex::Bytes bytes;if(!document.Read(start,static_cast<size_t>(length),bytes))return {};
    std::wstring body;
    if(rtf)body=L"DATura hex export\r\nSource: "+document.path+L"\r\nBuffer snapshot: offset 0x"+Number(start)+L", "+std::to_wstring(length)+L" bytes\r\n\r\nOffset (hex)       Hex bytes                                          Decoded text\r\n";
    else body=L"\\documentclass{article}\n\\usepackage[margin=1.5cm]{geometry}\n\\begin{document}\n\\section*{DATura hex export}\nOffset: "+Number(start)+L" (hex), "+std::to_wstring(length)+L" bytes.\n\\small\n\\begin{verbatim}\nOffset (hex)      Hex bytes\n";
    for(size_t i=0;i<bytes.size();i+=16) {
        size_t count=(std::min)(size_t(16),bytes.size()-i);DatHex::Bytes row(bytes.begin()+i,bytes.begin()+i+count);
        std::wostringstream offset;offset<<std::hex<<std::uppercase<<std::setw(16)<<std::setfill(L'0')<<start+i;
        auto hex=FormatBytes(row,ByteFormat::Hex);body+=offset.str()+L"  "+hex;
        if(rtf)body+=std::wstring(47-hex.size(),L' ')+L"  "+DecodeEncoding(row.data(),row.size(),encoding);
        body+=rtf?L"\r\n":L"\n";
    }
    if(rtf)return L"{\\rtf1\\ansi\\ansicpg1252\\uc1\\deff0{\\fonttbl{\\f0\\fmodern MS Gothic;}}\\f0\\fs18 "+RtfEscape(body)+L"}";
    return body+L"\\end{verbatim}\n\\end{document}\n";
}
bool SaveRichSelection(const DatHex::Document& document,const std::wstring& path,uint64_t start,uint64_t length,bool rtf,int encoding) {
    auto output=RichSelection(document,start,length,rtf,encoding);return output&&SaveUtf8Copy(path,*output);
}
void ExportRichSelection(HWND h,State& state,bool rtf) {
    if(!state.length||state.length>1048576){Error(h,L"Select between 1 byte and 1 MB to export.");return;}
    wchar_t path[32768];wcscpy_s(path,rtf?L"dat-selection.rtf":L"dat-selection.tex");OPENFILENAMEW dialog={};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=h;
    dialog.lpstrFile=path;dialog.nMaxFile=32768;dialog.lpstrFilter=rtf?L"Rich Text hex dump\0*.rtf\0\0":L"LaTeX hex table\0*.tex\0\0";dialog.lpstrDefExt=rtf?L"rtf":L"tex";
    dialog.lpstrTitle=L"Export selection to a new document";dialog.Flags=OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetSaveFileNameW(&dialog))return;
    if(!SaveRichSelection(*state.document,path,state.selection,state.length,rtf,static_cast<int>(SendDlgItemMessageW(h,Encoding,CB_GETCURSEL,0,0))))Error(h,L"Could not create the document. Choose a new filename; existing files cannot be replaced.");
    else SetDlgItemTextW(h,Status,L"Created selection document with original byte offsets.");
}
void PublishReport(HWND h,State& s,const std::wstring& report) {
    s.reportReady=true;SetDlgItemTextW(h,AnalysisOutput,report.c_str());Buttons(h,s);
}
void SaveReport(HWND h,State& s) {
    if(!s.reportReady)return;
    std::wstring report=L"DATura analysis report\r\nSource: "+s.document->path+L"\r\nSource size: "+std::to_wstring(s.document->size)+
        L" bytes\r\nBuffer: "+(s.document->Dirty()?L"includes unsaved edits":L"matches source")+L"\r\n\r\n"+Text(h,AnalysisOutput);
    wchar_t path[32768]=L"dat-analysis.txt";OPENFILENAMEW dialog={};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=h;
    dialog.lpstrFile=path;dialog.nMaxFile=32768;dialog.lpstrFilter=L"UTF-8 analysis report\0*.txt\0\0";dialog.lpstrDefExt=L"txt";
    dialog.lpstrTitle=L"Export analysis to a new text file";dialog.Flags=OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetSaveFileNameW(&dialog))return;
    if(!SaveUtf8Copy(path,report))Error(h,L"Could not create the report. Choose a new filename; existing files cannot be replaced.");
    else SetDlgItemTextW(h,Status,(L"Created analysis report: "+std::wstring(path)).c_str());
}
struct DistributionView {
    DatHex::StatisticsResult stats;std::wstring source;uint64_t start=0;
    int hovered=-1;bool owned=false;
};
RECT ChartArea(int width,int height){return RECT{56,76,(std::max)(57,width-24),(std::max)(77,height-80)};}
int ChartByte(RECT area,int x,int y) {
    if(x<area.left||x>=area.right||y<area.top||y>=area.bottom)return -1;
    return (x-area.left)*256/(area.right-area.left);
}
int ChartHeight(uint64_t count,uint64_t maximum,int height){return maximum?static_cast<int>(double(count)/double(maximum)*height):0;}
LRESULT CALLBACK DistributionProcedure(HWND h,UINT message,WPARAM w,LPARAM l) {
    auto view=reinterpret_cast<DistributionView*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(message==WM_CREATE){view=reinterpret_cast<DistributionView*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(view));return 0;}
    if(message==WM_GETMINMAXINFO){reinterpret_cast<MINMAXINFO*>(l)->ptMinTrackSize={600,360};return 0;}
    if(message==WM_SIZE){InvalidateRect(h,nullptr,TRUE);return 0;}
    if(message==WM_MOUSEMOVE&&view){RECT bounds;GetClientRect(h,&bounds);int hovered=ChartByte(ChartArea(bounds.right,bounds.bottom),static_cast<short>(LOWORD(l)),static_cast<short>(HIWORD(l)));
        if(hovered!=view->hovered){view->hovered=hovered;InvalidateRect(h,nullptr,FALSE);}TRACKMOUSEEVENT tracking={sizeof(tracking),TME_LEAVE,h,0};TrackMouseEvent(&tracking);return 0;}
    if(message==WM_MOUSELEAVE&&view){view->hovered=-1;InvalidateRect(h,nullptr,FALSE);return 0;}
    if(message==WM_PAINT&&view) {
        PAINTSTRUCT paint;HDC dc=BeginPaint(h,&paint);RECT bounds;GetClientRect(h,&bounds);FillRect(dc,&bounds,GetSysColorBrush(COLOR_WINDOW));
        auto oldFont=SelectObject(dc,GetStockObject(DEFAULT_GUI_FONT));SetBkMode(dc,TRANSPARENT);SetTextColor(dc,GetSysColor(COLOR_WINDOWTEXT));
        RECT title={16,10,bounds.right-16,36};DrawTextW(dc,view->source.c_str(),-1,&title,DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS);
        std::wostringstream summary;summary<<L"Buffer snapshot | Offset 0x"<<Number(view->start)<<L" | "<<view->stats.total<<L" bytes | Entropy "<<std::fixed<<std::setprecision(4)<<view->stats.entropy<<L" bits/byte";
        RECT caption={16,38,bounds.right-16,64};DrawTextW(dc,summary.str().c_str(),-1,&caption,DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS);
        RECT area=ChartArea(bounds.right,bounds.bottom);FrameRect(dc,&area,GetSysColorBrush(COLOR_GRAYTEXT));
        uint64_t maximum=*std::max_element(view->stats.counts.begin(),view->stats.counts.end());
        HBRUSH bars=CreateSolidBrush(RGB(45,115,190)),highlight=CreateSolidBrush(RGB(210,105,30));
        for(int i=0;i<256;++i){RECT bar={area.left+i*(area.right-area.left)/256,area.bottom-ChartHeight(view->stats.counts[i],maximum,area.bottom-area.top-1),area.left+(i+1)*(area.right-area.left)/256,area.bottom-1};
            if(bar.top<bar.bottom)FillRect(dc,&bar,i==view->hovered?highlight:bars);}
        DeleteObject(bars);DeleteObject(highlight);
        for(int i:{0,64,128,192,255}){RECT label={area.left+i*(area.right-area.left)/256-10,area.bottom+6,area.left+i*(area.right-area.left)/256+24,area.bottom+26};auto text=Number(i);DrawTextW(dc,text.c_str(),-1,&label,DT_LEFT|DT_SINGLELINE);}
        RECT peak={2,area.top,52,area.top+40};auto peakText=std::to_wstring(maximum);DrawTextW(dc,peakText.c_str(),-1,&peak,DT_RIGHT|DT_WORDBREAK);
        std::wstring detail=L"Hover over a bar to inspect its byte value, count, and percentage.";
        if(view->hovered>=0){auto count=view->stats.counts[view->hovered];std::wostringstream text;text<<L"Byte 0x"<<Number(view->hovered)<<L" | Count "<<count<<L" | "<<std::fixed<<std::setprecision(4)<<(view->stats.total?100.0*count/view->stats.total:0)<<L"%";detail=text.str();}
        if(!view->stats.total)detail=L"Empty range: all byte counts and entropy are zero.";
        RECT footer={16,bounds.bottom-38,bounds.right-16,bounds.bottom-10};DrawTextW(dc,detail.c_str(),-1,&footer,DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS);
        SelectObject(dc,oldFont);EndPaint(h,&paint);return 0;
    }
    if(message==WM_CLOSE){DestroyWindow(h);return 0;}
    if(message==WM_NCDESTROY){SetWindowLongPtrW(h,GWLP_USERDATA,0);if(view&&view->owned)delete view;}
    return DefWindowProcW(h,message,w,l);
}
void ShowDistribution(HWND owner,State& state) {
    if(!state.chartStatistics)return;
    auto view=std::make_unique<DistributionView>();view->stats=*state.chartStatistics;view->source=state.document->path;view->start=state.chartStart;
    Win32ToolWindow::WideSpec spec{DistributionProcedure,L"DATuraByteDistribution",L"Byte frequency distribution",820,500};spec.style=WS_OVERLAPPEDWINDOW;spec.createParameter=view.get();
    HWND chart=Win32ToolWindow::Create(owner,spec);
    if(chart){view->owned=true;view.release();Win32ToolWindow::Show(chart,true);}else Error(owner,L"Could not open the byte distribution chart.");
}
void Analyze(HWND h,State& s,SearchJob::Kind kind,bool verify=false) {
    auto job=std::make_shared<SearchJob>();job->kind=kind;job->document=s.document;
    job->comparison=s.comparison;job->start=s.compareStart;
    job->hashAlgorithm=static_cast<DatHex::HashAlgorithm>(SendDlgItemMessageW(h,ChecksumType,CB_GETCURSEL,0,0));
    if(verify&&!DatHex::NormalizeChecksum(Text(h,ExpectedChecksum),job->hashAlgorithm,job->expectedChecksum)) {
        Error(h,L"Enter the complete expected hex checksum for the selected algorithm. For CRC-32 + SHA-256, enter the SHA-256 value. Spaces and a 0x prefix are accepted.");return;
    }
    bool selection=SendDlgItemMessageW(h,AnalysisScope,CB_GETCURSEL,0,0)==1;
    job->rangeStart=selection?s.selection:0;job->rangeLength=selection?s.length:s.document->size;
    try {
        std::thread([job] {
            auto cancel=[&]{return job->cancel.load();};auto progress=[&](uint64_t n){job->processed=n;};
            try {
                if(job->kind==SearchJob::Kind::Hashes)job->hashes=DatHex::HashDocument(*job->document,cancel,progress,job->failed,job->hashAlgorithm,job->rangeStart,job->rangeLength);
                else if(job->kind==SearchJob::Kind::Statistics)job->statistics=DatHex::Statistics(*job->document,cancel,progress,job->failed,job->rangeStart,job->rangeLength);
                else if(job->comparison)job->difference=DatHex::CompareDocuments(*job->document,*job->comparison,job->start,cancel,progress,job->failed);
                else job->failed=true;
            }catch(...){job->failed=true;}
            job->done=true;
        }).detach();
        s.highNibble=-1;s.job=job;s.lastChecksum.clear();s.reportReady=false;s.chartStatistics.reset();Buttons(h,s);
        SetDlgItemTextW(h,AnalysisOutput,kind==SearchJob::Kind::Statistics?L"Calculating byte frequencies and entropy...":kind==SearchJob::Kind::Hashes?L"Calculating the selected checksum of the current buffer...":L"Comparing the current buffer to the selected file...");
    }catch(...){Error(h,L"Could not start this analysis.");}
}
void Compare(HWND h,State& s) {
    std::wstring path;
    s.picking=true;bool picked=s.pickDatFile(h,s.root,L"Compare with a DAT or binary file",path);s.picking=false;
    if(!picked)return;
    auto document=std::make_shared<DatHex::Document>();if(!document->Open(path)){Error(h,L"Cannot open the comparison file.");return;}
    s.comparison=std::move(document);s.compareStart=0;Analyze(h,s,SearchJob::Kind::Compare);
}
void Patch(HWND h,State& s,bool fill) {
    DatHex::Bytes bytes;
    if(!DatHex::ParseHex(Text(h,PatchText),bytes)){Error(h,L"Enter complete hexadecimal byte pairs, for example: 01 FF 82 A0.");return;}
    if(!s.length||s.length>PageSize) {Error(h,L"Select between 1 byte and 64 KB to modify in one operation.");return;}
    if(fill) {
        if(bytes.size()>PageSize){Error(h,L"Fill patterns can contain up to 64 KB of hex bytes.");return;}
        bytes=RepeatedPattern(bytes,static_cast<size_t>(s.length));
    } else if(bytes.size()!=s.length) {Error(h,L"The replacement must contain exactly the selected number of bytes. File size is preserved.");return;}
    if(!s.document->Overwrite(s.selection,bytes)){Error(h,L"Could not apply these bytes.");return;}Changed(h,s);Refresh(h,s);
}
enum class Transform { Invert,And,Or,Xor,Reverse,Swap16,Swap32,Swap64 };
bool TransformBytes(DatHex::Bytes& bytes,Transform operation,const DatHex::Bytes& mask={}) {
    if(bytes.empty())return false;
    if(operation==Transform::And||operation==Transform::Or||operation==Transform::Xor) {
        if(mask.empty())return false;
        for(size_t i=0;i<bytes.size();++i){auto operand=mask[i%mask.size()];
            if(operation==Transform::And)bytes[i]&=operand;else if(operation==Transform::Or)bytes[i]|=operand;else bytes[i]^=operand;}
        return true;
    }
    if(operation==Transform::Invert){for(auto& byte:bytes)byte=static_cast<unsigned char>(~byte);return true;}
    size_t width=operation==Transform::Reverse?bytes.size():operation==Transform::Swap16?2:operation==Transform::Swap32?4:operation==Transform::Swap64?8:0;
    if(!width||bytes.size()%width)return false;
    for(size_t i=0;i<bytes.size();i+=width)std::reverse(bytes.begin()+i,bytes.begin()+i+width);
    return true;
}
void ApplyByteOperation(HWND h,State& s) {
    if(!s.edit||s.job)return;
    if(!s.length||s.length>PageSize){Error(h,L"Select between 1 byte and 64 KB for a byte operation.");return;}
    int index=static_cast<int>(SendDlgItemMessageW(h,ByteOperation,CB_GETCURSEL,0,0));if(index<0||index>7)return;
    auto operation=static_cast<Transform>(index);DatHex::Bytes mask,bytes;
    if(index>=1&&index<=3&&(!DatHex::ParseHex(Text(h,PatchText),mask)||mask.size()>PageSize)) {
        Error(h,L"Enter a hex mask in the overwrite/fill box, such as FF or 0F F0. The mask repeats across the selection.");return;
    }
    if(!s.document->Read(s.selection,static_cast<size_t>(s.length),bytes)){Error(h,L"Cannot read this selection.");return;}
    if(!TransformBytes(bytes,operation,mask)){Error(h,L"Byte swapping requires a selection length divisible by 2, 4, or 8 bytes for the chosen word size.");return;}
    if(!s.document->Overwrite(s.selection,bytes)){Error(h,L"Could not apply this byte operation.");return;}
    Changed(h,s);Refresh(h,s);SetDlgItemTextW(h,Status,L"Byte operation applied to the buffer. Undo restores it; Save Copy creates a separate file.");
}
// Numeric inputs are decimal unless explicitly prefixed with 0x. Endian is shared with the inspector.
bool SearchPattern(HWND h,const std::wstring& input,DatHex::Bytes& pattern) {
    int mode=static_cast<int>(SendDlgItemMessageW(h,FindMode,CB_GETCURSEL,0,0));
    if(mode==0)return DatHex::ParseHex(input,pattern);
    if(mode==17)return ParseBinary(input,pattern);
    if(mode==1) {
        UINT cp=EncodingPage(static_cast<int>(SendDlgItemMessageW(h,Encoding,CB_GETCURSEL,0,0)));if(!cp||!IsValidCodePage(cp))return false;
        BOOL substituted=FALSE;int n=WideCharToMultiByte(cp,WC_NO_BEST_FIT_CHARS,input.data(),static_cast<int>(input.size()),nullptr,0,nullptr,&substituted);
        if(!n||substituted)return false;
        pattern.resize(n);WideCharToMultiByte(cp,WC_NO_BEST_FIT_CHARS,input.data(),static_cast<int>(input.size()),reinterpret_cast<char*>(pattern.data()),n,nullptr,&substituted);return !substituted;
    }
    if(mode>=14&&mode<=16) {
        // Reject unpaired UTF-16 surrogates, rather than silently changing a pattern.
        for(size_t i=0;i<input.size();++i) {
            unsigned unit=input[i];
            if(unit>=0xd800&&unit<=0xdbff){if(++i>=input.size()||input[i]<0xdc00||input[i]>0xdfff)return false;}
            else if(unit>=0xdc00&&unit<=0xdfff)return false;
        }
        if(input.empty())return false;
        if(mode==14) {
            int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,input.data(),static_cast<int>(input.size()),nullptr,0,nullptr,nullptr);
            if(!n)return false;pattern.resize(n);
            return WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,input.data(),static_cast<int>(input.size()),reinterpret_cast<char*>(pattern.data()),n,nullptr,nullptr)==n;
        }
        pattern.resize(input.size()*2);
        for(size_t i=0;i<input.size();++i){pattern[i*2+(mode==15?0:1)]=static_cast<unsigned char>(input[i]);pattern[i*2+(mode==15?1:0)]=static_cast<unsigned char>(input[i]>>8);}
        return true;
    }
    auto first=input.find_first_not_of(L" \t\r\n"),last=input.find_last_not_of(L" \t\r\n");
    if(first==std::wstring::npos)return false;
    auto text=input.substr(first,last-first+1);
    if(mode==18||mode==19){uint64_t bits=0;
        if(mode==18){if(!DatHex::ParseNumber(text,bits,10))return false;}
        else {try{size_t consumed=0,prefix=text[0]==L'-'||text[0]==L'+'?1:0;int base=text.size()>prefix+2&&text[prefix]==L'0'&&(text[prefix+1]==L'x'||text[prefix+1]==L'X')?16:10;
            auto value=std::stoll(text,&consumed,base);if(consumed!=text.size())return false;bits=static_cast<uint64_t>(value);
        }catch(...){return false;}}
        pattern=EncodeLeb128(bits,mode==19);return true;
    }
    const size_t widths[]={1,1,2,2,3,3,4,4,8,8,4,8};
    if(mode<2||mode>13)return false;
    size_t width=widths[mode-2];uint64_t bits=0;
    try {
        if(mode>=12) {
            wchar_t* end=nullptr;errno=0;double value=wcstod(text.c_str(),&end);
            if(end!=text.c_str()+text.size()||errno==ERANGE||!std::isfinite(value))return false;
            if(mode==12){float f=static_cast<float>(value);if(!std::isfinite(f))return false;uint32_t u;std::memcpy(&u,&f,4);bits=u;}
            else std::memcpy(&bits,&value,8);
        } else if(mode%2==0) {
            if(!DatHex::ParseNumber(text,bits,10)||(width<8&&bits>=(uint64_t(1)<<(width*8))))return false;
        } else {
            size_t consumed=0;int base=10;size_t prefix=text[0]==L'-'||text[0]==L'+'?1:0;
            if(text.size()>prefix+2&&text[prefix]==L'0'&&(text[prefix+1]==L'x'||text[prefix+1]==L'X'))base=16;
            int64_t value=std::stoll(text,&consumed,base);
            if(consumed!=text.size()||(width<8&&(value<-(int64_t(1)<<(width*8-1))||value>=(int64_t(1)<<(width*8-1)))))return false;
            bits=static_cast<uint64_t>(value);
        }
    }catch(...){return false;}
    bool big=SendDlgItemMessageW(h,Endian,CB_GETCURSEL,0,0)==1;
    pattern.resize(width);for(size_t i=0;i<width;++i)pattern[big?width-1-i:i]=static_cast<unsigned char>(bits>>(8*i));return true;
}
void Replace(HWND h,State& s) {
    if(!s.edit)return;
    DatHex::Bytes pattern,replacement,current;
    if(!SearchPattern(h,Text(h,SearchText),pattern)||!SearchPattern(h,Text(h,ReplacementText),replacement)||pattern.empty()||pattern.size()>PageSize||replacement.size()!=pattern.size()) {
        Error(h,L"Enter valid search and replacement values of equal byte length. Numeric values use decimal or a 0x prefix, with the selected endian.");return;
    }
    if(s.length!=pattern.size()||!s.document->Read(s.selection,pattern.size(),current)||current!=pattern) {
        Error(h,L"Use Find to select a matching value before replacing it.");return;
    }
    if(!s.document->Overwrite(s.selection,replacement)){Error(h,L"Could not replace this match.");return;}
    Changed(h,s);Refresh(h,s);SetDlgItemTextW(h,Status,L"Match replaced in the buffer. Undo restores it; Save Copy creates a separate file.");
}
void Search(HWND h,State& s,bool again,SearchJob::Kind kind=SearchJob::Kind::Search) {
    DatHex::Bytes pattern;
    if(again)pattern=s.lastPattern;
    else if(!SearchPattern(h,Text(h,SearchText),pattern)) {
        Error(h,L"Enter a value valid for the search type. Numeric values use decimal or 0x; binary patterns need complete groups of 8 bits (optional 0b prefix).");return;
    }
    if(pattern.empty()||pattern.size()>PageSize){Error(h,L"Search patterns must contain 1 to 65536 bytes.");return;}
    auto job=std::make_shared<SearchJob>();job->kind=kind;job->document=s.document;job->pattern=pattern;
    if(kind==SearchJob::Kind::ReplaceAll) {
        if(!s.edit)return;
        if(!SearchPattern(h,Text(h,ReplacementText),job->replacement)||job->replacement.size()!=pattern.size()) {
            Error(h,L"Replace all requires a valid replacement with the same byte length as the search pattern.");return;
        }
    }
    bool selectionScope=SendDlgItemMessageW(h,SearchScope,CB_GETCURSEL,0,0)==1;
    job->rangeStart=again?s.lastSearchStart:selectionScope?s.selection:0;
    job->rangeLength=again?s.lastSearchLength:selectionScope?s.length:s.document->size;
    if(selectionScope&&!again&&!job->rangeLength){Error(h,L"Select a non-empty block before starting a selection search.");return;}
    uint64_t end=job->rangeStart+job->rangeLength;
    job->start=s.selection<job->rangeStart||s.selection>=end?job->rangeStart:again?(std::min)(end,s.selection+1):s.selection;
    if(kind==SearchJob::Kind::PreviousMatch)job->start=s.selection;
    s.lastPattern=pattern;s.lastSearchStart=job->rangeStart;s.lastSearchLength=job->rangeLength;
    try {
        std::thread([job]{
            try {
                if(job->kind==SearchJob::Kind::ReplaceAll) {
                    job->replacementPlan=job->document->PrepareReplaceAll(job->pattern,job->replacement,[&]{return job->cancel.load();},[&](uint64_t n){job->processed=n;},job->failed,job->replacementLimit,job->rangeStart,job->rangeLength);
                } else if(job->kind==SearchJob::Kind::AllMatches||job->kind==SearchJob::Kind::PreviousMatch) {
                    std::optional<uint64_t> wrapped;
                    job->document->ScanMatches(job->pattern,[&]{return job->cancel.load();},[&](uint64_t at) {
                        if(job->kind==SearchJob::Kind::AllMatches){++job->matchCount;if(job->matches.size()<10000)job->matches.push_back(at);}
                        else {wrapped=at;if(at<job->start)job->result=at;}
                        return true;
                    },[&](uint64_t n){job->processed=n;},job->failed,job->rangeStart,job->rangeLength);
                    if(job->kind==SearchJob::Kind::PreviousMatch&&!job->result)job->result=wrapped;
                } else {
                job->result=job->document->Find(job->pattern,job->start,[&]{return job->cancel.load();},job->failed,job->rangeStart+job->rangeLength);
                if(!job->result&&!job->failed&&!job->cancel&&job->start>job->rangeStart) {
                    job->result=job->document->Find(job->pattern,job->rangeStart,[&]{return job->cancel.load();},job->failed,job->rangeStart+job->rangeLength);
                }
                }
            }catch(...){job->failed=true;}
            job->done.store(true);
        }).detach();
        s.job=job;s.lastChecksum.clear();s.reportReady=false;s.chartStatistics.reset();Buttons(h,s);SetDlgItemTextW(h,Status,L"Searching the chosen range... Cancel stops the search.");
    }catch(...){Error(h,L"Could not start the search.");}
}
template<class Cancelled,class Progress>
bool SaveJoinedCopy(const DatHex::Document& first,const DatHex::Document& second,const std::wstring& path,Cancelled cancelled,Progress progress) {
    if(!first.Loaded()||!second.Loaded()||second.size>UINT64_MAX-first.size)return false;
    if(cancelled())return false;
    DatHex::NewCopy output(path);if(!output.Valid())return false;
    uint64_t processed=0;DatHex::Bytes bytes;
    for(const auto* document:{&first,&second})for(uint64_t offset=0;offset<document->size;){
        if(cancelled())return false;
        size_t count=static_cast<size_t>((std::min)(uint64_t(65536),document->size-offset));
        if(!document->Read(offset,count,bytes)||!output.Write(bytes.data(),bytes.size()))return false;
        offset+=count;processed+=count;progress(processed);
    }
    if(cancelled())return false;return output.Commit();
}
std::wstring SplitPath(const std::wstring& base,uint64_t index) {
    std::wostringstream suffix;suffix<<L".part"<<std::setw(4)<<std::setfill(L'0')<<index+1;return base+suffix.str();
}
template<class Cancelled,class Progress>
bool SaveSplitCopies(const DatHex::Document& document,const std::wstring& base,uint64_t partSize,Cancelled cancelled,Progress progress) {
    if(!document.Loaded()||!document.size||!partSize)return false;
    uint64_t count=1+(document.size-1)/partSize;if(count>1024||cancelled())return false;
    std::vector<std::unique_ptr<DatHex::NewCopy>> outputs;
    struct Rollback {decltype(outputs)& files;bool complete=false;~Rollback(){if(!complete)for(auto& file:files)file->Discard();}} rollback{outputs};
    // Keep owned handles until the whole set succeeds: collisions, failures and
    // cancellation roll back our files without deleting anyone else's output.
    for(uint64_t i=0;i<count;++i){if(cancelled())return false;
        auto output=std::make_unique<DatHex::NewCopy>(SplitPath(base,i));if(!output->Valid())return false;outputs.push_back(std::move(output));
    }
    DatHex::Bytes bytes;uint64_t offset=0;
    while(offset<document.size){if(cancelled())return false;
        size_t length=static_cast<size_t>((std::min)({uint64_t(65536),document.size-offset,partSize-offset%partSize}));
        if(!document.Read(offset,length,bytes)||!outputs[static_cast<size_t>(offset/partSize)]->Write(bytes.data(),bytes.size()))return false;
        offset+=length;progress(offset);
    }
    for(auto& output:outputs){if(cancelled()||!output->Commit())return false;}
    rollback.complete=true;return true;
}
void SplitFile(HWND h,State& s,uint64_t partSize) {
    if(!s.document->size){Error(h,L"This file is empty.");return;}
    uint64_t count=1+(s.document->size-1)/partSize;if(count>1024){Error(h,L"Choose a larger part size. A split can create up to 1,024 parts.");return;}
    struct Picking {State& state;Picking(State& value):state(value){state.picking=true;}~Picking(){state.picking=false;}} picking(s);
    wchar_t path[32768]=L"dat-split.bin";OPENFILENAMEW dialog={};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=h;
    dialog.lpstrFile=path;dialog.nMaxFile=32768;dialog.lpstrFilter=L"Binary part name\0*.bin\0All files\0*.*\0\0";dialog.lpstrDefExt=L"bin";
    dialog.lpstrTitle=L"Choose base name for numbered copies (.part0001, .part0002...)";dialog.Flags=OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetSaveFileNameW(&dialog))return;
    auto job=std::make_shared<SearchJob>();job->kind=SearchJob::Kind::Split;job->document=s.document;job->outputPath=path;job->rangeLength=s.document->size;job->partCount=count;
    try{std::thread([job,partSize]{try{job->failed=!SaveSplitCopies(*job->document,job->outputPath,partSize,[&]{return job->cancel.load();},[&](uint64_t n){job->processed=n;});}catch(...){job->failed=true;}job->done=true;}).detach();
        s.job=job;Buttons(h,s);SetDlgItemTextW(h,Status,L"Creating numbered copies. Cancel removes the unfinished set.");
    }catch(...){Error(h,L"Could not start splitting the buffer.");}
}
void JoinFile(HWND h,State& s,bool before) {
    struct Picking {State& state;Picking(State& value):state(value){state.picking=true;}~Picking(){state.picking=false;}} picking(s);
    std::wstring input;if(!s.pickDatFile(h,s.root,before?L"Choose file to join before this buffer":L"Choose file to join after this buffer",input))return;
    auto other=std::make_shared<DatHex::Document>();if(!other->Open(input)){Error(h,L"Could not read the chosen file.");return;}
    if(other->size>UINT64_MAX-s.document->size){Error(h,L"The combined size is too large.");return;}
    wchar_t path[32768]=L"joined-copy.bin";OPENFILENAMEW dialog={};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=h;
    dialog.lpstrFile=path;dialog.nMaxFile=32768;dialog.lpstrFilter=L"Binary copy\0*.bin\0All files\0*.*\0\0";dialog.lpstrDefExt=L"bin";
    dialog.lpstrTitle=L"Save joined files as a new copy";dialog.Flags=OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetSaveFileNameW(&dialog))return;
    auto job=std::make_shared<SearchJob>();job->kind=SearchJob::Kind::Join;job->document=s.document;job->comparison=other;job->outputPath=path;job->rangeLength=s.document->size+other->size;
    try{
        std::thread([job,before]{try{
            auto& first=before?*job->comparison:*job->document;auto& second=before?*job->document:*job->comparison;
            job->failed=!SaveJoinedCopy(first,second,job->outputPath,[&]{return job->cancel.load();},[&](uint64_t n){job->processed=n;});
        }catch(...){job->failed=true;}job->done=true;}).detach();
        s.job=job;Buttons(h,s);SetDlgItemTextW(h,Status,L"Joining files into a new copy. Cancel stops the operation.");
    }catch(...){Error(h,L"Could not start joining the files.");}
}
void Command(HWND h,State& s,int id) {
    if(s.picking)return;
    if(s.job) {if(id==Cancel)s.job->cancel=true;return;}
    s.highNibble=-1;s.activeNibble=-1;
    if(id==Open){Browse(h,s);return;}
    if(id==EditMode){s.edit=SendDlgItemMessageW(h,EditMode,BM_GETCHECK,0,0)==BST_CHECKED;Buttons(h,s);Inspect(h,s);return;}
    if(id==ViewMenu) {
        HMENU menu=CreatePopupMenu(),groups=CreatePopupMenu();
        const wchar_t* labels[]={L"1 byte",L"2 bytes",L"4 bytes",L"8 bytes",L"16 bytes"};
        for(int i=0;i<5;++i)AppendMenuW(groups,MF_STRING|(s.groupBytes==(1u<<i)?MF_CHECKED:0),Group1+i,labels[i]);
        AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(groups),L"Byte grouping");
        AppendMenuW(menu,MF_STRING|(s.displayMode==0?MF_CHECKED:0),ViewBoth,L"Hex and decoded text");
        AppendMenuW(menu,MF_STRING|(s.displayMode==1?MF_CHECKED:0),ViewHex,L"Hex only");AppendMenuW(menu,MF_STRING|(s.displayMode==2?MF_CHECKED:0),ViewText,L"Decoded text only");
        AppendMenuW(menu,MF_STRING|(s.inspectorHex?MF_CHECKED:0),InspectorHex,L"Hexadecimal integers in data inspector");
        RECT bounds={};GetWindowRect(GetDlgItem(h,ViewMenu),&bounds);
        int chosen=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,bounds.left,bounds.bottom,0,h,nullptr);DestroyMenu(menu);if(chosen)Command(h,s,chosen);return;
    }
    if(id==InspectorHex){s.inspectorHex=!s.inspectorHex;Inspect(h,s);return;}
    if(id>=Group1&&id<=Group16){s.groupBytes=1u<<(id-Group1);Refresh(h,s);return;}
    if(id>=ViewBoth&&id<=ViewText){s.displayMode=id-ViewBoth;Refresh(h,s);return;}
    if(id==Rows){s.rowBytes=8u<<SendDlgItemMessageW(h,Rows,CB_GETCURSEL,0,0);Refresh(h,s);return;}
    if(id==OffsetBase){s.decimal=SendDlgItemMessageW(h,OffsetBase,CB_GETCURSEL,0,0)==1;Bookmarks(h,s);Refresh(h,s);return;}
    if(id==Encoding) {
        int index=static_cast<int>(SendDlgItemMessageW(h,Encoding,CB_GETCURSEL,0,0));UINT page=EncodingPage(index);
        if(!page||!IsValidCodePage(page)){SendDlgItemMessageW(h,Encoding,CB_SETCURSEL,s.encodingIndex,0);Error(h,L"This character encoding is not installed on this Windows system.");return;}
        s.encodingIndex=index;Refresh(h,s);return;
    }

    if(!s.document->Loaded())return;
    if(id>=SetNumberedBookmark&&id<SetNumberedBookmark+10) {
        int slot=id-SetNumberedBookmark;
        auto found=std::find_if(s.bookmarks.begin(),s.bookmarks.end(),[&](const State::Bookmark& bookmark){return bookmark.slot==slot;});
        if(found==s.bookmarks.end()){if(s.bookmarks.size()>=256){Error(h,L"Remove a bookmark before assigning another numbered slot.");return;}s.bookmarks.push_back({s.selection,s.length,slot});}
        else {found->offset=s.selection;found->length=s.length;}
        Bookmarks(h,s);SetDlgItemTextW(h,Status,(L"Bookmark "+std::to_wstring(slot)+L" set. Ctrl+"+std::to_wstring(slot)+L" returns to it.").c_str());return;
    }
    if(id>=GoNumberedBookmark&&id<GoNumberedBookmark+10) {
        int slot=id-GoNumberedBookmark;
        auto found=std::find_if(s.bookmarks.begin(),s.bookmarks.end(),[&](const State::Bookmark& bookmark){return bookmark.slot==slot;});
        if(found!=s.bookmarks.end())Select(h,s,found->offset,found->length);
        else SetDlgItemTextW(h,Status,L"This numbered bookmark has not been set.");return;
    }
    switch(id) {
    case Previous: if(s.offset){s.offset-=PageSize;s.selection=s.offset;s.length=s.document->size?1:0;Refresh(h,s);}break;
    case Next: if(s.document->size-s.offset>PageSize){s.offset+=PageSize;s.selection=s.offset;s.length=1;Refresh(h,s);}break;
    case Endian: Inspect(h,s);break;
    case Export: Save(h,s,false);break;
    case Split64K: SplitFile(h,s,65536);break;
    case Split1M: SplitFile(h,s,1048576);break;
    case Split16M: SplitFile(h,s,16777216);break;
    case JoinBefore: JoinFile(h,s,true);break;
    case JoinAfter: JoinFile(h,s,false);break;
    case SaveCopy: Save(h,s,true);break;
    case Undo: if(s.edit&&s.document->Undo()){Changed(h,s);Refresh(h,s);}break;
    case Redo: if(s.edit&&s.document->Redo()){Changed(h,s);Refresh(h,s);}break;
    case PasteDecoded: if(s.edit)Paste(h,s,PasteFormat::Decoded);break;
    case PasteUtf8: if(s.edit)Paste(h,s,PasteFormat::Utf8);break;
    case NextValue1: case NextValue2: case NextValue3: case NextValue4: case NextValue8: case NextValue16:
    case PreviousValue1: case PreviousValue2: case PreviousValue3: case PreviousValue4: case PreviousValue8: case PreviousValue16: StepValue(h,s,id);break;
    case Pointer16: case Pointer24: case Pointer32: case Pointer64:
    case Relative16: case Relative24: case Relative32: case Relative64: FollowPointer(h,s,id);break;
    case CopyInspector: InspectorReport(h,s,true);break;
    case ExportInspector: InspectorReport(h,s,false);break;
    case PasteEscaped: if(s.edit)Paste(h,s,PasteFormat::Escaped);break;
    case PasteBinary: if(s.edit)Paste(h,s,PasteFormat::Binary);break;
    case CopyBinary: MouseSelection(h,s);Copy(h,s,true,ByteFormat::Binary);break;
    case PasteBase64: if(s.edit)Paste(h,s,PasteFormat::Base64);break;
    case PasteOverwrite: if(s.edit)Paste(h,s);break;
    case ExportBlock: SaveBlock(h,s);break;
    case ExportIntelHex: ExportIntelSelection(h,s);break;
    case ExportCArray: ExportFormattedSelection(h,s,ByteFormat::CArray);break;
    case ExportEscaped: ExportFormattedSelection(h,s,ByteFormat::Escaped);break;
    case ExportBase64: ExportFormattedSelection(h,s,ByteFormat::Base64);break;
    case ExportPascal: ExportFormattedSelection(h,s,ByteFormat::Pascal);break;
    case ExportJava: ExportFormattedSelection(h,s,ByteFormat::Java);break;
    case ExportCSharp: ExportFormattedSelection(h,s,ByteFormat::CSharp);break;
    case ExportVB: ExportFormattedSelection(h,s,ByteFormat::VB);break;
    case ExportSRecord: ExportDocumentSelection(h,s,false);break;
    case ExportHtml: ExportDocumentSelection(h,s,true);break;
    case ExportPlainTable: PlainTable(h,s,false);break;
    case CopyPlainTable: MouseSelection(h,s);PlainTable(h,s,true);break;
    case ExportRtf: ExportRichSelection(h,s,true);break;
    case ExportTex: ExportRichSelection(h,s,false);break;
    case ExportSelectionMenu: {
        HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,ExportBlock,L"Binary selection...");AppendMenuW(menu,MF_STRING,ExportIntelHex,L"Intel HEX selection...");AddFormattedExports(menu);
        RECT bounds={};GetWindowRect(GetDlgItem(h,ExportSelectionMenu),&bounds);
        int chosen=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,bounds.left,bounds.bottom,0,h,nullptr);DestroyMenu(menu);if(chosen)Command(h,s,chosen);break;
    }
    case Checksums: Analyze(h,s,SearchJob::Kind::Hashes);break;
    case VerifyChecksum: Analyze(h,s,SearchJob::Kind::Hashes,true);break;
    case CopyChecksum: if(!s.lastChecksum.empty()&&!Clipboard(h,s.lastChecksum))Error(h,L"The checksum could not be copied. Try again when the clipboard is available.");break;
    case ExportReport: SaveReport(h,s);break;
    case CompareFile: Compare(h,s);break;
    case NextDifference: if(s.comparison)Analyze(h,s,SearchJob::Kind::Compare);break;
    case AddBookmark:
        if(s.bookmarks.size()>=256){Error(h,L"This file already has 256 bookmarks.");break;}
        s.bookmarks.push_back({s.selection,s.length});Bookmarks(h,s);break;
    case GoBookmark: {
        auto index=SendDlgItemMessageW(h,BookmarkList,CB_GETCURSEL,0,0);
        if(index>=0 && static_cast<size_t>(index)<s.bookmarks.size())Select(h,s,s.bookmarks[index].offset,s.bookmarks[index].length);break;
    }
    case RemoveBookmark: {
        auto index=SendDlgItemMessageW(h,BookmarkList,CB_GETCURSEL,0,0);
        if(index>=0 && static_cast<size_t>(index)<s.bookmarks.size())s.bookmarks.erase(s.bookmarks.begin()+index);
        Bookmarks(h,s);break;
    }
    case Go: {
        uint64_t offset;if(!DatHex::ResolveOffset(Text(h,GoText),s.selection,s.document->size,s.decimal?10:16,offset)){Error(h,L"Enter an absolute offset, or + / - followed by a distance from the current byte. Use the selected offset base or a 0x prefix. The destination must be inside the file or at its end.");break;}
        Select(h,s,offset,offset<s.document->size?1:0);SetFocus(GetDlgItem(h,Dump));break;
    }
    case Navigation: {
        HMENU menu=CreatePopupMenu();
        AppendMenuW(menu,MF_STRING,FileStart,L"Go to file start\tCtrl+Home");AppendMenuW(menu,MF_STRING,FileEnd,L"Go to file end\tCtrl+End");
        AppendMenuW(menu,MF_STRING,SelectToStart,L"Select to file start\tCtrl+Shift+Home");AppendMenuW(menu,MF_STRING,SelectToEnd,L"Select to file end\tCtrl+Shift+End");
        UINT flags=MF_STRING|(s.document->Dirty()?0:MF_GRAYED);
        AppendMenuW(menu,flags,NextModified,L"Next modified byte (wraps)");AppendMenuW(menu,flags,PreviousModified,L"Previous modified byte (wraps)");
        NumberedBookmarkMenus(menu,s);PointerMenus(menu);ValueMenus(menu);
        AppendMenuW(menu,MF_STRING,CopyOffset,L"Copy offset\tAlt+Insert");
        RECT bounds={};GetWindowRect(GetDlgItem(h,Navigation),&bounds);
        int chosen=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,bounds.left,bounds.bottom,0,h,nullptr);DestroyMenu(menu);
        if(chosen)Command(h,s,chosen);break;
    }
    case FileStart: Select(h,s,0,s.document->size?1:0);break;
    case FileEnd: Select(h,s,s.document->size,0);break;
    case SelectToStart: Select(h,s,0,s.selection);break;
    case SelectToEnd: Select(h,s,s.selection,s.document->size-s.selection);break;
    case NextModified: case PreviousModified: {
        auto at=s.document->ModifiedOffset(s.selection,id==PreviousModified);
        if(at)Select(h,s,*at,1);else SetDlgItemTextW(h,Status,L"The buffer contains no modified bytes.");break;
    }
    case CopyOffset:
        if(!Clipboard(h,(s.decimal?L"":L"0x")+Number(s.selection,s.decimal)))Error(h,L"The offset could not be copied. Try again when the clipboard is available.");
        break;
    case SelectBlock: {
        uint64_t start,length;
        if(!DatHex::ParseNumber(Text(h,StartText),start,s.decimal?10:16)||!DatHex::ParseNumber(Text(h,LengthText),length,s.decimal?10:16)){Error(h,L"Enter a valid start and length using the selected offset base.");break;}
        Select(h,s,start,length);break;
    }
    case CopyHex: MouseSelection(h,s);Copy(h,s,true);break;
    case CopyText: MouseSelection(h,s);Copy(h,s,false);break;
    case CopyFormatted: {
        int index=static_cast<int>(SendDlgItemMessageW(h,CopyFormat,CB_GETCURSEL,0,0));
        if(index>=0&&index<=6){MouseSelection(h,s);Copy(h,s,true,static_cast<ByteFormat>(index+1));}break;
    }
    case CopyCArray: MouseSelection(h,s);Copy(h,s,true,ByteFormat::CArray);break;
    case CopyEscaped: MouseSelection(h,s);Copy(h,s,true,ByteFormat::Escaped);break;
    case CopyBase64: MouseSelection(h,s);Copy(h,s,true,ByteFormat::Base64);break;
    case CopyPascal: MouseSelection(h,s);Copy(h,s,true,ByteFormat::Pascal);break;
    case CopyJava: MouseSelection(h,s);Copy(h,s,true,ByteFormat::Java);break;
    case CopyCSharp: MouseSelection(h,s);Copy(h,s,true,ByteFormat::CSharp);break;
    case CopyVB: MouseSelection(h,s);Copy(h,s,true,ByteFormat::VB);break;
    case Overwrite: if(s.edit)Patch(h,s,false);break;
    case Fill: if(s.edit)Patch(h,s,true);break;
    case ApplyOperation: ApplyByteOperation(h,s);break;
    case ReplaceMatch: Replace(h,s);break;
    case ReplaceAll: if(s.edit)Search(h,s,false,SearchJob::Kind::ReplaceAll);break;
    case ByteStatistics: Analyze(h,s,SearchJob::Kind::Statistics);break;
    case DistributionChart: ShowDistribution(h,s);break;
    case FindAll: Search(h,s,false,SearchJob::Kind::AllMatches);break;
    case FindPrevious: if(!s.lastPattern.empty())Search(h,s,true,SearchJob::Kind::PreviousMatch);break;
    case GoMatch: {
        auto index=SendDlgItemMessageW(h,MatchList,CB_GETCURSEL,0,0);
        if(index>=0&&static_cast<size_t>(index)<s.matches.size())Select(h,s,s.matches[index],s.matchLength);break;
    }
    case Find: Search(h,s,false);break;
    case FindNext: if(!s.lastPattern.empty())Search(h,s,true);break;
    }
}
LRESULT CALLBACK DumpProcedure(HWND edit,UINT m,WPARAM w,LPARAM l) {
    HWND h=GetParent(edit);auto s=reinterpret_cast<State*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(!s)return DefWindowProcW(edit,m,w,l);
    if(m==WM_KEYDOWN||m==WM_SYSKEYDOWN) {
        bool ctrl=(GetKeyState(VK_CONTROL)&0x8000)!=0;
        if(w==VK_INSERT&&(GetKeyState(VK_MENU)&0x8000)){Command(h,*s,CopyOffset);return 0;}
        if(ctrl&&(w==VK_LEFT||w==VK_RIGHT)){MoveNibble(h,*s,w==VK_RIGHT);return 0;}
        if(ctrl&&(w==VK_HOME||w==VK_END)) {
            bool extend=(GetKeyState(VK_SHIFT)&0x8000)!=0;
            Command(h,*s,w==VK_HOME?(extend?SelectToStart:FileStart):(extend?SelectToEnd:FileEnd));return 0;
        }
        int bookmark=BookmarkShortcut(w,ctrl,(GetKeyState(VK_SHIFT)&0x8000)!=0);if(bookmark){Command(h,*s,bookmark);return 0;}
        if(ctrl) {
            int id=w=='C'?CopyHex:w=='V'?PasteOverwrite:w=='B'?AddBookmark:w=='F'?Find:w=='G'?Go:w=='S'?SaveCopy:w=='Z'?Undo:w=='Y'?Redo:0;
            if(w=='F') {ActivateToolTab(h,*s,2);SetFocus(GetDlgItem(h,SearchText));return 0;}
            if(w=='G') {ActivateToolTab(h,*s,3);SetFocus(GetDlgItem(h,GoText));return 0;}
            if(w=='A'&&!s->job){Select(h,*s,0,s->document->size);return 0;}
            if(id) {Command(h,*s,id);return 0;}
        }
        if(w==VK_F3) {Command(h,*s,(GetKeyState(VK_SHIFT)&0x8000)?FindPrevious:FindNext);return 0;}
        if(w==VK_ESCAPE){s->highNibble=-1;s->activeNibble=-1;if(s->job)s->job->cancel=true;else {Inspect(h,*s);Highlight(h,*s);}return 0;}
        if(!s->job && s->document->size && !(GetKeyState(VK_SHIFT)&0x8000) && !ctrl) {
            uint64_t at=s->selection;
            if(w==VK_LEFT)at=at?at-1:0;
            else if(w==VK_RIGHT)at=(std::min)(s->document->size-1,at+1);
            else if(w==VK_UP)at=at>=s->rowBytes?at-s->rowBytes:0;
            else if(w==VK_DOWN)at=(std::min)(s->document->size-1,at+s->rowBytes);
            else if(w==VK_PRIOR)at=at>=PageSize?at-PageSize:0;
            else if(w==VK_NEXT)at=(std::min)(s->document->size-1,at+PageSize);
            else at=NoByte;
            if(at!=NoByte){Select(h,*s,at,1);return 0;}
        }
    }
    if(m==WM_CHAR) {
        if(w>=32 && w<127)TypeHex(h,*s,static_cast<wchar_t>(w));
        return 0;
    }
    if(m==WM_PASTE){Command(h,*s,PasteOverwrite);return 0;}
    if(m==WM_CONTEXTMENU) {
        MouseSelection(h,*s);HMENU menu=CreatePopupMenu();
        AppendMenuW(menu,MF_STRING,CopyHex,L"Copy hex\tCtrl+C");AppendMenuW(menu,MF_STRING,CopyText,L"Copy decoded text");
        AppendMenuW(menu,MF_STRING,CopyInspector,L"Copy complete data inspector readout");
        HMENU formats=CreatePopupMenu();
        AppendMenuW(formats,MF_STRING,CopyBinary,L"Binary digits (8 per byte)");
        AppendMenuW(formats,MF_STRING,CopyPlainTable,L"Hex table with offsets and decoded text");
        AppendMenuW(formats,MF_STRING,CopyCArray,L"C byte array");AppendMenuW(formats,MF_STRING,CopyEscaped,L"Escaped bytes (\\xNN)");AppendMenuW(formats,MF_STRING,CopyBase64,L"Base64");
        AppendMenuW(formats,MF_STRING,CopyPascal,L"Pascal byte array");AppendMenuW(formats,MF_STRING,CopyJava,L"Java byte array");
        AppendMenuW(formats,MF_STRING,CopyCSharp,L"C# byte array");AppendMenuW(formats,MF_STRING,CopyVB,L"VB.NET byte array");
        AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(formats),L"Copy as");
        AppendMenuW(menu,MF_STRING|(s->edit&&!s->job?0:MF_GRAYED),PasteOverwrite,L"Paste overwrite\tCtrl+V");
        HMENU pasteFormats=CreatePopupMenu();UINT pasteFlags=MF_STRING|(s->edit&&!s->job?0:MF_GRAYED);
        AppendMenuW(pasteFormats,pasteFlags,PasteDecoded,L"Decoded text (chosen encoding)");AppendMenuW(pasteFormats,pasteFlags,PasteUtf8,L"UTF-8 text");AppendMenuW(pasteFormats,pasteFlags,PasteBase64,L"Base64 bytes");AppendMenuW(pasteFormats,pasteFlags,PasteEscaped,L"Escaped bytes (\\xNN)");AppendMenuW(pasteFormats,pasteFlags,PasteBinary,L"Binary digits");AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(pasteFormats),L"Paste overwrite as");
        AppendMenuW(menu,MF_STRING,CopyOffset,L"Copy offset\tAlt+Insert");
        AppendMenuW(menu,MF_STRING,SelectToEnd,L"Select to file end");
        AppendMenuW(menu,MF_STRING,AddBookmark,L"Bookmark selection\tCtrl+B");
        NumberedBookmarkMenus(menu,*s);PointerMenus(menu);ValueMenus(menu);
        AppendMenuW(menu,MF_STRING,ExportBlock,L"Export selection as binary...");
        AppendMenuW(menu,MF_STRING,ExportIntelHex,L"Export selection as Intel HEX...");
        HMENU exports=CreatePopupMenu();AddFormattedExports(exports);AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(exports),L"Export selection as");
        AppendMenuW(menu,MF_STRING,SaveCopy,L"Save Copy...\tCtrl+S");
        int x=static_cast<short>(LOWORD(l)),y=static_cast<short>(HIWORD(l));if(x==-1){POINT p={10,10};ClientToScreen(edit,&p);x=p.x;y=p.y;}
        int chosen=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,x,y,0,h,nullptr);DestroyMenu(menu);if(chosen)Command(h,*s,chosen);return 0;
    }
    LRESULT result=CallWindowProcW(s->originalEdit,edit,m,w,l);
    if(m==WM_LBUTTONDOWN){s->highNibble=-1;s->activeNibble=-1;}
    if(m==WM_LBUTTONUP||(m==WM_KEYUP&&s->activeNibble<0))MouseSelection(h,*s);
    return result;
}
LRESULT CALLBACK InputProcedure(HWND edit,UINT m,WPARAM w,LPARAM l) {
    HWND h=GetParent(edit);auto s=reinterpret_cast<State*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(!s)return DefWindowProcW(edit,m,w,l);
    if(m==WM_KEYDOWN && w==VK_RETURN) {
        int id=GetDlgCtrlID(edit);Command(h,*s,id==GoText?Go:id==SearchText?Find:Overwrite);return 0;
    }
    if(m==WM_CHAR && w==VK_RETURN)return 0;
    return CallWindowProcW(s->originalInput,edit,m,w,l);
}
LRESULT CALLBACK Procedure(HWND h,UINT m,WPARAM w,LPARAM l) {
    auto s=reinterpret_cast<State*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    switch(m) {
    case WM_CREATE: {
        s=reinterpret_cast<State*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(s));
        Control(h,L"BUTTON",L"Open DAT...",Open,10,10,100);Control(h,L"BUTTON",L"Save Copy...",SaveCopy,118,10,110);
        INITCOMMONCONTROLSEX commonControls={sizeof(commonControls),ICC_TAB_CLASSES};InitCommonControlsEx(&commonControls);
        HWND tabs=Control(h,WC_TABCONTROLW,L"",ToolTabs,10,48,1064,0,116);
        const wchar_t* tabNames[]={L"View & copy",L"Edit & select",L"Search & replace",L"Navigation",L"Analysis"};
        for(int i=0;i<5;++i){TCITEMW item={};item.mask=TCIF_TEXT;item.pszText=const_cast<wchar_t*>(tabNames[i]);TabCtrl_InsertItem(tabs,i,&item);}
        SetWindowPos(tabs,HWND_BOTTOM,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
        Control(h,L"BUTTON",L"Export page...",Export,236,10,112);Control(h,L"BUTTON",L"Undo",Undo,356,10,62);Control(h,L"BUTTON",L"Redo",Redo,426,10,62);
        Control(h,L"BUTTON",L"Enable editing",EditMode,498,10,126,BS_AUTOCHECKBOX);
        Combo(h,Encoding,634,10,152,{L"Shift-JIS (CP932)",L"ASCII",L"Windows (1252)",L"DOS (437)",L"EBCDIC (500)",L"Macintosh (10000)"});Combo(h,Rows,796,10,92,{L"8 bytes/row",L"16 bytes/row",L"32 bytes/row"});SendDlgItemMessageW(h,Rows,CB_SETCURSEL,2,0);
        Combo(h,OffsetBase,898,10,110,{L"Hex offsets",L"Decimal offsets"});Control(h,L"BUTTON",L"View...",ViewMenu,1018,10,56);
        Control(h,L"BUTTON",L"Previous page",Previous,10,46,112);Control(h,L"BUTTON",L"Next page",Next,130,46,102);
        Control(h,L"EDIT",L"0",GoText,242,46,148,ES_AUTOHSCROLL);Control(h,L"BUTTON",L"Go to offset",Go,398,46,100);
        Control(h,L"EDIT",L"",SearchText,508,46,224,ES_AUTOHSCROLL);Combo(h,FindMode,742,46,92,{L"Hex bytes",L"Text",L"UInt8",L"Int8",L"UInt16",L"Int16",L"UInt24",L"Int24",L"UInt32",L"Int32",L"UInt64",L"Int64",L"Float32",L"Float64",L"UTF-8 text",L"UTF-16 LE text",L"UTF-16 BE text",L"Binary bytes",L"ULEB128 (unsigned)",L"LEB128 (signed)"});
        Control(h,L"BUTTON",L"Find",Find,844,46,58);Control(h,L"BUTTON",L"Find next",FindNext,910,46,82);Control(h,L"BUTTON",L"Cancel",Cancel,1000,46,64);
        Control(h,L"STATIC",L"Start",StartLabel,10,84,40);Control(h,L"EDIT",L"0",StartText,52,82,148,ES_AUTOHSCROLL);
        Control(h,L"STATIC",L"Length",LengthLabel,210,84,46);Control(h,L"EDIT",L"1",LengthText,262,82,100,ES_AUTOHSCROLL);
        Control(h,L"BUTTON",L"Select block",SelectBlock,370,82,104);Control(h,L"BUTTON",L"Copy hex",CopyHex,484,82,90);Control(h,L"BUTTON",L"Copy text",CopyText,584,82,90);
        Control(h,L"EDIT",L"",PatchText,10,118,430,ES_AUTOHSCROLL);Control(h,L"BUTTON",L"Overwrite selection",Overwrite,450,118,148);Control(h,L"BUTTON",L"Fill pattern",Fill,608,118,114);
        Combo(h,Endian,732,118,144,{L"Little endian",L"Big endian"});
        Control(h,L"BUTTON",L"Paste overwrite",PasteOverwrite,886,118,142);
        Control(h,L"BUTTON",L"Bookmark",AddBookmark,10,154,96);Combo(h,BookmarkList,114,154,204,{});
        Control(h,L"BUTTON",L"Go",GoBookmark,328,154,52);Control(h,L"BUTTON",L"Remove",RemoveBookmark,388,154,72);
        Control(h,L"BUTTON",L"Compare file...",CompareFile,470,154,122);Control(h,L"BUTTON",L"Next difference",NextDifference,600,154,128);
        Control(h,L"BUTTON",L"Checksums",Checksums,736,154,100);Control(h,L"BUTTON",L"Export selection...",ExportSelectionMenu,846,154,150);
        Control(h,L"EDIT",L"",ReplacementText,690,82,160,ES_AUTOHSCROLL);Control(h,L"BUTTON",L"Replace match",ReplaceMatch,858,82,100);Control(h,L"BUTTON",L"Replace all",ReplaceAll,966,82,98);
        Control(h,L"BUTTON",L"Find previous",FindPrevious,10,190,112);Control(h,L"BUTTON",L"Find all",FindAll,130,190,90);
        Combo(h,MatchList,230,190,250,{});Control(h,L"BUTTON",L"Go to match",GoMatch,490,190,100);
        Control(h,L"BUTTON",L"Byte statistics",ByteStatistics,600,190,132);
        Control(h,L"BUTTON",L"Navigation...",Navigation,742,190,140);
        Combo(h,SearchScope,892,190,172,{L"Search whole file",L"Search selection"});
        Combo(h,ChecksumType,10,226,210,{L"CRC-32 + SHA-256",L"CRC-32",L"Adler-32",L"MD5",L"SHA-1",L"SHA-256",L"SHA-512",L"Checksum-8",L"Checksum-16",L"Checksum-32",L"Checksum-64",L"CRC-16/CCITT-FALSE",L"CRC-16/X-25",L"CRC-64/ECMA-182",L"CRC-32C (Castagnoli)",L"CRC-32/BZIP2",L"CRC-16/ARC",L"CRC-16/MODBUS"});
        Combo(h,AnalysisScope,230,226,220,{L"Analyze whole file",L"Analyze selection"});
        Combo(h,CopyFormat,470,226,180,{L"C byte array",L"Escaped bytes",L"Base64",L"Pascal byte array",L"Java byte array",L"C# byte array",L"VB.NET byte array"});
        Control(h,L"BUTTON",L"Copy formatted",CopyFormatted,660,226,134);
        Combo(h,ByteOperation,806,226,170,{L"Invert bits",L"AND mask",L"OR mask",L"XOR mask",L"Reverse bytes",L"Swap 16-bit words",L"Swap 32-bit words",L"Swap 64-bit words"});
        Control(h,L"BUTTON",L"Apply",ApplyOperation,986,226,78);
        SendDlgItemMessageW(h,FindMode,CB_SETDROPPEDWIDTH,180,0);
        Control(h,L"STATIC",L"Expected checksum (hex)",ExpectedLabel,10,266,166);
        Control(h,L"EDIT",L"",ExpectedChecksum,182,262,430,ES_AUTOHSCROLL);SendDlgItemMessageW(h,ExpectedChecksum,EM_SETLIMITTEXT,512,0);
        Control(h,L"BUTTON",L"Verify checksum",VerifyChecksum,622,262,150);
        Control(h,L"BUTTON",L"Copy checksum",CopyChecksum,782,262,126);Control(h,L"BUTTON",L"Export report...",ExportReport,918,262,146);
        Control(h,L"BUTTON",L"Distribution chart",DistributionChart,806,298,258);
        Control(h,L"STATIC",L"Open a DAT. Editing affects an in-memory buffer; Save Copy always creates a new file.",Info,10,300,786);
        static HMODULE richEdit=LoadLibraryW(L"Msftedit.dll");
        if(!richEdit)return -1;
        HWND dump=Control(h,MSFTEDIT_CLASS,L"",Dump,10,328,780,ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|ES_AUTOHSCROLL|WS_VSCROLL|WS_HSCROLL,420);
        SendMessageW(dump,EM_EXLIMITTEXT,0,2000000);
        SendMessageW(dump,EM_SETUNDOLIMIT,0,0);
        s->originalEdit=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(dump,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(DumpProcedure)));
        for(int id:{GoText,SearchText,PatchText})s->originalInput=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(GetDlgItem(h,id),GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(InputProcedure)));
        s->font=CreateFontW(-15,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,0,FIXED_PITCH,L"MS Gothic");SendMessageW(dump,WM_SETFONT,reinterpret_cast<WPARAM>(s->font),TRUE);
        Control(h,L"EDIT",L"Select bytes to inspect their values.",Inspector,800,328,264,ES_MULTILINE|ES_READONLY|WS_VSCROLL,260);
        Control(h,L"EDIT",L"Analysis: use Checksums or Compare file.",AnalysisOutput,800,490,264,ES_MULTILINE|ES_READONLY|WS_VSCROLL,150);
        Control(h,L"STATIC",L"",Status,10,614,1040);SetDlgItemTextW(h,ExportSelectionMenu,L"Export / file tools...");LayoutToolTabs(h,*s);Buttons(h,*s);SetTimer(h,1,100,nullptr);return 0;
    }
    case WM_GETMINMAXINFO: reinterpret_cast<MINMAXINFO*>(l)->ptMinTrackSize={1100,580};return 0;
    case WM_SIZE: {
        int width=LOWORD(l),height=HIWORD(l);MoveWindow(GetDlgItem(h,ToolTabs),10,48,(std::max)(1,width-20),116,TRUE);
        MoveWindow(GetDlgItem(h,Info),10,172,(std::max)(1,width-304),24,TRUE);
        MoveWindow(GetDlgItem(h,DistributionChart),(std::max)(10,width-284),170,274,26,TRUE);
        MoveWindow(GetDlgItem(h,Dump),10,200,(std::max)(1,width-304),(std::max)(1,height-238),TRUE);
        MoveWindow(GetDlgItem(h,Inspector),(std::max)(10,width-284),200,274,(std::max)(1,height-406),TRUE);
        MoveWindow(GetDlgItem(h,AnalysisOutput),(std::max)(10,width-284),(std::max)(200,height-196),274,158,TRUE);
        MoveWindow(GetDlgItem(h,Status),10,(std::max)(200,height-30),(std::max)(1,width-20),24,TRUE);return 0;
    }
    case WM_NOTIFY:
        if(s&&reinterpret_cast<NMHDR*>(l)->idFrom==ToolTabs&&reinterpret_cast<NMHDR*>(l)->code==TCN_SELCHANGE){s->toolTab=TabCtrl_GetCurSel(GetDlgItem(h,ToolTabs));LayoutToolTabs(h,*s);return 0;}break;
    case WM_COMMAND:
        if(s && (HIWORD(w)==BN_CLICKED||HIWORD(w)==CBN_SELCHANGE))Command(h,*s,LOWORD(w));return 0;
    case WM_TIMER:
        if(s && s->job) {
            if(s->job->done.load()) {
                auto job=s->job;s->job.reset();Buttons(h,*s);
                if(job->cancel && !((job->kind==SearchJob::Kind::Join||job->kind==SearchJob::Kind::Split)&&!job->failed)) {
                    SetDlgItemTextW(h,Status,L"Operation cancelled.");
                    if(job->kind==SearchJob::Kind::Split)SetDlgItemTextW(h,Status,L"Split cancelled. The unfinished output set was removed.");
                    else if(job->kind==SearchJob::Kind::Join)SetDlgItemTextW(h,Status,L"Join cancelled. No complete output copy was created.");
                    else if(job->kind!=SearchJob::Kind::Search)SetDlgItemTextW(h,AnalysisOutput,L"Analysis cancelled. No complete result is available.");
                } else if(job->kind==SearchJob::Kind::Split) {
                    if(job->failed)Error(h,L"Could not create the split copies. Existing parts cannot be replaced; choose a new base name. The unfinished set was removed.");
                    else SetDlgItemTextW(h,Status,(L"Created "+std::to_wstring(job->partCount)+L" numbered copies: "+job->outputPath+L".part0001...").c_str());
                } else if(job->kind==SearchJob::Kind::Join) {
                    if(job->failed)Error(h,L"Could not create the joined copy. Choose a new filename; existing files cannot be replaced.");
                    else SetDlgItemTextW(h,Status,(L"Created joined copy: "+job->outputPath).c_str());
                } else if(job->replacementLimit) {
                    SetDlgItemTextW(h,AnalysisOutput,L"Replace all exceeded 100,000 matches or 1 MB of replacement bytes. No changes were applied.");
                    SetDlgItemTextW(h,Status,L"Replace all limit reached; the buffer is unchanged.");
                } else if(job->failed) {
                    SetDlgItemTextW(h,AnalysisOutput,L"Analysis failed. No complete result is available.");Error(h,L"The operation could not read or analyze the file.");
                } else if(job->kind==SearchJob::Kind::Search||job->kind==SearchJob::Kind::PreviousMatch) {
                    if(job->result)Select(h,*s,*job->result,job->pattern.size());else SetDlgItemTextW(h,Status,L"Pattern not found in the chosen range.");
                } else if(job->kind==SearchJob::Kind::ReplaceAll&&job->replacementPlan) {
                    auto count=job->replacementPlan->count;
                    bool applied=false;
                    try{applied=s->document->CommitReplacement(std::move(*job->replacementPlan));}catch(...){applied=false;}
                    if(!applied){Error(h,L"Could not apply replacements. The buffer is unchanged.");}
                    else {
                        if(count){Changed(h,*s);Refresh(h,*s);}
                        auto report=std::to_wstring(count)+L" non-overlapping matches replaced. Range: 0x"+Number(job->rangeStart)+L" + "+std::to_wstring(job->rangeLength)+L" bytes. "+(count?L"Undo restores the entire operation.":L"The buffer is unchanged.");
                        PublishReport(h,*s,report);SetDlgItemTextW(h,Status,report.c_str());
                    }
                } else if(job->kind==SearchJob::Kind::AllMatches) {
                    s->matches=std::move(job->matches);s->matchLength=job->pattern.size();
                    SendDlgItemMessageW(h,MatchList,CB_RESETCONTENT,0,0);
                    for(auto at:s->matches){auto label=L"0x"+Number(at)+L" ("+std::to_wstring(at)+L")";SendDlgItemMessageW(h,MatchList,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));}
                    if(!s->matches.empty())SendDlgItemMessageW(h,MatchList,CB_SETCURSEL,0,0);
                    Buttons(h,*s);
                    auto report=std::to_wstring(job->matchCount)+L" matches in range 0x"+Number(job->rangeStart)+L" + "+std::to_wstring(job->rangeLength)+L" bytes (overlapping matches included).";
                    if(job->matchCount>s->matches.size())report+=L" Showing the first 10,000 offsets.";
                    PublishReport(h,*s,report);SetDlgItemTextW(h,Status,L"Find all completed. Choose an offset and use Go to match.");
                } else if(job->kind==SearchJob::Kind::Statistics&&job->statistics) {
                    s->chartStatistics=job->statistics;s->chartStart=job->rangeStart;
                    const auto& stats=*job->statistics;std::wostringstream report;
                    report<<L"CURRENT BUFFER STATISTICS\r\nRange: 0x"<<Number(job->rangeStart)<<L" + "<<job->rangeLength<<L" bytes\r\nBytes: "<<stats.total<<L"\r\nEntropy: "<<std::fixed<<std::setprecision(4)<<stats.entropy<<L" bits/byte\r\n\r\nByte    Count    Frequency\r\n";
                    for(size_t i=0;i<256;++i)report<<std::hex<<std::uppercase<<std::setw(2)<<std::setfill(L'0')<<i<<std::dec<<L"    "<<stats.counts[i]<<L"    "<<std::setprecision(2)<<(stats.total?100.0*stats.counts[i]/stats.total:0)<<L"%\r\n";
                    PublishReport(h,*s,report.str());SetDlgItemTextW(h,Status,L"Byte statistics completed for the selected analysis scope.");
                } else if(job->kind==SearchJob::Kind::Hashes && job->hashes) {
                    std::wostringstream report;report<<L"CURRENT BUFFER CHECKSUMS\r\nRange: 0x"<<Number(job->rangeStart)<<L" + "<<job->rangeLength<<L" bytes\r\n";
                    if(job->hashAlgorithm==DatHex::HashAlgorithm::Crc32Sha256) {
                        report<<L"CRC-32: "<<std::hex<<std::uppercase<<std::setw(8)<<std::setfill(L'0')<<job->hashes->crc32;
                        report<<L"\r\nSHA-256:\r\n"<<job->hashes->sha256;
                    }else report<<DatHex::HashName(job->hashAlgorithm)<<L":\r\n"<<job->hashes->digest;
                    s->lastChecksum=job->hashes->digest;
                    bool verified=!job->expectedChecksum.empty(),matches=verified&&job->expectedChecksum==job->hashes->digest;
                    if(verified)report<<L"\r\n\r\nExpected:\r\n"<<job->expectedChecksum<<L"\r\n"<<(matches?L"MATCH":L"MISMATCH");
                    PublishReport(h,*s,report.str());SetDlgItemTextW(h,Status,verified?(matches?L"Checksum matches the expected value.":L"Checksum does not match the expected value."):L"Checksum completed for the selected analysis scope.");
                } else if(job->kind==SearchJob::Kind::Compare) {
                    std::wstring report=L"COMPARE WITH\r\n"+job->comparison->path+L"\r\n";
                    if(job->difference) {
                        const auto& diff=*job->difference;
                        report+=L"Difference at 0x"+Number(diff.offset)+L"\r\nBuffer: "+(diff.left?Number(*diff.left):L"EOF")+
                            L" | Other file: "+(diff.right?Number(*diff.right):L"EOF");
                        s->compareStart=diff.offset+1;
                        Select(h,*s,(std::min)(diff.offset,s->document->size),diff.left?1:0);
                    } else {
                        report+=job->start?L"No further differences. Next difference starts again from the beginning.":L"Files are identical to the current buffer.";
                        s->compareStart=0;
                    }
                    PublishReport(h,*s,report);
                }
            } else {
                double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-s->job->started).count();
                SetDlgItemTextW(h,Status,ScanStatus(*s->job,elapsed).c_str());
            }
        }else if(s && GetFocus()==GetDlgItem(h,Dump))MouseSelection(h,*s);
        return 0;
    case WM_CLOSE:
        if(s&&(s->picking||!Discard(h,*s)))return 0;DestroyWindow(h);return 0;
    case WM_DESTROY: KillTimer(h,1);if(s&&s->job)s->job->cancel=true;if(window==h)window=nullptr;return 0;
    case WM_NCDESTROY: SetWindowLongPtrW(h,GWLP_USERDATA,0);if(s){DeleteObject(s->font);s->font=nullptr;if(s->ownedByWindow)delete s;}break;
    }
    return DefWindowProcW(h,m,w,l);
}
}
void Show(HWND owner,const char* root) {
    if(window){SetRootPath(root);Win32ToolWindow::Show(window,true);return;}
    auto s=std::make_unique<State>();s->root=BrowseRoot(root);
    Win32ToolWindow::WideSpec spec{Procedure,L"DATuraDatHexReader",L"DAT Hex Reader",1180,760};spec.style=WS_OVERLAPPEDWINDOW;spec.createParameter=s.get();
    window=Win32ToolWindow::Create(owner,spec);if(window){s->ownedByWindow=true;s.release();Win32ToolWindow::Show(window,true);}
}
void SetRootPath(const char* root) {
    if(window){auto s=reinterpret_cast<State*>(GetWindowLongPtrW(window,GWLP_USERDATA));if(s)s->root=BrowseRoot(root);}
}
}
