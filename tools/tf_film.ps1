param(
  [string]$Tag = "film",
  [int]$HoldTitle = 20,
  [int]$HoldChooser = 20,
  [int]$After = 40
)
# Film the T -> S transition at fixed intervals so the failure is visible frame by frame.
$ErrorActionPreference = "Continue"
$log = "D:\dsh-project\LawnProject\tools\.tf_film.log"
function L($m) { $l = "{0}  {1}" -f (Get-Date -Format "HH:mm:ss.fff"), $m; Write-Host $l; Add-Content $log $l -Encoding UTF8 }
Set-Content $log "=== film $Tag ===" -Encoding UTF8

$sig = @'
using System;
using System.Runtime.InteropServices;
public class TFX {
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern bool IsHungAppWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static void Key(IntPtr h, char c) {
    PostMessage(h, 0x0100, (IntPtr)(int)c, (IntPtr)0x00140001);
    PostMessage(h, 0x0101, (IntPtr)(int)c, (IntPtr)0xC0140001);
    PostMessage(h, 0x0102, (IntPtr)(int)c, (IntPtr)0x00140001);
  }
}
'@
if (-not ("TFX" -as [type])) { Add-Type -TypeDefinition $sig }
Add-Type -AssemblyName System.Drawing

$rel = "D:\dsh-project\LawnProject\Release"
$out = "D:\dsh-project\LawnProject\docs\film"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$frame = 0
function Snap($h, $note) {
  $script:frame++
  if (-not [TFX]::IsWindow($h)) { L "frame$($script:frame) $note : window gone"; return }
  $r = New-Object TFX+RECT
  if (-not [TFX]::GetClientRect($h, [ref]$r) -or $r.Right -le 0) { L "frame$($script:frame) $note : no rect"; return }
  $b = New-Object System.Drawing.Bitmap $r.Right, $r.Bottom
  $g = [System.Drawing.Graphics]::FromImage($b); $dc = $g.GetHdc()
  [void][TFX]::PrintWindow($h, $dc, 1); $g.ReleaseHdc($dc); $g.Dispose()
  $name = "{0:d3}_{1}.png" -f $script:frame, $note
  $b.Save("$out\$name", [System.Drawing.Imaging.ImageFormat]::Png); $b.Dispose()
  L "frame$($script:frame) $note"
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

# 阶段 A：标题画面停留
$t = 0
while ($t -lt $HoldTitle) { Start-Sleep -Seconds 4; $t += 4; $p.Refresh()
  if ($p.HasExited) { L "!! EXITED during title at ${t}s"; exit 1 } }
Snap $h "A-title"
L "title held ${t}s OK"

[TFX]::Key($h, [char]0x54); L "sent T"
$t = 0
while ($t -lt $HoldChooser) { Start-Sleep -Seconds 4; $t += 4; $p.Refresh()
  if ($p.HasExited) { L "!! EXITED during chooser at ${t}s"; exit 1 } }
Snap $h "B-chooser"
L "chooser held ${t}s OK"

[TFX]::Key($h, [char]0x53); L "sent S"
$t = 0
while ($t -lt $After) {
  Start-Sleep -Seconds 2; $t += 2; $p.Refresh()
  if ($p.HasExited) { L "!! EXITED after S at ${t}s"; break }
  Snap $h ("C-{0:d2}s" -f $t)
}
$p.Refresh(); L "DONE exited=$($p.HasExited)"
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; L "closed" }
