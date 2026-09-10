#include "stdafx.h"
#include "dat_replacement_dialog.h"
#include "ffxi_dat_resolver.h"
#include "ffxi_install_path.h"
#include "win32_tool_window.h"
#include <sstream>

namespace DatReplacementDialog
{
namespace
{
enum { Root = 101, Enabled, Browse, Save, Refresh, History };
std::string registryKey;
FFXIDatResolver::Settings pending;
HWND window = nullptr;
constexpr const char* valueName = "DatReplacements";

HWND Control(HWND parent,const char* type,const char* text,int id,int x,int y,int width,int height,DWORD style=0)
{
    HWND result=CreateWindowExA(0,type,text,WS_CHILD|WS_VISIBLE|style,x,y,width,height,parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleA(nullptr),nullptr);
    SendMessageA(result,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
    return result;
}
void UpdateHistory(HWND hwnd)
{
    const auto active=FFXIDatResolver::CurrentSettings();
    std::ostringstream text;
    text<<"ACTIVE THIS SESSION: "<<(active.enabled?"enabled":"disabled")
        <<"\r\nRetail root: "<<active.installRoot<<"\r\nReplacement root: "<<active.replacementRoot
        <<"\r\n\r\nRecent DAT opens (newest first, maximum 128 distinct paths)."
          "\r\nOpen success does not mean the DAT parsed successfully.\r\n";
    const auto records=FFXIDatResolver::RecentOpens();
    for(auto it=records.rbegin();it!=records.rend();++it)
        text<<"\r\n"<<(it->resolution.source==FFXIDatResolver::Source::Replacement?"REPLACEMENT":"RETAIL")
            <<" | "<<(it->error?"Open failed, Windows error "+std::to_string(it->error):"Opened")
            <<"\r\nLogical: "<<it->resolution.logicalPath<<"\r\nSource: "<<it->resolution.sourcePath<<"\r\n";
    SetDlgItemTextA(hwnd,History,text.str().c_str());
}
void SaveSettings(HWND hwnd)
{
    char path[MAX_PATH]={}; GetDlgItemTextA(hwnd,Root,path,MAX_PATH);
    const bool enabled=SendDlgItemMessageA(hwnd,Enabled,BM_GETCHECK,0,0)==BST_CHECKED;
    if(enabled)
    {
        const DWORD attrs=GetFileAttributesA(path);
        if(!path[0] || attrs==INVALID_FILE_ATTRIBUTES || !(attrs&FILE_ATTRIBUTE_DIRECTORY))
        { MessageBoxA(hwnd,"Choose an existing replacement folder before enabling it.","DAT replacements",MB_OK|MB_ICONINFORMATION); return; }
    }
    const std::string normalized=path[0]?FFXIDatResolver::Detail::Absolute(path):"";
    const std::string saved=(enabled?"1\n":"0\n")+normalized;
    HKEY key=nullptr;
    LONG status=RegCreateKeyExA(HKEY_CURRENT_USER,registryKey.c_str(),0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr);
    if(status==ERROR_SUCCESS)
    {
        status=RegSetValueExA(key,valueName,0,REG_SZ,reinterpret_cast<const BYTE*>(saved.c_str()),static_cast<DWORD>(saved.size()+1));
        RegCloseKey(key);
    }
    if(status!=ERROR_SUCCESS)
    { MessageBoxA(hwnd,"Could not save DAT replacement settings.","DAT replacements",MB_OK|MB_ICONERROR); return; }
    pending.enabled=enabled; pending.replacementRoot=normalized;
    MessageBoxA(hwnd,"Settings saved. Restart DATura to apply them consistently to models, rooms, fonts, textures and resources.\n\n"
        "The current session continues using the active policy shown below.","Restart required",MB_OK|MB_ICONINFORMATION);
}
LRESULT CALLBACK Procedure(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam)
{
    switch(message)
    {
    case WM_CREATE:
        Control(hwnd,"STATIC","Settings for next startup",0,16,16,500,22);
        Control(hwnd,"BUTTON","Enable DAT replacements",Enabled,16,44,250,24,BS_AUTOCHECKBOX|WS_TABSTOP);
        SendDlgItemMessageA(hwnd,Enabled,BM_SETCHECK,pending.enabled?BST_CHECKED:BST_UNCHECKED,0);
        Control(hwnd,"EDIT",pending.replacementRoot.c_str(),Root,16,78,590,25,WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP);
        SendDlgItemMessageA(hwnd,Root,EM_SETLIMITTEXT,MAX_PATH-1,0);
        Control(hwnd,"BUTTON","Browse...",Browse,620,78,100,25,WS_TABSTOP);
        Control(hwnd,"STATIC","Mirror retail paths inside this folder, e.g. ROM\\1\\35.DAT or ROM2\\0\\1.DAT.",0,16,116,710,22);
        Control(hwnd,"STATIC","Missing replacement: retail fallback. Existing replacements are used as-is, without fallback.",0,16,141,710,22);
        Control(hwnd,"BUTTON","Save for next startup",Save,16,178,180,28,WS_TABSTOP);
        Control(hwnd,"BUTTON","Refresh sources",Refresh,212,178,140,28,WS_TABSTOP);
        Control(hwnd,"EDIT","",History,16,222,710,310,WS_BORDER|ES_MULTILINE|ES_READONLY|WS_VSCROLL|WS_HSCROLL);
        SendDlgItemMessageA(hwnd,History,EM_SETLIMITTEXT,1024*1024,0);
        UpdateHistory(hwnd);
        return 0;
    case WM_COMMAND:
        switch(LOWORD(wparam))
        {
        case Browse:
        {
            char path[MAX_PATH]={};
            if(FFXIInstallPath::BrowseForFolder(hwnd,"Select a replacement root containing ROM folders",path,sizeof(path)))
                SetDlgItemTextA(hwnd,Root,path);
            return 0;
        }
        case Save: SaveSettings(hwnd); return 0;
        case Refresh: UpdateHistory(hwnd); return 0;
        }
        break;
    case WM_DESTROY: if(window==hwnd) window=nullptr; return 0;
    }
    return DefWindowProcA(hwnd,message,wparam,lparam);
}
}
void Initialize(const char* installRoot,const char* key)
{
    registryKey=key?key:"";
    char saved[MAX_PATH+4]={};
    pending={installRoot?installRoot:"","",false};
    const bool loaded=FFXIInstallPath::LoadSavedPath(registryKey.c_str(),valueName,saved,sizeof(saved));
    saved[sizeof(saved)-1]='\0';
    if(loaded &&
        (saved[0]=='0' || saved[0]=='1') && saved[1]=='\n')
    { pending.enabled=saved[0]=='1'; pending.replacementRoot=saved+2; }
    FFXIDatResolver::Configure(pending);
}
void Show(HWND owner)
{
    if(!window)
    {
        const Win32ToolWindow::Spec spec{Procedure,"DATuraDatReplacements","DAT Replacements and Sources",760,590};
        window=Win32ToolWindow::Create(owner,spec);
    }
    if(window) { UpdateHistory(window); Win32ToolWindow::Show(window,true); }
}
}
