#include "../../DATura/dat_hex_reader.cpp"
#include <cassert>
int main() {
    using namespace DatHexReader;
    assert(BrowseRoot(nullptr)==L"C:\\Program Files (x86)\\PlayOnline\\SquareEnix\\FINAL FANTASY XI\\" );
    assert(BrowseRoot("D:\\Games\\FFXI" )==L"D:\\Games\\FFXI" );
    const unsigned char ascii[]={0,1,0x20,0x41,0x7f};
    assert(Decode(ascii,5,false)==L"_. A.");
    const unsigned char japanese[]={0x82,0xa0,0xa6};
    assert(Decode(japanese,3,true)==L"\x3042\xff66");
    const unsigned char invalid[]={0x82,0x20};
    assert(Decode(invalid,2,true)==L". ");
    assert(Decode(japanese,1,true)==L".");
    std::filesystem::path fixture=L"artifacts/dat-hex-reader-check/fixture.dat";
    { std::ofstream out(fixture,std::ios::binary); std::string bytes(PageSize+1,'A'); out.write(bytes.data(),bytes.size()); }
    State* state=new State;
    state->path=fixture.wstring(); state->size=PageSize+1;
    state->file.open(fixture,std::ios::binary);
    Win32ToolWindow::WideSpec spec{Procedure,L"DATuraHexReaderTest",L"Test",960,640}; spec.createParameter=state;
    HWND h=Win32ToolWindow::Create(nullptr,spec); assert(h); state->ownedByWindow=true;
    Refresh(h,*state);
    assert(state->text.find(L"0000FFF0")!=std::wstring::npos);
    assert(IsWindowEnabled(GetDlgItem(h,Next)));
    state->offset=PageSize; Refresh(h,*state);
    assert(state->text.find(L"00010000  41 ")!=std::wstring::npos);
    assert(!IsWindowEnabled(GetDlgItem(h,Next)));
    assert(IsWindowEnabled(GetDlgItem(h,Previous)));
    state->file.close();
    { std::ofstream empty(fixture,std::ios::binary|std::ios::trunc); }
    state->file.open(fixture,std::ios::binary); state->size=0; state->offset=0; Refresh(h,*state);
    assert(state->text.find(L"00000000  ")==std::wstring::npos);
    assert(!IsWindowEnabled(GetDlgItem(h,Next)));
    DestroyWindow(h);
    std::filesystem::remove(fixture);
    puts("DAT hex reader decoding and page checks passed.");
}
