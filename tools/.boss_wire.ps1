$ErrorActionPreference = "Stop"
. "D:\dsh-project\LawnProject\tools\gbkfunc.ps1"
$file = "D:\dsh-project\LawnProject\src\Lawn\ThirtyFlags.cpp"

# 1) boss health: give ZOMBIE_BOSS the shared 300k pool instead of returning early
$ok = Edit-Gbk $file @'
    if (theZombie->mZombieType == ZOMBIE_BOSS)
        return;

    if (!theZombie->IsOnBoard())
        return;
'@ @'
    if (!theZombie->IsOnBoard())
        return;

    // Final boss: shared three-phase health pool (plan 8.1).
    if (theZombie->mZombieType == ZOMBIE_BOSS)
    {
        theZombie->mBodyHealth = TF_BOSS_HEALTH;
        theZombie->mBodyMaxHealth = TF_BOSS_HEALTH;
        return;
    }
'@
if (-not $ok) { Write-Host "FAIL boss hp patch"; exit 1 }

# 2) allow the boss on the final flag only
$ok2 = Edit-Gbk $file @'
    if (theFlag >= 29)
    {
        theAllowed[(int)ZOMBIE_JALAPENO_HEAD] = true;
        theAllowed[(int)ZOMBIE_GATLING_HEAD] = true;
        theAllowed[(int)ZOMBIE_SQUASH_HEAD] = true;
        theAllowed[(int)ZOMBIE_TALLNUT_HEAD] = true;
    }
}
'@ @'
    if (theFlag >= 29)
    {
        theAllowed[(int)ZOMBIE_JALAPENO_HEAD] = true;
        theAllowed[(int)ZOMBIE_GATLING_HEAD] = true;
        theAllowed[(int)ZOMBIE_SQUASH_HEAD] = true;
        theAllowed[(int)ZOMBIE_TALLNUT_HEAD] = true;
    }
    // Final boss only ever appears on flag 30; ThirtyFlagsBossUpdate spawns it explicitly.
    if (theFlag >= TF_BOSS_FLAG)
        theAllowed[(int)ZOMBIE_BOSS] = true;
}
'@
if (-not $ok2) { Write-Host "FAIL mask patch"; exit 1 }

# 3) drive the boss from the per-frame board hook
$ok3 = Edit-Gbk $file @'
    ThirtyFlagsUpdateSod(theBoard);
    gThirtyFlags.UpdateKillStreak();
'@ @'
    ThirtyFlagsUpdateSod(theBoard);
    gThirtyFlags.UpdateKillStreak();
    ThirtyFlagsBossUpdate(theBoard);
'@
if (-not $ok3) { Write-Host "FAIL update patch"; exit 1 }

Write-Host "all hooks wired"
