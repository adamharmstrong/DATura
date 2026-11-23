#include "stdafx.h"
#include "zone_diagnostics_dialog.h"
#include "win32_tool_window.h"
#include <commdlg.h>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>

namespace ZoneDiagnosticsDialog
{
namespace
{
enum { Cell = 101, Slope, Slice, MinY, MaxY, Analyze, Html, Csv, SummaryText };
struct Job
{
    std::shared_ptr<const ZoneSceneDiagnostics::Snapshot> snapshot;
    ZoneCoverage::Options options;
    ZoneCoverage::Result result;
    std::atomic<bool> done = false;
};
struct State
{
    bool ownedByWindow = false;
    std::shared_ptr<const ZoneSceneDiagnostics::Snapshot> snapshot;
    std::shared_ptr<Job> job;
};
HWND window = nullptr;
HWND Control(HWND parent, const wchar_t* type, const wchar_t* text, int id,
             int x, int y, int width, int height, DWORD style = 0)
{
    HWND result = CreateWindowExW(0,type,text,WS_CHILD|WS_VISIBLE|style,x,y,width,height,
        parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
    SendMessageW(result,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
    return result;
}
bool Number(HWND hwnd, int id, double& number)
{
    wchar_t text[80] = {};
    GetDlgItemTextW(hwnd,id,text,80);
    wchar_t* end = nullptr;
    number = std::wcstod(text,&end);
    while (end && iswspace(*end)) ++end;
    return end != text && end && !*end && std::isfinite(number);
}
void EnableExports(HWND hwnd, bool enable)
{
    EnableWindow(GetDlgItem(hwnd,Html),enable);
    EnableWindow(GetDlgItem(hwnd,Csv),enable);
}
void Start(HWND hwnd, State& state)
{
    if (state.job && !state.job->done.load()) return;
    auto job = std::make_shared<Job>();
    job->snapshot = state.snapshot;
    double slope = 0;
    job->options.useHeightRange = SendDlgItemMessageW(hwnd,Slice,BM_GETCHECK,0,0)==BST_CHECKED;
    if (!Number(hwnd,Cell,job->options.cellSize) || job->options.cellSize < .01 ||
        !Number(hwnd,Slope,slope) || slope < 0 || slope > 90 ||
        (job->options.useHeightRange && (!Number(hwnd,MinY,job->options.minY) ||
         !Number(hwnd,MaxY,job->options.maxY) || job->options.minY >= job->options.maxY)))
    {
        MessageBoxW(hwnd,L"Use a cell size of at least 0.01, a slope from 0 to 90 degrees, and increasing finite Y bounds.",
            L"Invalid diagnostic options",MB_OK|MB_ICONINFORMATION);
        return;
    }
    job->options.minAbsNormalY = slope == 90 ? 0 : std::cos(slope*3.14159265358979323846/180.0);
    EnableExports(hwnd,false);
    EnableWindow(GetDlgItem(hwnd,Analyze),false);
    SetDlgItemTextW(hwnd,SummaryText,L"Analyzing the captured scene... You can close this window safely.");
    // Worker owns only immutable snapshot data, never live scene pointers or HWNDs.
    try
    {
        std::thread([job]
        {
            try { job->result = ZoneCoverage::Build(job->snapshot->triangles,job->options); }
            catch (const std::exception& error) { job->result.error = error.what(); }
            catch (...) { job->result.error = "Unexpected analysis failure."; }
            job->done.store(true);
        }).detach();
        state.job = job;
        SetTimer(hwnd,1,100,nullptr);
    }
    catch (const std::exception& error)
    {
        EnableWindow(GetDlgItem(hwnd,Analyze),true);
        SetDlgItemTextA(hwnd,SummaryText,error.what());
    }
}
void Save(HWND hwnd, const State& state, bool html)
{
    if (!state.job || !state.job->done.load() || !state.job->result.success) return;
    wchar_t path[32768] = L"zone-coverage";
    OPENFILENAMEW dialog = {}; dialog.lStructSize=sizeof(dialog); dialog.hwndOwner=hwnd;
    dialog.lpstrFile=path; dialog.nMaxFile=32768;
    dialog.lpstrFilter=html ? L"HTML report\0*.html\0\0" : L"Coverage CSV\0*.csv\0\0";
    dialog.lpstrDefExt=html ? L"html" : L"csv";
    dialog.Flags=OFN_OVERWRITEPROMPT|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&dialog)) return;
    try
    {
        std::ofstream out(std::filesystem::path(path),std::ios::binary);
        if (!out) throw std::runtime_error("Cannot open the selected output file.");
        if (html) ZoneSceneDiagnostics::WriteHtml(out,*state.job->snapshot,state.job->options,state.job->result);
        else ZoneSceneDiagnostics::WriteCsv(out,state.job->result);
        out.close();
        if (!out) throw std::runtime_error("Could not finish saving the selected file.");
        MessageBoxW(hwnd,html ? L"Report saved. Open the HTML file in a browser to pan, zoom and compare layers."
            : L"CSV saved. It contains occupied cells only, in scene coordinates.",L"Zone diagnostics",MB_OK);
    }
    catch (const std::exception& error) { MessageBoxA(hwnd,error.what(),"Export failed",MB_OK|MB_ICONERROR); }
}
LRESULT CALLBACK Procedure(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    auto* state = reinterpret_cast<State*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        state = static_cast<State*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(state));
    }
    switch (message)
    {
    case WM_CREATE:
        Control(hwnd,L"STATIC",L"Cell size",0,16,18,70,20);
        Control(hwnd,L"EDIT",L"4",Cell,90,14,75,24,WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP);
        Control(hwnd,L"STATIC",L"Maximum slope (degrees)",0,190,18,165,20);
        Control(hwnd,L"EDIT",L"60",Slope,360,14,65,24,WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP);
        Control(hwnd,L"BUTTON",L"Limit Y",Slice,16,52,80,24,BS_AUTOCHECKBOX|WS_TABSTOP);
        Control(hwnd,L"STATIC",L"Minimum",0,110,56,65,20);
        Control(hwnd,L"EDIT",L"-10",MinY,180,52,80,24,WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP);
        Control(hwnd,L"STATIC",L"Maximum",0,280,56,65,20);
        Control(hwnd,L"EDIT",L"10",MaxY,350,52,80,24,WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP);
        EnableWindow(GetDlgItem(hwnd,MinY),false); EnableWindow(GetDlgItem(hwnd,MaxY),false);
        Control(hwnd,L"STATIC",L"Y increases downward. Reopen this window to capture a changed zone.",0,16,90,710,24);
        Control(hwnd,L"BUTTON",L"Analyze",Analyze,16,126,100,28,WS_TABSTOP);
        Control(hwnd,L"BUTTON",L"Save HTML...",Html,130,126,115,28,WS_TABSTOP);
        Control(hwnd,L"BUTTON",L"Save CSV...",Csv,260,126,115,28,WS_TABSTOP);
        Control(hwnd,L"EDIT",L"",SummaryText,16,170,710,330,WS_BORDER|ES_MULTILINE|ES_READONLY|WS_VSCROLL);
        Start(hwnd,*state);
        return 0;
    case WM_COMMAND:
        if (!state) break;
        switch (LOWORD(wparam))
        {
        case Slice:
            EnableWindow(GetDlgItem(hwnd,MinY),SendDlgItemMessageW(hwnd,Slice,BM_GETCHECK,0,0)==BST_CHECKED);
            EnableWindow(GetDlgItem(hwnd,MaxY),SendDlgItemMessageW(hwnd,Slice,BM_GETCHECK,0,0)==BST_CHECKED); return 0;
        case Analyze: Start(hwnd,*state); return 0;
        case Html: Save(hwnd,*state,true); return 0;
        case Csv: Save(hwnd,*state,false); return 0;
        }
        break;
    case WM_TIMER:
        if (state && state->job && state->job->done.load())
        {
            KillTimer(hwnd,1);
            SetDlgItemTextA(hwnd,SummaryText,ZoneSceneDiagnostics::Summary(*state->job->snapshot,
                state->job->options,state->job->result).c_str());
            EnableWindow(GetDlgItem(hwnd,Analyze),true);
            EnableExports(hwnd,state->job->result.success);
        }
        return 0;
    case WM_DESTROY: KillTimer(hwnd,1); if (window == hwnd) window = nullptr; return 0;
    case WM_NCDESTROY:
        SetWindowLongPtrW(hwnd,GWLP_USERDATA,0);
        if (state && state->ownedByWindow) delete state;
        return DefWindowProcW(hwnd,message,wparam,lparam);
    }
    return DefWindowProcW(hwnd,message,wparam,lparam);
}
}

void Show(HWND owner, ZoneSceneDiagnostics::Snapshot snapshot)
{
    if (window) DestroyWindow(window);
    auto state = std::make_unique<State>();
    state->snapshot = std::make_shared<ZoneSceneDiagnostics::Snapshot>(std::move(snapshot));
    Win32ToolWindow::WideSpec spec{Procedure,L"DATuraZoneDiagnostics",L"Zone Geometry Diagnostics",760,560};
    spec.createParameter=state.get();
    window = Win32ToolWindow::Create(owner,spec);
    if (window)
    {
        state->ownedByWindow = true;
        state.release();
        Win32ToolWindow::Show(window,true);
    }
}
}
