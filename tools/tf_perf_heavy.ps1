<#
  三十旗「重载」性能基准（静默）
  目标：逼近玩家后期的真实规模（多行铺满 + 大量僵尸/投射物/粒子/骨骼动画），
        判断在重载下 update 与 draw 谁先爆，以及 sprite(quads/calls) 的规模。
  流程：启动(不抢焦点) -> T -> S -> 推 10 旗（每旗后 q 补种新解锁行）-> 静置 -> 采集 40s
  产出：docs/perf/heavy.txt
#>
param([string]$Tag = "heavy")
$ErrorActionPreference = "Continue"
$rel  = "D:/dsh-project/LawnProject/Release"
$out  = "D:/dsh-project/LawnProject/docs/perf"
$logf = "$rel\pvzdebug.log"
$tflog = "$rel\thirtyflags_flow.log"
$rep  = "$out\$Tag.txt"
New-Item -ItemType Directory -Force -Path $out | Out-Null
Set-Content -Path $rep -Value "heavy tag=$Tag start $(Get-Date -Format 'HH:mm:ss')"

$sig = @'
using System;
using System.Runtime.InteropServices;
public class TFHV {
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
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
    return "shot";
  }
}
'@
if (-not ("TFHV" -as [type])) { Add-Type -TypeDefinition $sig -ReferencedAssemblies System.Drawing }

Get-Process PlantsVsZombies -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 800
Set-Content -Path $tflog -Value ""

$p = Start-Process -FilePath "$rel\PlantsVsZombies.exe" -WorkingDirectory $rel -PassThru -WindowStyle Minimized
$h = [IntPtr]::Zero
for ($i = 0; $i -lt 120; $i++) {
  Start-Sleep -Milliseconds 500
  $p.Refresh()
  if ($p.HasExited) { Add-Content $rep "EXITED early"; exit 1 }
  if ($p.MainWindowHandle -ne [IntPtr]::Zero) { $h = $p.MainWindowHandle; break }
}
if ($h -eq [IntPtr]::Zero) { Add-Content $rep "NO WINDOW"; exit 1 }
[TFHV]::SilentShow($h)

for ($i = 0; $i -lt 120; $i++) {
  Start-Sleep -Milliseconds 500
  if (Select-String -Path $logf -Pattern 'loading completed' -Quiet) { break }
}
Start-Sleep -Seconds 3
[TFHV]::Click($h, 400, 560); Start-Sleep -Seconds 4
[TFHV]::Key($h, [char]0x54); Start-Sleep -Seconds 8     # T
[TFHV]::Key($h, [char]0x53); Start-Sleep -Seconds 12    # S

# 推 10 旗：每旗解锁新行后立刻 q 补种，让场上植物/僵尸规模持续抬升
foreach ($k in 1..10) {
  [TFHV]::Key($h, [char]0x2E); Start-Sleep -Seconds 4     # .
  [TFHV]::Key($h, [char]0x31); Start-Sleep -Seconds 2     # 1 强化
  [TFHV]::Key($h, [char]0x53); Start-Sleep -Seconds 6     # S 开局
  [TFHV]::Key($h, [char]0x71); Start-Sleep -Milliseconds 800   # q 补种
  Add-Content $rep "flag$k : $((Get-Content $tflog -Tail 1) -join '')"
}
foreach ($k in 1..2) { [TFHV]::Key($h, [char]0x71); Start-Sleep -Milliseconds 900 }
Start-Sleep -Seconds 15
[void]([TFHV]::Shot($h, "$out/$Tag-filled.png"))

$before = @(Get-Content $tflog | Where-Object { $_ -match 'TFPerf' }).Count
Start-Sleep -Seconds 40
$all = @(Get-Content $tflog | Where-Object { $_ -match 'TFPerf' })
Add-Content $rep "---- TFPerf heavy window ----"
$all | Select-Object -Skip $before | Add-Content $rep
[void]([TFHV]::Shot($h, "$out/$Tag-end.png"))
$p.Refresh()
Add-Content $rep "alive=$(-not $p.HasExited) done $(Get-Date -Format 'HH:mm:ss')"
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
