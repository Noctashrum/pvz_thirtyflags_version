$ErrorActionPreference = "Stop"
$enc = [System.Text.Encoding]::GetEncoding(936)

# ---------------- header: int mBossID -> Zombie* mBossPtr
$h = "D:\dsh-project\LawnProject\src\Lawn\ThirtyFlags.h"
$t = [System.IO.File]::ReadAllText($h, $enc)
$old = "    int                         mBossID;"
if ($t.IndexOf($old) -lt 0) { Write-Host "FAIL h anchor"; exit 1 }
$t = $t.Replace($old, "    Zombie*                     mBossPtr;")
[System.IO.File]::WriteAllText($h, $t, $enc)
Write-Host "OK header mBossPtr"

# ---------------- cpp
$f = "D:\dsh-project\LawnProject\src\Lawn\ThirtyFlags.cpp"
$c = [System.IO.File]::ReadAllText($f, $enc)

$pairs = @(
  @{ o = "    mBossID = -1;"; n = "    mBossPtr = NULL;" },

  @{ o = "    gThirtyFlags.mBossID = aBoss->mZombieID;"; n = "    gThirtyFlags.mBossPtr = aBoss;" },

  @{ o = "                theBoard->mApp->PlayFoley(FOLEY_LOSE_MUSIC);`r`n                theBoard->mNextWaveCounter = 0;";
     n = "                // The mech detonates: it dies outright, which ends the fight as a win.`r`n                aBoss->mBodyHealth = 0;`r`n                aBoss->DieNoLoot();`r`n                theBoard->mApp->PlayFoley(FOLEY_BOSS_EXPLOSION_SMALL);" },

  @{ o = "    Zombie* aBoss = theBoard->ZombieTryToGet(gThirtyFlags.mBossID);";
     n = "    Zombie* aBoss = TFBossResolve(theBoard);" },

  @{ o = "        gThirtyFlags.mBossPhase = 0;`r`n        gThirtyFlags.mBossID = -1;`r`n        return;";
     n = "        gThirtyFlags.mBossPhase = 0;`r`n        gThirtyFlags.mBossPtr = NULL;`r`n        return;" }
)

foreach ($p in $pairs) {
  $o = $p.o
  if ($c.IndexOf($o) -lt 0) {
    # try LF variant
    $o2 = $o -replace "`r`n", "`n"
    if ($c.IndexOf($o2) -ge 0) { $o = $o2; $p.n = $p.n -replace "`r`n", "`n" }
    else { Write-Host ("FAIL anchor: " + ($p.o -replace "`r`n", " | ")); continue }
  }
  $c = $c.Replace($o, $p.n)
  Write-Host ("OK  " + ($p.o.Substring(0, [Math]::Min(48, $p.o.Length)) -replace "`r`n", " | "))
}

# ---------------- insert TFBossResolve helper before ThirtyFlagsBossUpdate
$anchor = "void ThirtyFlagsBossUpdate(Board* theBoard)"
$ai = $c.IndexOf($anchor)
if ($ai -lt 0) { Write-Host "FAIL resolve anchor"; exit 1 }
$eol = if ($c.IndexOf("`r`n") -ge 0) { "`r`n" } else { "`n" }
$helper = @(
  '// Zombie slots are recycled, so a cached pointer is only trusted while the zombie is',
  '// still present in the board array and alive.',
  'static Zombie* TFBossResolve(Board* theBoard)',
  '{',
  '    Zombie* aCached = gThirtyFlags.mBossPtr;',
  '    if (!aCached)',
  '        return NULL;',
  '',
  '    for (unsigned int i = 0; i < theBoard->mZombies.mMaxUsedCount; i++)',
  '    {',
  '        Zombie* aSlot = &theBoard->mZombies.mBlock[i].mItem;',
  '        if (aSlot == aCached && (theBoard->mZombies.mBlock[i].mID & DATA_ARRAY_INDEX_MASK) != DATA_ARRAY_INDEX_MASK)',
  '            return aCached->mDead ? NULL : aCached;',
  '    }',
  '    return NULL;',
  '}',
  ''
) -join $eol
$c = $c.Insert($ai, $helper + $eol)
Write-Host "OK TFBossResolve inserted"

[System.IO.File]::WriteAllText($f, $c, $enc)
Write-Host "written"
