$ErrorActionPreference = "Stop"
. "D:\dsh-project\LawnProject\tools\gbkfunc.ps1"

# ---- 1) EMP: shooters stop firing while the pulse is active
$ok = Edit-Gbk "D:\dsh-project\LawnProject\src\Lawn\Plant.cpp" @'
void Plant::Fire(Zombie* theTargetZombie, int theRow, PlantWeapon thePlantWeapon)
{
    if (mSeedType == SeedType::SEED_FUMESHROOM)
'@ @'
void Plant::Fire(Zombie* theTargetZombie, int theRow, PlantWeapon thePlantWeapon)
{
    // 【三十旗】僵王 P2「EMP 脉冲」：脉冲期间所有射手类停火。
    if (ThirtyFlagsBossIsEmpActive())
        return;

    if (mSeedType == SeedType::SEED_FUMESHROOM)
'@
if (-not $ok) { Write-Host "FAIL emp"; exit 1 }

# ---- 2) P3 domain: zombies lifesteal on eating
$ok2 = Edit-Gbk "D:\dsh-project\LawnProject\src\Lawn\ThirtyFlags.cpp" @'
    if (gThirtyFlags.HasMutation(MUTATION_LIFESTEAL))
    {
        int aHeal = max(1, (int)((float)theZombie->mBodyMaxHealth * 0.02f));
        theZombie->mBodyHealth = min(theZombie->mBodyHealth + aHeal, theZombie->mBodyMaxHealth);
    }
'@ @'
    // 【三十旗】僵王 P3「尸王领域」：领域期间全体僵尸啃食均吸血。
    if (gThirtyFlags.HasMutation(MUTATION_LIFESTEAL) || ThirtyFlagsBossIsDomainActive())
    {
        float aRate = ThirtyFlagsBossIsDomainActive() ? 0.04f : 0.02f;
        int aHeal = max(1, (int)((float)theZombie->mBodyMaxHealth * aRate));
        theZombie->mBodyHealth = min(theZombie->mBodyHealth + aHeal, theZombie->mBodyMaxHealth);
    }
'@
if (-not $ok2) { Write-Host "FAIL domain lifesteal"; exit 1 }

Write-Host "EMP + domain wired"
