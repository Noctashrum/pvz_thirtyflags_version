param(
  [string]$Tag = "probe",
  [int]$PauseAfterT = 6,
  [switch]$SkipCards,
  [switch]$SkipStart,
  [switch]$KeepAlive
)
# ThirtyFlags unattended driver, instrumented. Every step appends to a log IMMEDIATELY
# (Set-Content -NoNewline) so an external observer can see exactly where it stops.
$ErrorActionPreference = "Continue"
$log = "D:\dsh-project\LawnProject\tools\.tf_probe.log"
function L($m) {
  $line = "{0}  {1}" -f (Get-Date -Format "HH:mm:ss.fff"), $m
  Write-Host $line
  Add-Content -Path $log -Value $line -Encoding UTF8
}
Set-Content -Path $log -Value "=== probe $Tag start ===" -Encoding UTF8

$sig = @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public class TFD {
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool IsHungAppWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  public static int LP(int x, int y) { return (y << 16) | (x & 0xFFFF); }
  public static void Key(IntPtr h, char c) {
    PostMessage(h, 0x0100, (IntPtr)(int)c, (IntPtr)0x00140001);
    PostMessage(h, 0x0101, (IntPtr)(int)c, (IntPtr)0xC0140001);
    PostMessage(h, 0x0102, (IntPtr)(int)c, (IntPtr)0x00140001);
  }
  public static void Click(IntPtr h, int x, int y) {
    POINT p; p.X = x; p.Y = y; ClientToScreen(h, ref p); SetCursorPos(p.X, p.Y);
    PostMessage(h, 0x0200, (IntPtr)0, (IntPtr)LP(x, y));
    PostMessage(h, 0x0201, (IntPtr)1, (IntPtr)LP(x, y));
    PostMessage(h, 0x0202, (IntPtr)0, (IntPtr)LP(x, y));
  }
}
'@
if (-not ("TFD" -as [type])) { Add-Type -TypeDefinition $sig }
Add-Type -AssemblyName System.Drawing

$rel = "D:\dsh-project\LawnProject\Release"
$out = "D:\dsh-project\LawnProject\docs"

function SaveShot($h, $name) {
  $r = New-Object TFD+RECT
  if (-not [TFD]::GetClientRect($h, [ref]$r)) { L "shot/$name FAILED GetClientRect"; return }
  L "shot/$name rect=$($r.Right)x$($r.Bottom)"
  $b = New-Object System.Drawing.Bitmap $r.Right, $r.Bottom
  $g = [System.Drawing.Graphics]::FromImage($b); $dc = $g.GetHdc()
  L "shot/$name calling PrintWindow..."
  $ok = [TFD]::PrintWindow($h, $dc, 1)
  L "shot/$name PrintWindow returned ok=$ok"
  $g.ReleaseHdc($dc); $g.Dispose()
  $p = "$out\v_$name.png"; $b.Save($p, [System.Drawing.Imaging.ImageFormat]::Png); $b.Dispose()
  L "shot/$name saved"
}

Get-Process PlantsVsZombies -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 600
$p = Start-Process -FilePath "$rel\PlantsVsZombies.exe" -WorkingDirectory $rel -PassThru
L "launched pid=$($p.Id)"
$h = [IntPtr]::Zero
for ($i = 0; $i -lt 40; $i++) {
  Start-Sleep -Milliseconds 400
  $p.Refresh()
  if ($p.HasExited) { L "!! exited before window"; exit 1 }
  if ($p.MainWindowHandle -ne [IntPtr]::Zero) { $h = $p.MainWindowHandle; break }
}
L "hwnd=$h after $([math]::Round($i*0.4,1))s"
Start-Sleep -Seconds 3
L "hung=$([TFD]::IsHungAppWindow($h)) before T"

[TFD]::Key($h, [char]0x54)
L "posted T"
Start-Sleep -Seconds $PauseAfterT
$p.Refresh(); L "after T: exited=$($p.HasExited) hung=$([TFD]::IsHungAppWindow($h))"
SaveShot $h "$Tag-1-chooser"

if (-not $SkipCards) {
  $rowY = @(164, 240, 330, 434, 520)
  $colX = @(27, 82, 137, 192, 247, 302, 357, 412)
  $n = 0
  foreach ($y in $rowY) {
    foreach ($x in $colX) {
      if ($n -ge 10) { break }
      [TFD]::Click($h, $x, $y); Start-Sleep -Milliseconds 220
      [TFD]::Click($h, 560, 300); Start-Sleep -Milliseconds 220
      $n++
    }
    if ($n -ge 10) { break }
  }
  L "picked $n cards; hung=$([TFD]::IsHungAppWindow($h))"
  SaveShot $h "$Tag-2-full"
}

if (-not $SkipStart) {
  [TFD]::Click($h, 214, 566)
  L "clicked start; waiting 4s"
  Start-Sleep -Seconds 4
  $p.Refresh(); L "after start: exited=$($p.HasExited) hung=$([TFD]::IsHungAppWindow($h))"
  SaveShot $h "$Tag-3-board"
  Start-Sleep -Seconds 8
  $p.Refresh(); L "after 8s: exited=$($p.HasExited) hung=$([TFD]::IsHungAppWindow($h))"
  SaveShot $h "$Tag-4-later"
}

$p.Refresh()
L "DONE exited=$($p.HasExited)"
if (-not $KeepAlive) {
  if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; L "closed" }
} else {
  Set-Content -Path "D:\dsh-project\LawnProject\tools\.tf_pid" -Value $p.Id
  L "PID=$($p.Id) left running"
}
