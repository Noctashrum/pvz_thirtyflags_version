<#
  三十旗性能采样（稳健版，静默运行）
  特点：
    * 每一步都写日志，失败可诊断
    * 「进局」有重试：按 T/S 后若 30s 内流水日志没有 TF 行，就再试一轮（最多 5 次）
    * 进局后用 q 铺满，静置后采样；输出 docs/perf/<Tag>.txt
    * 静默：最小化创建 + 不激活显示 + PostMessage 输入（不抢焦点、不动鼠标）
  用法：powershell -File tools/tf_measure.ps1 -Tag xxx [-FillSeconds 30] [-NoFill]
#>
param([string]$Tag = "measure", [int]$FillSeconds = 30, [switch]$NoFill)
$ErrorActionPreference = "Continue"
$rel  = "D:/dsh-project/LawnProject/Release"
$out  = "D:/dsh-project/LawnProject/docs/perf"
$logf = "$rel\pvzdebug.log"
$tflog = "$rel\thirtyflags_flow.log"
$rep  = "$out\$Tag.txt"
New-Item -ItemType Directory -Force -Path $out | Out-Null
function L($m) { Add-Content $rep ("[{0}] {1}" -f (Get-Date -Format 'HH:mm:ss'), $m) }
Set-Content -Path $rep -Value "measure tag=$Tag fill=$FillSeconds nofill=$NoFill"

$sig = @'
using System;
using System.Runtime.InteropServices;
public class TFM {
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
  [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
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
}
'@
if (-not ("TFM" -as [type])) { Add-Type -TypeDefinition $sig -ReferencedAssemblies System.Drawing }

function GetTfCount { @(Get-Content $tflog -ErrorAction SilentlyContinue | Where-Object { $_ -match 'TFPerf' }).Count }

Get-Process PlantsVsZombies -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 1000
Set-Content -Path $tflog -Value ""
Set-Content -Path $logf -Value ""
L "launching"

$p = Start-Process -FilePath "$rel\PlantsVsZombies.exe" -WorkingDirectory $rel -PassThru -WindowStyle Minimized
if ($p -eq $null) { L "Start-Process failed"; exit 1 }
L "pid=$($p.Id)"

$h = [IntPtr]::Zero
for ($i = 0; $i -lt 60; $i++) {
  Start-Sleep -Milliseconds 500
  $p.Refresh()
  if ($p.HasExited) { L "EXITED before window (exit=$($p.ExitCode))"; exit 1 }
  if ($p.MainWindowHandle -ne [IntPtr]::Zero) { $h = $p.MainWindowHandle; break }
}
if ($h -eq [IntPtr]::Zero) { L "NO WINDOW after 30s"; exit 1 }
[TFM]::SilentShow($h)
L "hwnd=$h"

$loaded = $false
for ($i = 0; $i -lt 120; $i++) {
  Start-Sleep -Milliseconds 500
  $p.Refresh()
  if ($p.HasExited) { L "EXITED during load"; exit 1 }
  if (Select-String -Path $logf -Pattern 'loading completed' -Quiet) { $loaded = $true; break }
}
L "loaded=$loaded"
Start-Sleep -Seconds 3

# ---- 进局（带重试）----
$entered = $false
for ($try = 1; $try -le 5; $try++) {
  $p.Refresh()
  if ($p.HasExited) { L "EXITED while entering"; exit 1 }

  if ($try -eq 1) {
    [TFM]::Click($h, 400, 560)      # 标题 -> 主菜单
    Start-Sleep -Seconds 4
  }
  [TFM]::Key($h, [char]0x54)        # T -> 三十旗
  Start-Sleep -Seconds 6
  [TFM]::Key($h, [char]0x53)        # S -> 开局
  Start-Sleep -Seconds 8

  # 若弹窗抢了焦点，补按 1 / S 试图走掉
  [TFM]::Key($h, [char]0x31); Start-Sleep -Milliseconds 800
<<<<<<< HEAD
  [TFM]::Key($h, [char]0x53); Start-Sleep -Seconds 3
  # 「继续游戏?」弹窗：点【新游戏】（客户端坐标 ≈ 557,480）确保进新局
  [TFM]::Click($h, 557, 480); Start-Sleep -Seconds 4
  [TFM]::Click($h, 395, 480); Start-Sleep -Seconds 3
=======
  [TFM]::Key($h, [char]0x53); Start-Sleep -Seconds 6
>>>>>>> parent of 9baa09d (表现层特效层：加色四边形池 + 按类型分组绘制（§12 结论的正解）)

  for ($w = 0; $w -lt 8; $w++) {
    if ((GetTfCount) -gt 0) { $entered = $true; break }
    Start-Sleep -Seconds 2
  }
  L "try$try entered=$entered tfLines=$(GetTfCount)"
  if ($entered) { break }
}

if (-not $entered) { L "FAILED to enter game"; if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }; exit 1 }

if (-not $NoFill) {
  foreach ($k in 1..3) { [TFM]::Key($h, [char]0x71); Start-Sleep -Milliseconds 800 }   # q 铺满
  L "filled"
}
Start-Sleep -Seconds $FillSeconds

L ("shot: " + [TFM]::Shot($h, "$out/$Tag.png"))
$all = @(Get-Content $tflog | Where-Object { $_ -match 'TFPerf' })
L "---- TFPerf (last 12) ----"
$all | Select-Object -Last 12 | Add-Content $rep
L "total tf lines=$($all.Count) done"
$p.Refresh()
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
