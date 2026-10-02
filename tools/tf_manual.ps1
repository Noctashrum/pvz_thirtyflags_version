<#
  ThirtyFlags manual test runner  (observer only - sends NO input)

  It starts the game, then every few seconds records a screenshot plus a status line.
  You play the game yourself; this only watches. It cannot misclick anything.

  HOW TO ENTER THE MODE
    1. Title screen   : press  T       -> card chooser (whole plant pool unlocked)
    2. Card chooser   : press  S       -> auto-pick 10 seeds and start the battle

  TESTING THE FINAL BOSS (flag 30) WITHOUT PLAYING 29 FLAGS
    Once the battle has started, press  TAB  -> jumps straight to flag 30 and rebuilds the board.
    The boss spawns a moment later. Watch the log for:
        boss summoned hp=300000 row=N
        boss phase -> 2      (at 70% health)
        boss phase -> 3      (at 30% health)

  IN-BATTLE KEYS (no unlock needed)
    TAB   jump to flag 30 (final boss)
    .     advance one flag
    ,     clear the current flag

  WHAT TO LOOK AT ONCE THE LEVEL STARTS
    - top-left corner shows a diagnostic HUD:
        FLAG n/30        current flag
        unlock abcde     which of the 5 lanes are open (1 = open)
        water  abcde     which lanes are water (9 = lane not open yet)
        upg / elite / kill
    - flag 1-3 should be ONE lane only, and it should grow as flags advance
    - unlocked lanes should show a sod-rolling animation
    - from flag 18 on, a lane becomes water
    - flag 30: the giant mech boss walks in and uses a different skill each phase


  USAGE
    powershell -ExecutionPolicy Bypass -File tools\tf_manual.ps1
    powershell -ExecutionPolicy Bypass -File tools\tf_manual.ps1 -Seconds 900
    powershell -ExecutionPolicy Bypass -File tools\tf_manual.ps1 -ShotEvery 0     (no screenshots)
#>
param(
  [int]$Seconds = 420,
  [int]$ShotEvery = 12
)

$ErrorActionPreference = "Continue"
$sig = @'
using System;
using System.Runtime.InteropServices;
public class MTR {
  [DllImport("user32.dll")] public static extern bool IsHungAppWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
'@
if (-not ("MTR" -as [type])) { Add-Type -TypeDefinition $sig }
Add-Type -AssemblyName System.Drawing

$rel = "D:\dsh-project\LawnProject\Release"
$shots = "D:\dsh-project\LawnProject\docs\run"
$log = "$rel\thirtyflags.log"

New-Item -ItemType Directory -Force -Path $shots | Out-Null
Get-ChildItem $shots -Filter *.png -ErrorAction SilentlyContinue | Remove-Item -Force
if (Test-Path $log) { Remove-Item $log -Force }

Write-Host "=============================================================="
Write-Host " ThirtyFlags manual test"
Write-Host "   title screen : press  T   -> card chooser"
Write-Host "   chooser      : press  S   -> auto-pick + start battle"
Write-Host "   watching for : $Seconds s   (screenshot every ${ShotEvery}s)"
Write-Host "   shots        : $shots"
Write-Host "   log          : $log"
Write-Host "=============================================================="

Get-Process PlantsVsZombies -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 600

$p = Start-Process -FilePath "$rel\PlantsVsZombies.exe" -WorkingDirectory $rel -PassThru
Write-Host "launched pid=$($p.Id)  $(Get-Date -Format HH:mm:ss)"
Start-Sleep -Seconds 8
$p.Refresh()
$h = $p.MainWindowHandle
Write-Host "hwnd=$h  (do not drag the game window; dragging stalls its loading thread)"

$outcome = "still running when the timer ran out"
$t = 0
$shot = 0
while ($t -lt $Seconds) {
  Start-Sleep -Seconds 3
  $t += 3
  $p.Refresh()
  if ($p.HasExited) {
    $outcome = "EXITED BY ITSELF at ${t}s (exit code $($p.ExitCode))"
    break
  }
  if ($ShotEvery -gt 0 -and ($t % $ShotEvery) -eq 0) {
    $shot++
    $hung = $false
    if ([MTR]::IsWindow($h)) {
      $r = New-Object MTR+RECT
      if ([MTR]::GetClientRect($h, [ref]$r) -and $r.Right -gt 0) {
        $b = New-Object System.Drawing.Bitmap $r.Right, $r.Bottom
        $g = [System.Drawing.Graphics]::FromImage($b); $dc = $g.GetHdc()
        [void][MTR]::PrintWindow($h, $dc, 1); $g.ReleaseHdc($dc); $g.Dispose()
        $b.Save(("$shots\{0:d3}_{1:d4}s.png" -f $shot, $t), [System.Drawing.Imaging.ImageFormat]::Png)
        $b.Dispose()
      }
      $hung = [MTR]::IsHungAppWindow($h)
    }
    $line = "{0}  t={1}s hung={2} responding={3} cpu={4}s shot={5}" -f (Get-Date -Format HH:mm:ss), $t, $hung, $p.Responding, [math]::Round($p.CPU, 1), $shot
    Write-Host $line
    Add-Content -Path $log -Value ("[watcher] " + $line) -Encoding UTF8
  }
}

$p.Refresh()
Write-Host ""
Write-Host "=============================================================="
Write-Host " RESULT: $outcome"
if ($p.HasExited) { Write-Host " exit code: $($p.ExitCode)" }
else { Write-Host " game still running - close it yourself when you are done" }
Write-Host " screenshots: $shots"
Write-Host "=============================================================="
Write-Host ""
Write-Host "--- thirtyflags.log ---"
if (Test-Path $log) { Get-Content $log } else { Write-Host "(no log written)" }
