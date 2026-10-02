$ErrorActionPreference = "Stop"
$enc = [System.Text.Encoding]::GetEncoding(936)
$file = "D:\dsh-project\LawnProject\src\Lawn\ThirtyFlags.h"
$txt = [System.IO.File]::ReadAllText($file, $enc)
$eol = if ($txt.IndexOf("`r`n") -ge 0) { "`r`n" } else { "`n" }

# 1) runtime boss state inside the class (anchor on the final member + closing brace)
$anchorClass = "    int                         GetKillStreak() const { return mKillStreak; }" + $eol + "};" + $eol
if ($txt.IndexOf($anchorClass) -lt 0) { Write-Host "FAIL class anchor"; exit 1 }
$add1 = @(
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
$txt = $txt.Replace($anchorClass, $add1 + $eol + $anchorClass)

# 2) constants after TF_LAWN_ROWS line
$anchorConst = "constexpr int   TF_LAWN_ROWS            = 5;"
$ci = $txt.IndexOf($anchorConst)
if ($ci -lt 0) { Write-Host "FAIL const anchor"; exit 1 }
$lineEnd = $txt.IndexOf($eol, $ci) + $eol.Length
$add2 = @(
  '',
  '// ---- Final boss (plan 8.1 / 8.2) ----',
  'constexpr int   TF_BOSS_HEALTH                 = 300000;',
  'constexpr int   TF_BOSS_PHASE2_PERCENT         = 70;',
  'constexpr int   TF_BOSS_PHASE3_PERCENT         = 30;',
  'constexpr int   TF_BOSS_SELF_DESTRUCT_PERCENT  = 10;',
  'constexpr int   TF_BOSS_SELF_DESTRUCT_FRAMES   = 3600;',
  'constexpr int   TF_BOSS_WARN_FRAMES            = 90;',
  'constexpr int   TF_BOSS_HORDE_INTERVAL         = 1200;',
  'constexpr int   TF_BOSS_SLAM_INTERVAL          = 900;',
  'constexpr int   TF_BOSS_EMP_FRAMES             = 600;',
  'constexpr int   TF_BOSS_ROAR_FRAMES            = 480;',
  'constexpr int   TF_BOSS_ROAR_SPEED_PERCENT     = 50;',
  'constexpr int   TF_BOSS_ROAR_INTERVAL          = 1500;',
  'constexpr int   TF_BOSS_DOMAIN_INTERVAL        = 900;',
  'constexpr int   TF_BOSS_TERRAIN_INTERVAL       = 1800;'
) -join $eol
$txt = $txt.Insert($lineEnd, $add2 + $eol)

# 3) hook declarations after ThirtyFlagsBoardUpdate
$anchorHook = "void    ThirtyFlagsBoardUpdate(Board* theBoard);"
$hi = $txt.IndexOf($anchorHook)
if ($hi -lt 0) { Write-Host "FAIL hook anchor"; exit 1 }
$hLineEnd = $txt.IndexOf($eol, $hi) + $eol.Length
$add3 = @(
  '',
  '// ---- Final boss hooks ----',
  'void    ThirtyFlagsBossUpdate(Board* theBoard);',
  'bool    ThirtyFlagsBossWantsBoss(int theFlag);',
  'bool    ThirtyFlagsBossIsEmpActive();',
  'bool    ThirtyFlagsBossIsDomainActive();'
) -join $eol
$txt = $txt.Insert($hLineEnd, $add3 + $eol)

[System.IO.File]::WriteAllText($file, $txt, $enc)
Write-Host "ThirtyFlags.h updated"
