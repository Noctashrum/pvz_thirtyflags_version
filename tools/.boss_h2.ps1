$ErrorActionPreference = "Stop"
$enc = [System.Text.Encoding]::GetEncoding(936)
$file = "D:\dsh-project\LawnProject\src\Lawn\ThirtyFlags.h"
$txt = [System.IO.File]::ReadAllText($file, $enc)
$eol = if ($txt.IndexOf("`r`n") -ge 0) { "`r`n" } else { "`n" }

$old = @(
  '    // ---- Final boss (flag 30) ----',
  '    int                         mBossPhase;',
  '    int                         mBossID;',
  '    int                         mBossSkillTimer;',
  '    int                         mBossWarnTimer;',
  '    int                         mBossWarnSkill;',
  '    int                         mBossWarnRow;',
  '    int                         mBossSelfDestruct;',
  '    int                         mBossEmpTimer;',
  '    int                         mBossRoarTimer;',
  '    int                         mBossDomainTimer;',
  '    int                         GetKillStreak() const { return mKillStreak; }'
) -join $eol

$new = @(
  '    int                         GetKillStreak() const { return mKillStreak; }',
  '',
  '    // ---- Final boss (flag 30) ----',
  '    int                         mBossPhase;',
  '    int                         mBossID;',
  '    int                         mBossSkillTimer;',
  '    int                         mBossWarnTimer;',
  '    int                         mBossWarnSkill;',
  '    int                         mBossWarnRow;',
  '    int                         mBossSelfDestruct;',
  '    int                         mBossEmpTimer;',
  '    int                         mBossRoarTimer;',
  '    int                         mBossDomainTimer;'
) -join $eol

if ($txt.IndexOf($old) -lt 0) { Write-Host "FAIL reorder anchor"; exit 1 }
$txt = $txt.Replace($old, $new)
[System.IO.File]::WriteAllText($file, $txt, $enc)
Write-Host "reordered OK"
