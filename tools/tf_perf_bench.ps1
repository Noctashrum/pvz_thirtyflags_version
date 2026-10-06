<#
  三十旗性能基准（自持进程版）—— 我自己造"重载现场"，不依赖玩家手动游玩。

  流程：
    启动 -> T 进三十旗 -> S 开局
    阶段 A：空场基线（20s）
    推 3 面旗（解锁 3 行，. / 1 / S）
    阶段 B：按 q 铺满全场（机枪豌豆 + 火炬树桩 + 南瓜 + 睡莲）-> 采 30s
    阶段 C：再推 4 面旗 + 再按 q 补种 -> 采 40s（行更多、僵尸更多）

  产出：docs/perf/perf_bench_report.txt + 各阶段截图 + [TFPerf] 行摘录

  注意（踩过的坑）：
    * 脚本必须全程持有 $p，否则环境可能回收游戏进程；
    * .ps1 必须带 UTF-8 BOM，否则 PS 5.1 按 GBK 解码会把中文注释吃掉；
    * 每推一面旗会回到「重选卡」界面，必须再按 S 才能继续。
#>
$ErrorActionPreference = "Continue"
$rel  = "D:/dsh-project/LawnProject/Release"
$out  = "D:/dsh-project/LawnProject/docs/perf"
$logf = "$rel\pvzdebug.log"
$tflog = "$rel\thirtyflags_flow.log"
$rep  = "$out\perf_bench_report.txt"
New-Item -ItemType Directory -Force -Path $out | Out-Null
Set-Content -Path $rep -Value "start $(Get-Date -Format 'HH:mm:ss')"

$sig = @'
using System;
using System.Runtime.InteropServices;
public class TFP {
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  public static void Key(IntPtr h, char c) {
    PostMessage(h, 0x0100, (IntPtr)(int)c, (IntPtr)0x00140001);
    PostMessage(h, 0x0101, (IntPtr)(int)c, (IntPtr)0xC0140001);
    PostMessage(h, 0x0102, (IntPtr)(int)c, (IntPtr)0x00140001);
  }
  public static void Click(IntPtr h, int x, int y) {
    IntPtr lp = (IntPtr)((y << 16) | (x & 0xFFFF));
    PostMessage(h, 0x0200, IntPtr.Zero, lp);
    System.Threading.Thread.Sleep(70);
    PostMessage(h, 0x0204, (IntPtr)1, lp);
    System.Threading.Thread.Sleep(60);
    PostMessage(h, 0x0205, IntPtr.Zero, lp);
  }
}
'@
if (-not ("TFP" -as [type])) { Add-Type -TypeDefinition $sig }
Add-Type -AssemblyName System.Drawing

$sigS = @"
using System;
using System.Runtime.InteropServices;
public class TFPS {
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  // 静默：显示但不激活、并压到窗口栈底——不抢焦点、不遮挡用户正在用的窗口
  public static void SilentShow(IntPtr h) {
    ShowWindow(h, 4 /*SW_SHOWNOACTIVATE*/);
    SetWindowPos(h, (IntPtr)1 /*HWND_BOTTOM*/, 0, 0, 0, 0,
                 0x0001 /*NOSIZE*/ | 0x0002 /*NOMOVE*/ | 0x0010 /*NOACTIVATE*/);
  }
}
"@
if (-not ("TFPS" -as [type])) { Add-Type -TypeDefinition $sigS }

function Shot([IntPtr]$h, $name) {
  $r = New-Object TFPS+RECT
  [void][TFPS]::GetClientRect($h, [ref]$r)
  if ($r.Right -le 0) { return "NORECT" }
  $b = New-Object System.Drawing.Bitmap $r.Right, $r.Bottom
  $g = [System.Drawing.Graphics]::FromImage($b); $dc = $g.GetHdc()
  [void][TFPS]::PrintWindow($h, $dc, 2)
  $g.ReleaseHdc($dc); $g.Dispose()
  $b.Save("$out/$name.png", [System.Drawing.Imaging.ImageFormat]::Png)
  $b.Dispose()
  return "shot $name ${($r.Right)}x$($r.Bottom)"
}

# 采集：把自某个时间点之后新出现的 [TFPerf] 行取出来
function Collect($tag, $sinceLines) {
  $all = @(Get-Content $tflog -ErrorAction SilentlyContinue | Where-Object { $_ -match 'TFPerf' })
  $new = $all | Select-Object -Skip $sinceLines
  Add-Content $rep "---- [$tag] TFPerf lines ----"
  if ($new.Count -eq 0) { Add-Content $rep "  (none)" } else { $new | Add-Content $rep }
  return $all.Count
}

Get-Process PlantsVsZombies -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 800
if (Test-Path $logf)  { Set-Content -Path $logf  -Value "" }
if (Test-Path $tflog) { Set-Content -Path $tflog -Value "" }

# 静默启动：最小化创建（不抢焦点），随后以「不激活」方式显示并压到栈底。
# 为什么不直接最小化跑：D3D 窗口在最小化时会停渲染，测不到真实帧耗时。
$p = Start-Process -FilePath "$rel\PlantsVsZombies.exe" -WorkingDirectory $rel -PassThru -WindowStyle Minimized
Add-Content $rep "launched pid=$($p.Id) (silent)"

$h = [IntPtr]::Zero
for ($i = 0; $i -lt 120; $i++) {
  Start-Sleep -Milliseconds 500
  $p.Refresh()
  if ($p.HasExited) { Add-Content $rep "EXITED before window"; exit 1 }
  if ($p.MainWindowHandle -ne [IntPtr]::Zero) { $h = $p.MainWindowHandle; break }
}
if ($h -eq [IntPtr]::Zero) { Add-Content $rep "NO WINDOW"; exit 1 }
[TFPS]::SilentShow($h)
Add-Content $rep "hwnd=$h (shown without activation)"

for ($i = 0; $i -lt 120; $i++) {
  Start-Sleep -Milliseconds 500
  $p.Refresh()
  if ($p.HasExited) { Add-Content $rep "EXITED during load"; exit 1 }
  if (Select-String -Path $logf -Pattern 'loading completed' -Quiet) { break }
}
Start-Sleep -Seconds 3

[TFP]::Click($h, 400, 560)      # 标题 -> 主菜单
Start-Sleep -Seconds 4
[TFP]::Key($h, [char]0x54)      # T -> 三十旗
Start-Sleep -Seconds 8
[TFP]::Key($h, [char]0x53)      # S -> 开局
Start-Sleep -Seconds 12
Add-Content $rep "started: $((Get-Content $tflog -Tail 1) -join '')"
[void](Shot $h "bench-a-start")

# ---- 阶段 A：空场基线 ----
$n = Collect "A 空场基线 20s" 0
Start-Sleep -Seconds 20
$n = Collect "A 空场基线 20s" $n
[void](Shot $h "bench-a-baseline")

# ---- 推 3 面旗，解锁 3 行 ----
foreach ($k in 1..3) {
  [TFP]::Key($h, [char]0x2E)    # . 推一旗
  Start-Sleep -Seconds 5
  [TFP]::Key($h, [char]0x31)    # 强化三选一：选第一张
  Start-Sleep -Seconds 3
  [TFP]::Key($h, [char]0x53)    # S 重选卡开局
  Start-Sleep -Seconds 9
}
Add-Content $rep "after 3 flags: $((Get-Content $tflog -Tail 1) -join '')"

# ---- 阶段 B：q 铺满全场 ----
foreach ($k in 1..3) { [TFP]::Key($h, [char]0x71); Start-Sleep -Milliseconds 900 }   # q
Start-Sleep -Seconds 5
[void](Shot $h "bench-b-filled")
$n = Collect "B 铺满(q) 30s" 0
Start-Sleep -Seconds 30
$n = Collect "B 铺满(q) 30s" $n

# ---- 阶段 C：再推 4 旗，补种，采集 ----
foreach ($k in 1..4) {
  [TFP]::Key($h, [char]0x2E)
  Start-Sleep -Seconds 5
  [TFP]::Key($h, [char]0x31)
  Start-Sleep -Seconds 3
  [TFP]::Key($h, [char]0x53)
  Start-Sleep -Seconds 8
  foreach ($j in 1..2) { [TFP]::Key($h, [char]0x71); Start-Sleep -Milliseconds 900 }  # q 补种新行
}
Add-Content $rep "after 7 flags: $((Get-Content $tflog -Tail 1) -join '')"
[void](Shot $h "bench-c-filled")
$n = Collect "C 7旗满场 40s" 0
Start-Sleep -Seconds 40
[void](Collect "C 7旗满场 40s" $n)
[void](Shot $h "bench-c-end")

$p.Refresh()
Add-Content $rep "alive=$(-not $p.HasExited)"
Copy-Item $tflog "$out\thirtyflags_flow_snapshot.log" -Force
Add-Content $rep "done $(Get-Date -Format 'HH:mm:ss')"

if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
