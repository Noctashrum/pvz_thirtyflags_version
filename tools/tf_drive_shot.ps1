<#
  一次性驱动 + 截图：假设游戏已经在运行（不新起进程）。
  用途：手工验证渲染改动（UV 重映射必须肉眼核对像素）。
  用法：powershell -File tools/tf_drive_shot.ps1 -Tag atlasverify
#>
param([string]$Tag = "shot", [int]$Fill = 1)
$ErrorActionPreference = "Continue"
$out = "D:/dsh-project/LawnProject/docs/perf"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$rep = "$out/drive_$Tag.txt"
Set-Content -Path $rep -Value "drive tag=$Tag $(Get-Date -Format 'HH:mm:ss')"
function L($m) { Add-Content $rep $m; }

$sig = @'
using System;
using System.Runtime.InteropServices;
public class TFD {
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static void Key(IntPtr h, char c) {
    PostMessage(h, 0x0100, (IntPtr)(int)c, (IntPtr)0x00140001);
    PostMessage(h, 0x0101, (IntPtr)(int)c, (IntPtr)0xC0140001);
    PostMessage(h, 0x0102, (IntPtr)(int)c, (IntPtr)0x00140001);
  }
  public static void Click(IntPtr h, int x, int y) {
    IntPtr lp = (IntPtr)((y << 16) | (x & 0xFFFF));
    PostMessage(h, 0x0200, IntPtr.Zero, lp); System.Threading.Thread.Sleep(70);
    PostMessage(h, 0x0204, (IntPtr)1, lp);   System.Threading.Thread.Sleep(60);
    PostMessage(h, 0x0205, IntPtr.Zero, lp);
  }
  public static void SilentShow(IntPtr h) {
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
if (-not ("TFD" -as [type])) { Add-Type -TypeDefinition $sig -ReferencedAssemblies System.Drawing }

$p = Get-Process PlantsVsZombies -ErrorAction SilentlyContinue | Select-Object -First 1
if ($p -eq $null) { L "no running process"; exit 1 }
$p.Refresh()
$h = $p.MainWindowHandle
L "pid=$($p.Id) hwnd=$h"
if ($h -eq [IntPtr]::Zero) {
  for ($i = 0; $i -lt 20; $i++) { Start-Sleep -Seconds 1; $p.Refresh(); if ($p.MainWindowHandle -ne [IntPtr]::Zero) { $h = $p.MainWindowHandle; break } }
}
if ($h -eq [IntPtr]::Zero) { L "still no hwnd"; exit 1 }
[TFD]::SilentShow($h)
L ("silent show ok; " + [TFD]::Shot($h, "$out/${Tag}_before.png"))

[TFD]::Click($h, 400, 560); Start-Sleep -Seconds 4
[TFD]::Key($h, [char]0x54); Start-Sleep -Seconds 7      # T
[TFD]::Key($h, [char]0x53); Start-Sleep -Seconds 10     # S
[TFD]::Key($h, [char]0x31); Start-Sleep -Seconds 1
[TFD]::Key($h, [char]0x53); Start-Sleep -Seconds 8
L ("after T/S; " + [TFD]::Shot($h, "$out/${Tag}_ingame.png"))

if ($Fill -gt 0) {
  foreach ($k in 1..3) { [TFD]::Key($h, [char]0x71); Start-Sleep -Milliseconds 900 }
  Start-Sleep -Seconds 6
}
L ("filled; " + [TFD]::Shot($h, "$out/${Tag}_filled.png"))
L "done"
