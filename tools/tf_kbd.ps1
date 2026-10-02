param(
  [string]$Tag = "kbd",
  [int]$WatchSeconds = 25,
  [switch]$KeepAlive
)
# ThirtyFlags keyboard-only verification: T (title) -> chooser, S (chooser) -> auto-pick + start.
# No mouse input at all, so this cannot click the wrong UI.
$ErrorActionPreference = "Continue"
$log = "D:\dsh-project\LawnProject\tools\.tf_probe.log"
function L($m) {
  $line = "{0}  {1}" -f (Get-Date -Format "HH:mm:ss.fff"), $m
  Write-Host $line; Add-Content -Path $log -Value $line -Encoding UTF8
}
Set-Content -Path $log -Value "=== kbd $Tag ===" -Encoding UTF8

$sig = @'
using System;
using System.Runtime.InteropServices;
public class TFK {
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern bool IsHungAppWindow(IntPtr h);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static void Key(IntPtr h, char c) {
    PostMessage(h, 0x0100, (IntPtr)(int)c, (IntPtr)0x00140001);
    PostMessage(h, 0x0101, (IntPtr)(int)c, (IntPtr)0xC0140001);
    PostMessage(h, 0x0102, (IntPtr)(int)c, (IntPtr)0x00140001);
  }
}
'@
if (-not ("TFK" -as [type])) { Add-Type -TypeDefinition $sig }
Add-Type -AssemblyName System.Drawing

$rel = "D:\dsh-project\LawnProject\Release"
$out = "D:\dsh-project\LawnProject\docs"
function SaveShot($h, $name) {
  $r = New-Object TFK+RECT
  if (-not [TFK]::GetClientRect($h, [ref]$r)) { L "shot/$name FAILED"; return }
  $b = New-Object System.Drawing.Bitmap $r.Right, $r.Bottom
  $g = [System.Drawing.Graphics]::FromImage($b); $dc = $g.GetHdc()
  $ok = [TFK]::PrintWindow($h, $dc, 1)
  $g.ReleaseHdc($dc); $g.Dispose()
  $b.Save("$out\k_$name.png", [System.Drawing.Imaging.ImageFormat]::Png); $b.Dispose()
  L "shot k_$name.png ok=$ok"
}

Get-Process PlantsVsZombies -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 600
$p = Start-Process -FilePath "$rel\PlantsVsZombies.exe" -WorkingDirectory $rel -PassThru
L "launched pid=$($p.Id)"
$h = [IntPtr]::Zero
for ($i = 0; $i -lt 40; $i++) {
  Start-Sleep -Milliseconds 400; $p.Refresh()
  if ($p.HasExited) { L "!! exited before window"; exit 1 }
  if ($p.MainWindowHandle -ne [IntPtr]::Zero) { $h = $p.MainWindowHandle; break }
}
L "hwnd=$h"
Start-Sleep -Seconds 3

[TFK]::Key($h, [char]0x54); L "sent T"
Start-Sleep -Seconds 6
SaveShot $h "$Tag-1-chooser"

[TFK]::Key($h, [char]0x53); L "sent S (auto-pick + start)"
Start-Sleep -Seconds 4
$p.Refresh(); L "after S: exited=$($p.HasExited) hung=$([TFK]::IsHungAppWindow($h))"
SaveShot $h "$Tag-2-board"

$elapsed = 4
while ($elapsed -lt $WatchSeconds) {
  Start-Sleep -Seconds 4; $elapsed += 4
  $p.Refresh()
  if ($p.HasExited) { L "!! exited at ${elapsed}s"; break }
  L "t=${elapsed}s hung=$([TFK]::IsHungAppWindow($h))"
}
SaveShot $h "$Tag-3-watch"
$p.Refresh(); L "DONE exited=$($p.HasExited)"

if (-not $KeepAlive) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; L "closed" } }
else { Set-Content -Path "D:\dsh-project\LawnProject\tools\.tf_pid" -Value $p.Id; L "PID=$($p.Id) running" }
