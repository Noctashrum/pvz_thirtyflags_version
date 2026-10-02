# Boss smoke test: keyboard only (PostMessage), window never moved, runs in background.
# T (title) -> chooser, S (chooser) -> auto-pick + start, TAB (board) -> jump to flag 30.
$log = "D:\dsh-project\LawnProject\tools\.tf_boss.log"
function L($m) { $l = "{0}  {1}" -f (Get-Date -Format "HH:mm:ss.fff"), $m; Write-Host $l; Add-Content $log $l -Encoding UTF8 }
Set-Content $log "=== boss smoke ===" -Encoding UTF8

$sig = @'
using System;
using System.Runtime.InteropServices;
public class BS {
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool IsHungAppWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static void Key(IntPtr h, char c) {
    PostMessage(h, 0x0100, (IntPtr)(int)c, (IntPtr)0x00140001);
    PostMessage(h, 0x0101, (IntPtr)(int)c, (IntPtr)0xC0140001);
    PostMessage(h, 0x0102, (IntPtr)(int)c, (IntPtr)0x00140001);
  }
}
'@
if (-not ("BS" -as [type])) { Add-Type -TypeDefinition $sig }
Add-Type -AssemblyName System.Drawing

$rel = "D:\dsh-project\LawnProject\Release"
$logfile = "$rel\thirtyflags.log"
$shots = "D:\dsh-project\LawnProject\docs\boss"
New-Item -ItemType Directory -Force -Path $shots | Out-Null
Get-ChildItem $shots -Filter *.png -ErrorAction SilentlyContinue | Remove-Item -Force
if (Test-Path $logfile) { Remove-Item $logfile -Force }

Get-Process PlantsVsZombies -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 600
$p = Start-Process -FilePath "$rel\PlantsVsZombies.exe" -WorkingDirectory $rel -PassThru
L "launched pid=$($p.Id)"
$h = [IntPtr]::Zero
for ($i = 0; $i -lt 50; $i++) {
  Start-Sleep -Milliseconds 400; $p.Refresh()
  if ($p.HasExited) { L "!! exited before window"; exit 1 }
  if ($p.MainWindowHandle -ne [IntPtr]::Zero) { $h = $p.MainWindowHandle; break }
}
L "hwnd=$h  (window is NOT moved)"

function Snap($name) {
  $rr = New-Object BS+RECT
  [void][BS]::GetClientRect($h, [ref]$rr)
  if ($rr.Right -le 0) { return }
  $b = New-Object System.Drawing.Bitmap $rr.Right, $rr.Bottom
  $g = [System.Drawing.Graphics]::FromImage($b); $dc = $g.GetHdc()
  [void][BS]::PrintWindow($h, $dc, 1); $g.ReleaseHdc($dc); $g.Dispose()
  $b.Save("$shots\$name.png", [System.Drawing.Imaging.ImageFormat]::Png); $b.Dispose()
  L "shot $name"
}

# Wait until the title screen reports loading finished (the log records it), then send T.
# Pressing T before the loader finishes wedges the app, so this wait is mandatory.
$waited = 0
while ($waited -lt 60) {
  Start-Sleep -Seconds 2; $waited += 2
  $p.Refresh()
  if ($p.HasExited) { L "!! exited while waiting for load"; exit 1 }
  if ((Test-Path $logfile) -and (Select-String -Path $logfile -Pattern 'loading completed' -Quiet)) { break }
}
L "title load finished after ${waited}s"
Start-Sleep -Seconds 2
[BS]::Key($h, [char]0x54); L "sent T"
Start-Sleep -Seconds 8
Snap "1-chooser"
[BS]::Key($h, [char]0x53); L "sent S"
Start-Sleep -Seconds 30
$p.Refresh(); L "after S: exited=$($p.HasExited) hung=$([BS]::IsHungAppWindow($h))"
Snap "2-board"

[BS]::Key($h, [char]0x09); L "sent TAB (jump to flag 30)"
$t = 0
while ($t -lt 90) {
  Start-Sleep -Seconds 6; $t += 6
  $p.Refresh()
  if ($p.HasExited) { L "!! EXITED at ${t}s after TAB"; break }
  L "t=${t}s hung=$([BS]::IsHungAppWindow($h)) cpu=$([math]::Round($p.CPU,1))"
  if ($t -eq 30 -or $t -eq 60 -or $t -eq 84) { Snap ("3-boss-{0}s" -f $t) }
}
$p.Refresh(); L "DONE exited=$($p.HasExited)"
if (-not $p.HasExited) { Snap "4-final"; Stop-Process -Id $p.Id -Force; L "closed" }
Write-Host "--- thirtyflags.log ---"
if (Test-Path $logfile) { Get-Content $logfile }
