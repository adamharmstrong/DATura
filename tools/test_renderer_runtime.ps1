param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug')
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class RendererSmoke {
 public delegate bool EnumProc(IntPtr w, IntPtr p);
 [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr p);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr w, out uint p);
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr w, StringBuilder s, int n);
 [DllImport("user32.dll")] public static extern IntPtr GetMenu(IntPtr w);
 [DllImport("user32.dll")] public static extern int GetMenuItemCount(IntPtr m);
 [DllImport("user32.dll")] public static extern IntPtr GetSubMenu(IntPtr m, int n);
 [DllImport("user32.dll")] public static extern uint GetMenuItemID(IntPtr m, int n);
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetMenuString(IntPtr m, uint n, StringBuilder s, int c, uint f);
 [DllImport("user32.dll")] public static extern IntPtr SendMessageTimeout(IntPtr w, uint m, UIntPtr a, IntPtr b, uint f, uint t, out UIntPtr r);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr w, uint m, UIntPtr a, IntPtr b);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr w, out Rect r);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr w, IntPtr dc, uint f);
 [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr w, int x, int y, int width, int height, bool repaint);
 public struct Rect { public int L,T,R,B; }
 public static IntPtr Find(int pid, string cls) {
  IntPtr found=IntPtr.Zero;
  EnumWindows((w,p)=>{uint id; GetWindowThreadProcessId(w,out id);
   var s=new StringBuilder(256); GetClassName(w,s,256);
   if(id==pid && s.ToString()==cls) found=w; return true;},IntPtr.Zero);
  return found;
 }
}
'@
$repo = Split-Path $PSScriptRoot -Parent
$outDir = Join-Path $repo "artifacts\renderer-smoke-$Configuration"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
function Send-Checked([IntPtr]$Window,[uint32]$Message,[uint64]$Value=0,[Int64]$Data=0) {
 $result=[UIntPtr]::Zero
 $ok=[RendererSmoke]::SendMessageTimeout($Window,$Message,[UIntPtr]$Value,[IntPtr]$Data,2,15000,[ref]$result)
 if($ok -eq [IntPtr]::Zero) { throw "Message $Message timed out" }
}
function Menu-Entries([IntPtr]$Menu) {
 for($i=0;$i -lt [RendererSmoke]::GetMenuItemCount($Menu);$i++) {
  $s=New-Object System.Text.StringBuilder 512
  [void][RendererSmoke]::GetMenuString($Menu,$i,$s,512,0x400)
  $sub=[RendererSmoke]::GetSubMenu($Menu,$i)
  if($sub -ne [IntPtr]::Zero) { Menu-Entries $sub }
  else { [pscustomobject]@{Id=[RendererSmoke]::GetMenuItemID($Menu,$i);Text=$s.ToString()} }
 }
}
function Capture([IntPtr]$Window,[string]$Name) {
 $rect=New-Object RendererSmoke+Rect
 [void][RendererSmoke]::GetWindowRect($Window,[ref]$rect)
 $bitmap=New-Object System.Drawing.Bitmap ($rect.R-$rect.L),($rect.B-$rect.T)
 $graphics=[System.Drawing.Graphics]::FromImage($bitmap)
 $dc=$graphics.GetHdc()
 try { $ok=[RendererSmoke]::PrintWindow($Window,$dc,2) }
 finally { $graphics.ReleaseHdc($dc);$graphics.Dispose() }
 try { $bitmap.Save((Join-Path $outDir "$Name.png")) } finally { $bitmap.Dispose() }
 Write-Output "CAPTURE $Name PrintWindow=$ok"
}
$process=Start-Process (Join-Path $repo "x64\$Configuration\DATura.exe") -WorkingDirectory (Join-Path $repo "x64\$Configuration") -WindowStyle Hidden -PassThru
try {
 $deadline=[DateTime]::UtcNow.AddSeconds(30)
 do {
  Start-Sleep -Milliseconds 250
  if($process.HasExited) { throw "Startup exited: $($process.ExitCode)" }
  $window=[RendererSmoke]::Find($process.Id,'FFXIViewerWndClass')
 } while($window -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $deadline)
 if($window -eq [IntPtr]::Zero) { throw 'Main window missing' }
 Send-Checked $window 0
 Start-Sleep -Seconds 2
 Capture $window 'title'
 $entries=@(Menu-Entries ([RendererSmoke]::GetMenu($window)))
 $entries | Export-Csv (Join-Path $outDir 'menu.csv') -NoTypeInformation
 foreach($name in @('NPC','Jeuno Mog House','West Ronfaure')) {
  $entry=if($name -eq 'NPC') { $entries | Where-Object Id -eq 10000 | Select-Object -First 1 } else { $entries | Where-Object Text -Like "*$name*" | Select-Object -First 1 }
  if(!$entry) { throw "Menu target missing: $name" }
  Write-Output "LOAD $name ID=$($entry.Id) label=$($entry.Text)"
  Send-Checked $window 0x111 $entry.Id
  Start-Sleep -Seconds 3
  Send-Checked $window 0
  Capture $window $name.Replace(' ','-')
 }
 [void][RendererSmoke]::MoveWindow($window,40,40,1000,740,$true)
 Start-Sleep -Seconds 2
 Send-Checked $window 0
 Capture $window 'resized'
 Send-Checked $window 0x111 1004
 Start-Sleep -Seconds 2
 Capture $window 'return-title'
 [void][RendererSmoke]::PostMessage($window,0x10,[UIntPtr]::Zero,[IntPtr]::Zero)
 if(!$process.WaitForExit(15000)) { throw 'Shutdown timed out' }
 if($process.ExitCode -ne 0) { throw "Exit code $($process.ExitCode)" }
 Write-Output "PASS $Configuration runtime load/resize/return/shutdown; visual images require inspection."
} finally {
 if(!$process.HasExited) {
  [void][RendererSmoke]::PostMessage([RendererSmoke]::Find($process.Id,'FFXIViewerWndClass'),0x10,[UIntPtr]::Zero,[IntPtr]::Zero)
 }
}
