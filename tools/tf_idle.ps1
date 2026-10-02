param([int]$Seconds = 45)
# No input at all. Verifies whether the app is stable sitting on the title screen.
$log = "D:\dsh-project\LawnProject\tools\.tf_idle.log"
function L($m) { $l = "{0}  {1}" -f (Get-Date -Format "HH:mm:ss.fff"), $m; Write-Host $l; Add-Content $log $l -Encoding UTF8 }
Set-Content $log "=== idle test ===" -Encoding UTF8

$sig = @'
using System;
using System.Runtime.InteropServices;
public class IDL {
  [DllImport("user32.dll")] public static extern bool IsHungAppWindow(IntPtr h);
}
'@
if (-not ("IDL" -as [type])) { Add-Type -TypeDefinition $sig }

$rel = "D:\dsh-project\LawnProject\Release"
Get-Process PlantsVsZombies -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 600
$p = Start-Process -FilePath "$rel\PlantsVsZombies.exe" -WorkingDirectory $rel -PassThru
L "launched pid=$($p.Id)"
Start-Sleep -Seconds 8
$p.Refresh(); $h = $p.MainWindowHandle
L "hwnd=$h exited=$($p.HasExited)"

$t = 0
while ($t -lt $Seconds) {
  Start-Sleep -Seconds 5; $t += 5
  $p.Refresh()
  if ($p.HasExited) { L "!! EXITED at ${t}s (no input)"; break }
  $cpu = [math]::Round($p.CPU, 1)
  L "t=${t}s hung=$([IDL]::IsHungAppWindow($h)) cpu=${cpu}s responding=$($p.Responding)"
}
$p.Refresh()
if (-not $p.HasExited) { L "STABLE for ${t}s, cpu=$([math]::Round($p.CPU,1))s"; Stop-Process -Id $p.Id -Force; L "closed" }
