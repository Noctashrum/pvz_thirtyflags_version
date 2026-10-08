<#
  极简截图：启动 -> 进三十旗 -> 等一会儿 -> 截图。没有其它逻辑（少即是稳）。
  用法： powershell -NoProfile -File tools\tf_shots.ps1 -Tag name [-Hold 20]
  静默：最小化创建 + 不激活显示 + PostMessage 按键（不抢焦点、不动鼠标）
#>
param([string]$Tag = "shot", [int]$Hold = 20)
$ErrorActionPreference = "Continue"
$rel = "D:/dsh-project/LawnProject/Release"
$out = "D:/dsh-project/LawnProject/docs/perf"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$rep = "$out/shot_$Tag.txt"
Set-Content -Path $rep -Value "shot tag=$Tag $(Get-Date -Format 'HH:mm:ss')"
function L($m) { Add-Content $rep ("[{0}] {1}" -f (Get-Date -Format 'HH:mm:ss'), $m) }

$cs = @'
using System;
using System.Runtime.InteropServices;
public class TFS2 {
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static void Key(IntPtr h, int c) {
    PostMessage(h, 0x0100, (IntPtr)c, (IntPtr)0x00140001);
    PostMessage(h, 0x0101, (IntPtr)c, (IntPtr)0xC0140001);
    PostMessage(h, 0x0102, (IntPtr)c, (IntPtr)0x00140001);
  }
  public static void Click(IntPtr h, int x, int y) {
    IntPtr lp = (IntPtr)((y << 16) | (x & 0xFFFF));
    PostMessage(h, 0x0200, IntPtr.Zero, lp); System.Threading.Thread.Sleep(60);
    PostMessage(h, 0x0204, (IntPtr)1, lp);   System.Threading.Thread.Sleep(50);
    PostMessage(h, 0x0205, IntPtr.Zero, lp);
  }
  public static void Silent(IntPtr h) {
    ShowWindow(h, 4);
    SetWindowPos(h, (IntPtr)1, 0, 0, 0, 0, 0x0001 | 0x0002 | 0x0010);
  }
  public static string Shot(IntPtr h, string path) {
    RECT r; GetClientRect(h, out r);
    if (r.Right <= 0) return "NORECT";
    var b = new System.Drawing.Bitmap(r.Right, r.Bottom);
    var g = System.Drawing.Graphics.FromImage(b);
    IntPtr dc = g.GetHdc();
    PrintWindow(h, dc, 2);
    g.ReleaseHdc(dc); g.Dispose();
    b.Save(path, System.Drawing.Imaging.ImageFormat.Png);
    b.Dispose();
    return "shot " + r.Right + "x" + r.Bottom;
  }
}
'@
try { if (-not ("TFS2" -as [type])) { Add-Type -TypeDefinition $cs -ReferencedAssemblies System.Drawing } }
catch { L ("Add-Type FAILED: " + $_.Exception.Message); exit 1 }
L "Add-Type ok"

Get-Process PlantsVsZombies -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2

$p = Start-Process -FilePath "$rel\PlantsVsZombies.exe" -WorkingDirectory $rel -PassThru -WindowStyle Minimized
L "pid=$($p.Id)"

$h = [IntPtr]::Zero
for ($i = 0; $i -lt 60; $i++) {
  Start-Sleep -Seconds 1
  $p.Refresh()
  if ($p.HasExited) { L "EXITED early"; exit 1 }
  if ($p.MainWindowHandle -ne [IntPtr]::Zero) { $h = $p.MainWindowHandle; break }
}
if ($h -eq [IntPtr]::Zero) { L "no window"; exit 1 }
[TFS2]::Silent($h)
L "hwnd=$h"

Start-Sleep -Seconds 45          # 等加载（宁可多等）
L "waited load"

[TFS2]::Click($h, 400, 560); Start-Sleep -Seconds 4     # 标题 -> 主菜单
[TFS2]::Key($h, 0x54); Start-Sleep -Seconds 8           # T -> 三十旗
[TFS2]::Key($h, 0x53); Start-Sleep -Seconds 10          # S -> 开局
L ("shot1: " + [TFS2]::Shot($h, "$out/${Tag}_a.png"))

Start-Sleep -Seconds $Hold
L ("shot2: " + [TFS2]::Shot($h, "$out/${Tag}_b.png"))

$p.Refresh()
L "alive=$(-not $p.HasExited)"
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
L "done"
