<#
  三十旗性能 A/B 对照（静默运行）
  用法： powershell -File tools/tf_perf_ab.ps1 -Tag on
  流程：启动(不抢焦点) -> T -> S -> 推 3 旗 -> q 铺满 -> 静置 10s -> 采集 25s -> 退出
  产出：docs/perf/ab_<tag>.txt（含 [TFPerf] 行）+ docs/perf/ab_<tag>.png（截图）
  说明：植物部分由 q 决定，是确定性的；僵尸波次有随机性，所以主要看 sprite 比值与同规模下的 draw。
#>
param([string]$Tag = "run")
$ErrorActionPreference = "Continue"
$rel  = "D:/dsh-project/LawnProject/Release"
$out  = "D:/dsh-project/LawnProject/docs/perf"
$logf = "$rel\pvzdebug.log"
$tflog = "$rel\thirtyflags_flow.log"
$rep  = "$out\ab_$Tag.txt"
New-Item -ItemType Directory -Force -Path $out | Out-Null
Set-Content -Path $rep -Value "A/B tag=$Tag start $(Get-Date -Format 'HH:mm:ss')"

$sig = @'
using System;
using System.Runtime.InteropServices;
public class TFAB {
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
if (-not ("TFAB" -as [type])) { Add-Type -TypeDefinition $sig -ReferencedAssemblies System.Drawing }

Get-Process PlantsVsZombies -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 800
Set-Content -Path $tflog -Value ""

$p = Start-Process -FilePath "$rel\PlantsVsZombies.exe" -WorkingDirectory $rel -PassThru -WindowStyle Minimized
$h = [IntPtr]::Zero
for ($i = 0; $i -lt 120; $i++) {
  Start-Sleep -Milliseconds 500
  $p.Refresh()
  if ($p.HasExited) { Add-Content $rep "EXITED early"; exit 1 }
  if ($p.MainWindowHandle -ne [IntPtr]::Zero) { $h = $p.MainWindowHandle; break }
}
if ($h -eq [IntPtr]::Zero) { Add-Content $rep "NO WINDOW"; exit 1 }
[TFAB]::SilentShow($h)
Add-Content $rep "hwnd=$h"

for ($i = 0; $i -lt 120; $i++) {
  Start-Sleep -Milliseconds 500
  if (Select-String -Path $logf -Pattern 'loading completed' -Quiet) { break }
}
Start-Sleep -Seconds 3
[TFAB]::Click($h, 400, 560)
Start-Sleep -Seconds 4
[TFAB]::Key($h, [char]0x54); Start-Sleep -Seconds 8      # T
[TFAB]::Key($h, [char]0x53); Start-Sleep -Seconds 12     # S 开局

foreach ($k in 1..3) {                                   # 推 3 旗解锁 3 行
  [TFAB]::Key($h, [char]0x2E); Start-Sleep -Seconds 5
  [TFAB]::Key($h, [char]0x31); Start-Sleep -Seconds 3
  [TFAB]::Key($h, [char]0x53); Start-Sleep -Seconds 9
}
foreach ($k in 1..3) { [TFAB]::Key($h, [char]0x71); Start-Sleep -Milliseconds 900 }   # q 铺满
Start-Sleep -Seconds 10
Add-Content $rep ("settle: " + (Shot $h "$out/ab_$Tag.png"))

$before = @(Get-Content $tflog | Where-Object { $_ -match 'TFPerf' }).Count
Start-Sleep -Seconds 25
$lines = @(Get-Content $tflog | Where-Object { $_ -match 'TFPerf' })
Add-Content $rep "---- TFPerf (25s window, tag=$Tag) ----"
$lines | Select-Object -Skip $before | Add-Content $rep
$p.Refresh()
Add-Content $rep "alive=$(-not $p.HasExited) done $(Get-Date -Format 'HH:mm:ss')"
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
