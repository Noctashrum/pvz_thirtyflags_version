param(
  [string]$Tag = "run",
  [switch]$KeepAlive
)
# ThirtyFlags unattended driver: PostMessage delivers input directly to the window
# (keybd_event needs real keyboard focus, which is unreliable from a background shell).
$ErrorActionPreference = "Stop"
$sig = @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public class TF {
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
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
if (-not ("TF" -as [type])) { Add-Type -TypeDefinition $sig }
Add-Type -AssemblyName System.Drawing

$rel = "D:\dsh-project\LawnProject\Release"
$out = "D:\dsh-project\LawnProject\docs"
function Shot($h, $name) {
  $r = New-Object TF+RECT; [void][TF]::GetClientRect($h, [ref]$r)
  if ($r.Right -le 0) { Write-Host "  [$name] no client rect"; return }
  $b = New-Object System.Drawing.Bitmap $r.Right, $r.Bottom
  $g = [System.Drawing.Graphics]::FromImage($b); $dc = $g.GetHdc()
  [void][TF]::PrintWindow($h, $dc, 1); $g.ReleaseHdc($dc); $g.Dispose()
  $p = "$out\v_$name.png"; $b.Save($p, [System.Drawing.Imaging.ImageFormat]::Png); $b.Dispose()
  Write-Host "  [$name] saved"
}
function ClickAt($h, $x, $y, $wait = 300) { [TF]::Click($h, $x, $y); Start-Sleep -Milliseconds $wait }

Get-Process PlantsVsZombies -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 600
$p = Start-Process -FilePath "$rel\PlantsVsZombies.exe" -WorkingDirectory $rel -PassThru
$h = [IntPtr]::Zero
for ($i = 0; $i -lt 40; $i++) {
  Start-Sleep -Milliseconds 400
  $p.Refresh()
  if ($p.HasExited) { Write-Host "!! process exited before window"; exit 1 }
  if ($p.MainWindowHandle -ne [IntPtr]::Zero) { $h = $p.MainWindowHandle; break }
}
Write-Host "hwnd=$h after $([math]::Round($i*0.4,1))s"
Start-Sleep -Seconds 3

# Title screen: post T to jump straight into Thirty Flags card chooser.
[TF]::Key($h, [char]0x54)
Start-Sleep -Seconds 5
Shot $h "$Tag-1-chooser"

# Fill all 10 seed slots: click a card in the pool, then click a slot.
$rowY = @(164, 240, 330, 434, 520)
$colX = @(27, 82, 137, 192, 247, 302, 357, 412)
$n = 0
foreach ($y in $rowY) {
  foreach ($x in $colX) {
    if ($n -ge 10) { break }
    ClickAt $h $x $y 260
    ClickAt $h 560 300 260
    $n++
  }
  if ($n -ge 10) { break }
}
Write-Host "picked $n cards"
Shot $h "$Tag-2-full"

# Start the level.
ClickAt $h 214 566 1800
Shot $h "$Tag-3-board"

Start-Sleep -Seconds 8
Shot $h "$Tag-4-later"

Write-Host "alive=$(-not $p.HasExited)"
if (-not $KeepAlive) {
  if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; Write-Host "closed" }
} else {
  Set-Content -Path "D:\dsh-project\LawnProject\tools\.tf_pid" -Value $p.Id
  Write-Host "PID=$($p.Id) left running"
}
