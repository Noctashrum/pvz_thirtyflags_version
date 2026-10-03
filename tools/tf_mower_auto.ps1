<#
  三十旗小推车自动验证（自持进程版）
  在一个脚本里完成：启动 -> 进关 -> 开局 -> 连推 5 面旗 -> 快照日志
  运行方式：以后台任务跑，保证脚本存活期间游戏进程也存活。
#>
$ErrorActionPreference = "Continue"
$rel = "D:/dsh-project/LawnProject/Release"
$out = "D:/dsh-project/LawnProject/docs/mower"
$logf = "$rel\pvzdebug.log"
$tflog = "$rel\thirtyflags_flow.log"
$rep  = "$out\mower_auto_report.txt"
New-Item -ItemType Directory -Force -Path $out | Out-Null
Set-Content -Path $rep -Value "start $(Get-Date -Format 'HH:mm:ss')"

$sig = @'
using System;
using System.Runtime.InteropServices;
public class TFA {
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
if (-not ("TFA" -as [type])) { Add-Type -TypeDefinition $sig }

Add-Type -AssemblyName System.Drawing
function Shot([IntPtr]$h, $name) {
  $sig2 = @"
using System;
using System.Runtime.InteropServices;
public class TFS {
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@
  if (-not ("TFS" -as [type])) { Add-Type -TypeDefinition $sig2 }
  $r = New-Object TFS+RECT
  [void][TFS]::GetClientRect($h, [ref]$r)
  if ($r.Right -le 0) { return "NORECT" }
  $b = New-Object System.Drawing.Bitmap $r.Right, $r.Bottom
  $g = [System.Drawing.Graphics]::FromImage($b); $dc = $g.GetHdc()
  [void][TFS]::PrintWindow($h, $dc, 2)
  $g.ReleaseHdc($dc); $g.Dispose()
  $b.Save("D://dsh-project//LawnProject//docs//mower//$name.png", [System.Drawing.Imaging.ImageFormat]::Png)
  $b.Dispose()
  return "shot $name $($r.Right)x$($r.Bottom)"
}

Get-Process PlantsVsZombies -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 800

# 先把旧日志清空，否则下面等 "First Draw Time" 会被上一次会话的残留内容骗过去
if (Test-Path $logf)  { Set-Content -Path $logf  -Value "" }
if (Test-Path $tflog) { Set-Content -Path $tflog -Value "" }

$p = Start-Process -FilePath "$rel\PlantsVsZombies.exe" -WorkingDirectory $rel -PassThru
Add-Content $rep "launched pid=$($p.Id)"

# 1) 先死等窗口句柄
$h = [IntPtr]::Zero
for ($i = 0; $i -lt 120; $i++) {
  Start-Sleep -Milliseconds 500
  $p.Refresh()
  if ($p.HasExited) { Add-Content $rep "EXITED before window"; exit 1 }
  if ($p.MainWindowHandle -ne [IntPtr]::Zero) { $h = $p.MainWindowHandle; break }
}
if ($h -eq [IntPtr]::Zero) { Add-Content $rep "NO WINDOW after 60s"; exit 1 }
Add-Content $rep "hwnd=$h"

# 2) 再等加载真正完成
for ($i = 0; $i -lt 120; $i++) {
  Start-Sleep -Milliseconds 500
  $p.Refresh()
  if ($p.HasExited) { Add-Content $rep "EXITED during load"; exit 1 }
  if (Select-String -Path $logf -Pattern 'loaded\] loading completed' -Quiet) { break }
}
Add-Content $rep "loaded=$((Select-String -Path $logf -Pattern 'loading completed' -Quiet))"
Start-Sleep -Seconds 3

[TFA]::Click($h, 400, 560)          # 标题 -> 主菜单
Start-Sleep -Seconds 4
[TFA]::Key($h, [char]0x54)          # T -> 三十旗
Start-Sleep -Seconds 8
Add-Content $rep "after T: $((Get-Content $logf -Tail 2) -join ' | ')"
[TFA]::Key($h, [char]0x53)          # S -> 自动选卡开局
Start-Sleep -Seconds 10
Add-Content $rep "after S: $((Get-Content $logf -Tail 2) -join ' | ')"

# 每推一面旗后都会回到「重选卡」界面，必须再按一次 S 才能继续推进；
# 行扩展发生在第 4/8/13/18 旗，所以至少要推到第 5 旗才能看到新行的小推车。
foreach ($n in 2..5) {
  [TFA]::Key($h, [char]0x2E)        # . -> 推进一面旗
  Start-Sleep -Seconds 5
  [TFA]::Key($h, [char]0x31)        # 1 -> 强化三选一选第一张（弹窗会抢焦点）
  Start-Sleep -Seconds 3
  [TFA]::Key($h, [char]0x53)        # S -> 重选卡界面直接开局
  Start-Sleep -Seconds 9
  $p.Refresh()
  Add-Content $rep "flag$n alive=$(-not $p.HasExited) tf=$((Get-Content $tflog -Tail 1) -join '')"
  if (-not $p.HasExited) { Add-Content $rep ("  " + (Shot $h "auto-flag$n")) }
}

Copy-Item $logf  "$out\pvzdebug_snapshot.log" -Force
Copy-Item $tflog "$out\thirtyflags_flow_snapshot.log" -Force
Add-Content $rep "---- TFMower / FlagChanged lines (thirtyflags_flow.log) ----"
Get-Content $tflog | Where-Object { $_ -match 'TFMower|FlagChanged' } | Add-Content $rep
Add-Content $rep "done $(Get-Date -Format 'HH:mm:ss')"
