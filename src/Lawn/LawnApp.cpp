#include <time.h>
#include "LawnApp.h"
#include "Resources.h"
#include "Board.h"
#include "ZenGarden.h"
#include "Cutscene.h"
#include "Challenge.h"
#include "Music.h"
#include "../TodLib/Attachment.h"
#include "PoolEffect.h"
#include "ProfileMgr.h"
#include "PopDRMComm.h"
#include <cstdio>
#include <cstdarg>
#include <string>
#include <windows.h>
#include "../TodLib/PvzDebug.h"

// ====================================================================================================
// [ThirtyFlags] Diagnostic log.
// Release builds have no asserts and write no logs, so failures can only be guessed at.
// Key state transitions and unhandled exceptions go to Release\thirtyflags.log so that
// "app exited by itself" / "not responding" can be diagnosed after the fact.
// ====================================================================================================
static const char* gTfPhase = "boot";

// Both of these still write thirtyflags.log. They now also mirror into pvzdebug.log,
// so every existing diagnostic point (startup phases, boss nodes, WM_CLOSE) appears in
// one place with a timestamp, a level and the surrounding context.
static void TfLogWrite(const char* theFormat, ...)
{
#ifdef TF_PLAYER_BUILD
    (void)theFormat;
    return;
#else
    va_list aArgs;
    va_start(aArgs, theFormat);
    char aBuf[1024];
    vsnprintf(aBuf, sizeof(aBuf), theFormat, aArgs);
    va_end(aArgs);
    aBuf[sizeof(aBuf) - 1] = '\0';

    char aMirrored[1200];
    sprintf_s(aMirrored, sizeof(aMirrored), "[%s] %s\n", gTfPhase, aBuf);
    PvzLogRaw(aMirrored);

    FILE* aFile = fopen("thirtyflags.log", "a");
    if (!aFile)
        return;

    SYSTEMTIME aTime;
    GetLocalTime(&aTime);
    fprintf(aFile, "%02d:%02d:%02d.%03d [%s] %s\n", aTime.wHour, aTime.wMinute,
        aTime.wSecond, aTime.wMilliseconds, gTfPhase, aBuf);
    fclose(aFile);
#endif
}

void LawnApp::TfLog(const char* theFormat, ...)
{
    va_list aArgs;
    va_start(aArgs, theFormat);
    char aBuf[1024];
    vsnprintf(aBuf, sizeof(aBuf), theFormat, aArgs);
    va_end(aArgs);
    aBuf[sizeof(aBuf) - 1] = '\0';

    char aMirrored[1200];
    sprintf_s(aMirrored, sizeof(aMirrored), "%s\n", aBuf);
    PvzLogRaw(aMirrored);

    FILE* aFile = fopen("thirtyflags.log", "a");
    if (!aFile)
        return;

    fprintf(aFile, "%s", aBuf);
    fprintf(aFile, "\n");
    fclose(aFile);
}

// ====================================================================================================
// [PvzDebug] Crash state dump.
// PvzDebug calls this from inside its exception filter, so a crash report says which
// mode / level / wave the game was in, not just where it faulted. Keep it simple and
// non-allocating: it runs while the process is already broken.
// ====================================================================================================
static void LawnAppCrashStateDump(void* theContext)
{
    if (gLawnApp == nullptr)
    {
        PvzLogRaw("      app: not created yet\n");
        return;
    }

    char aLine[512];

    sprintf_s(aLine, sizeof(aLine), "      scene=%d gameMode=%d board=%p\n",
        (int)gLawnApp->mGameScene, (int)gLawnApp->mGameMode, (void*)gLawnApp->mBoard);
    PvzLogRaw(aLine);

    Board* aBoard = gLawnApp->mBoard;
    if (aBoard == nullptr)
    {
        PvzLogRaw("      board: none\n");
        return;
    }

    int aSurvivalStage = (aBoard->mChallenge != nullptr) ? aBoard->mChallenge->mSurvivalStage : -1;

    sprintf_s(aLine, sizeof(aLine),
        "      level=%d wave=%d/%d survivalStage=%d\n",
        aBoard->mLevel, aBoard->mCurrentWave, aBoard->mNumWaves, aSurvivalStage);
    PvzLogRaw(aLine);

    // Pool occupancy: if any of these sits at its cap, a spawn is about to fail.
    sprintf_s(aLine, sizeof(aLine),
        "      pools: zombies=%u/%u plants=%u/%u projectiles=%u/%u coins=%u/%u "
        "griditems=%u/%u lawnmowers=%u/%u\n",
        aBoard->mZombies.mSize, aBoard->mZombies.mMaxSize,
        aBoard->mPlants.mSize, aBoard->mPlants.mMaxSize,
        aBoard->mProjectiles.mSize, aBoard->mProjectiles.mMaxSize,
        aBoard->mCoins.mSize, aBoard->mCoins.mMaxSize,
        aBoard->mGridItems.mSize, aBoard->mGridItems.mMaxSize,
        aBoard->mLawnMowers.mSize, aBoard->mLawnMowers.mMaxSize);
    PvzLogRaw(aLine);
}

#include "GameButton.h"
#include "UserDialog.h"
#include "TypingCheck.h"
#include "AwardScreen.h"
#include "TitleScreen.h"
#include "StoreScreen.h"
#include "CheatDialog.h"
#include "GameSelector.h"
#include "CreditScreen.h"
#include "../TodLib/EffectSystem.h"
#include "AlmanacDialog.h"
#include "NewUserDialog.h"
#include "ContinueDialog.h"
#include "ReanimationLawn.h"
#include "ChallengeScreen.h"
#include "NewOptionsDialog.h"
#include "SeedChooserScreen.h"
#include "ThirtyFlags.h"
#include "TFDialog.h"
#include "MessageWidget.h"
#include "../SexyAppFramework/WidgetManager.h"
#include "../SexyAppFramework/Checkbox.h"
#include "../SexyAppFramework/BassMusicInterface.h"
//#include "../SexyAppFramework/BetaSupport.h"
#include "../resource.h"

using namespace Sexy;

bool		gIsPartnerBuild		= false;
bool		gSlowMo				= false;	//0x6A9EAA
bool		gFastMo				= false;	//0x6A9EAB
LawnApp*	gLawnApp			= nullptr;  //0x6A9EC0
int			gSlowMoCounter		= 0;		//0x6A9EC4

FoleyParams gLawnFoleyParamArray[(int)FoleyType::NUM_FOLEY] = {  //0x69FAD0
	{ FoleyType::FOLEY_SUN,                     10.0f,  { &Sexy::SOUND_POINTS }, 0U },
	{ FoleyType::FOLEY_SPLAT,                   10.0f,  { &Sexy::SOUND_SPLAT, &Sexy::SOUND_SPLAT2, &Sexy::SOUND_SPLAT3 }, 0U },
	{ FoleyType::FOLEY_LAWNMOWER,               10.0f,  { &Sexy::SOUND_LAWNMOWER }, 0U },
	{ FoleyType::FOLEY_THROW,                   10.0f,  { &Sexy::SOUND_THROW, &Sexy::SOUND_THROW, &Sexy::SOUND_THROW, &Sexy::SOUND_THROW2 }, 0U },
	{ FoleyType::FOLEY_SPAWN_SUN,               10.0f,  { &Sexy::SOUND_THROW }, 0U },
	{ FoleyType::FOLEY_CHOMP,                   0.0f,   { &Sexy::SOUND_CHOMP, &Sexy::SOUND_CHOMP2 }, 0U },
	{ FoleyType::FOLEY_CHOMP_SOFT,              4.0f,   { &Sexy::SOUND_CHOMPSOFT }, 0U },
	{ FoleyType::FOLEY_PLANT,                   0.0f,   { &Sexy::SOUND_PLANT, &Sexy::SOUND_PLANT2 }, 0U },
	{ FoleyType::FOLEY_USE_SHOVEL,              0.0f,   { &Sexy::SOUND_PLANT2 }, 0U },
	{ FoleyType::FOLEY_DROP,                    0.0f,   { &Sexy::SOUND_TAP2 }, 0U },
	{ FoleyType::FOLEY_BLEEP,                   0.0f,   { &Sexy::SOUND_BLEEP }, 0U },
	{ FoleyType::FOLEY_GROAN,                   0.0f,   { &Sexy::SOUND_GROAN, &Sexy::SOUND_GROAN2, &Sexy::SOUND_GROAN3, &Sexy::SOUND_GROAN4, &Sexy::SOUND_GROAN5, &Sexy::SOUND_GROAN6 }, 0U },
	{ FoleyType::FOLEY_BRAINS,                  0.0f,   { &Sexy::SOUND_GROAN, &Sexy::SOUND_GROAN2, &Sexy::SOUND_GROAN3, &Sexy::SOUND_GROAN4, &Sexy::SOUND_GROAN5, &Sexy::SOUND_GROAN6, &Sexy::SOUND_SUKHBIR4, &Sexy::SOUND_SUKHBIR5, &Sexy::SOUND_SUKHBIR6 }, 0U },
	{ FoleyType::FOLEY_SUKHBIR,                 0.0f,   { &Sexy::SOUND_GROAN, &Sexy::SOUND_GROAN2, &Sexy::SOUND_GROAN3, &Sexy::SOUND_GROAN4, &Sexy::SOUND_GROAN5, &Sexy::SOUND_GROAN6, &Sexy::SOUND_SUKHBIR, &Sexy::SOUND_SUKHBIR2, &Sexy::SOUND_SUKHBIR3 }, 0U },
	{ FoleyType::FOLEY_JACKINTHEBOX,            0.0f,   { &Sexy::SOUND_JACKINTHEBOX }, 7U },
	{ FoleyType::FOLEY_ART_CHALLENGE,           0.0f,   { &Sexy::SOUND_DIAMOND }, 0U },
	{ FoleyType::FOLEY_ZAMBONI,                 5.0f,   { &Sexy::SOUND_ZAMBONI }, 0U },
	{ FoleyType::FOLEY_THUNDER,                 10.0f,  { &Sexy::SOUND_THUNDER }, 0U },
	{ FoleyType::FOLEY_FROZEN,                  0.0f,   { &Sexy::SOUND_FROZEN }, 0U },
	{ FoleyType::FOLEY_ZOMBIESPLASH,            10.0f,  { &Sexy::SOUND_PLANT_WATER, &Sexy::SOUND_ZOMBIE_ENTERING_WATER }, 0U },
	{ FoleyType::FOLEY_BOWLINGIMPACT,           -3.0f,  { &Sexy::SOUND_BOWLINGIMPACT }, 0U },
	{ FoleyType::FOLEY_SQUISH,                  0.0f,   { &Sexy::SOUND_CHOMP, &Sexy::SOUND_CHOMP2 }, 0U },
	{ FoleyType::FOLEY_TIRE_POP,                0.0f,   { &Sexy::SOUND_BALLOON_POP }, 0U },
	{ FoleyType::FOLEY_EXPLOSION,               0.0f,   { &Sexy::SOUND_EXPLOSION }, 0U },
	{ FoleyType::FOLEY_SLURP,                   2.0f,   { &Sexy::SOUND_SLURP }, 0U },
	{ FoleyType::FOLEY_LIMBS_POP,               10.0f,  { &Sexy::SOUND_LIMBS_POP }, 0U },
	{ FoleyType::FOLEY_POGO_ZOMBIE,             4.0f,   { &Sexy::SOUND_POGO_ZOMBIE }, 0U },
	{ FoleyType::FOLEY_SNOW_PEA_SPARKLES,       10.0f,  { &Sexy::SOUND_SNOW_PEA_SPARKLES }, 0U },
	{ FoleyType::FOLEY_ZOMBIE_FALLING,          10.0f,  { &Sexy::SOUND_ZOMBIE_FALLING_1, &Sexy::SOUND_ZOMBIE_FALLING_2 }, 0U },
	{ FoleyType::FOLEY_PUFF,                    10.0f,  { &Sexy::SOUND_PUFF }, 0U },
	{ FoleyType::FOLEY_FUME,                    10.0f,  { &Sexy::SOUND_FUME }, 0U },
	{ FoleyType::FOLEY_COIN,                    10.0f,  { &Sexy::SOUND_COIN }, 0U },
	{ FoleyType::FOLEY_KERNEL_SPLAT,            10.0f,  { &Sexy::SOUND_KERNELPULT, &Sexy::SOUND_KERNELPULT2 }, 0U },
	{ FoleyType::FOLEY_DIGGER,                  0.0f,   { &Sexy::SOUND_DIGGER_ZOMBIE }, 7U },
	{ FoleyType::FOLEY_JACK_SURPRISE,           1.0f,   { &Sexy::SOUND_JACK_SURPRISE, &Sexy::SOUND_JACK_SURPRISE, &Sexy::SOUND_JACK_SURPRISE2 }, 0U },
	{ FoleyType::FOLEY_VASE_BREAKING,           -5.0f,  { &Sexy::SOUND_VASE_BREAKING }, 0U },
	{ FoleyType::FOLEY_POOL_CLEANER,            2.0f,   { &Sexy::SOUND_POOL_CLEANER }, 0U },
	{ FoleyType::FOLEY_BASKETBALL,              10.0f,  { &Sexy::SOUND_BASKETBALL }, 0U },
	{ FoleyType::FOLEY_IGNITE,                  5.0f,   { &Sexy::SOUND_IGNITE, &Sexy::SOUND_IGNITE, &Sexy::SOUND_IGNITE, &Sexy::SOUND_IGNITE2 }, 0U },
	{ FoleyType::FOLEY_FIREPEA,                 10.0f,  { &Sexy::SOUND_FIREPEA }, 0U },
	{ FoleyType::FOLEY_THUMP,                   2.0f,   { &Sexy::SOUND_GARGANTUAR_THUMP }, 0U },
	{ FoleyType::FOLEY_SQUASH_HMM,              2.0f,   { &Sexy::SOUND_SQUASH_HMM, &Sexy::SOUND_SQUASH_HMM, &Sexy::SOUND_SQUASH_HMM2 }, 0U },
	{ FoleyType::FOLEY_MAGNETSHROOM,            2.0f,   { &Sexy::SOUND_MAGNETSHROOM }, 0U },
	{ FoleyType::FOLEY_BUTTER,                  2.0f,   { &Sexy::SOUND_BUTTER }, 0U },
	{ FoleyType::FOLEY_BUNGEE_SCREAM,           2.0f,   { &Sexy::SOUND_BUNGEE_SCREAM, &Sexy::SOUND_BUNGEE_SCREAM2, &Sexy::SOUND_BUNGEE_SCREAM3 }, 0U },
	{ FoleyType::FOLEY_BOSS_EXPLOSION_SMALL,    2.0f,   { &Sexy::SOUND_EXPLOSION }, 0U },
	{ FoleyType::FOLEY_SHIELD_HIT,              10.0f,  { &Sexy::SOUND_SHIELDHIT, &Sexy::SOUND_SHIELDHIT2 }, 0U },
	{ FoleyType::FOLEY_SWING,                   2.0f,   { &Sexy::SOUND_SWING }, 0U },
	{ FoleyType::FOLEY_BONK,                    2.0f,   { &Sexy::SOUND_BONK }, 0U },
	{ FoleyType::FOLEY_RAIN,                    0.0f,   { &Sexy::SOUND_RAIN }, 5U },
	{ FoleyType::FOLEY_DOLPHIN_BEFORE_JUMPING,  0.0f,   { &Sexy::SOUND_DOLPHIN_BEFORE_JUMPING }, 0U },
	{ FoleyType::FOLEY_DOLPHIN_APPEARS,         0.0f,   { &Sexy::SOUND_DOLPHIN_APPEARS }, 0U },
	{ FoleyType::FOLEY_PLANT_WATER,             0.0f,   { &Sexy::SOUND_PLANT_WATER }, 0U },
	{ FoleyType::FOLEY_ZOMBIE_ENTERING_WATER,   0.0f,   { &Sexy::SOUND_ZOMBIE_ENTERING_WATER }, 0U },
	{ FoleyType::FOLEY_GRAVEBUSTERCHOMP,        0.0f,   { &Sexy::SOUND_GRAVEBUSTERCHOMP }, 4U },
	{ FoleyType::FOLEY_CHERRYBOMB,              0.0f,   { &Sexy::SOUND_CHERRYBOMB }, 0U },
	{ FoleyType::FOLEY_JALAPENO_IGNITE,         0.0f,   { &Sexy::SOUND_JALAPENO }, 0U },
	{ FoleyType::FOLEY_REVERSE_EXPLOSION,       0.0f,   { &Sexy::SOUND_REVERSE_EXPLOSION }, 0U },
	{ FoleyType::FOLEY_PLASTIC_HIT,             5.0f,   { &Sexy::SOUND_PLASTICHIT, &Sexy::SOUND_PLASTICHIT2 }, 0U },
	{ FoleyType::FOLEY_WINMUSIC,                0.0f,   { &Sexy::SOUND_WINMUSIC }, 8U },
	{ FoleyType::FOLEY_BALLOONINFLATE,          10.0f,  { &Sexy::SOUND_BALLOONINFLATE }, 0U },
	{ FoleyType::FOLEY_BIGCHOMP,                -2.0f,  { &Sexy::SOUND_BIGCHOMP }, 0U },
	{ FoleyType::FOLEY_MELONIMPACT,             -5.0f,  { &Sexy::SOUND_MELONIMPACT, &Sexy::SOUND_MELONIMPACT2 }, 0U },
	{ FoleyType::FOLEY_PLANTGROW,               -2.0f,  { &Sexy::SOUND_PLANTGROW }, 0U },
	{ FoleyType::FOLEY_SHOOP,                   -5.0f,  { &Sexy::SOUND_SHOOP }, 0U },
	{ FoleyType::FOLEY_JUICY,                   2.0f,   { &Sexy::SOUND_JUICY }, 0U },
	{ FoleyType::FOLEY_NEWSPAPER_RARRGH,        -2.0f,  { &Sexy::SOUND_NEWSPAPER_RARRGH, &Sexy::SOUND_NEWSPAPER_RARRGH2, &Sexy::SOUND_NEWSPAPER_RARRGH2 }, 0U },
	{ FoleyType::FOLEY_NEWSPAPER_RIP,           -2.0f,  { &Sexy::SOUND_NEWSPAPER_RIP }, 0U },
	{ FoleyType::FOLEY_FLOOP,                   0.0f,   { &Sexy::SOUND_FLOOP }, 0U },
	{ FoleyType::FOLEY_COFFEE,                  0.0f,   { &Sexy::SOUND_COFFEE }, 0U },
	{ FoleyType::FOLEY_LOW_GROAN,               2.0f,   { &Sexy::SOUND_LOWGROAN, &Sexy::SOUND_LOWGROAN2 }, 0U },
	{ FoleyType::FOLEY_PRIZE,                   0.0f,   { &Sexy::SOUND_PRIZE }, 0U },
	{ FoleyType::FOLEY_YUCK,                    1.0f,   { &Sexy::SOUND_YUCK, &Sexy::SOUND_YUCK, &Sexy::SOUND_YUCK2 }, 0U },
	{ FoleyType::FOLEY_UMBRELLA,                2.0f,   { &Sexy::SOUND_THROW2 }, 0U },
	{ FoleyType::FOLEY_GRASSSTEP,               2.0f,   { &Sexy::SOUND_GRASSSTEP }, 0U },
	{ FoleyType::FOLEY_SHOVEL,                  5.0f,   { &Sexy::SOUND_SHOVEL }, 0U },
	{ FoleyType::FOLEY_COB_LAUNCH,              10.0f,  { &Sexy::SOUND_COBLAUNCH }, 0U },
	{ FoleyType::FOLEY_WATERING,                10.0f,  { &Sexy::SOUND_WATERING }, 0U },
	{ FoleyType::FOLEY_POLEVAULT,               5.0f,   { &Sexy::SOUND_POLEVAULT }, 0U },
	{ FoleyType::FOLEY_GRAVESTONE_RUMBLE,       10.0f,  { &Sexy::SOUND_GRAVESTONE_RUMBLE }, 0U },
	{ FoleyType::FOLEY_DIRT_RISE,               5.0f,   { &Sexy::SOUND_DIRT_RISE }, 0U },
	{ FoleyType::FOLEY_FERTILIZER,              0.0f,   { &Sexy::SOUND_FERTILIZER }, 0U },
	{ FoleyType::FOLEY_PORTAL,                  0.0f,   { &Sexy::SOUND_PORTAL }, 0U },
	{ FoleyType::FOLEY_WAKEUP,                  0.0f,   { &Sexy::SOUND_WAKEUP }, 0U },
	{ FoleyType::FOLEY_BUGSPRAY,                0.0f,   { &Sexy::SOUND_BUGSPRAY }, 0U },
	{ FoleyType::FOLEY_SCREAM,                  0.0f,   { &Sexy::SOUND_SCREAM }, 0U },
	{ FoleyType::FOLEY_PAPER,                   0.0f,   { &Sexy::SOUND_PAPER }, 0U },
	{ FoleyType::FOLEY_MONEYFALLS,              0.0f,   { &Sexy::SOUND_MONEYFALLS }, 0U },
	{ FoleyType::FOLEY_IMP,                     5.0f,   { &Sexy::SOUND_IMP, &Sexy::SOUND_IMP2 }, 0U },
	{ FoleyType::FOLEY_HYDRAULIC_SHORT,         3.0f,   { &Sexy::SOUND_HYDRAULIC_SHORT }, 0U },
	{ FoleyType::FOLEY_HYDRAULIC,               0.0f,   { &Sexy::SOUND_HYDRAULIC }, 0U },
	{ FoleyType::FOLEY_GARGANTUDEATH,           3.0f,   { &Sexy::SOUND_GARGANTUDEATH }, 0U },
	{ FoleyType::FOLEY_CERAMIC,                 0.0f,   { &Sexy::SOUND_CERAMIC }, 0U },
	{ FoleyType::FOLEY_BOSS_BOULDER_ATTACK,     0.0f,   { &Sexy::SOUND_BOSSBOULDERATTACK }, 0U },
	{ FoleyType::FOLEY_CHIME,                   0.0f,   { &Sexy::SOUND_CHIME }, 0U },
	{ FoleyType::FOLEY_CRAZY_DAVE_SHORT,        0.0f,   { &Sexy::SOUND_CRAZYDAVESHORT1, &Sexy::SOUND_CRAZYDAVESHORT2, &Sexy::SOUND_CRAZYDAVESHORT3 }, 16U },
	{ FoleyType::FOLEY_CRAZY_DAVE_LONG,         0.0f,   { &Sexy::SOUND_CRAZYDAVELONG1, &Sexy::SOUND_CRAZYDAVELONG2, &Sexy::SOUND_CRAZYDAVELONG3 }, 16U },
	{ FoleyType::FOLEY_CRAZY_DAVE_EXTRA_LONG,   0.0f,   { &Sexy::SOUND_CRAZYDAVEEXTRALONG1, &Sexy::SOUND_CRAZYDAVEEXTRALONG2, &Sexy::SOUND_CRAZYDAVEEXTRALONG3 }, 16U },
	{ FoleyType::FOLEY_CRAZY_DAVE_CRAZY,        0.0f,   { &Sexy::SOUND_CRAZYDAVECRAZY }, 0U },
	{ FoleyType::FOLEY_PHONOGRAPH,              0.0f,   { &Sexy::SOUND_PHONOGRAPH }, 0U },
	{ FoleyType::FOLEY_DANCER,                  0.0f,   { &Sexy::SOUND_DANCER }, 6U },
	{ FoleyType::FOLEY_FINAL_FANFARE,           0.0f,   { &Sexy::SOUND_FINALFANFARE }, 0U },
	{ FoleyType::FOLEY_CRAZY_DAVE_SCREAM,       0.0f,   { &Sexy::SOUND_CRAZYDAVESCREAM }, 0U },
	{ FoleyType::FOLEY_CRAZY_DAVE_SCREAM_2,     0.0f,   { &Sexy::SOUND_CRAZYDAVESCREAM2 }, 0U }
};

ParticleParams gLawnParticleArray[(int)ParticleEffect::NUM_PARTICLES] = {  // 0x6A0FF0
	{ ParticleEffect::PARTICLE_MELONSPLASH, "particles\\MelonImpact.xml" },
	{ ParticleEffect::PARTICLE_WINTERMELON, "particles\\WinterMelonImpact.xml" },
	{ ParticleEffect::PARTICLE_FUMECLOUD, "particles\\FumeCloud.xml" },
	{ ParticleEffect::PARTICLE_POPCORNSPLASH, "particles\\PopcornSplash.xml" },
	{ ParticleEffect::PARTICLE_POWIE, "particles\\Powie.xml" },
	{ ParticleEffect::PARTICLE_JACKEXPLODE, "particles\\JackExplode.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_HEAD, "particles\\ZombieHead.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_ARM, "particles\\ZombieArm.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_TRAFFIC_CONE, "particles\\ZombieTrafficCone.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_PAIL, "particles\\ZombiePail.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_HELMET, "particles\\ZombieHelmet.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_FLAG, "particles\\ZombieFlag.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_DOOR, "particles\\ZombieDoor.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_NEWSPAPER, "particles\\ZombieNewspaper.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_HEADLIGHT, "particles\\ZombieHeadLight.xml" },
	{ ParticleEffect::PARTICLE_POW, "particles\\Pow.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_POGO, "particles\\ZombiePogo.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_NEWSPAPER_HEAD, "particles\\ZombieNewspaperHead.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_BALLOON_HEAD, "particles\\ZombieBalloonHead.xml" },
	{ ParticleEffect::PARTICLE_SOD_ROLL, "particles\\SodRoll.xml" },
	{ ParticleEffect::PARTICLE_GRAVE_STONE_RISE, "particles\\GraveStoneRise.xml" },
	{ ParticleEffect::PARTICLE_PLANTING, "particles\\Planting.xml" },
	{ ParticleEffect::PARTICLE_PLANTING_POOL, "particles\\PlantingPool.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_RISE, "particles\\ZombieRise.xml" },
	{ ParticleEffect::PARTICLE_GRAVE_BUSTER, "particles\\GraveBuster.xml" },
	{ ParticleEffect::PARTICLE_GRAVE_BUSTER_DIE, "particles\\GraveBusterDie.xml" },
	{ ParticleEffect::PARTICLE_POOL_SPLASH, "particles\\PoolSplash.xml" },
	{ ParticleEffect::PARTICLE_ICE_SPARKLE, "particles\\IceSparkle.xml" },
	{ ParticleEffect::PARTICLE_SEED_PACKET, "particles\\SeedPacket.xml" },
	{ ParticleEffect::PARTICLE_TALL_NUT_BLOCK, "particles\\TallNutBlock.xml" },
	{ ParticleEffect::PARTICLE_DOOM, "particles\\Doom.xml" },
	{ ParticleEffect::PARTICLE_DIGGER_RISE, "particles\\DiggerRise.xml" },
	{ ParticleEffect::PARTICLE_DIGGER_TUNNEL, "particles\\DiggerTunnel.xml" },
	{ ParticleEffect::PARTICLE_DANCER_RISE, "particles\\DancerRise.xml" },
	{ ParticleEffect::PARTICLE_POOL_SPARKLY, "particles\\PoolSparkly.xml" },
	{ ParticleEffect::PARTICLE_WALLNUT_EAT_SMALL, "particles\\WallnutEatSmall.xml" },
	{ ParticleEffect::PARTICLE_WALLNUT_EAT_LARGE, "particles\\WallnutEatLarge.xml" },
	{ ParticleEffect::PARTICLE_PEA_SPLAT, "particles\\PeaSplat.xml" },
	{ ParticleEffect::PARTICLE_BUTTER_SPLAT, "particles\\ButterSplat.xml" },
	{ ParticleEffect::PARTICLE_CABBAGE_SPLAT, "particles\\CabbageSplat.xml" },
	{ ParticleEffect::PARTICLE_PUFF_SPLAT, "particles\\PuffSplat.xml" },
	{ ParticleEffect::PARTICLE_STAR_SPLAT, "particles\\StarSplat.xml" },
	{ ParticleEffect::PARTICLE_ICE_TRAP, "particles\\IceTrap.xml" },
	{ ParticleEffect::PARTICLE_SNOWPEA_SPLAT, "particles\\SnowPeaSplat.xml" },
	{ ParticleEffect::PARTICLE_SNOWPEA_PUFF, "particles\\SnowPeaPuff.xml" },
	{ ParticleEffect::PARTICLE_SNOWPEA_TRAIL, "particles\\SnowPeaTrail.xml" },
	{ ParticleEffect::PARTICLE_LANTERN_SHINE, "particles\\LanternShine.xml" },
	{ ParticleEffect::PARTICLE_SEED_PACKET_PICKUP, "particles\\Award.xml" },
	{ ParticleEffect::PARTICLE_POTATO_MINE, "particles\\PotatoMine.xml" },
	{ ParticleEffect::PARTICLE_POTATO_MINE_RISE, "particles\\PotatoMineRise.xml" },
	{ ParticleEffect::PARTICLE_PUFFSHROOM_TRAIL, "particles\\PuffShroomTrail.xml" },
	{ ParticleEffect::PARTICLE_PUFFSHROOM_MUZZLE, "particles\\PuffShroomMuzzle.xml" },
	{ ParticleEffect::PARTICLE_SEED_PACKET_FLASH, "particles\\SeedPacketFlash.xml" },
	{ ParticleEffect::PARTICLE_WHACK_A_ZOMBIE_RISE, "particles\\WhackAZombieRise.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_LADDER, "particles\\ZombieLadder.xml" },
	{ ParticleEffect::PARTICLE_UMBRELLA_REFLECT, "particles\\UmbrellaReflect.xml" },
	{ ParticleEffect::PARTICLE_SEED_PACKET_PICK, "particles\\SeedPacketPick.xml" },
	{ ParticleEffect::PARTICLE_ICE_TRAP_ZOMBIE, "particles\\IceTrapZombie.xml" },
	{ ParticleEffect::PARTICLE_ICE_TRAP_RELEASE, "particles\\IceTrapRelease.xml" },
	{ ParticleEffect::PARTICLE_ZAMBONI_SMOKE, "particles\\ZamboniSmoke.xml" },
	{ ParticleEffect::PARTICLE_GLOOMCLOUD, "particles\\GloomCloud.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_POGO_HEAD, "particles\\ZombiePogoHead.xml" },
	{ ParticleEffect::PARTICLE_ZAMBONI_TIRE, "particles\\ZamboniTire.xml" },
	{ ParticleEffect::PARTICLE_ZAMBONI_EXPLOSION, "particles\\ZamboniExplosion.xml" },
	{ ParticleEffect::PARTICLE_ZAMBONI_EXPLOSION2, "particles\\ZamboniExplosion2.xml" },
	{ ParticleEffect::PARTICLE_CATAPULT_EXPLOSION, "particles\\CatapultExplosion.xml" },
	{ ParticleEffect::PARTICLE_MOWER_CLOUD, "particles\\MowerCloud.xml" },
	{ ParticleEffect::PARTICLE_BOSS_ICE_BALL, "particles\\BossIceBallTrail.xml" },
	{ ParticleEffect::PARTICLE_BLASTMARK, "particles\\BlastMark.xml" },
	{ ParticleEffect::PARTICLE_COIN_PICKUP_ARROW, "particles\\CoinPickupArrow.xml" },
	{ ParticleEffect::PARTICLE_PRESENT_PICKUP, "particles\\PresentPickup.xml" },
	{ ParticleEffect::PARTICLE_IMITATER_MORPH, "particles\\ImitaterMorph.xml" },
	{ ParticleEffect::PARTICLE_MOWERED_ZOMBIE_HEAD, "particles\\MoweredZombieHead.xml" },
	{ ParticleEffect::PARTICLE_MOWERED_ZOMBIE_ARM, "particles\\MoweredZombieArm.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_HEAD_POOL, "particles\\ZombieHeadPool.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_BOSS_FIREBALL, "particles\\Zombie_boss_fireball.xml" },
	{ ParticleEffect::PARTICLE_FIREBALL_DEATH, "particles\\FireballDeath.xml" },
	{ ParticleEffect::PARTICLE_ICEBALL_DEATH, "particles\\IceballDeath.xml" },
	{ ParticleEffect::PARTICLE_ICEBALL_TRAIL, "particles\\Iceball_Trail.xml" },
	{ ParticleEffect::PARTICLE_FIREBALL_TRAIL, "particles\\Fireball_Trail.xml" },
	{ ParticleEffect::PARTICLE_BOSS_EXPLOSION, "particles\\BossExplosion.xml" },
	{ ParticleEffect::PARTICLE_SCREEN_FLASH, "particles\\ScreenFlash.xml" },
	{ ParticleEffect::PARTICLE_TROPHY_SPARKLE, "particles\\TrophySparkle.xml" },
	{ ParticleEffect::PARTICLE_PORTAL_CIRCLE, "particles\\PortalCircle.xml" },
	{ ParticleEffect::PARTICLE_PORTAL_SQUARE, "particles\\PortalSquare.xml" },
	{ ParticleEffect::PARTICLE_POTTED_PLANT_GLOW, "particles\\PottedPlantGlow.xml" },
	{ ParticleEffect::PARTICLE_POTTED_WATER_PLANT_GLOW, "particles\\PottedWaterPlantGlow.xml" },
	{ ParticleEffect::PARTICLE_POTTED_ZEN_GLOW, "particles\\PottedZenGlow.xml" },
	{ ParticleEffect::PARTICLE_MIND_CONTROL, "particles\\MindControl.xml" },
	{ ParticleEffect::PARTICLE_VASE_SHATTER, "particles\\VaseShatter.xml" },
	{ ParticleEffect::PARTICLE_VASE_SHATTER_LEAF, "particles\\VaseShatterLeaf.xml" },
	{ ParticleEffect::PARTICLE_VASE_SHATTER_ZOMBIE, "particles\\VaseShatterZombie.xml" },
	{ ParticleEffect::PARTICLE_AWARD_PICKUP_ARROW, "particles\\AwardPickupArrow.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_SEAWEED, "particles\\Zombie_seaweed.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_MUSTACHE, "particles\\ZombieMustache.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_SUNGLASS, "particles\\ZombieFutureGlasses.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_PINATA, "particles\\Pinata.xml" },
	{ ParticleEffect::PARTICLE_DUST_SQUASH, "particles\\Dust_Squash.xml" },
	{ ParticleEffect::PARTICLE_DUST_FOOT, "particles\\Dust_Foot.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_DAISIES, "particles\\Daisy.xml" },
	{ ParticleEffect::PARTICLE_CREDIT_STROBE, "particles\\Credits_Strobe.xml" },
	{ ParticleEffect::PARTICLE_CREDITS_RAYSWIPE, "particles\\Credits_RaysWipe.xml" },
	{ ParticleEffect::PARTICLE_CREDITS_ZOMBIEHEADWIPE, "particles\\Credits_ZombieHeadWipe.xml" },
	{ ParticleEffect::PARTICLE_STARBURST, "particles\\Starburst.xml" },
	{ ParticleEffect::PARTICLE_CREDITS_FOG, "particles\\Credits_fog.xml" },
	{ ParticleEffect::PARTICLE_PERSENT_PICK_UP_ARROW, "particles\\UpsellArrow.xml" },
};  // 0x6A0FF0

ReanimationParams gLawnReanimationArray[(int)ReanimationType::NUM_REANIMS] = { //0x6A1340
	{ ReanimationType::REANIM_LOADBAR_SPROUT,                       "reanim\\LoadBar_sprout.reanim",                    1 },
	{ ReanimationType::REANIM_LOADBAR_ZOMBIEHEAD,                   "reanim\\LoadBar_Zombiehead.reanim",                1 },
	{ ReanimationType::REANIM_SODROLL,                              "reanim\\SodRoll.reanim",                           0 },
	{ ReanimationType::REANIM_FINAL_WAVE,                           "reanim\\FinalWave.reanim",                         1 },
	{ ReanimationType::REANIM_PEASHOOTER,                           "reanim\\PeaShooterSingle.reanim",                  0 },
	{ ReanimationType::REANIM_WALLNUT,                              "reanim\\Wallnut.reanim",                           0 },
	{ ReanimationType::REANIM_LILYPAD,                              "reanim\\Lilypad.reanim",                           0 },
	{ ReanimationType::REANIM_SUNFLOWER,                            "reanim\\SunFlower.reanim",                         0 },
	{ ReanimationType::REANIM_LAWNMOWER,                            "reanim\\LawnMower.reanim",                         0 },
	{ ReanimationType::REANIM_READYSETPLANT,                        "reanim\\StartReadySetPlant.reanim",                1 },
	{ ReanimationType::REANIM_CHERRYBOMB,                           "reanim\\CherryBomb.reanim",                        0 },
	{ ReanimationType::REANIM_SQUASH,                               "reanim\\Squash.reanim",                            0 },
	{ ReanimationType::REANIM_DOOMSHROOM,                           "reanim\\DoomShroom.reanim",                        0 },
	{ ReanimationType::REANIM_SNOWPEA,                              "reanim\\SnowPea.reanim",                           0 },
	{ ReanimationType::REANIM_REPEATER,                             "reanim\\PeaShooter.reanim",                        0 },
	{ ReanimationType::REANIM_SUNSHROOM,                            "reanim\\SunShroom.reanim",                         0 },
	{ ReanimationType::REANIM_TALLNUT,                              "reanim\\Tallnut.reanim",                           0 },
	{ ReanimationType::REANIM_FUMESHROOM,                           "reanim\\Fumeshroom.reanim",                        0 },
	{ ReanimationType::REANIM_PUFFSHROOM,                           "reanim\\Puffshroom.reanim",                        0 },
	{ ReanimationType::REANIM_HYPNOSHROOM,                          "reanim\\Hypnoshroom.reanim",                       0 },
	{ ReanimationType::REANIM_CHOMPER,                              "reanim\\Chomper.reanim",                           0 },
	{ ReanimationType::REANIM_ZOMBIE,                               "reanim\\Zombie.reanim",                            0 },
	{ ReanimationType::REANIM_SUN,                                  "reanim\\Sun.reanim",                               0 },
	{ ReanimationType::REANIM_POTATOMINE,                           "reanim\\PotatoMine.reanim",                        0 },
	{ ReanimationType::REANIM_SPIKEWEED,                            "reanim\\Caltrop.reanim",                           0 },
	{ ReanimationType::REANIM_SPIKEROCK,                            "reanim\\SpikeRock.reanim",                         0 },
	{ ReanimationType::REANIM_THREEPEATER,                          "reanim\\ThreePeater.reanim",                       0 },
	{ ReanimationType::REANIM_MARIGOLD,                             "reanim\\Marigold.reanim",                          0 },
	{ ReanimationType::REANIM_ICESHROOM,                            "reanim\\IceShroom.reanim",                         0 },
	{ ReanimationType::REANIM_ZOMBIE_FOOTBALL,                      "reanim\\Zombie_football.reanim",                   0 },
	{ ReanimationType::REANIM_ZOMBIE_NEWSPAPER,                     "reanim\\Zombie_paper.reanim",                      0 },
	{ ReanimationType::REANIM_ZOMBIE_ZAMBONI,                       "reanim\\Zombie_zamboni.reanim",                    0 },
	{ ReanimationType::REANIM_SPLASH,                               "reanim\\splash.reanim",                            0 },
	{ ReanimationType::REANIM_JALAPENO,                             "reanim\\Jalapeno.reanim",                          0 },
	{ ReanimationType::REANIM_JALAPENO_FIRE,                        "reanim\\fire.reanim",                              0 },
	{ ReanimationType::REANIM_COIN_SILVER,                          "reanim\\Coin_silver.reanim",                       0 },
	{ ReanimationType::REANIM_ZOMBIE_CHARRED,                       "reanim\\Zombie_charred.reanim",                    0 },
	{ ReanimationType::REANIM_ZOMBIE_CHARRED_IMP,                   "reanim\\Zombie_charred_imp.reanim",                0 },
	{ ReanimationType::REANIM_ZOMBIE_CHARRED_DIGGER,                "reanim\\Zombie_charred_digger.reanim",             0 },
	{ ReanimationType::REANIM_ZOMBIE_CHARRED_ZAMBONI,               "reanim\\Zombie_charred_zamboni.reanim",            0 },
	{ ReanimationType::REANIM_ZOMBIE_CHARRED_CATAPULT,              "reanim\\Zombie_charred_catapult.reanim",           0 },
	{ ReanimationType::REANIM_ZOMBIE_CHARRED_GARGANTUAR,            "reanim\\Zombie_charred_gargantuar.reanim",         0 },
	{ ReanimationType::REANIM_SCRAREYSHROOM,                        "reanim\\ScaredyShroom.reanim",                     0 },
	{ ReanimationType::REANIM_PUMPKIN,                              "reanim\\Pumpkin.reanim",                           0 },
	{ ReanimationType::REANIM_PLANTERN,                             "reanim\\Plantern.reanim",                          0 },
	{ ReanimationType::REANIM_TORCHWOOD,                            "reanim\\Torchwood.reanim",                         0 },
	{ ReanimationType::REANIM_SPLITPEA,                             "reanim\\SplitPea.reanim",                          0 },
	{ ReanimationType::REANIM_SEASHROOM,                            "reanim\\SeaShroom.reanim",                         0 },
	{ ReanimationType::REANIM_BLOVER,                               "reanim\\Blover.reanim",                            0 },
	{ ReanimationType::REANIM_FLOWER_POT,                           "reanim\\Pot.reanim",                               0 },
	{ ReanimationType::REANIM_CACTUS,                               "reanim\\Cactus.reanim",                            0 },
	{ ReanimationType::REANIM_DANCER,                               "reanim\\Zombie_Jackson.reanim",                    0 },
	{ ReanimationType::REANIM_TANGLEKELP,                           "reanim\\Tanglekelp.reanim",                        0 },
	{ ReanimationType::REANIM_STARFRUIT,                            "reanim\\Starfruit.reanim",                         0 },
	{ ReanimationType::REANIM_POLEVAULTER,                          "reanim\\Zombie_polevaulter.reanim",                0 },
	{ ReanimationType::REANIM_BALLOON,                              "reanim\\Zombie_balloon.reanim",                    0 },
	{ ReanimationType::REANIM_GARGANTUAR,                           "reanim\\Zombie_gargantuar.reanim",                 0 },
	{ ReanimationType::REANIM_IMP,                                  "reanim\\Zombie_imp.reanim",                        0 },
	{ ReanimationType::REANIM_DIGGER,                               "reanim\\Zombie_digger.reanim",                     0 },
	{ ReanimationType::REANIM_DIGGER_DIRT,                          "reanim\\Digger_rising_dirt.reanim",                0 },
	{ ReanimationType::REANIM_ZOMBIE_DOLPHINRIDER,                  "reanim\\Zombie_dolphinrider.reanim",               0 },
	{ ReanimationType::REANIM_POGO,                                 "reanim\\Zombie_pogo.reanim",                       0 },
	{ ReanimationType::REANIM_BACKUP_DANCER,                        "reanim\\Zombie_dancer.reanim",                     0 },
	{ ReanimationType::REANIM_BOBSLED,                              "reanim\\Zombie_bobsled.reanim",                    0 },
	{ ReanimationType::REANIM_JACKINTHEBOX,                         "reanim\\Zombie_jackbox.reanim",                    0 },
	{ ReanimationType::REANIM_SNORKEL,                              "reanim\\Zombie_snorkle.reanim",                    0 },
	{ ReanimationType::REANIM_BUNGEE,                               "reanim\\Zombie_bungi.reanim",                      0 },
	{ ReanimationType::REANIM_CATAPULT,                             "reanim\\Zombie_catapult.reanim",                   0 },
	{ ReanimationType::REANIM_LADDER,                               "reanim\\Zombie_ladder.reanim",                     0 },
	{ ReanimationType::REANIM_PUFF,                                 "reanim\\Puff.reanim",                              0 },
	{ ReanimationType::REANIM_SLEEPING,                             "reanim\\Z.reanim",                                 0 },
	{ ReanimationType::REANIM_GRAVE_BUSTER,                         "reanim\\Gravebuster.reanim",                       0 },
	{ ReanimationType::REANIM_ZOMBIES_WON,                          "reanim\\ZombiesWon.reanim",                        1 },
	{ ReanimationType::REANIM_MAGNETSHROOM,                         "reanim\\Magnetshroom.reanim",                      0 },
	{ ReanimationType::REANIM_BOSS,                                 "reanim\\Zombie_boss.reanim",                       0 },
	{ ReanimationType::REANIM_CABBAGEPULT,                          "reanim\\Cabbagepult.reanim",                       0 },
	{ ReanimationType::REANIM_KERNELPULT,                           "reanim\\Cornpult.reanim",                          0 },
	{ ReanimationType::REANIM_MELONPULT,                            "reanim\\Melonpult.reanim",                         0 },
	{ ReanimationType::REANIM_COFFEEBEAN,                           "reanim\\Coffeebean.reanim",                        1 },
	{ ReanimationType::REANIM_UMBRELLALEAF,                         "reanim\\Umbrellaleaf.reanim",                      0 },
	{ ReanimationType::REANIM_GATLINGPEA,                           "reanim\\GatlingPea.reanim",                        0 },
	{ ReanimationType::REANIM_CATTAIL,                              "reanim\\Cattail.reanim",                           0 },
	{ ReanimationType::REANIM_GLOOMSHROOM,                          "reanim\\GloomShroom.reanim",                       0 },
	{ ReanimationType::REANIM_BOSS_ICEBALL,                         "reanim\\Zombie_boss_iceball.reanim",               1 },
	{ ReanimationType::REANIM_BOSS_FIREBALL,                        "reanim\\Zombie_boss_fireball.reanim",              1 },
	{ ReanimationType::REANIM_COBCANNON,                            "reanim\\CobCannon.reanim",                         0 },
	{ ReanimationType::REANIM_GARLIC,                               "reanim\\Garlic.reanim",                            0 },
	{ ReanimationType::REANIM_GOLD_MAGNET,                          "reanim\\GoldMagnet.reanim",                        0 },
	{ ReanimationType::REANIM_WINTER_MELON,                         "reanim\\WinterMelon.reanim",                       0 },
	{ ReanimationType::REANIM_TWIN_SUNFLOWER,                       "reanim\\TwinSunflower.reanim",                     0 },
	{ ReanimationType::REANIM_POOL_CLEANER,                         "reanim\\PoolCleaner.reanim",                       0 },
	{ ReanimationType::REANIM_ROOF_CLEANER,                         "reanim\\RoofCleaner.reanim",                       0 },
	{ ReanimationType::REANIM_FIRE_PEA,                             "reanim\\FirePea.reanim",                           0 },
	{ ReanimationType::REANIM_IMITATER,                             "reanim\\Imitater.reanim",                          0 },
	{ ReanimationType::REANIM_YETI,                                 "reanim\\Zombie_yeti.reanim",                       0 },
	{ ReanimationType::REANIM_BOSS_DRIVER,                          "reanim\\Zombie_Boss_driver.reanim",                0 },
	{ ReanimationType::REANIM_LAWN_MOWERED_ZOMBIE,                  "reanim\\LawnMoweredZombie.reanim",                 0 },
	{ ReanimationType::REANIM_CRAZY_DAVE,                           "reanim\\CrazyDave.reanim",                         1 },
	{ ReanimationType::REANIM_TEXT_FADE_ON,                         "reanim\\TextFadeOn.reanim",                        0 },
	{ ReanimationType::REANIM_HAMMER,                               "reanim\\Hammer.reanim",                            0 },
	{ ReanimationType::REANIM_SLOT_MACHINE_HANDLE,                  "reanim\\SlotMachine.reanim",                       0 },
	{ ReanimationType::REANIM_CREDITS_FOOTBALL,                     "reanim\\Credits_Football.reanim",                  1 },
	{ ReanimationType::REANIM_CREDITS_JACKBOX,                      "reanim\\Credits_Jackbox.reanim",                   1 },
	{ ReanimationType::REANIM_SELECTOR_SCREEN,                      "reanim\\SelectorScreen.reanim",                    3 },
	{ ReanimationType::REANIM_PORTAL_CIRCLE,                        "reanim\\Portal_Circle.reanim",                     0 },
	{ ReanimationType::REANIM_PORTAL_SQUARE,                        "reanim\\Portal_Square.reanim",                     0 },
	{ ReanimationType::REANIM_ZENGARDEN_SPROUT,                     "reanim\\ZenGarden_sprout.reanim",                  0 },
	{ ReanimationType::REANIM_ZENGARDEN_WATERINGCAN,                "reanim\\ZenGarden_wateringcan.reanim",             1 },
	{ ReanimationType::REANIM_ZENGARDEN_FERTILIZER,                 "reanim\\ZenGarden_fertilizer.reanim",              1 },
	{ ReanimationType::REANIM_ZENGARDEN_BUGSPRAY,                   "reanim\\ZenGarden_bugspray.reanim",                1 },
	{ ReanimationType::REANIM_ZENGARDEN_PHONOGRAPH,                 "reanim\\ZenGarden_phonograph.reanim",              1 },
	{ ReanimationType::REANIM_DIAMOND,                              "reanim\\Diamond.reanim",                           0 },
	{ ReanimationType::REANIM_ZOMBIE_HAND,                          "reanim\\Zombie_hand.reanim",                       1 },
	{ ReanimationType::REANIM_STINKY,                               "reanim\\Stinky.reanim",                            0 },
	{ ReanimationType::REANIM_RAKE,                                 "reanim\\Rake.reanim",                              0 },
	{ ReanimationType::REANIM_RAIN_CIRCLE,                          "reanim\\Rain_circle.reanim",                       0 },
	{ ReanimationType::REANIM_RAIN_SPLASH,                          "reanim\\Rain_splash.reanim",                       0 },
	{ ReanimationType::REANIM_ZOMBIE_SURPRISE,                      "reanim\\Zombie_surprise.reanim",                   0 },
	{ ReanimationType::REANIM_COIN_GOLD,                            "reanim\\Coin_gold.reanim",                         0 },
	{ ReanimationType::REANIM_TREEOFWISDOM,                         "reanim\\TreeOfWisdom.reanim",                      1 },
	{ ReanimationType::REANIM_TREEOFWISDOM_CLOUDS,                  "reanim\\TreeOfWisdomClouds.reanim",                1 },
	{ ReanimationType::REANIM_TREEOFWISDOM_TREEFOOD,                "reanim\\TreeFood.reanim",                          1 },
	{ ReanimationType::REANIM_CREDITS_MAIN,                         "reanim\\Credits_Main.reanim",                      3 },
	{ ReanimationType::REANIM_CREDITS_MAIN2,                        "reanim\\Credits_Main2.reanim",                     3 },
	{ ReanimationType::REANIM_CREDITS_MAIN3,                        "reanim\\Credits_Main3.reanim",                     3 },
	{ ReanimationType::REANIM_ZOMBIE_CREDITS_DANCE,                 "reanim\\Zombie_credits_dance.reanim",              0 },
	{ ReanimationType::REANIM_CREDITS_STAGE,                        "reanim\\Credits_stage.reanim",                     1 },
	{ ReanimationType::REANIM_CREDITS_BIGBRAIN,                     "reanim\\Credits_BigBrain.reanim",                  1 },
	{ ReanimationType::REANIM_CREDITS_FLOWER_PETALS,                "reanim\\Credits_Flower_petals.reanim",             1 },
	{ ReanimationType::REANIM_CREDITS_INFANTRY,                     "reanim\\Credits_Infantry.reanim",                  1 },
	{ ReanimationType::REANIM_CREDITS_THROAT,                       "reanim\\Credits_Throat.reanim",                    1 },
	{ ReanimationType::REANIM_CREDITS_CRAZYDAVE,                    "reanim\\Credits_CrazyDave.reanim",                 1 },
	{ ReanimationType::REANIM_CREDITS_BOSSDANCE,                    "reanim\\Credits_Bossdance.reanim",                 1 },
	{ ReanimationType::REANIM_ZOMBIE_CREDITS_SCREEN_DOOR,           "reanim\\Zombie_Credits_Screendoor.reanim",         1 },
	{ ReanimationType::REANIM_ZOMBIE_CREDITS_CONEHEAD,              "reanim\\Zombie_Credits_Conehead.reanim",           1 },
	{ ReanimationType::REANIM_CREDITS_ZOMBIEARMY1,                  "reanim\\Credits_ZombieArmy1.reanim",               1 },
	{ ReanimationType::REANIM_CREDITS_ZOMBIEARMY2,                  "reanim\\Credits_ZombieArmy2.reanim",               1 },
	{ ReanimationType::REANIM_CREDITS_TOMBSTONES,                   "reanim\\Credits_Tombstones.reanim",                1 },
	{ ReanimationType::REANIM_CREDITS_SOLARPOWER,                   "reanim\\Credits_SolarPower.reanim",                1 },
	{ ReanimationType::REANIM_CREDITS_ANYHOUR,                      "reanim\\Credits_Anyhour.reanim",                   3 },
	{ ReanimationType::REANIM_CREDITS_WEARETHEUNDEAD,               "reanim\\Credits_WeAreTheUndead.reanim",            1 },
	{ ReanimationType::REANIM_CREDITS_DISCOLIGHTS,                  "reanim\\Credits_DiscoLights.reanim",               1 },
	{ ReanimationType::REANIM_FLAG,                                 "reanim\\Zombie_FlagPole.reanim",                   0 },
};

TrailParams gLawnTrailArray[(int)TrailType::NUM_TRAILS] = { //0x6A19F4
	{ TrailType::TRAIL_ICE, "particles\\IceTrail.trail" }
};

int gLawnStringFormatCount = 12;
TodStringListFormat gLawnStringFormats[12] = {    //0x6A5010
	{ _S("NORMAL"),           nullptr,    Color(40,   50,     90,     255),       0,      0U },
	{ _S("FLAVOR"),           nullptr,    Color(143,  67,     27,     255),       0,      1U },
	{ _S("KEYWORD"),          nullptr,    Color(143,  67,     27,     255),       0,      0U },
	{ _S("NOCTURNAL"),        nullptr,    Color(136,  50,     170,    255),       0,      0U },
	{ _S("AQUATIC"),          nullptr,    Color(11,   161,    219,    255),       0,      0U },
	{ _S("STAT"),             nullptr,    Color(204,  36,     29,     255),       0,      0U },
	{ _S("METAL"),            nullptr,    Color(204,  36,     29,     255),       0,      2U },
	{ _S("KEYMETAL"),         nullptr,    Color(143,  67,     27,     255),       0,      2U },
	{ _S("SHORTLINE"),        nullptr,    Color(0,    0,      0,      0),         -9,     0U },
	{ _S("EXTRASHORTLINE"),   nullptr,    Color(0,    0,      0,      0),         -14,    0U },
	{ _S("CREDITS1"),         nullptr,    Color(0,    0,      0,      0),         3,      0U },
	{ _S("CREDITS2"),         nullptr,    Color(0,    0,      0,      0),         2,      0U }
};

//0x44E8A0
bool LawnGetCloseRequest()
{
	if (gLawnApp == nullptr)
		return false;

	return gLawnApp->mCloseRequest;
}

//0x44E8C0
bool LawnHasUsedCheatKeys()
{
	return gLawnApp && gLawnApp->mPlayerInfo && gLawnApp->mPlayerInfo->mHasUsedCheatKeys;
}

//0x44E8F0
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
	// As early as possible: anything that fails after this point gets recorded.
	// PvzDebugInit also installs the crash handler, so faults during startup are
	// caught too (previously the only evidence was a bare crash.txt).
	PvzDebugInit(nullptr);

	TodStringListSetColors(gLawnStringFormats, gLawnStringFormatCount);
	gGetCurrentLevelName = LawnGetCurrentLevelName;
	gAppCloseRequest = LawnGetCloseRequest;
	gAppHasUsedCheatKeys = LawnHasUsedCheatKeys;
	gExtractResourcesByName = Sexy::ExtractResourcesByName;

	gLawnApp = new LawnApp();
	gLawnApp->mChangeDirTo = (!Sexy::FileExists("properties\\resources.xml") && Sexy::FileExists("..\\properties\\resources.xml")) ? ".." : ".";

	gLawnApp->Init();
	gLawnApp->Start();

	gLawnApp->Shutdown();
	delete gLawnApp;

	PvzDebugShutdown();

	return 0;
};

//0x44EAA0
LawnApp::LawnApp()
{
	mBoard = nullptr;
	mGameSelector = nullptr;
	mChallengeScreen = nullptr;
	mSeedChooserScreen = nullptr;
	mAwardScreen = nullptr;
	mCreditScreen = nullptr;
	mTitleScreen = nullptr;
	mSoundSystem = nullptr;
	mKonamiCheck = nullptr;
	mMustacheCheck = nullptr;
	mMoustacheCheck = nullptr;
	mSuperMowerCheck = nullptr;
	mSuperMowerCheck2 = nullptr;
	mFutureCheck = nullptr;
	mPinataCheck = nullptr;
	mDanceCheck = nullptr;
	mDaisyCheck = nullptr;
	mSukhbirCheck = nullptr;
	mMustacheMode = false;
	mSuperMowerMode = false;
	mFutureMode = false;
	mPinataMode = false;
	mDanceMode = false;
	mDaisyMode = false;
	mSukhbirMode = false;
	mGameScene = GameScenes::SCENE_LOADING;
	mPoolEffect = nullptr;
	mZenGarden = nullptr;
	mEffectSystem = nullptr;
	mReanimatorCache = nullptr;
	mCloseRequest = false;
	mWidth = BOARD_WIDTH;
	mHeight = BOARD_HEIGHT;
	mFullscreenBits = 32;
	mAppCounter = 0;
	mAppRandSeed = _time64(nullptr);
	mTrialType = TrialType::TRIALTYPE_NONE;
	mDebugTrialLocked = false;
	mMuteSoundsForCutscene = false;
	mMusicVolume = 0.85;
	mSfxVolume = 0.5525;
	mAutoStartLoadingThread = false;
	mDebugKeysEnabled = false;
	mRegKey = "PopCap\\PlantsVsZombies";
	mProdName = "PlantsVsZombies";
	std::string aTitleName = "Plants vs. Zombies";
#ifdef _DEBUG
	aTitleName += " BETA ";
	aTitleName += mProductVersion;
#endif
	mTitle = StringToSexyStringFast(aTitleName);
	mCustomCursorsEnabled = false;
	mPlayerInfo = nullptr;
	mLastLevelStats = new LevelStats();
	mFirstTimeGameSelector = true;
	mGameMode = GameMode::GAMEMODE_ADVENTURE;
	mEasyPlantingCheat = false;
	mAutoEnable3D = true;
	Tod_SWTri_AddAllDrawTriFuncs();
	mLoadingZombiesThreadCompleted = true;
	mGamesPlayed = 0;
	mMaxExecutions = 0;
	mMaxPlays = 0;
	mMaxTime = 0;
	mCompletedLoadingThreadTasks = 0;
	mProfileMgr = new ProfileMgr();
	mRegisterResourcesLoaded = false;
	mTodCheatKeys = false;
	mCrazyDaveReanimID = ReanimationID::REANIMATIONID_NULL;
	mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_OFF;
	mCrazyDaveBlinkCounter = 0;
	mCrazyDaveBlinkReanimID = ReanimationID::REANIMATIONID_NULL;
	mCrazyDaveMessageIndex = -1;
	mBigArrowCursor = LoadCursor(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDC_CURSOR1));
	mDRM = nullptr;
}

//0x44EDD00x44EDF0
LawnApp::~LawnApp()
{
	if (mBoard)
	{
		WriteCurrentUserConfig();
	}

	if (mBoard)
	{
		mBoardResult = BoardResult::BOARDRESULT_QUIT_APP;
		mBoard->TryToSaveGame();
		mWidgetManager->RemoveWidget(mBoard);
		delete mBoard;
		mBoard = nullptr;
	}

	if (mTitleScreen)
	{
		mWidgetManager->RemoveWidget(mTitleScreen);
		delete mTitleScreen;
	}

	delete mSoundSystem;
	delete mMusic;

	if (mKonamiCheck)
	{
		delete mKonamiCheck;
	}
	if (mMustacheCheck)
	{
		delete mMustacheCheck;
	}
	if (mMoustacheCheck)
	{
		delete mMoustacheCheck;
	}
	if (mSuperMowerCheck)
	{
		delete mSuperMowerCheck;
	}
	if (mSuperMowerCheck2)
	{
		delete mSuperMowerCheck2;
	}
	if (mFutureCheck)
	{
		delete mFutureCheck;
	}
	if (mPinataCheck)
	{
		delete mPinataCheck;
	}
	if (mDanceCheck)
	{
		delete mDanceCheck;
	}
	if (mDaisyCheck)
	{
		delete mDaisyCheck;
	}
	if (mSukhbirCheck)
	{
		delete mSukhbirCheck;
	}


	if (mGameSelector)
	{
		mWidgetManager->RemoveWidget(mGameSelector);
		delete mGameSelector;
	}
	if (mChallengeScreen)
	{
		mWidgetManager->RemoveWidget(mChallengeScreen);
		delete mChallengeScreen;
	}
	if (mSeedChooserScreen)
	{
		mWidgetManager->RemoveWidget(mSeedChooserScreen);
		delete mSeedChooserScreen;
	}
	if (mAwardScreen)
	{
		mWidgetManager->RemoveWidget(mAwardScreen);
		delete mAwardScreen;
	}
	if (mCreditScreen)
	{
		mWidgetManager->RemoveWidget(mCreditScreen);
		delete mCreditScreen;
	}

	delete mProfileMgr;
	delete mLastLevelStats;

	mResourceManager->DeleteResources("");
#ifdef _DEBUG
	BetaSubmit(true);
#endif
}

//0x44F200
void LawnApp::Shutdown()
{
    gTfPhase = "shutdown";
    TfLogWrite("Shutdown called (loadingThreadCompleted=%d)", (int)mLoadingThreadCompleted);
	if (!mLoadingThreadCompleted)
	{
		mLoadingFailed = true;
		return;
	}

	if (!mShutdown)
	{
		for (int i = 0; i < Dialogs::NUM_DIALOGS; i++)
		{
			KillDialog(i);
		}

		if (mBoard)
		{
			mBoardResult = BoardResult::BOARDRESULT_QUIT_APP;
			mBoard->TryToSaveGame();
			KillBoard();
			WriteCurrentUserConfig();
		}

		ProcessSafeDeleteList();

		if (mPoolEffect)
		{
			mPoolEffect->PoolEffectDispose();
			delete mPoolEffect;
			mPoolEffect = nullptr;
		}

		if (mZenGarden)
		{
			delete mZenGarden;
			mZenGarden = nullptr;
		}

		if (mEffectSystem)
		{
			mEffectSystem->EffectSystemDispose();
			delete mEffectSystem;
			mEffectSystem = nullptr;
		}

		if (mReanimatorCache)
		{
			mReanimatorCache->ReanimatorCacheDispose();
			delete mReanimatorCache;
			mReanimatorCache = nullptr;
		}

		FilterEffectDisposeForApp();
		TodParticleFreeDefinitions();
		ReanimatorFreeDefinitions();
		TrailFreeDefinitions();
		FreeGlobalAllocators();
		UpdateRegisterInfo();
		SexyAppBase::Shutdown();

		if (mDRM)
		{
			delete mDRM;
		}
		mDRM = nullptr;
	}
}

//0x44F380
void LawnApp::KillBoard()
{
	FinishModelessDialogs();
	KillSeedChooserScreen();
	if (mBoard)
	{
#ifdef _DEBUG
		BetaRecordLevelStats();
#endif
		mBoard->DisposeBoard();
		mWidgetManager->RemoveWidget(mBoard);
		SafeDeleteWidget(mBoard);
		mBoard = nullptr;
	}

	SetCursor(CURSOR_POINTER);
}

//0x44F410
bool LawnApp::CanPauseNow()
{
	if (mBoard == nullptr)
		return false;

	if (mSeedChooserScreen && mSeedChooserScreen->mMouseVisible)
		return false;

	if (mBoard->mBoardFadeOutCounter >= 0)
		return false;

	if (mCrazyDaveState != CrazyDaveState::CRAZY_DAVE_OFF)
		return false;

	if (mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN || mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
		return false;

	return GetDialogCount() <= 0;
}

void LawnApp::GotFocus()
{
}

//0x44F460
void LawnApp::LostFocus()
{
    if (mGameMode == GameMode::GAMEMODE_THIRTY_FLAGS) return;   // v5: 失焦不弹暂停框（用户要求去掉）
	if (!mTodCheatKeys && CanPauseNow())
	{
		DoPauseDialog();
	}
}

//0x44F480
void LawnApp::WriteToRegistry()
{
	if (mPlayerInfo)
	{
		RegistryWriteString("CurUser", SexyStringToStringFast(mPlayerInfo->mName));
		mPlayerInfo->SaveDetails();
	}

	SexyAppBase::WriteToRegistry();
}

//0x44F530
void LawnApp::ReadFromRegistry()
{
	SexyApp::ReadFromRegistry();
}

//0x44F540
bool LawnApp::WriteCurrentUserConfig()
{
	if (mPlayerInfo)
		mPlayerInfo->SaveDetails();

	return true;
}

//0x44F560
void LawnApp::PreNewGame(GameMode theGameMode, bool theLookForSavedGame)
{
    gTfPhase = "PreNewGame";
    TfLogWrite("PreNewGame mode=%d", (int)theGameMode);
	//if (NeedRegister())
	//{
	//	ShowGameSelector();
	//	return;
	//}

	mGameMode = theGameMode;
	if (theLookForSavedGame && TryLoadGame())
		return;

	std::string aFileName = GetSavedGameName(mGameMode, mPlayerInfo->mId);
	EraseFile(aFileName);
	NewGame();
}

//0x44F5F0
void LawnApp::MakeNewBoard()
{
    gTfPhase = "MakeNewBoard";
    TfLogWrite("MakeNewBoard mode=%d", (int)mGameMode);
	KillBoard();
	mBoard = new Board(this);
	mBoard->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mBoard);
	mWidgetManager->BringToBack(mBoard);
	mWidgetManager->SetFocus(mBoard);
}

//0x44F6B0
void LawnApp::StartPlaying()
{
    gTfPhase = "play";
    TfLogWrite("StartPlaying");
	KillSeedChooserScreen();
	mBoard->StartLevel();
	mGameScene = GameScenes::SCENE_PLAYING;

	// 【三十旗】关卡内「下一旗预告」（策划案「交付边界」）：
	//   每面旗开局时用原版提示条播报本面旗的主题 / 强度 / 地形，让玩家在换旗前就知道要面对什么。
	//   主题分段与 ThirtyFlags.cpp 的 gThirtyFlagsFlagDefs.mTheme 一致；该字段是窄字符字面量，
	//   宽字符构建（_USE_WIDE_STRING）下不能直接显示，故此处用 _S 宽字面量复述。
	if (ThirtyFlagsMode() && mBoard != nullptr && mBoard->mChallenge != nullptr)
	{
		int aFlag = ThirtyFlagsCurrentFlag(mBoard);
		const ThirtyFlagsFlagDef& aDef = ThirtyFlags::GetFlagDef(aFlag);

		const SexyChar* aTheme = _S("独木难支");
		if (aFlag >= TF_BOSS_FLAG)		aTheme = _S("僵王终战");
		else if (aFlag > 24)			aTheme = _S("双方超模");
		else if (aFlag > 19)			aTheme = _S("双向水路");
		else if (aFlag > 17)			aTheme = _S("水路登场");
		else if (aFlag > 12)			aTheme = _S("四线铺开");
		else if (aFlag > 7)			aTheme = _S("三线成形");
		else if (aFlag > 3)			aTheme = _S("场地打开");

		SexyString aPreview = StrFormat(_S("%s · 第 %d/%d 面旗 · 强度 %d"), aTheme, aFlag, TF_TOTAL_FLAGS, aDef.mZombiePoints);
		if (aDef.mWaterRows != 0)
			aPreview += _S(" · 水路");
		if (aDef.mFogCols > 0)
			aPreview += _S(" · 迷雾");

		mBoard->DisplayAdvice(aPreview, MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_NONE);
	}
}

//0x44F700
bool LawnApp::SaveFileExists()
{
	std::string aFileName = GetSavedGameName(GameMode::GAMEMODE_ADVENTURE, mPlayerInfo->mId);
	return this->FileExists(aFileName);
}

//0x44F7A0
bool LawnApp::TryLoadGame()
{
	std::string aSaveName = GetSavedGameName(mGameMode, mPlayerInfo->mId);
	mMusic->StopAllMusic();

	if (this->FileExists(aSaveName))
	{
		MakeNewBoard();
		if (mBoard->LoadGame(aSaveName))
		{
			mFirstTimeGameSelector = false;
			DoContinueDialog();
			return true;
		}

		KillBoard();
	}

	return false;
}

//0x44F890
void LawnApp::NewGame()
{
	mFirstTimeGameSelector = false;

	MakeNewBoard();
	mBoard->InitLevel();
	mBoardResult = BoardResult::BOARDRESULT_NONE;
	mGameScene = GameScenes::SCENE_LEVEL_INTRO;

	ShowSeedChooserScreen();
	mBoard->mCutScene->StartLevelIntro();
}

//0x44F8E0
void LawnApp::ShowGameSelector()
{
	KillBoard();
	//UpdateRegisterInfo();
	if (mGameSelector)
	{
		mWidgetManager->RemoveWidget(mGameSelector);
		SafeDeleteWidget(mGameSelector);
	}

	mGameScene = GameScenes::SCENE_MENU;
	mGameSelector = new GameSelector(this);
	mGameSelector->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mGameSelector);
	mWidgetManager->BringToBack(mGameSelector);
	mWidgetManager->SetFocus(mGameSelector);

	//if (NeedRegister())
	//{
	//	DoNeedRegisterDialog();
	//}
}

//0x44F9E0
void LawnApp::KillGameSelector()
{
	if (mGameSelector)
	{
		mWidgetManager->RemoveWidget(mGameSelector);
		SafeDeleteWidget(mGameSelector);
		mGameSelector = nullptr;
	}
}

//0x44FA20
void LawnApp::ShowAwardScreen(AwardType theAwardType)
{
	mGameScene = GameScenes::SCENE_AWARD;
	mAwardScreen = new AwardScreen(this, theAwardType);
	mAwardScreen->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mAwardScreen);
	mWidgetManager->BringToBack(mAwardScreen);
	mWidgetManager->SetFocus(mAwardScreen);
}

//0x44FAF0
void LawnApp::KillAwardScreen()
{
	if (mAwardScreen)
	{
		mWidgetManager->RemoveWidget(mAwardScreen);
		SafeDeleteWidget(mAwardScreen);
		mAwardScreen = nullptr;
	}
}

//0x44FB30
void LawnApp::ShowCreditScreen()
{
	mCreditScreen = new CreditScreen(this);
	mCreditScreen->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mCreditScreen);
	mWidgetManager->BringToBack(mCreditScreen);
	mWidgetManager->SetFocus(mCreditScreen);
}

//0x44FBF0
void LawnApp::KillCreditScreen()
{
	if (mCreditScreen)
	{
		mWidgetManager->RemoveWidget(mCreditScreen);
		SafeDeleteWidget(mCreditScreen);
		mCreditScreen = nullptr;
	}
}

//0x44FC30
void LawnApp::ShowChallengeScreen(ChallengePage thePage)
{
	mGameScene = GameScenes::SCENE_CHALLENGE;
	mChallengeScreen = new ChallengeScreen(this, thePage);
	mChallengeScreen->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mChallengeScreen);
	mWidgetManager->BringToBack(mChallengeScreen);
	mWidgetManager->SetFocus(mChallengeScreen);
}

//0x44FD00
void LawnApp::KillChallengeScreen()
{
	if (mChallengeScreen)
	{
		mWidgetManager->RemoveWidget(mChallengeScreen);
		SafeDeleteWidget(mChallengeScreen);
		mChallengeScreen = nullptr;
	}
}

//0x44FD40
StoreScreen* LawnApp::ShowStoreScreen()
{
	//FinishModelessDialogs();
	TOD_ASSERT(!GetDialog((int)Dialogs::DIALOG_STORE));

	StoreScreen* aStoreScreen = new StoreScreen(this);
	AddDialog(aStoreScreen);
	mWidgetManager->SetFocus(aStoreScreen);

	return aStoreScreen;
}

void LawnApp::KillStoreScreen()
{
	if (GetDialog(Dialogs::DIALOG_STORE))
	{
		KillDialog(Dialogs::DIALOG_STORE);
		ClearUpdateBacklog(false);
	}
}

//0x44FDC0
void LawnApp::ShowSeedChooserScreen()
{
	TOD_ASSERT(mSeedChooserScreen == nullptr);

	mSeedChooserScreen = new SeedChooserScreen();
	mSeedChooserScreen->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mSeedChooserScreen);
	mWidgetManager->BringToBack(mSeedChooserScreen);
}

//0x44FE70
void LawnApp::KillSeedChooserScreen()
{
	if (mSeedChooserScreen)
	{
		mWidgetManager->RemoveWidget(mSeedChooserScreen);
		SafeDeleteWidget(mSeedChooserScreen);
		mSeedChooserScreen = nullptr;
	}
}

void LawnApp::EndLevel()
{
	KillBoard();
	if (IsAdventureMode())
	{
		NewGame();
	}

	NewGame();
}

//0x44FEB0
void LawnApp::DoBackToMain()
{
	mMusic->StopAllMusic();
	mSoundSystem->CancelPausedFoley();
	WriteCurrentUserConfig();
	KillNewOptionsDialog();
	KillBoard();
	ShowGameSelector();
}

//0x44FF00
void LawnApp::DoConfirmBackToMain()
{
	LawnDialog* aDialog = (LawnDialog*)DoDialog(
		Dialogs::DIALOG_CONFIRM_BACK_TO_MAIN,
		true,
		_S("[LEAVE_GAME_HEADER]"/*"Leave Game?"*/),
		_S("[LEAVE_GAME]"/*"Do you want to return\nto the main menu?\n\nYour game will be saved."*/),
		_S(""),
		Dialog::BUTTONS_YES_NO
	);

	aDialog->mLawnYesButton->mLabel = TodStringTranslate(_S("[LEAVE_BUTTON]"));
	aDialog->mLawnNoButton->mLabel = TodStringTranslate(_S("[DIALOG_BUTTON_CANCEL]"));
	//aDialog->CalcSize(0, 0);
}

//0x4500D0
void LawnApp::DoNewOptions(bool theFromGameSelector)
{
	//FinishModelessDialogs();

	NewOptionsDialog* aDialog = new NewOptionsDialog(this, theFromGameSelector);
	CenterDialog(aDialog, IMAGE_OPTIONS_MENUBACK->mWidth, IMAGE_OPTIONS_MENUBACK->mHeight);
	AddDialog(Dialogs::DIALOG_NEWOPTIONS, aDialog);
	mWidgetManager->SetFocus(aDialog);
}

//0x450180
AlmanacDialog* LawnApp::DoAlmanacDialog(SeedType theSeedType, ZombieType theZombieType)
{
	PerfTimer mTimer;
	mTimer.Start();

	//FinishModelessDialogs();

	AlmanacDialog* aDialog = new AlmanacDialog(this);
	AddDialog(Dialogs::DIALOG_ALMANAC, aDialog);
	mWidgetManager->SetFocus(aDialog);

	if (theSeedType != SeedType::SEED_NONE)
	{
		aDialog->ShowPlant(theSeedType);
	}
	else if (theZombieType != ZombieType::ZOMBIE_INVALID)
	{
		aDialog->ShowZombie(theZombieType);
	}

	int aDuration = mTimer.GetDuration();
	TodTrace("almanac load time: %d ms", aDuration);

	return aDialog;
}

//0x450220
void LawnApp::DoContinueDialog()
{
	ContinueDialog* aDialog = new ContinueDialog(this);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	AddDialog(Dialogs::DIALOG_CONTINUE, aDialog);
}

//0x4502C0
void LawnApp::DoPauseDialog()
{
	mBoard->Pause(true);
	//FinishModelessDialogs();

	LawnDialog* aDialog = (LawnDialog*)DoDialog(
		Dialogs::DIALOG_PAUSED,
		true,
		_S("[GAME_PAUSED]"/*"GAME PAUSED"*/),
		_S("[CLICK_TO_RESUME]"/*"Click to resume game"*/), 
		_S("[RESUME_GAME]"/*"Resume Game"*/),
		Dialog::BUTTONS_FOOTER
	);

	aDialog->mReanimation->AddReanimation(72.0f, 42.0f, ReanimationType::REANIM_ZOMBIE_NEWSPAPER);
	aDialog->mSpaceAfterHeader = 155;
	aDialog->CalcSize(0, 10);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
}

//0x4504B0
int LawnApp::LawnMessageBox(int theDialogId, const SexyChar* theHeaderName, const SexyChar* theLinesName, const SexyChar* theButton1Name, const SexyChar* theButton2Name, int theButtonMode)
{
	Widget* aOldFocus = mWidgetManager->mFocusWidget;

	LawnDialog* aDialog = (LawnDialog*)DoDialog(theDialogId, true, theHeaderName, theLinesName, theButton1Name, theButtonMode);
	if (aDialog->mLawnYesButton)
	{
		aDialog->mLawnYesButton->mLabel = TodStringTranslate(theButton1Name);
	}
	if (aDialog->mLawnNoButton)
	{
		aDialog->mLawnNoButton->mLabel = TodStringTranslate(theButton2Name);
	}
	//aDialog->CalcSize(0, 0);

	mWidgetManager->SetFocus(aDialog);
	int aResult = aDialog->WaitForResult(true);
	mWidgetManager->SetFocus(aOldFocus);

	return aResult;
}

//0x450770
Dialog* LawnApp::DoDialog(int theDialogId, bool isModal, const SexyString& theDialogHeader, const SexyString& theDialogLines, const SexyString& theDialogFooter, int theButtonMode)
{
	SexyString aHeader = TodStringTranslate(theDialogHeader);
	SexyString aLines = TodStringTranslate(theDialogLines);
	SexyString aFooter = TodStringTranslate(theDialogFooter);

	Dialog* aDialog = SexyAppBase::DoDialog(theDialogId, isModal, aHeader, aLines, aFooter, theButtonMode);
	if (mWidgetManager->mFocusWidget == nullptr)
	{
		mWidgetManager->mFocusWidget = aDialog;
	}

	return aDialog;
}

Dialog* LawnApp::DoDialogDelay(int theDialogId, bool isModal, const SexyString& theDialogHeader, const SexyString& theDialogLines, const SexyString& theDialogFooter, int theButtonMode)
{
	LawnDialog* aDialog = (LawnDialog*)SexyAppBase::DoDialog(theDialogId, isModal, theDialogHeader, theDialogLines, theDialogFooter, theButtonMode);
	aDialog->SetButtonDelay(30);
	return aDialog;
}

//0x450880
void LawnApp::DoUserDialog()
{
	KillDialog(Dialogs::DIALOG_USERDIALOG);

	UserDialog* aDialog = new UserDialog(this);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	AddDialog(Dialogs::DIALOG_USERDIALOG, aDialog);
	mWidgetManager->SetFocus(aDialog);
}

//0x450930
void LawnApp::FinishUserDialog(bool isYes)
{
	UserDialog* aUserDialog = (UserDialog*)GetDialog(Dialogs::DIALOG_USERDIALOG);
	if (aUserDialog)
	{
		if (isYes)
		{
			PlayerInfo* aProfile = mProfileMgr->GetProfile(aUserDialog->GetSelName());
			if (aProfile)
			{
				mPlayerInfo = aProfile;
				mWidgetManager->MarkAllDirty();

				if (mGameSelector)
				{
					mGameSelector->SyncProfile(true);
				}
			}
		}

		KillDialog(Dialogs::DIALOG_USERDIALOG);
	}
}

//0x450A10
void LawnApp::DoCreateUserDialog()
{
	KillDialog(Dialogs::DIALOG_CREATEUSER);

	NewUserDialog* aDialog = new NewUserDialog(this, false);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	AddDialog(Dialogs::DIALOG_CREATEUSER, aDialog);
}

//0x450AC0
void LawnApp::FinishCreateUserDialog(bool isYes)
{
	NewUserDialog* aNewUserDialog = (NewUserDialog*)GetDialog(Dialogs::DIALOG_CREATEUSER);
	if (aNewUserDialog == nullptr)
		return;

	SexyString aName = aNewUserDialog->GetName();

	if (isYes && aName.empty())
	{
		DoDialog(
			Dialogs::DIALOG_CREATEUSERERROR,
			true,
			_S("[ENTER_YOUR_NAME]"/*"Enter Your Name"*/),
			_S("[USER_ERROR_MESSAGE]"/*"Please enter your name to create a new user profile for storing high score data and game progress."*/),
			_S("[DIALOG_BUTTON_OK]"/*"OK"*/),
			Dialog::BUTTONS_FOOTER
		);
	}
	else if (mPlayerInfo == nullptr && (!isYes || aName.empty()))
	{
		DoDialog(
			Dialogs::DIALOG_CREATEUSERERROR,
			true,
			_S("[ENTER_YOUR_NAME]"/*"Enter Your Name"*/),
			_S("[USER_ERROR_MESSAGE]"/*"Please enter your name to create a new user profile for storing high score data and game progress."*/),
			_S("[DIALOG_BUTTON_OK]"/*"OK"*/),
			Dialog::BUTTONS_FOOTER
		);
	}
	else if (!isYes)
	{
		KillDialog(Dialogs::DIALOG_CREATEUSER);
	}
	else
	{
		PlayerInfo* aProfile = mProfileMgr->AddProfile(aName);
		if (aProfile == nullptr)
		{
			DoDialog(
				Dialogs::DIALOG_CREATEUSERERROR,
				true,
				_S("[NAME_CONFLICT]"/*"Name Conflict"*/),
				_S("[ENTER_UNIQUE_PLAYER_NAME]"/*"The name you entered is already being used.  Please enter a unique player name"*/),
				_S("[DIALOG_BUTTON_OK]"/*"OK"*/),
				Dialog::BUTTONS_FOOTER
			);
		}
		else
		{
			mProfileMgr->Save();
			mPlayerInfo = aProfile;

			KillDialog(Dialogs::DIALOG_USERDIALOG);
			KillDialog(Dialogs::DIALOG_CREATEUSER);
			mWidgetManager->MarkAllDirty();

			if (mGameSelector)
			{
				mGameSelector->SyncProfile(true);
			}
		}
	}
}

//0x450E20
void LawnApp::DoConfirmDeleteUserDialog(const SexyString& theName)
{
	KillDialog(Dialogs::DIALOG_CONFIRMDELETEUSER);
	DoDialog(
		Dialogs::DIALOG_CONFIRMDELETEUSER, 
		true, 
		_S("[ARE_YOU_SURE]"/*"Are You Sure"*/), 
		StrFormat(TodStringTranslate(_S("[DELETE_USER_WARNING]")).c_str(), theName.c_str()), 
		// StrFormat(_S("This will permanently remove '%s' from the player roster!"), theName.c_str()),
		_S(""), 
		Dialog::BUTTONS_YES_NO
	);
}

//0x450F40
void LawnApp::FinishConfirmDeleteUserDialog(bool isYes)
{
	KillDialog(Dialogs::DIALOG_CONFIRMDELETEUSER);
	UserDialog* aUserDialog = (UserDialog*)GetDialog(Dialogs::DIALOG_USERDIALOG);
	if (aUserDialog == nullptr)
		return;

	mWidgetManager->SetFocus(aUserDialog);

	if (!isYes)
		return;

	SexyString aCurName = mPlayerInfo ? mPlayerInfo->mName : _S("");
	SexyString aName = aUserDialog->GetSelName();
	if (aName == aCurName)
	{
		mPlayerInfo = nullptr;
	}

	mProfileMgr->DeleteProfile(aName);
	aUserDialog->FinishDeleteUser();
	if (mPlayerInfo == nullptr)
	{
		mPlayerInfo = mProfileMgr->GetProfile(aUserDialog->GetSelName());
		if (mPlayerInfo == nullptr)
		{
			mPlayerInfo = mProfileMgr->GetAnyProfile();
		}
	}

	mProfileMgr->Save();
	if (mPlayerInfo == nullptr)
	{
		DoCreateUserDialog();
	}

	mWidgetManager->MarkAllDirty();
	if (mGameSelector != nullptr)
	{
		mGameSelector->SyncProfile(true);
	}
}

//0x451180
void LawnApp::DoRenameUserDialog(const SexyString& theName)
{
	KillDialog(Dialogs::DIALOG_RENAMEUSER);

	NewUserDialog* aDialog = new NewUserDialog(this, true);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	aDialog->SetName(theName);
	AddDialog(Dialogs::DIALOG_RENAMEUSER, aDialog);
}

//0x451260
void LawnApp::FinishRenameUserDialog(bool isYes)
{
	UserDialog* aUserDialog = (UserDialog*)GetDialog(Dialogs::DIALOG_USERDIALOG);
	if (!isYes)
	{
		KillDialog(Dialogs::DIALOG_RENAMEUSER);
		mWidgetManager->SetFocus(aUserDialog);
		return;
	}

	NewUserDialog* aNewUserDialog = (NewUserDialog*)GetDialog(Dialogs::DIALOG_RENAMEUSER);
	if (aUserDialog == nullptr || aNewUserDialog == nullptr)
		return;

	SexyString anOldName = aUserDialog->GetSelName();
	SexyString aNewName = aNewUserDialog->GetName();
	if (aNewName.empty())
		return;
	
	bool isCurrentUser = mProfileMgr->GetProfile(anOldName) == mPlayerInfo;
	if (!mProfileMgr->RenameProfile(anOldName, aNewName))
	{
		DoDialog(
			Dialogs::DIALOG_RENAMEUSERERROR,
			true,
			_S("Name Conflict"/*"[NAME_CONFLICT]"*/),
			_S("The name you entered is already being used.  Please enter a unique player name"/*"[ENTER_UNIQUE_PLAYER_NAME]"*/),
			_S("OK"/*"[DIALOG_BUTTON_OK]"*/),
			Dialog::BUTTONS_FOOTER
		);
		return;
	}

	mProfileMgr->Save();
	if (isCurrentUser)
	{
		mPlayerInfo = mProfileMgr->GetProfile(aNewName);
	}

	aUserDialog->FinishRenameUser(aNewName);
	mWidgetManager->MarkAllDirty();
	KillDialog(Dialogs::DIALOG_RENAMEUSER);
	mWidgetManager->SetFocus(aUserDialog);
}

//0x451490
void LawnApp::FinishNameError(int theId)
{
	KillDialog(theId);

	NewUserDialog* aNewUserDialog = (NewUserDialog*)GetDialog(theId == Dialogs::DIALOG_CREATEUSERERROR ? Dialogs::DIALOG_CREATEUSER : Dialogs::DIALOG_RENAMEUSER);
	if (aNewUserDialog)
	{
		mWidgetManager->SetFocus(aNewUserDialog->mNameEditWidget);
	}
}

//0x4514D0
void LawnApp::FinishRestartConfirmDialog()
{
	mSawYeti = mBoard->mKilledYeti;

	KillDialog(Dialogs::DIALOG_CONTINUE);
	KillDialog(Dialogs::DIALOG_RESTARTCONFIRM);
	KillBoard();

	PreNewGame(mGameMode, false);
}

void LawnApp::DoCheatDialog()
{
	KillDialog(Dialogs::DIALOG_CHEAT);

	CheatDialog* aDialog = new CheatDialog(this);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	AddDialog(Dialogs::DIALOG_CHEAT, aDialog);
}

void LawnApp::FinishCheatDialog(bool isYes)
{
	CheatDialog* aCheatDialog = (CheatDialog*)GetDialog(Dialogs::DIALOG_CHEAT);
	if (aCheatDialog == nullptr)
		return;

	if (isYes && !aCheatDialog->ApplyCheat())
		return;

	KillDialog(Dialogs::DIALOG_CHEAT);
	if (isYes)
	{
		mMusic->StopAllMusic();
		mBoardResult = BoardResult::BOARDRESULT_CHEAT;
		PreNewGame(mGameMode, false);
	}
}

void LawnApp::FinishTimesUpDialog()
{
	KillDialog(Dialogs::DIALOG_TIMESUP);
}

void LawnApp::DoConfirmSellDialog(const SexyString& theMessage)
{
	Dialog* aConfirmDialog = DoDialog(Dialogs::DIALOG_ZEN_SELL, true, _S("[ZEN_SELL_HEADER]"), theMessage, _S(""), Dialog::BUTTONS_YES_NO);
	aConfirmDialog->mYesButton->mLabel = TodStringTranslate(_S("[DIALOG_BUTTON_YES]"));
	aConfirmDialog->mNoButton->mLabel = TodStringTranslate(_S("[DIALOG_BUTTON_NO]"));
}

void LawnApp::DoConfirmPurchaseDialog(const SexyString& theMessage)
{
	LawnDialog* aComfirmDialog = (LawnDialog*)DoDialog(Dialogs::DIALOG_STORE_PURCHASE, true, _S("买下这个物品？"), theMessage, _S(""), Dialog::BUTTONS_YES_NO);
	aComfirmDialog->mLawnYesButton->mLabel = TodStringTranslate(_S("[DIALOG_BUTTON_YES]"));
	aComfirmDialog->mLawnNoButton->mLabel = TodStringTranslate(_S("[DIALOG_BUTTON_NO]"));
}

//0x451580
Dialog* LawnApp::NewDialog(int theDialogId, bool isModal, const SexyString& theDialogHeader, const SexyString& theDialogLines, const SexyString& theDialogFooter, int theButtonMode)
{
	if (theDialogId == Dialogs::DIALOG_THIRTY_FLAGS)
	{
		ThirtyFlagsDialog* aTFDialog = new ThirtyFlagsDialog(mBoard);
		CenterDialog(aTFDialog, aTFDialog->mWidth, aTFDialog->mHeight);
		return aTFDialog;
	}

	LawnDialog* aDialog = new LawnDialog(
		this, 
		theDialogId, 
		isModal, 
		theDialogHeader, 
		theDialogLines, 
		theDialogFooter, 
		theButtonMode
	);

	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	return aDialog;
}

//0x451630
bool LawnApp::KillNewOptionsDialog()
{
	NewOptionsDialog* aNewOptionsDialog = (NewOptionsDialog*)GetDialog(Dialogs::DIALOG_NEWOPTIONS);
	if (aNewOptionsDialog == nullptr)
		return false;

	bool wantWindowed = !aNewOptionsDialog->mFullscreenCheckbox->IsChecked();
	bool want3D = aNewOptionsDialog->mHardwareAccelerationCheckbox->IsChecked();
	SwitchScreenMode(wantWindowed, want3D, false);

	KillDialog(Dialogs::DIALOG_NEWOPTIONS);
	ClearUpdateBacklog();
	return true;
}

//0x4516C0
bool LawnApp::KillAlmanacDialog()
{
	if (GetDialog(Dialogs::DIALOG_ALMANAC))
	{
		KillDialog(Dialogs::DIALOG_ALMANAC);
		ClearUpdateBacklog(false);
		return true;
	}

	return false;
}

//0x4516F0
bool LawnApp::NeedPauseGame()
{
	if (mDialogList.size() == 0)
		return false;

	if (mDialogList.size() == 1 && mDialogList.front()->mId != Dialogs::DIALOG_NEW_GAME)
	{
		int anId = mDialogList.front()->mId;
		if (anId == Dialogs::DIALOG_CHOOSER_WARNING || anId == Dialogs::DIALOG_PURCHASE_PACKET_SLOT || anId == Dialogs::DIALOG_IMITATER)
		{
			return false;
		}
	}

	return (mBoard == nullptr || mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN) && (mBoard == nullptr || mGameMode != GameMode::GAMEMODE_TREE_OF_WISDOM);
}

//0x451780
void LawnApp::ModalOpen()
{
	if (mBoard && NeedPauseGame())
	{
		mBoard->Pause(true);
	}
}

void LawnApp::ModalClose()
{
}

//0x451800
bool LawnApp::KillDialog(int theDialogId)
{
	if (SexyAppBase::KillDialog(theDialogId))
	{
		if (mDialogMap.size() == 0)
		{
			if (mBoard)
			{
				mWidgetManager->SetFocus(mBoard);
			}
			else if (mGameSelector)
			{
				mWidgetManager->SetFocus(mGameSelector);
			}
		}

		if (mBoard && !NeedPauseGame())
		{
			mBoard->Pause(false);
		}

		return true;
	}

	return false;
}

//0x451870
void LawnApp::ShowResourceError(bool doExit)
{
	SexyAppBase::ShowResourceError(doExit);
}

void BetaSubmitFunc()
{
	if (gLawnApp)
	{
		gLawnApp->BetaSubmit(false);
	}
}

//0x451880
void LawnApp::Init()
{
    // PvzDebug's filter was installed in WinMain; re-assert it here because other
    // subsystems sometimes install their own, and register the state dump now that
    // gLawnApp exists. PvzDebug's report supersedes the old TfUnhandledFilter:
    // it adds the fault target, registers, the recent log and a minidump.
    PvzDebugInstallCrashHandler();
    PvzDebugSetStateDump(LawnAppCrashStateDump, nullptr);

    gTfPhase = "init";
    TfLogWrite("=== app init ===");
    TfLog("=== thirtyflags session start ===");
	DoParseCmdLine();
	if (!mTodCheatKeys)
	{
		mOnlyAllowOneCopyToRun = true;
	}

	//if (!gSexyCache->Connected() &&
	//	gLawnApp->mTodCheatKeys &&
	//	MessageBox(gLawnApp->mHWnd, _S("Start SexyCache now?"), _S("SexyCache"), MB_YESNO) == IDYES &&
	//	WinExec("SexyCache.exe", SW_MINIMIZE) >= 32)
	//{
	//	gSexyCache = SexyCache();
	//}
	//if (gSexyCache->Connected() && !gLawnApp->mTodCheatKeys)
	//{
	//	gSexyCache->Disconnect();
	//}

	mSessionID = _time32(nullptr);
	mPlayTimeActiveSession = 0;
	mPlayTimeInactiveSession = 0;
	mBoardResult = BoardResult::BOARDRESULT_NONE;
	mSawYeti = false;

	SexyApp::Init();
#ifdef _DEBUG
	TodAssertInitForApp();
	gBetaSubmitFunc = BetaSubmitFunc;
	TodLog("session id: %u", mSessionID);
#endif

	if (!mResourceManager->ParseResourcesFile("properties\\resources.xml"))
	{
		ShowResourceError(true);
		return;
	}

	if (!TodLoadResources("Init"))
	{
		return;
	}

	PerfTimer mTimer;
	mTimer.Start();

	mProfileMgr->Load();

	std::string aCurUser;
	if (mPlayerInfo == nullptr && RegistryReadString("CurUser", &aCurUser))
	{
		mPlayerInfo = mProfileMgr->GetProfile(StringToSexyStringFast(aCurUser));
	}
	if (mPlayerInfo == nullptr)
	{
		mPlayerInfo = mProfileMgr->GetAnyProfile();
	}

	mMaxExecutions = GetInteger("MaxExecutions", 0);
	mMaxPlays = GetInteger("MaxPlays", 0);
	mMaxTime = GetInteger("MaxTime", 60);

	mTitleScreen = new TitleScreen(this);
	mTitleScreen->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mTitleScreen);
	mWidgetManager->SetFocus(mTitleScreen);

#ifdef _DEBUG
	int aDuration = mTimer.GetDuration();
	TodTrace("loading: 'profiles' %d ms", aDuration);
#endif
	mTimer.Start();

	mMusic = new Music();
	mSoundSystem = new TodFoley();
	mEffectSystem = new EffectSystem();
	mEffectSystem->EffectSystemInitialize();

	mKonamiCheck = new TypingCheck();
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_UP);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_UP);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_DOWN);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_DOWN);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_LEFT);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_RIGHT);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_LEFT);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_RIGHT);
	mKonamiCheck->AddChar('b');
	mKonamiCheck->AddChar('a');
	mMustacheCheck = new TypingCheck("mustache");
	mMoustacheCheck = new TypingCheck("moustache");
	mSuperMowerCheck = new TypingCheck("trickedout");
	mSuperMowerCheck2 = new TypingCheck("tricked out");
	mFutureCheck = new TypingCheck("future");
	mPinataCheck = new TypingCheck("pinata");
	mDanceCheck = new TypingCheck("dance");
	mDaisyCheck = new TypingCheck("daisies");
	mSukhbirCheck = new TypingCheck("sukhbir");

#ifdef _DEBUG
	aDuration = mTimer.GetDuration();
	TodTrace("loading: 'system' %d ms", aDuration);
#endif
	mTimer.Start();

	ReanimatorLoadDefinitions(gLawnReanimationArray, ReanimationType::NUM_REANIMS);
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_LOADBAR_SPROUT, true);
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_LOADBAR_ZOMBIEHEAD, true);

#ifdef _DEBUG
	aDuration = mTimer.GetDuration();
	TodTrace("loading: 'loaderbar' %d ms", aDuration);
#endif
	mTimer.Start();
}

//0x4522A0
bool LawnApp::ChangeDirHook(const char* theIntendedPath)
{
	return false;
}

//0x4522B0
void LawnApp::Start()
{
    gTfPhase = "start";
    TfLogWrite("app start");
	if (mLoadingFailed)
		return;

	SexyAppBase::Start();
}

//0x4522C0
bool LawnApp::DebugKeyDown(int theKey)
{
	return SexyAppBase::DebugKeyDown(theKey);
}

//0x4522E0
void LawnApp::HandleCmdLineParam(const std::string& theParamName, const std::string& theParamValue)
{
	if (theParamName == "-tod")
	{
#ifdef _DEBUG
		mTodCheatKeys = true;
		mDebugKeysEnabled = true;
#endif
	}
	else
	{
		SexyApp::HandleCmdLineParam(theParamName, theParamValue);
	}
}

//0x452310
bool LawnApp::UpdatePlayerProfileForFinishingLevel()
{
	bool aUnlockedNewChallenge = false;

	if (IsAdventureMode())
	{
		if (mBoard->mLevel == FINAL_LEVEL)
		{
			mPlayerInfo->SetLevel(1);
			mPlayerInfo->mFinishedAdventure++;
			if (mPlayerInfo->mFinishedAdventure == 1)
			{
				mPlayerInfo->mNeedsMessageOnGameSelector = 1;
			}
		}
		else
		{
			mPlayerInfo->SetLevel(mBoard->mLevel + 1);
		}

		if (!HasFinishedAdventure() && mBoard->mLevel == 34)
		{
			mPlayerInfo->mNeedsMagicTacoReward = 1;
		}
	}
	else if (IsSurvivalMode())
	{
		if (mBoard->IsFinalSurvivalStage())
		{
			aUnlockedNewChallenge = !HasBeatenChallenge(mGameMode);
			mBoard->SurvivalSaveScore();

			if (aUnlockedNewChallenge && HasFinishedAdventure())
			{
				int aNumTrophies = GetNumTrophies(ChallengePage::CHALLENGE_PAGE_SURVIVAL);
				if (aNumTrophies != 8 && aNumTrophies != 9)
				{
					mPlayerInfo->mHasNewSurvival = true;
				}
			}
		}
	}
	else if (IsPuzzleMode())
	{
		aUnlockedNewChallenge = !HasBeatenChallenge(mGameMode);
		mPlayerInfo->mChallengeRecords[GetCurrentChallengeIndex()]++;

		if (!HasFinishedAdventure() && (mGameMode == GameMode::GAMEMODE_SCARY_POTTER_3 || mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_3))
		{
			aUnlockedNewChallenge = false;
		}

		if (aUnlockedNewChallenge)
		{
			if (IsScaryPotterLevel())
			{
				mPlayerInfo->mHasNewScaryPotter = 1;
			}
			else
			{
				mPlayerInfo->mHasNewIZombie = 1;
			}
		}
	}
	else
	{
		aUnlockedNewChallenge = !HasBeatenChallenge(mGameMode);
		mPlayerInfo->mChallengeRecords[GetCurrentChallengeIndex()]++;

		if (aUnlockedNewChallenge && HasFinishedAdventure())
		{
			int aNumTrophies = GetNumTrophies(ChallengePage::CHALLENGE_PAGE_CHALLENGE);
			if (aNumTrophies <= 17)
			{
				mPlayerInfo->mHasNewMiniGame = 1;
			}
		}
	}

	WriteCurrentUserConfig();

	return aUnlockedNewChallenge;
}

//0x4524F0
void LawnApp::CheckForGameEnd()
{
	if (mBoard == nullptr || !mBoard->mLevelComplete)
		return;

	bool aUnlockedNewChallenge = UpdatePlayerProfileForFinishingLevel();

	if (IsAdventureMode())
	{
		int aLevel = mBoard->mLevel;
		KillBoard();

		if (IsFirstTimeAdventureMode() && aLevel < NUM_LEVELS)
		{
			ShowAwardScreen(AwardType::AWARD_FORLEVEL);
		}
		else if (aLevel == FINAL_LEVEL)
		{
			if (mPlayerInfo->mFinishedAdventure == 1)
			{
				ShowAwardScreen(AwardType::AWARD_FORLEVEL);
			}
			else
			{
				ShowAwardScreen(AwardType::AWARD_CREDITS_ZOMBIENOTE);
			}
		}
		else if (aLevel == 9 || aLevel == 19 || aLevel == 29 || aLevel == 39 || aLevel == 49)
		{
			ShowAwardScreen(AwardType::AWARD_FORLEVEL);
		}
		else
		{
			PreNewGame(mGameMode, false);
		}
	}
	else if (IsSurvivalMode())
	{
		if (mBoard->IsFinalSurvivalStage())
		{
			KillBoard();

			if (aUnlockedNewChallenge && HasFinishedAdventure())
			{
				ShowAwardScreen(AwardType::AWARD_FORLEVEL);
			}
			else
			{
				ShowChallengeScreen(ChallengePage::CHALLENGE_PAGE_SURVIVAL);
			}
		}
		else
		{
			mBoard->mChallenge->mSurvivalStage++;

			if (mGameMode == GameMode::GAMEMODE_THIRTY_FLAGS)
			{
				ThirtyFlagsIntermissionFinish(mBoard);
				ThirtyFlagsIntermissionBegin(mBoard);
				ThirtyFlagsAdvanceFlag(mBoard);
			}

			KillGameSelector();
			mBoard->InitSurvivalStage();
		}
	}
	else if (IsPuzzleMode())
	{
		KillBoard();

		if (aUnlockedNewChallenge)
		{
			ShowAwardScreen(AwardType::AWARD_FORLEVEL);
		}
		else
		{
			ShowChallengeScreen(ChallengePage::CHALLENGE_PAGE_PUZZLE);
		}
	}
	else
	{
		KillBoard();

		if (aUnlockedNewChallenge && HasFinishedAdventure())
		{
			ShowAwardScreen(AwardType::AWARD_FORLEVEL);
		}
		else
		{
			ShowChallengeScreen(ChallengePage::CHALLENGE_PAGE_CHALLENGE);
		}
	}
}

void LawnApp::UpdatePlayTimeStats()
{
	static int aLastTime = -1;

	int aTickCount = GetTickCount();
	int aSession = (aTickCount - aLastTime) / 1000;

	if (mPlayerInfo && !mPlayerInfo->mHasUsedCheatKeys && !mDebugKeysEnabled && mTodCheatKeys)
	{
		mPlayerInfo->mHasUsedCheatKeys = 1;
	}

	if (aLastTime == -1)
	{
		aLastTime = aTickCount;
		return;
	}

	if (aSession > 0)
	{
		aLastTime = aTickCount;

		if ((mBoard == nullptr || !mBoard->mPaused) && mHasFocus && mLastTimerTime - mLastUserInputTick <= 10000)
		{
			mPlayTimeActiveSession += aSession;

			if (mBoard)
			{
				mBoard->mPlayTimeActiveLevel += aSession;
			}

			if (mPlayerInfo)
			{
				mPlayerInfo->mPlayTimeActivePlayer += aSession;
			}
		}
		else
		{
			mPlayTimeInactiveSession += aSession;

			if (mBoard)
			{
				mBoard->mPlayTimeInactiveLevel += aSession;
			}

			if (mPlayerInfo)
			{
				mPlayerInfo->mPlayTimeInactivePlayer += aSession;
			}
		}
	}
}

//0x452650
void LawnApp::UpdateFrames()
{
	if ((!mActive || mMinimized) && mBoard)
	{
		mBoard->ResetFPSStats();
	}

#ifdef _DEBUG
	UpdatePlayTimeStats();
#endif

	int aUpdateCount = 1;
	if (gSlowMo)
	{
		++gSlowMoCounter;
		if (gSlowMoCounter < 4)
		{
			aUpdateCount = 0;
		}
		else
		{
			gSlowMoCounter = 0;
		}
	}
	else if (gFastMo)
	{
		// 【三十旗】策划案 9.2「2 倍速」：原版 6 键是 20 倍超快，
		// 不适合当常规播放速度；三十旗模式下改为精确 2 倍。
		aUpdateCount = ThirtyFlagsMode() ? 2 : 20;
	}

	for (int i = 0; i < aUpdateCount; i++)
	{
		mAppCounter++;
		
		if (mBoard)
		{
			mBoard->ProcessDeleteQueue();
		}

		SexyApp::UpdateFrames();

		mMusic->MusicUpdate();
		if (mLoadingThreadCompleted && mEffectSystem)
		{
			mEffectSystem->ProcessDeleteQueue();
		}

		CheckForGameEnd();
	}
}

void LawnApp::ToggleSlowMo()
{
	gSlowMoCounter = 0;
	gSlowMo = !gSlowMo;
	gFastMo = false;
}

void LawnApp::ToggleFastMo()
{
	gSlowMo = false;
	gFastMo = !gFastMo;
}

//0x452740
void LawnApp::LoadGroup(const char* theGroupName, int theGroupAveMsToLoad)
{
	PerfTimer aTimer;
	aTimer.Start();

	mResourceManager->StartLoadResources(theGroupName);
	while (!mShutdown && !mCloseRequest && !mLoadingFailed && TodLoadNextResource())
	{
		mCompletedLoadingThreadTasks += theGroupAveMsToLoad;
	}

	if (mShutdown || mCloseRequest)
		return;

	if (mResourceManager->HadError() || !ExtractResourcesByName(mResourceManager, theGroupName))
	{
		ShowResourceError();
		mLoadingFailed = true;
	}

	int aTotalGroupWeight = mResourceManager->GetNumResources(theGroupName) * theGroupAveMsToLoad;
	int aGroupTime = max(aTimer.GetDuration(), 0);
	TraceLoadGroup(theGroupName, aGroupTime, aTotalGroupWeight, theGroupAveMsToLoad);
}

//0x4528E0
void LawnApp::LoadingThreadProc()
{
	if (!TodLoadResources("LoaderBar"))
		return;

	TodStringListLoad("Properties\\LawnStrings.txt");

	if (mTitleScreen)
	{
		mTitleScreen->mLoaderScreenIsLoaded = true;
	}

	const char* groups[] = { "LoadingFonts", "LoadingImages", "LoadingSounds" };
	int group_ave_ms_to_load[] = { 54, 9, 54 };
	for (int i = 0; i < 3; i++)
	{
		mNumLoadingThreadTasks += mResourceManager->GetNumResources(groups[i]) * group_ave_ms_to_load[i];
	}
	mNumLoadingThreadTasks += 636;
	mNumLoadingThreadTasks += GetNumPreloadingTasks();
	mNumLoadingThreadTasks += mMusic->GetNumLoadingTasks();

	PerfTimer aTimer;
	aTimer.Start();

	TodHesitationTrace("start loading");
	TodHesitationBracket aHesitationResources("Resources");
	TodHesitationTrace("loading thread start");

	LoadGroup("LoadingImages", 9);
	LoadGroup("LoadingFonts", 54);
	if (mLoadingFailed || mShutdown || mCloseRequest)
		return;

	aHesitationResources.EndBracket();
	TodTrace("loading '%s' %d ms", "resources", (int)aTimer.GetDuration());

	mMusic->MusicInit();
	int aDuration = max(aTimer.GetDuration(), 0);
	aTimer.Start();

	mPoolEffect = new PoolEffect();
	mPoolEffect->PoolEffectInitialize();
	mZenGarden = new ZenGarden();
	mReanimatorCache = new ReanimatorCache();
	mReanimatorCache->ReanimatorCacheInitialize();
	TodFoleyInitialize(gLawnFoleyParamArray, LENGTH(gLawnFoleyParamArray));

	TodTrace("loading '%s' %d ms", "stuff", (int)aTimer.GetDuration());
	aTimer.Start();

	TrailLoadDefinitions(gLawnTrailArray, LENGTH(gLawnTrailArray));
	TodTrace("loading '%s' %d ms", "trail", (int)aTimer.GetDuration());
	aTimer.Start();
	TodHesitationTrace("trail");
	
	TodParticleLoadDefinitions(gLawnParticleArray, LENGTH(gLawnParticleArray));
	aDuration = max(aTimer.GetDuration(), 0);
	aTimer.Start();

	PreloadForUser();
	if (mLoadingFailed || mShutdown || mCloseRequest)
		return;

	aDuration = max(aTimer.GetDuration(), 0);
	aTimer.Start();

	GetNumPreloadingTasks();
	LoadGroup("LoadingSounds", 54);
	TodHesitationTrace("finished loading");
}

//0x452C60
void LawnApp::FastLoad(GameMode theGameMode)
{
	if (!mShutdown)
	{
		mWidgetManager->RemoveWidget(mTitleScreen);
		SafeDeleteWidget(mTitleScreen);
		mTitleScreen = nullptr;

		PreNewGame(theGameMode, false);
	}
}

void LawnApp::LoadingThreadCompleted()
{
}

//0x452CB0
void LawnApp::LoadingCompleted()
{
    gTfPhase = "loaded";
    TfLogWrite("loading completed");
	mWidgetManager->RemoveWidget(mTitleScreen);
	SafeDeleteWidget(mTitleScreen);
	mTitleScreen = nullptr;

	mResourceManager->DeleteImage("IMAGE_TITLESCREEN");

	ShowGameSelector();
}

//0x452D80
void LawnApp::URLOpenFailed(const std::string& theURL)
{
	SexyAppBase::URLOpenFailed(theURL);
	KillDialog(Dialogs::DIALOG_OPENURL_WAIT);
	CopyToClipboard(theURL);

	std::string aString = 
		"Please open the following URL in your browser\n\n" + 
		theURL + 
		"\n\nFor your convenience, this URL has already been copied to your clipboard.";

	DoDialog(Dialogs::DIALOG_OPENURL_WAIT, true, _S("Open Browser"), _S("OK"), StringToSexyStringFast(aString), Dialog::BUTTONS_FOOTER);
}

//0x452EE0
void LawnApp::URLOpenSucceeded(const std::string& theURL)
{
	SexyAppBase::URLOpenSucceeded(theURL);
	KillDialog(Dialogs::DIALOG_OPENURL_WAIT);
}

//0x452F00
bool LawnApp::OpenURL(const std::string& theURL, bool shutdownOnOpen)
{
	DoDialog(
		Dialogs::DIALOG_OPENURL_WAIT, 
		true, 
		_S("Opening Browser"), 
		_S("Opening Browser"), 
		_S(""), 
		Dialog::BUTTONS_NONE
	);

	DrawDirtyStuff();

	return SexyAppBase::OpenURL(theURL, shutdownOnOpen);
}

//0x453040
void LawnApp::ConfirmQuit()
{
	SexyString aBody = TodStringTranslate(_S("[QUIT_MESSAGE]"));
	SexyString aHeader = TodStringTranslate(_S("[QUIT_HEADER]"));
	LawnDialog* aDialog = (LawnDialog*)DoDialog(Dialogs::DIALOG_QUIT, true, aHeader, aBody, _S(""), Dialog::BUTTONS_OK_CANCEL);
	aDialog->mLawnYesButton->mLabel = TodStringTranslate(_S("[QUIT_BUTTON]"));
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
}

//0x4531D0
void LawnApp::PreDisplayHook()
{
	SexyApp::PreDisplayHook();
}

void LawnApp::ButtonPress(int theId)
{
}

//0x4531E0
void LawnApp::ButtonDepress(int theId)
{
	if (theId % 10000 >= 2000 && theId % 10000 < 3000)
	{
		switch (theId - 2000)
		{
		case Dialogs::DIALOG_NEW_GAME:
			KillDialog(Dialogs::DIALOG_NEW_GAME);
			ShowGameSelector();
			return;

		case Dialogs::DIALOG_NEWOPTIONS:
			KillNewOptionsDialog();
			return;

		case Dialogs::DIALOG_PREGAME_NAG:
			DoRegister();
			return;

		case Dialogs::DIALOG_LOAD_GAME:
			return;

		case Dialogs::DIALOG_CONFIRM_UPDATE_CHECK:
			KillDialog(Dialogs::DIALOG_CONFIRM_UPDATE_CHECK);
			CheckForUpdates();
			return;

		case Dialogs::DIALOG_QUIT:
			KillDialog(Dialogs::DIALOG_QUIT);
			SendMessage(mHWnd, WM_CLOSE, NULL, NULL);
			return;

		case Dialogs::DIALOG_NAG:
			KillDialog(Dialogs::DIALOG_NAG);
			DoRegister();
			return;

		case Dialogs::DIALOG_INFO:
			KillDialog(Dialogs::DIALOG_INFO);
			return;

		case Dialogs::DIALOG_PAUSED:
			KillDialog(Dialogs::DIALOG_PAUSED);
			return;

		case Dialogs::DIALOG_NO_MORE_MONEY:
			KillDialog(Dialogs::DIALOG_NO_MORE_MONEY);
			mBoard->AddSunMoney(100);
			return;

		case Dialogs::DIALOG_BONUS:
			KillDialog(Dialogs::DIALOG_BONUS);
			return;

		case Dialogs::DIALOG_TF_ENTER:
			// 【三十旗】确认框 Yes：启动三十旗（mode=73，pvzdebug 实测验证值）。
			KillDialog(Dialogs::DIALOG_TF_ENTER);
			// 【三十旗】GameMode::GAMEMODE_THIRTY_FLAGS 枚举值 = 71（pvzdebug.log
			// 26 次三十旗会话全部 mode=71 实锤——73 是 GAMEMODE_INTRO 启动动画，勿混淆）
			PreNewGame(GameMode::GAMEMODE_THIRTY_FLAGS, false);
			return;

		case Dialogs::DIALOG_CONFIRM_BACK_TO_MAIN:
			KillDialog(Dialogs::DIALOG_CONFIRM_BACK_TO_MAIN);
			mBoardResult = BoardResult::BOARDRESULT_QUIT;
			mBoard->TryToSaveGame();
			DoBackToMain();
			return;

		case Dialogs::DIALOG_USERDIALOG:
			FinishUserDialog(true);
			return;

		case Dialogs::DIALOG_CREATEUSER:
			FinishCreateUserDialog(true);
			return;

		case Dialogs::DIALOG_CONFIRMDELETEUSER:
			FinishConfirmDeleteUserDialog(true);
			return;

		case Dialogs::DIALOG_RENAMEUSER:
			FinishRenameUserDialog(true);
			return;

		case Dialogs::DIALOG_CREATEUSERERROR:
		case Dialogs::DIALOG_RENAMEUSERERROR:
			FinishNameError(theId - 2000);
			return;

		case Dialogs::DIALOG_CHEAT:
			FinishCheatDialog(true);
			return;

		case Dialogs::DIALOG_RESTARTCONFIRM:
			FinishRestartConfirmDialog();
			return;

		case Dialogs::DIALOG_TIMESUP:
			FinishTimesUpDialog();
			return;

		case 20008:
			KillDialog(20008);
			KillDialog(Dialogs::DIALOG_CHECKING_UPDATES);
			return;

		default:
			KillDialog(theId - 2000);
			return;
		}
	}

	if (theId % 10000 >= 3000 && theId < 4000)
	{
		switch (theId - 3000)
		{
		case Dialogs::DIALOG_PREGAME_NAG:
			KillDialog(Dialogs::DIALOG_PREGAME_NAG);
			Shutdown();
			return;

		case Dialogs::DIALOG_LOAD_GAME:
			KillDialog(Dialogs::DIALOG_LOAD_GAME);
			return;

		case Dialogs::DIALOG_USERDIALOG:
			FinishUserDialog(false);
			return;

		case Dialogs::DIALOG_CREATEUSER:
			FinishCreateUserDialog(false);
			return;

		case Dialogs::DIALOG_CONFIRMDELETEUSER:
			FinishConfirmDeleteUserDialog(false);
			return;

		case Dialogs::DIALOG_RENAMEUSER:
			FinishRenameUserDialog(false);
			return;

		case Dialogs::DIALOG_CHEAT:
			FinishCheatDialog(false);
			return;

		case Dialogs::DIALOG_TIMESUP:
			FinishTimesUpDialog();
			return;

		case 10008:
			KillDialog(10008);
			KillDialog(Dialogs::DIALOG_CHECKING_UPDATES);
			return;

		default:
			KillDialog(theId - 3000);
			return;
		}
	}
}

void LawnApp::CenterDialog(Dialog* theDialog, int theWidth, int theHeight)
{
	theDialog->Resize((BOARD_WIDTH - theWidth) / 2, (BOARD_HEIGHT - theHeight) / 2, theWidth, theHeight);
}

//0x453630
void LawnApp::PlayFoley(FoleyType theFoleyType)
{
	if (!mMuteSoundsForCutscene)
	{
		mSoundSystem->PlayFoley(theFoleyType);
	}
}

//0x453650
void LawnApp::PlayFoleyPitch(FoleyType theFoleyType, float thePitch)
{
	if (!mMuteSoundsForCutscene)
	{
		mSoundSystem->PlayFoleyPitch(theFoleyType, thePitch);
	}
}

//0x453670
SexyString LawnApp::GetStageString(int theLevel)
{
	int aArea = ClampInt((theLevel - 1) / LEVELS_PER_AREA + 1, 1, ADVENTURE_AREAS + 1);
	int aSub = theLevel - (aArea - 1) * LEVELS_PER_AREA;
	return StrFormat(_S("%d-%d"), aArea, aSub);
}

bool LawnApp::IsAdventureMode()
{
	return mGameMode == GameMode::GAMEMODE_ADVENTURE;
}

//0x4536D0
bool LawnApp::IsSurvivalMode()
{
	if (mGameMode == GameMode::GAMEMODE_THIRTY_FLAGS)
	{
		return true;
	}
	return mGameMode >= GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1 && mGameMode <= GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_5;
}

//0x4536F0
bool LawnApp::IsPuzzleMode()
{
	return
		(mGameMode >= GameMode::GAMEMODE_SCARY_POTTER_1 && mGameMode <= GameMode::GAMEMODE_SCARY_POTTER_ENDLESS) ||
		(mGameMode >= GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_1 && mGameMode <= GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS);
}

//0x453710
bool LawnApp::IsChallengeMode()
{
	return !IsAdventureMode() && !IsPuzzleMode() && !IsSurvivalMode();
}

bool LawnApp::IsSurvivalNormal(GameMode theGameMode)
{
	int aLevel = theGameMode - GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1;
	return aLevel >= 0 && aLevel <= 4;
}

bool LawnApp::IsSurvivalHard(GameMode theGameMode)
{
	int aLevel = theGameMode - GameMode::GAMEMODE_SURVIVAL_HARD_STAGE_1;
	return aLevel >= 0 && aLevel <= 4;
}

bool LawnApp::IsSurvivalEndless(GameMode theGameMode)
{
	int aLevel = theGameMode - GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_1;
	return aLevel >= 0 && aLevel <= 4;
}

bool LawnApp::IsEndlessScaryPotter(GameMode theGameMode)
{
	return theGameMode == GameMode::GAMEMODE_SCARY_POTTER_ENDLESS;
}

bool LawnApp::IsEndlessIZombie(GameMode theGameMode)
{
	return theGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS;
}

//0x453740
bool LawnApp::IsContinuousChallenge()
{
	return 
		IsArtChallenge() || 
		IsSlotMachineLevel() || 
		IsFinalBossLevel() || 
		mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED || 
		mGameMode == GameMode::GAMEMODE_UPSELL || 
		mGameMode == GameMode::GAMEMODE_INTRO || 
		mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED_TWIST;
}

bool LawnApp::IsArtChallenge()
{
	if (mBoard == nullptr)
		return false;

	return 
		mGameMode == GameMode::GAMEMODE_CHALLENGE_ART_CHALLENGE_WALLNUT || 
		mGameMode == GameMode::GAMEMODE_CHALLENGE_ART_CHALLENGE_SUNFLOWER || 
		mGameMode == GameMode::GAMEMODE_CHALLENGE_SEEING_STARS;
}

//0x4537B0
bool LawnApp::IsSquirrelLevel()
{
	return mBoard && mGameMode == GameMode::GAMEMODE_CHALLENGE_SQUIRREL;
}

//0x4537D0
bool LawnApp::IsIZombieLevel()
{
	if (mBoard == nullptr)
		return false;

	return
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_1 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_2 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_3 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_4 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_5 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_6 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_7 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_8 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_9 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS;
}

//0x453820
bool LawnApp::IsShovelLevel()
{
	return mBoard && mGameMode == GameMode::GAMEMODE_CHALLENGE_SHOVEL;
}

//0x453840
bool LawnApp::IsWallnutBowlingLevel()
{
	if (mBoard == nullptr)
		return false;

	if (mGameMode == GameMode::GAMEMODE_CHALLENGE_WALLNUT_BOWLING || mGameMode == GameMode::GAMEMODE_CHALLENGE_WALLNUT_BOWLING_2)
		return true;

	return IsAdventureMode() && mPlayerInfo->mLevel == 5;
}

//0x453870
bool LawnApp::IsSlotMachineLevel()
{
	return (mBoard && mGameMode == GameMode::GAMEMODE_CHALLENGE_SLOT_MACHINE);
}

//0x453890
bool LawnApp::IsWhackAZombieLevel()
{
	if (mBoard == nullptr)
		return false;

	if (mGameMode == GameMode::GAMEMODE_CHALLENGE_WHACK_A_ZOMBIE)
		return true;

	return IsAdventureMode() && mPlayerInfo->mLevel == 15;
}

//0x4538C0
bool LawnApp::IsLittleTroubleLevel()
{
	return (mBoard && (mGameMode == GameMode::GAMEMODE_CHALLENGE_LITTLE_TROUBLE || (mGameMode == GameMode::GAMEMODE_ADVENTURE && mPlayerInfo->mLevel == 25)));
}

//0x4538F0
bool LawnApp::IsScaryPotterLevel()
{
	if (mGameMode >= GameMode::GAMEMODE_SCARY_POTTER_1 && mGameMode <= GameMode::GAMEMODE_SCARY_POTTER_ENDLESS)
		return true;

	return IsAdventureMode() && mPlayerInfo->mLevel == 35;
}

//0x453920
bool LawnApp::IsStormyNightLevel()
{
	if (mBoard == nullptr)
		return false;

	if (mGameMode == GameMode::GAMEMODE_CHALLENGE_STORMY_NIGHT)
		return true;

	return IsAdventureMode() && mPlayerInfo->mLevel == 40;
}

//0x453950
bool LawnApp::IsBungeeBlitzLevel()
{
	if (mBoard == nullptr)
		return false;

	if (mGameMode == GameMode::GAMEMODE_CHALLENGE_BUNGEE_BLITZ)
		return true;

	return IsAdventureMode() && mPlayerInfo->mLevel == 45;
}

//0x453980
bool LawnApp::IsMiniBossLevel()
{
	if (mBoard == nullptr)
		return false;

	return
		(IsAdventureMode() && mPlayerInfo->mLevel == 10) ||
		(IsAdventureMode() && mPlayerInfo->mLevel == 20) ||
		(IsAdventureMode() && mPlayerInfo->mLevel == 30);
}

//0x4539D0
bool LawnApp::IsFinalBossLevel()
{
	if (mBoard == nullptr)
		return false;

	if (mGameMode == GameMode::GAMEMODE_CHALLENGE_FINAL_BOSS)
		return true;

	return IsAdventureMode() && mPlayerInfo->mLevel == 50;
}

//0x453A00
bool LawnApp::IsChallengeWithoutSeedBank()
{
	return 
		mGameMode == GameMode::GAMEMODE_CHALLENGE_RAINING_SEEDS || 
		mGameMode == GameMode::GAMEMODE_UPSELL || 
		mGameMode == GameMode::GAMEMODE_INTRO || 
		IsWhackAZombieLevel() || 
		IsSquirrelLevel() || 
		IsScaryPotterLevel() || 
		mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN || 
		mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM;
}

bool LawnApp::IsNight()
{
	if (IsIceDemo() || mPlayerInfo == nullptr)
		return false;

	return (mPlayerInfo->mLevel >= 11 && mPlayerInfo->mLevel <= 20) || (mPlayerInfo->mLevel >= 31 && mPlayerInfo->mLevel <= 40) || mPlayerInfo->mLevel == 50;
}

int LawnApp::GetCurrentChallengeIndex()
{
	return (int)mGameMode - (int)GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1;
}

ChallengeDefinition& LawnApp::GetCurrentChallengeDef()
{
	return GetChallengeDefinition(GetCurrentChallengeIndex());
}

PottedPlant* LawnApp::GetPottedPlantByIndex(int thePottedPlantIndex)
{
	TOD_ASSERT(thePottedPlantIndex >= 0 && thePottedPlantIndex < mPlayerInfo->mNumPottedPlants);
	return &mPlayerInfo->mPottedPlant[thePottedPlantIndex];
}

//0x453A50
bool LawnApp::UpdateApp()
{
	if (mCloseRequest)
	{
		Shutdown();
		return false;
	}

	//if (mLoadingThreadCompleted)
	//{
	//	LoadingThreadCompleted();
	//}

	bool updated = SexyAppBase::UpdateApp();

	//if (mLoadingThreadCompleted && !mExitToTop)
	//{
	//	CheckForUpdates();
	//}

	return updated;
}

//0x453A70
void LawnApp::CloseRequestAsync()
{
    TfLogWrite("window close requested by user (WM_CLOSE) - this is normal, not a crash");
	mDeferredMessages.clear();
	mExitToTop = true;
	mCloseRequest = true;
}

//0x453A90
SeedType LawnApp::GetAwardSeedForLevel(int theLevel)
{
	int aArea = (theLevel - 1) / LEVELS_PER_AREA + 1;
	int aSub = (theLevel - 1) % LEVELS_PER_AREA + 1;
	int aSeedsHasGot = (aArea - 1) * 8 + aSub;
	if (aSub >= 10)
	{
		aSeedsHasGot -= 2;
	}
	else if (aSub >= 5)
	{
		aSeedsHasGot -= 1;
	}
	if (aSeedsHasGot > 40)
	{
		aSeedsHasGot = 40;
	}
	
	return (SeedType)aSeedsHasGot;
}

//0x453AC0
int LawnApp::GetSeedsAvailable()
{
	// 【三十旗】全卡可用（用户需求：关卡内默认全卡+10槽）。
	// 按玩家冒险进度算可用卡数会让低进度档（1-1 按进入）开始按钮失效
	//（SeedChooserScreen 要求 可用卡数 >= 卡槽数 才响应）。
	if (mGameMode == GameMode::GAMEMODE_THIRTY_FLAGS)
		return SeedType::NUM_SEEDS_IN_CHOOSER;

	int aLevel = mPlayerInfo->GetLevel();
	if (HasFinishedAdventure() || aLevel > NUM_LEVELS)
	{
		return SeedType::NUM_SEEDS_IN_CHOOSER;
	}

	SeedType aSeedTypeMax = GetAwardSeedForLevel(aLevel);
	return min(SeedType::NUM_SEEDS_IN_CHOOSER, aSeedTypeMax);
}

//0x453B20
bool LawnApp::HasSeedType(SeedType theSeedType)
{
	if (mGameMode == GameMode::GAMEMODE_THIRTY_FLAGS)
	{
		return theSeedType >= 0 && theSeedType < SeedType::NUM_SEED_TYPES;
	}

	if (IsTrialStageLocked() && theSeedType >= SeedType::SEED_JALAPENO)
		return false;

	/*  呕
	if (theSeedType >= SeedType::SEED_TWINSUNFLOWER && theSeedType <= SeedType::SEED_IMITATER)
		return mPlayerInfo->mPurchases[theSeedType - SeedType::SEED_GATLINGPEA];
	*/

	if (theSeedType == SeedType::SEED_TWINSUNFLOWER)
	{
		return mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PLANT_TWINSUNFLOWER] > 0;
	}
	if (theSeedType == SeedType::SEED_GLOOMSHROOM)
	{
		return mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PLANT_GLOOMSHROOM] > 0;
	}
	if (theSeedType == SeedType::SEED_CATTAIL)
	{
		return mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PLANT_CATTAIL] > 0;
	}
	if (theSeedType == SeedType::SEED_WINTERMELON)
	{
		return mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PLANT_WINTERMELON] > 0;
	}
	if (theSeedType == SeedType::SEED_GOLD_MAGNET)
	{
		return mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PLANT_GOLD_MAGNET] > 0;
	}
	if (theSeedType == SeedType::SEED_SPIKEROCK)
	{
		return mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PLANT_SPIKEROCK] > 0;
	}
	if (theSeedType == SeedType::SEED_COBCANNON)
	{
		return mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PLANT_COBCANNON] > 0;
	}
	if (theSeedType == SeedType::SEED_IMITATER)
	{
		return mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PLANT_IMITATER] > 0;
	}

	return theSeedType < GetSeedsAvailable();
}

bool LawnApp::SeedTypeAvailable(SeedType theSeedType)
{
	return (theSeedType == SeedType::SEED_GATLINGPEA && mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PLANT_GATLINGPEA]) || HasSeedType(theSeedType);
}

//0x453C30
Reanimation* LawnApp::AddReanimation(float theX, float theY, int theRenderOrder, ReanimationType theReanimationType)
{
	return mEffectSystem->mReanimationHolder->AllocReanimation(theX, theY, theRenderOrder, theReanimationType);
}

//0x453C80
TodParticleSystem* LawnApp::AddTodParticle(float theX, float theY, int theRenderOrder, ParticleEffect theEffect)
{
	return mEffectSystem->mParticleHolder->AllocParticleSystem(theX, theY, theRenderOrder, theEffect);
}

ParticleSystemID LawnApp::ParticleGetID(TodParticleSystem* theParticle)
{
	return (ParticleSystemID)mEffectSystem->mParticleHolder->mParticleSystems.DataArrayGetID(theParticle);
}

ReanimationID LawnApp::ReanimationGetID(Reanimation* theReanimation)
{
	return (ReanimationID)mEffectSystem->mReanimationHolder->mReanimations.DataArrayGetID(theReanimation);
}

TodParticleSystem* LawnApp::ParticleGet(ParticleSystemID theParticleID)
{
	return mEffectSystem->mParticleHolder->mParticleSystems.DataArrayGet((unsigned int)theParticleID);
}

TodParticleSystem* LawnApp::ParticleTryToGet(ParticleSystemID theParticleID)
{
	return mEffectSystem->mParticleHolder->mParticleSystems.DataArrayTryToGet((unsigned int)theParticleID);
}

Reanimation* LawnApp::ReanimationGet(ReanimationID theReanimationID)
{
	return mEffectSystem->mReanimationHolder->mReanimations.DataArrayGet((unsigned int)theReanimationID);
}

//0x453CB0
Reanimation* LawnApp::ReanimationTryToGet(ReanimationID theReanimationID)
{
	return mEffectSystem->mReanimationHolder->mReanimations.DataArrayTryToGet((unsigned int)theReanimationID);
}

//0x453CF0
void LawnApp::RemoveReanimation(ReanimationID theReanimationID)
{
	Reanimation* aReanim = ReanimationTryToGet(theReanimationID);
	if (aReanim)
	{
		aReanim->ReanimationDie();
	}
}

void LawnApp::RemoveParticle(ParticleSystemID theParticleID)
{
	TodParticleSystem* aParticle = ParticleTryToGet(theParticleID);
	if (aParticle)
	{
		aParticle->ParticleSystemDie();
	}
}

//0x453D20
bool LawnApp::AdvanceCrazyDaveText()
{
	SexyString aMessageName = StrFormat(_S("[CRAZY_DAVE_%d]"), mCrazyDaveMessageIndex + 1);
	if (!TodStringListExists(aMessageName))
	{
		return false;
	}

	CrazyDaveTalkIndex(mCrazyDaveMessageIndex + 1);
	return true;
}

//0x453DC0
SexyString LawnApp::GetCrazyDaveText(int theMessageIndex)
{
	SexyString aMessage = StrFormat(_S("[CRAZY_DAVE_%d]"), theMessageIndex);
	aMessage = TodReplaceString(aMessage, _S("{PLAYER_NAME}"), mPlayerInfo->mName);
	aMessage = TodReplaceString(aMessage, _S("{MONEY}"), GetMoneyString(mPlayerInfo->mCoins));
	int aCost = StoreScreen::GetItemCost(StoreItem::STORE_ITEM_PACKET_UPGRADE);
	aMessage = TodReplaceString(aMessage, _S("{UPGRADE_COST}"), GetMoneyString(aCost));
	return aMessage;
}

//0x454070
bool LawnApp::CanShowAlmanac()
{
	if (IsIceDemo())
		return false;

	if (mPlayerInfo == nullptr)
		return false;

	return HasFinishedAdventure() || mPlayerInfo->mLevel >= 15;
}

//0x454090
bool LawnApp::CanShowStore()
{
	if (IsIceDemo())
		return false;

	if (mPlayerInfo == nullptr)
		return false;

	return HasFinishedAdventure() || mPlayerInfo->mHasSeenUpsell || mPlayerInfo->mLevel >= 25;
}

//0x4540C0
bool LawnApp::CanShowZenGarden()
{
	if (mPlayerInfo == nullptr)
		return false;

	if (IsTrialStageLocked())
		return false;

	return HasFinishedAdventure() || mPlayerInfo->mLevel >= 45;
}

bool LawnApp::CanSpawnYetis()
{
	const ZombieDefinition& aZombieDef = GetZombieDefinition(ZombieType::ZOMBIE_YETI);
	return HasFinishedAdventure() && (mPlayerInfo->mFinishedAdventure >= 2 || mPlayerInfo->mLevel >= aZombieDef.mStartingLevel);
}

//0x454120
bool LawnApp::HasBeatenChallenge(GameMode theGameMode)
{
	if (mPlayerInfo == nullptr)
		return false;

	int aChallengeIndex = theGameMode - GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1;
	TOD_ASSERT(aChallengeIndex >= 0 && aChallengeIndex < NUM_CHALLENGE_MODES);
	if (IsSurvivalNormal(theGameMode))
	{
		return mPlayerInfo->mChallengeRecords[aChallengeIndex] >= SURVIVAL_NORMAL_FLAGS;
	}
	if (IsSurvivalHard(theGameMode))
	{
		return mPlayerInfo->mChallengeRecords[aChallengeIndex] >= SURVIVAL_HARD_FLAGS;
	}
	if (IsSurvivalEndless(theGameMode) || IsEndlessScaryPotter(theGameMode) || IsEndlessIZombie(theGameMode))
	{
		return false;
	}
	return mPlayerInfo->mChallengeRecords[aChallengeIndex] > 0;
}

//0x454170
bool LawnApp::HasFinishedAdventure()
{
	return mPlayerInfo && mPlayerInfo->mFinishedAdventure > 0;
}

//0x454190
bool LawnApp::IsFirstTimeAdventureMode()
{
	return IsAdventureMode() && !HasFinishedAdventure();
}

//0x4541B0
void LawnApp::CrazyDaveEnter()
{
	TOD_ASSERT(mCrazyDaveState == CRAZY_DAVE_OFF);
	TOD_ASSERT(!ReanimationTryToGet(mCrazyDaveReanimID));

	Reanimation* aCrazyDaveReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_CRAZY_DAVE);
	aCrazyDaveReanim->mIsAttachment = true;
	aCrazyDaveReanim->SetBasePoseFromAnim("anim_idle_handing");
	mCrazyDaveReanimID = ReanimationGetID(aCrazyDaveReanim);
	aCrazyDaveReanim->PlayReanim("anim_enter", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);

	mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_ENTERING;
	mCrazyDaveMessageIndex = -1;
	mCrazyDaveMessageText.clear();
	mCrazyDaveBlinkCounter = RandRangeInt(400, 800);

	if (mGameScene == GameScenes::SCENE_LEVEL_INTRO && IsStormyNightLevel())
	{
		aCrazyDaveReanim->mColorOverride = Color(64, 64, 64);
	}
}

//0x4542F0
void LawnApp::CrazyDaveDie()
{
	Reanimation* aCrazyDaveReanim = ReanimationTryToGet(mCrazyDaveReanimID);
	if (aCrazyDaveReanim)
	{
		aCrazyDaveReanim->ReanimationDie();

		mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_OFF;
		mCrazyDaveReanimID = ReanimationID::REANIMATIONID_NULL;
		mCrazyDaveMessageIndex = -1;
		mCrazyDaveMessageText.clear();

		CrazyDaveStopSound();
	}
}

//0x454350
void LawnApp::CrazyDaveLeave()
{
	Reanimation* aCrazyDaveReanim = ReanimationTryToGet(mCrazyDaveReanimID);
	if (aCrazyDaveReanim)
	{
		if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_IDLING)
		{
			CrazyDaveDoneHanding();
		}

		aCrazyDaveReanim->PlayReanim("anim_leave", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 24.0f);
		aCrazyDaveReanim->SetImageOverride("Dave_mouths", nullptr);

		mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_LEAVING;
		mCrazyDaveMessageIndex = -1;
		mCrazyDaveMessageText.clear();

		CrazyDaveStopSound();
	}
}

//0x454430
void LawnApp::CrazyDaveTalkIndex(int theMessageIndex)
{
	mCrazyDaveMessageIndex = theMessageIndex;
	SexyString aMessageText = GetCrazyDaveText(theMessageIndex);
	CrazyDaveTalkMessage(aMessageText);
}

//0x4544A0
void LawnApp::CrazyDaveDoneHanding()
{
	Reanimation* aCrazyDaveReanim = ReanimationGet(mCrazyDaveReanimID);
	ReanimatorTrackInstance* aHandTrackInstance = aCrazyDaveReanim->GetTrackInstanceByName("Dave_handinghand");
	AttachmentDie(aHandTrackInstance->mAttachmentID);

	TodTrace("DoneHanding");
}

//0x454520
void LawnApp::CrazyDaveStopSound()
{
	mSoundSystem->StopFoley(FoleyType::FOLEY_CRAZY_DAVE_SHORT);
	mSoundSystem->StopFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
	mSoundSystem->StopFoley(FoleyType::FOLEY_CRAZY_DAVE_EXTRA_LONG);
	mSoundSystem->StopFoley(FoleyType::FOLEY_CRAZY_DAVE_CRAZY);
}

//0x454570
void LawnApp::CrazyDaveTalkMessage(const SexyString& theMessage)
{
	Reanimation* aCrazyDaveReanim = ReanimationGet(mCrazyDaveReanimID);

	bool doHanding = false;
	if (theMessage.find(_S("{HANDING}")) != SexyString::npos)
	{
		doHanding = true;
	}
	if ((mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_IDLING) && !doHanding)
	{
		CrazyDaveDoneHanding();
	}

	bool doSound = true;
	if (theMessage.find(_S("{NO_SOUND}")) != SexyString::npos)
	{
		doSound = false;
	}
	else
	{
		CrazyDaveStopSound();
	}

	int aWordsCount = 0;
	bool isControlWord = false;
	for (int i = 0; i < theMessage.size(); i++)
	{
		if (theMessage[i] == _S('{'))
		{
			isControlWord = true;
		}
		else if (theMessage[i] == _S('}'))
		{
			isControlWord = false;
		}
		else if (!isControlWord)
		{
			aWordsCount++;
		}
	}

	aCrazyDaveReanim->SetImageOverride("Dave_mouths", nullptr);

	if (mCrazyDaveState != CrazyDaveState::CRAZY_DAVE_TALKING || doSound)
	{
		if (doHanding)
		{
			aCrazyDaveReanim->PlayReanim("anim_talk_handing", ReanimLoopType::REANIM_LOOP, 50, 12.0f);

			if (doSound)
			{
				if (theMessage.find(_S("{SHORT_SOUND}")) != SexyString::npos)
				{
					PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_SHORT);
				}
				else if (theMessage.find(_S("{SCREAM}")) != SexyString::npos)
				{
					PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_SCREAM);
				}
				else
				{
					PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
				}
			}
			
			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_TALKING;
		}
		else if (theMessage.find(_S("{SHAKE}")) != SexyString::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_crazy", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 50, 12.0f);

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_CRAZY);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_TALKING;
		}
		else if (theMessage.find(_S("{SCREAM}")) != SexyString::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_smalltalk", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 50, 12.0f);

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_SCREAM);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_TALKING;
		}
		else if (theMessage.find(_S("{SCREAM2}")) != SexyString::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_mediumtalk", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 50, 12.0f);

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_SCREAM_2);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_TALKING;
		}
		else if (theMessage.find(_S("{SHOW_WALLNUT}")) != SexyString::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_talk_handing", ReanimLoopType::REANIM_LOOP, 50, 12.0f);

			Reanimation* aWallnutReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_WALLNUT);
			aWallnutReanim->PlayReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 0, 12.0f);
			TodTrace("Handed");

			ReanimatorTrackInstance* aHandTrackInstance = aCrazyDaveReanim->GetTrackInstanceByName("Dave_handinghand");
			AttachEffect* aAttachEffect = AttachReanim(aHandTrackInstance->mAttachmentID, aWallnutReanim, 100.0f, 393.0f);
			aAttachEffect->mOffset.m00 = 1.2f;
			aAttachEffect->mOffset.m11 = 1.2f;

			aCrazyDaveReanim->Update();

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_SCREAM_2);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_TALKING;
		}
		else if (theMessage.find(_S("{SHOW_HAMMER}")) != SexyString::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_talk_handing", ReanimLoopType::REANIM_LOOP, 50, 12.0f);

			Reanimation* aHammerReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_HAMMER);
			aHammerReanim->PlayReanim("anim_whack_zombie", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);
			aHammerReanim->mAnimTime = 1.0f;

			ReanimatorTrackInstance* aHandTrackInstance = aCrazyDaveReanim->GetTrackInstanceByName("Dave_handinghand");
			AttachEffect* aAttachEffect = AttachReanim(aHandTrackInstance->mAttachmentID, aHammerReanim, 62.0f, 445.0f);
			aAttachEffect->mOffset.m00 = 1.5f;
			aAttachEffect->mOffset.m11 = 1.5f;

			aCrazyDaveReanim->Update();

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_TALKING;
		}
		else if (theMessage.find(_S("{SHOW_FERTILIZER}")) != SexyString::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_talk_handing", ReanimLoopType::REANIM_LOOP, 50, 12.0f);

			Reanimation* aFertilizerReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_ZENGARDEN_FERTILIZER);
			aFertilizerReanim->PlayReanim("bag", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);
			aFertilizerReanim->mAnimRate = 0.0f;

			ReanimatorTrackInstance* aHandTrackInstance = aCrazyDaveReanim->GetTrackInstanceByName("Dave_handinghand");
			AttachReanim(aHandTrackInstance->mAttachmentID, aFertilizerReanim, 102.0f, 412.0f);
			aCrazyDaveReanim->Update();

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_TALKING;
		}
		else if (theMessage.find(_S("{SHOW_TREE_FOOD}")) != SexyString::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_talk_handing", ReanimLoopType::REANIM_LOOP, 50, 12.0f);

			Reanimation* aTreeFoodReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_TREEOFWISDOM_TREEFOOD);
			aTreeFoodReanim->PlayReanim("bag", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);
			aTreeFoodReanim->mAnimRate = 0.0f;

			ReanimatorTrackInstance* aHandTrackInstance = aCrazyDaveReanim->GetTrackInstanceByName("Dave_handinghand");
			AttachReanim(aHandTrackInstance->mAttachmentID, aTreeFoodReanim, 102.0f, 412.0f);
			aCrazyDaveReanim->Update();

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_TALKING;
		}
		else if (theMessage.find(_S("{SHOW_MONEYBAG}")) != SexyString::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_talk_handing", ReanimLoopType::REANIM_LOOP, 50, 12.0f);

			Reanimation* aMoneyBagReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_ZENGARDEN_FERTILIZER);
			aMoneyBagReanim->PlayReanim("bag", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);
			aMoneyBagReanim->mAnimRate = 0.0f;
			aMoneyBagReanim->SetImageOverride("bag", IMAGE_MONEYBAG);

			ReanimatorTrackInstance* aHandTrackInstance = aCrazyDaveReanim->GetTrackInstanceByName("Dave_handinghand");
			AttachReanim(aHandTrackInstance->mAttachmentID, aMoneyBagReanim, 90.0f, 405.0f);
			aCrazyDaveReanim->Update();
			
			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_TALKING;
		}
		else
		{
			if (aWordsCount < 23)
			{
				aCrazyDaveReanim->PlayReanim("anim_smalltalk", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 50, 12.0f);

				if (doSound)
				{
					PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_SHORT);
				}

				mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_TALKING;
			}
			else if (aWordsCount < 52)
			{
				aCrazyDaveReanim->PlayReanim("anim_mediumtalk", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 50, 12.0f);

				if (doSound)
				{
					PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
				}

				mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_TALKING;
			}
			else
			{
				aCrazyDaveReanim->PlayReanim("anim_blahblah", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 50, 12.0f);

				if (doSound)
				{
					PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_EXTRA_LONG);
				}

				mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_TALKING;
			}
		}
	}

	mCrazyDaveMessageText = theMessage;
}

//0x454ED0
void LawnApp::CrazyDaveStopTalking()
{
	bool aDoneHanding = true;
	if (mGameMode == GameMode::GAMEMODE_UPSELL)
	{
		aDoneHanding = false;
	}
	if (aDoneHanding && mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING)
	{
		CrazyDaveDoneHanding();
	}

	Reanimation* aCrazyDaveReanim = ReanimationGet(mCrazyDaveReanimID);
	aCrazyDaveReanim->SetImageOverride("Dave_mouths", nullptr);
	if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING && !aDoneHanding)
	{
		aCrazyDaveReanim->PlayReanim("anim_idle_handing", ReanimLoopType::REANIM_LOOP, 20, 12.0f);
		mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_IDLING;
	}
	else if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_TALKING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING)
	{
		aCrazyDaveReanim->PlayReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 20, 12.0f);
		mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_IDLING;
	}

	mCrazyDaveMessageIndex = -1;
	mCrazyDaveMessageText.clear();
	CrazyDaveStopSound();
}

//0x455040
void LawnApp::UpdateCrazyDave()
{
	Reanimation* aCrazyDaveReanim = ReanimationTryToGet(mCrazyDaveReanimID);
	if (aCrazyDaveReanim == nullptr)
		return;

	if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_ENTERING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_TALKING)
	{
		if (aCrazyDaveReanim->mLoopCount > 0)
		{
			aCrazyDaveReanim->PlayReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 20, 12.0f);
			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_IDLING;
		}
	}
	else if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING)
	{
		if (aCrazyDaveReanim->mLoopCount > 0)
		{
			aCrazyDaveReanim->PlayReanim("anim_idle_handing", ReanimLoopType::REANIM_LOOP, 20, 12.0f);
			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_IDLING;
		}
	}
	else if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_LEAVING && aCrazyDaveReanim->mLoopCount > 0)
	{
		CrazyDaveDie();
	}

	if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_IDLING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_IDLING)
	{
		if (mCrazyDaveMessageText.find(_S("{MOUTH_BIG_SMILE}")) != SexyString::npos)
		{
			aCrazyDaveReanim->SetImageOverride("Dave_mouths", IMAGE_REANIM_CRAZYDAVE_MOUTH1);
		}
		else if (mCrazyDaveMessageText.find(_S("{MOUTH_SMALL_SMILE}")) != SexyString::npos)
		{
			aCrazyDaveReanim->SetImageOverride("Dave_mouths", IMAGE_REANIM_CRAZYDAVE_MOUTH5);
		}
		else if (mCrazyDaveMessageText.find(_S("{MOUTH_BIG_OH}")) != SexyString::npos)
		{
			aCrazyDaveReanim->SetImageOverride("Dave_mouths", IMAGE_REANIM_CRAZYDAVE_MOUTH4);
		}
		else if (mCrazyDaveMessageText.find(_S("{MOUTH_SMALL_OH}")) != SexyString::npos)
		{
			aCrazyDaveReanim->SetImageOverride("Dave_mouths", IMAGE_REANIM_CRAZYDAVE_MOUTH6);
		}
	}

	if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_IDLING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_TALKING || 
		mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_IDLING)
	{
		mCrazyDaveBlinkCounter--;
		if (mCrazyDaveBlinkCounter <= 0)
		{
			mCrazyDaveBlinkCounter = RandRangeInt(400, 800);
			Reanimation* aBlinkReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_CRAZY_DAVE);
			aBlinkReanim->SetFramesForLayer("anim_blink");
			aBlinkReanim->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_FULL_LAST_FRAME_AND_HOLD;
			aBlinkReanim->mAnimRate = 15.0f;
			aBlinkReanim->AttachToAnotherReanimation(aCrazyDaveReanim, "Dave_head");
			aBlinkReanim->mColorOverride = aCrazyDaveReanim->mColorOverride;
			aCrazyDaveReanim->AssignRenderGroupToTrack("Dave_eye", RENDER_GROUP_HIDDEN);
			mCrazyDaveBlinkReanimID = ReanimationGetID(aBlinkReanim);
		}
	}

	Reanimation* aBlinkReanim = ReanimationTryToGet(mCrazyDaveBlinkReanimID);
	if (aBlinkReanim && aBlinkReanim->mLoopCount > 0)
	{
		aCrazyDaveReanim->AssignRenderGroupToTrack("Dave_eye", RENDER_GROUP_NORMAL);
		RemoveReanimation(mCrazyDaveBlinkReanimID);
		mCrazyDaveBlinkReanimID = ReanimationID::REANIMATIONID_NULL;
	}

	aCrazyDaveReanim->Update();
}

//0x4552F0
void LawnApp::DrawCrazyDave(Graphics* g)
{
	Reanimation* aCrazyDaveReanim = ReanimationTryToGet(mCrazyDaveReanimID);
	if (aCrazyDaveReanim == nullptr)
		return;

	if (mCrazyDaveMessageText.size())
	{
		Image* aBubbleImage = IMAGE_STORE_SPEECHBUBBLE2;
		int aPosX = 285;
		int aPosY = 20;
		if (GetDialog(Dialogs::DIALOG_STORE))
		{
			aBubbleImage = IMAGE_STORE_SPEECHBUBBLE;
			aPosX -= 180;
			aPosY -= 78;
		}
		else if (mGameMode == GameMode::GAMEMODE_UPSELL)
		{
			aPosX += 130;
			aPosY += 70;
		}
		g->DrawImage(aBubbleImage, aPosX, aPosY);

		SexyString aBubbleText = mCrazyDaveMessageText;
		Rect aRect(aPosX + 25, aPosY + 6, 233, 144);
		if (aBubbleText.find(_S("{SHAKE}")) != SexyString::npos)
		{
			aBubbleText = TodReplaceString(aBubbleText, _S("{SHAKE}"), _S(""));
			aRect.mX += rand() % 2;
			aRect.mY += rand() % 2;
		}

		bool clickToContinue = true;
		if (mGameMode == GameMode::GAMEMODE_UPSELL)
		{
			clickToContinue = false;
		}
		else if (aBubbleText.find(_S("{NO_CLICK}")) != SexyString::npos)
		{
			aBubbleText = TodReplaceString(aBubbleText, _S("{NO_CLICK}"), _S(""));
			clickToContinue = false;
		}

		TodDrawStringWrapped(g, aBubbleText, aRect, FONT_BRIANNETOD16, Color::Black, DrawStringJustification::DS_ALIGN_CENTER_VERTICAL_MIDDLE);
		if (clickToContinue)
		{
			TodDrawString(g, _S("click to continue"), aPosX + 139, aPosY + 140, FONT_PICO129, Color::Black, DrawStringJustification::DS_ALIGN_CENTER);
		}
	}

	aCrazyDaveReanim->Draw(g);
}

//0x455670
int LawnApp::GetNumPreloadingTasks()
{
	int aTaskCount = 10;
	if (mPlayerInfo)
	{
		for (SeedType i = SeedType::SEED_PEASHOOTER; i < SeedType::NUM_SEED_TYPES; i = (SeedType)((int)i + 1))
		{
			if (SeedTypeAvailable(i) || HasFinishedAdventure())
			{
				aTaskCount++;
			}
		}

		for (ZombieType i = ZombieType::ZOMBIE_NORMAL; i < ZombieType::NUM_ZOMBIE_TYPES; i = (ZombieType)((int)i + 1))
		{
			if (HasFinishedAdventure() || mPlayerInfo->mLevel >= GetZombieDefinition(i).mStartingLevel)
			{
				if (i != ZombieType::ZOMBIE_BOSS &&
					i != ZombieType::ZOMBIE_CATAPULT &&
					i != ZombieType::ZOMBIE_GARGANTUAR &&
					i != ZombieType::ZOMBIE_DIGGER &&
					i != ZombieType::ZOMBIE_ZAMBONI)
				{
					aTaskCount++;
				}
			}
		}
	}
	return aTaskCount * 68;
}

//0x455720
void LawnApp::PreloadForUser()
{
	int aNumTasks = mCompletedLoadingThreadTasks + GetNumPreloadingTasks();
	if (mTitleScreen && mTitleScreen->mQuickLoadKey != KeyCode::KEYCODE_UNKNOWN)
	{
		TodTrace("preload canceled\n");
		mCompletedLoadingThreadTasks = aNumTasks;
		return;
	}

	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_PUFF, true);
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_LAWN_MOWERED_ZOMBIE, true);
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_READYSETPLANT, true);
	mCompletedLoadingThreadTasks += 68;
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_FINAL_WAVE, true);
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_SUN, true);
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_TEXT_FADE_ON, true);
	mCompletedLoadingThreadTasks += 68;
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIE, true);
	mCompletedLoadingThreadTasks += 68;
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIE_NEWSPAPER, true);
	mCompletedLoadingThreadTasks += 68;
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_SELECTOR_SCREEN, true);
	mCompletedLoadingThreadTasks += 340;
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIE_HAND, true);
	mCompletedLoadingThreadTasks += 68;

	if (mPlayerInfo)
	{
		for (SeedType i = SeedType::SEED_PEASHOOTER; i < SeedType::NUM_SEED_TYPES; i = (SeedType)((int)i + 1))
		{
			if (SeedTypeAvailable(i) || HasFinishedAdventure())
			{
				Plant::PreloadPlantResources(i);
				if (mCompletedLoadingThreadTasks < aNumTasks)
				{
					mCompletedLoadingThreadTasks += 68;
				}

				if (mTitleScreen && mTitleScreen->mQuickLoadKey != KeyCode::KEYCODE_UNKNOWN)
				{
					TodTrace("preload canceled\n");
					mCompletedLoadingThreadTasks = aNumTasks;
					return;
				}

				if (mShutdown || mCloseRequest)
				{
					return;
				}
			}
		}

		for (ZombieType i = ZombieType::ZOMBIE_NORMAL; i < ZombieType::NUM_ZOMBIE_TYPES;i = (ZombieType)((int)i + 1))
		{
			if (!HasFinishedAdventure() && mPlayerInfo->mLevel < GetZombieDefinition(i).mStartingLevel)
			{
				continue;
			}
			if (i == ZombieType::ZOMBIE_BOSS || i == ZombieType::ZOMBIE_CATAPULT || i == ZombieType::ZOMBIE_GARGANTUAR ||
				i == ZombieType::ZOMBIE_DIGGER || i == ZombieType::ZOMBIE_ZAMBONI)
			{
				continue;
			}

			Zombie::PreloadZombieResources(i);
			if (mCompletedLoadingThreadTasks < aNumTasks)
			{
				mCompletedLoadingThreadTasks += 68;
			}

			if (mTitleScreen && mTitleScreen->mQuickLoadKey != KeyCode::KEYCODE_UNKNOWN)
			{
				TodTrace("preload canceled\n");
				mCompletedLoadingThreadTasks = aNumTasks;
				return;
			}

			if (mShutdown || mCloseRequest)
			{
				return;
			}
		}
	}

	if (mCompletedLoadingThreadTasks != aNumTasks)
	{
		TodTrace("num preload tasks wasn't calculated correctly\n");
		mCompletedLoadingThreadTasks = aNumTasks;
	}
}

//0x455930
void LawnApp::EnforceCursor()
{
	if (mSEHOccured || !mMouseIn)
	{
		::SetCursor(LoadCursor(NULL, IDC_ARROW));
		return;
	}

	if (mOverrideCursor)
	{
		::SetCursor(mOverrideCursor);
		return;
	}

	switch (mCursorNum)
	{
	case CURSOR_POINTER:
		::SetCursor(LoadCursor(GetModuleHandle(NULL), MAKEINTRESOURCE(IDC_CURSOR1)));
		return;

	case CURSOR_HAND:
		::SetCursor(mHandCursor);
		return;

	case CURSOR_TEXT:
		::SetCursor(LoadCursor(NULL, IDC_IBEAM));
		return;

	case CURSOR_DRAGGING:
		::SetCursor(mDraggingCursor);
		return;

	case CURSOR_CIRCLE_SLASH:
		::SetCursor(LoadCursor(NULL, IDC_NO));
		return;

	case CURSOR_SIZEALL:
		::SetCursor(LoadCursor(NULL, IDC_SIZEALL));
		return;

	case CURSOR_SIZENESW:
		::SetCursor(LoadCursor(NULL, IDC_SIZENESW));
		return;

	case CURSOR_SIZENS:
		::SetCursor(LoadCursor(NULL, IDC_SIZENS));
		return;

	case CURSOR_SIZENWSE:
		::SetCursor(LoadCursor(NULL, IDC_SIZENWSE));
		return;

	case CURSOR_SIZEWE:
		::SetCursor(LoadCursor(NULL, IDC_SIZEWE));
		return;

	case CURSOR_WAIT:
		::SetCursor(LoadCursor(NULL, IDC_WAIT));
		return;

	case CURSOR_CUSTOM:
		::SetCursor(NULL);
		return;

	case CURSOR_NONE:
		::SetCursor(NULL);
		return;

	default:
		::SetCursor(LoadCursor(NULL, IDC_ARROW));
		return;
	}
}

//0x455AA0
SexyString LawnApp::Pluralize(int theCount, const SexyChar* theSingular, const SexyChar* thePlural)
{
	if (theCount == 1)
	{
		return TodReplaceNumberString(theSingular, _S("{COUNT}"), theCount);
	}

	return TodReplaceNumberString(thePlural, _S("{COUNT}"), theCount);
}

//0x455BA0
int LawnApp::GetNumTrophies(ChallengePage thePage)
{
	int aNumTrophies = 0;

	for (int i = 0; i < NUM_CHALLENGE_MODES; i++)
	{
		const ChallengeDefinition& aDef = GetChallengeDefinition(i);
		if (aDef.mPage == thePage && HasBeatenChallenge(aDef.mChallengeMode))
		{
			aNumTrophies++;
		}
	}

	return aNumTrophies;
}

//0x455C20
int LawnApp::TrophiesNeedForGoldSunflower()
{
	return 48 - GetNumTrophies(CHALLENGE_PAGE_SURVIVAL) - GetNumTrophies(CHALLENGE_PAGE_CHALLENGE) - GetNumTrophies(CHALLENGE_PAGE_PUZZLE);
}

//0x455C50
bool LawnApp::EarnedGoldTrophy()
{
	return HasFinishedAdventure() && TrophiesNeedForGoldSunflower() <= 0;
}

void LawnApp::FinishZenGardenToturial()
{
	mBoardResult = BoardResult::BOARDRESULT_WON;
	KillBoard();
	PreNewGame(GameMode::GAMEMODE_ADVENTURE, false);
}

//0x455C90
bool LawnApp::IsTrialStageLocked()
{
	if (mDebugTrialLocked)
		return true;

	if (mDRM && mDRM->QueryData())
		return false;

	return mTrialType == TrialType::TRIALTYPE_STAGELOCKED;
}

//0x455CC0
void LawnApp::InitHook()
{
#ifdef _DEBUG
	mDRM = nullptr;
#else
	mDRM = new PopDRMComm();
	mDRM->Connect();
	if (sexystricmp(GetString("MarketingMode", _S("")).c_str(), _S("StageLocked")) == 0)
	{
		mTrialType = TrialType::TRIALTYPE_STAGELOCKED;
		mDRM->EnableLocking();
	}
	else
	{
		mTrialType = TrialType::TRIALTYPE_NONE;
	}
#endif
}

//0x455E10
SexyString LawnApp::GetMoneyString(int theAmount)
{
	int aValue = theAmount * 10;
	if (aValue > 999999)
	{
		return StrFormat(_S("$%d,%03d,%03d"), aValue / 1000000, (aValue - aValue / 1000000 * 1000000) / 1000, aValue - aValue / 1000 * 1000);
	}
	else if (aValue > 9999)
	{
		return StrFormat(_S("$%d,%03d"), aValue / 1000, aValue - aValue / 1000 * 1000);
	}
	else
	{
		return StrFormat(_S("$%d"), aValue);
	}
}

//0x455EE0
SexyString LawnGetCurrentLevelName()
{
	if (gLawnApp == nullptr)
	{
		return _S("Before App");
	}
	if (gLawnApp->mGameScene == GameScenes::SCENE_LOADING)
	{
		return _S("Game Loading");
	}
	if (gLawnApp->mGameScene == GameScenes::SCENE_MENU)
	{
		return _S("Game Selector");
	}
	if (gLawnApp->mGameScene == GameScenes::SCENE_AWARD)
	{
		return _S("Award Screen");
	}
	if (gLawnApp->mGameScene == GameScenes::SCENE_CHALLENGE)
	{
		return _S("Challenge Screen");
	}
	if (gLawnApp->mGameScene == GameScenes::SCENE_CREDIT)
	{
		return _S("Credits");
	}
	if (gLawnApp->mBoard == nullptr)
	{
		return _S("Not Playing");
	}

	if (gLawnApp->IsFirstTimeAdventureMode())
	{
		return gLawnApp->GetStageString(gLawnApp->mBoard->mLevel);
	}
	if (gLawnApp->IsAdventureMode())
	{
		return StrFormat(_S("F%d"), gLawnApp->GetStageString(gLawnApp->mBoard->mLevel).c_str());
	}

	return gLawnApp->GetCurrentChallengeDef().mChallengeName;
}

//0x456060
bool LawnApp::CanDoPinataMode()
{
	if (mPlayerInfo == nullptr)
		return false;

	return mPlayerInfo->mChallengeRecords[(int)GameMode::GAMEMODE_TREE_OF_WISDOM - (int)GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1] >= 1000;
}

//0x456080
bool LawnApp::CanDoDanceMode()
{
	if (mPlayerInfo == nullptr)
		return false;

	return mPlayerInfo->mChallengeRecords[(int)GameMode::GAMEMODE_TREE_OF_WISDOM - (int)GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1] >= 500;
}

//0x4560A0
bool LawnApp::CanDoDaisyMode()
{
	if (mPlayerInfo == nullptr)
		return false;

	return mPlayerInfo->mChallengeRecords[(int)GameMode::GAMEMODE_TREE_OF_WISDOM - (int)GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1] >= 100;
}

//0x4560C0
void LawnApp::PlaySample(int theSoundNum)
{
	if (!mMuteSoundsForCutscene)
	{
		SexyAppBase::PlaySample(theSoundNum);
	}
}

//0x4560E0
void LawnApp::SwitchScreenMode(bool wantWindowed, bool is3d, bool force)
{
	SexyAppBase::SwitchScreenMode(wantWindowed, is3d, force);

	NewOptionsDialog* aNewOptionsDialog = (NewOptionsDialog*)GetDialog(Dialogs::DIALOG_NEWOPTIONS);
	if (aNewOptionsDialog)
	{
		aNewOptionsDialog->mFullscreenCheckbox->SetChecked(!mIsWindowed);
	}
}

/* #################################################################################################### */

void LawnApp::BetaSubmit(bool theAskForComments)
{
#if defined(_DEBUG) && FALSE
	if (gIsPartnerBuild || !mBetaSupport || mPlayingDemoBuffer)
		return;

	int oldCount = mUpdateCount;
	std::list<std::string> aUploadFileList;
	
	WIN32_FIND_DATA aData;
	HANDLE aHandle = FindFirstFile((GetAppDataFolder() + "userdata/*.txt").c_str(), &aData);
	if (aHandle != INVALID_HANDLE_VALUE)
	{
		do
		{
			std::string aFileName = GetAppDataFolder() + "userdata/" + aData.cFileName;
			BetaAddFile(aUploadFileList, aFileName, aData.cFileName);
		}
		while (FindNextFile(aHandle, &aData));
		FindClose(aHandle);
	}

	aHandle = FindFirstFile((GetAppDataFolder() + "userdata/*.dmp").c_str(), &aData);
	if (aHandle != INVALID_HANDLE_VALUE)
	{
		do
		{
			std::string aFileName = GetAppDataFolder() + "userdata/" + aData.cFileName;
			BetaAddFile(aUploadFileList, aFileName, aData.cFileName);
		}
		while (FindNextFile(aHandle, &aData));
		FindClose(aHandle);
	}

	aHandle = FindFirstFile((GetAppDataFolder() + "userdata/*.jpg").c_str(), &aData);
	if (aHandle != INVALID_HANDLE_VALUE)
	{
		do
		{
			std::string aFileName = GetAppDataFolder() + "userdata/" + aData.cFileName;
			BetaAddFile(aUploadFileList, aFileName, aData.cFileName);
		} while (FindNextFile(aHandle, &aData));
		FindClose(aHandle);
	}

	mRecordingDemoBuffer = true;
	mDemoBuffer.Clear();
	mDemoMarkerList.clear();

	TodTrace("Upload Started");

	bool aUploading = mBetaSupport->RequestDemoUpload("");
	if (theAskForComments)
	{
		mBetaSupport->RequestComments();
	}

	if (aUploading)
	{
		for (int i = 0; i < 1000; i++)
		{
			WinInetHTTPTransfer::EResult aResult = mBetaSupport->mWinInetHTTPTransfer.GetResultCode();

			if (aResult == WinInetHTTPTransfer::EResult::RESULT_NOT_COMPLETED || aResult == WinInetHTTPTransfer::EResult::RESULT_NOT_STARTED)
			{
				Sleep(50);
			}
			else
			{
				if (aResult == WinInetHTTPTransfer::EResult::RESULT_DONE)
				{
					TodTrace("Upload Complete");
					
					for (auto it = aUploadFileList.begin(); it != aUploadFileList.end(); it++)
					{
						unlink(it->c_str());
					}
				}
				else
				{
					TodTrace("Upload Failed %d", aResult);
				}

				break;
			}
		}
	}
#endif
}

void LawnApp::BetaRecordLevelStats()
{
#if defined(_DEBUG) && FALSE
	if (gIsPartnerBuild || !mPlayerInfo || !mBoard)
		return;

	if (mGameScene != GameScenes::SCENE_PLAYING && mGameScene != GameScenes::SCENE_ZOMBIES_WON)
		return;

	std::string aUserStatFile = StrFormat("%sstat%d.txt", gDebugDataFolder, mPlayerInfo->mId);

	FILE* aFileHandle = fopen(aUserStatFile.c_str(), "a");
	if (aFileHandle == nullptr)
	{
		TodTraceAndLog("Failed to open stat file\n");
		return;
	}

	std::string aStatLine = "";
	std::string aUsersFilename = GetAppDataFolder() + "userdata/users.dat";

	WIN32_FILE_ATTRIBUTE_DATA info;
	GetFileAttributesEx(aUsersFilename.c_str(), GET_FILEEX_INFO_LEVELS::GetFileExInfoStandard, &info);
	int aInstallationID = info.ftCreationTime.dwLowDateTime % 65535;

	aStatLine += mBetaSupport->mName;
	aStatLine += '\t';
	aStatLine += StrFormat("%u", mSessionID);
	aStatLine += '\t';
	aStatLine += StrFormat("%u", mBoard->mGameID);
	aStatLine += '\t';
	aStatLine += StrFormat("PvZ_%s", mProductVersion.c_str());
	aStatLine += '\t';

	__time64_t aTime;
	_time64(&aTime);

	char aTimeString[80];
	strftime(aTimeString, sizeof(aTimeString), "%m/%d/%y %H:%M:%S", _localtime64(&aTime));

	aStatLine += aTimeString;
	aStatLine += '\t';

	if (IsFirstTimeAdventureMode() || (mBoard->mLevel == FINAL_LEVEL && mPlayerInfo->mFinishedAdventure == 1))
	{
		aStatLine += "AdventureFirst";
		aStatLine += '\t';

		aStatLine += StrFormat("L%s", GetStageString(mBoard->mLevel).c_str());
		aStatLine += '\t';
	}
	else if (IsAdventureMode())
	{
		aStatLine += "AdventureCont";
		aStatLine += '\t';

		aStatLine += StrFormat("L%s", GetStageString(mBoard->mLevel).c_str());
		aStatLine += '\t';
	}
	else if (IsSurvivalMode())
	{
		aStatLine += "Survival";
		aStatLine += '\t';

		aStatLine += SexyStringToStringFast(GetCurrentChallengeDef().mChallengeName);
		aStatLine += '\t';
	}
	else if (IsScaryPotterLevel())
	{
		aStatLine += "Vasebreaker";
		aStatLine += '\t';

		aStatLine += SexyStringToStringFast(GetCurrentChallengeDef().mChallengeName);
		aStatLine += '\t';
	}
	else
	{
		aStatLine += "Challenge";
		aStatLine += '\t';

		aStatLine += SexyStringToStringFast(GetCurrentChallengeDef().mChallengeName);
		aStatLine += '\t';
	}

	switch (mBoardResult)
	{
	case BoardResult::BOARDRESULT_WON:				aStatLine += "Won";					break;
	case BoardResult::BOARDRESULT_RESTART:			aStatLine += "Restart";				break;
	case BoardResult::BOARDRESULT_QUIT:				aStatLine += "Quit";				break;
	case BoardResult::BOARDRESULT_QUIT_APP:			aStatLine += "Quit-App";			break;
	case BoardResult::BOARDRESULT_CHEAT:			aStatLine += "Quit-Cheat";			break;
	case BoardResult::BOARDRESULT_LOST:				aStatLine += "Lost";				break;
	default:										aStatLine += "Unknown";				break;
	}
	aStatLine += '\t';

	if (IsSurvivalMode())
	{
		aStatLine += StrFormat("%u", mBoard->GetSurvivalFlagsCompleted());
	}
	else if (IsEndlessScaryPotter(mGameMode) || IsEndlessIZombie(mGameMode))
	{
		aStatLine += StrFormat("%u", mBoard->mChallenge->mSurvivalStage);
	}
	aStatLine += '\t';

	Zombie* aWinningZombie = mBoard->GetWinningZombie();
	if (mBoardResult == BoardResult::BOARDRESULT_LOST && aWinningZombie)
	{
		aStatLine += SexyStringToStringFast(GetZombieDefinition(aWinningZombie->mZombieType).mZombieName);
		aStatLine += '\t';

		aStatLine += StrFormat("%d", aWinningZombie->mRow + 1);
		aStatLine += '\t';
	}
	else
	{
		aStatLine += "\t\t";
	}

	aStatLine += StrFormat("%d", mBoard->mCurrentWave);
	aStatLine += '\t';

	for (int i = 0; i < SEEDBANK_MAX; i++)
	{
		SeedPacket& aSeedPacket = mBoard->mSeedBank->mSeedPackets[i];

		if (aSeedPacket.mPacketType >= 0 && aSeedPacket.mPacketType < SeedType::NUM_SEED_TYPES)
		{
			aStatLine += SexyStringToStringFast(Plant::GetNameString(aSeedPacket.mPacketType));
		}
		else
		{
			aStatLine += '-';
		}
		aStatLine += '\t';

		aStatLine += StrFormat("%u", aSeedPacket.mTimesUsed);
		aStatLine += '\t';
	}

	aStatLine += StrFormat("%u", mBoard->mPlayTimeActiveLevel);
	aStatLine += '\t';
	aStatLine += StrFormat("%u", mBoard->mPlayTimeInactiveLevel);
	aStatLine += '\t';
	aStatLine += StrFormat("%u", mPlayTimeActiveSession);
	aStatLine += '\t';
	aStatLine += StrFormat("%u", mPlayTimeInactiveSession);
	aStatLine += '\t';
	aStatLine += StrFormat("%u", mPlayerInfo->mPlayTimeActivePlayer);
	aStatLine += '\t';
	aStatLine += StrFormat("%u", mPlayerInfo->mPlayTimeInactivePlayer);
	aStatLine += '\t';
	aStatLine += StrFormat("%d", mBoard->mMaxSunPlants);
	aStatLine += '\t';
	aStatLine += StrFormat("%d", mBoard->mPlantsEaten);
	aStatLine += '\t';
	aStatLine += StrFormat("%d", mBoard->mPlantsShoveled);
	aStatLine += '\t';
	aStatLine += StrFormat("%d", mBoard->mGravesCleared);
	aStatLine += '\t';
	aStatLine += StrFormat("%d", mBoard->mTriggeredLawnMowers);
	aStatLine += '\t';
	aStatLine += StrFormat("%d", mBoard->mSunMoney);
	aStatLine += '\t';
	aStatLine += SexyStringToStringFast(GetMoneyString(mPlayerInfo->mCoins));
	aStatLine += '\t';
	aStatLine += SexyStringToStringFast(GetMoneyString(mBoard->mCoinsCollected));
	aStatLine += '\t';

	int aDrawTime = GetTickCount() - mBoard->mStartDrawTime;

	float aAveFPS = 0.0f;
	if (aDrawTime > 0)
	{
		aAveFPS = (mDrawCount * (float)CLOCKS_PER_SEC + (float)CLOCKS_PER_SEC * 0.5f) / aDrawTime;
	}
	aStatLine += StrFormat("%.2f", aAveFPS);
	aStatLine += '\t';

	if (mBoard->mMinFPS != (float)CLOCKS_PER_SEC)
	{
		aStatLine += StrFormat("%.2f", mBoard->mMinFPS);
	}
	aStatLine += '\t';

	aStatLine += Is3DAccelerated() ? "Hardware" : "Software";
	aStatLine += '\t';
	aStatLine += mIsWindowed ? "Windowed" : "Full-screen";
	aStatLine += '\t';
	aStatLine += StrFormat("%u", mBoard->mPreloadTime);
	aStatLine += '\t';
	aStatLine += mPlayerInfo->mHasUsedCheatKeys ? "1" : "0";
	aStatLine += '\t';
	aStatLine += StrFormat("%d_%u", mPlayerInfo->mId, aInstallationID);
	aStatLine += '\t';
	aStatLine += StrFormat("%d", mBoard->mDiamondsCollected);
	aStatLine += '\t';
	aStatLine += StrFormat("%d", mBoard->mPottedPlantsCollected);
	aStatLine += '\t';
	aStatLine += StrFormat("%d", mBoard->mChocolateCollected);
	aStatLine += '\t';

	if (IsAdventureMode())
	{
		aStatLine += StrFormat("%d", mPlayerInfo->mFinishedAdventure);
		aStatLine += '\t';
	}
	else
	{
		int aChallengeIndex = GetCurrentChallengeIndex();
		TOD_ASSERT(aChallengeIndex >= 0 && aChallengeIndex < NUM_CHALLENGE_MODES);

		aStatLine += StrFormat("%d", mPlayerInfo->mChallengeRecords[aChallengeIndex]);
		aStatLine += '\t';
	}

	aStatLine += '\n';

	if (fwrite(aStatLine.c_str(), aStatLine.size() * sizeof(decltype(aStatLine)::value_type), 1U, aFileHandle) != 1U)
	{
		TodTraceAndLog("Failed to write to log file\n");
	}
	fclose(aFileHandle);
#endif
}

void LawnApp::BetaAddFile(std::list<std::string>& theUploadFileList, std::string theFileName, std::string theShortName)
{
#if defined(_DEBUG) && FALSE
	Buffer aBuffer;
	if (!gSexyApp->ReadBufferFromFile(theFileName, &aBuffer, false))
	{
		TodTraceAndLog("Failed to read stat file %s", theFileName.c_str());
		return;
	}

	theUploadFileList.push_front(theFileName);
	mBetaSupport->StoreDataLog(theShortName, aBuffer);
#endif
}

void LawnApp::TraceLoadGroup(const char* theGroupName, int theGroupTime, int theTotalGroupWeigth, int theTaskWeight)
{

}

/* #################################################################################################### */

void LawnApp::DoHighScoreDialog()
{
	
}

void LawnApp::DoRegister()
{

}

void LawnApp::DoRegisterError()
{

}

bool LawnApp::CanDoRegisterDialog()
{
	return false;
}

void LawnApp::DoNeedRegisterDialog()
{

}

void LawnApp::FinishModelessDialogs()
{

}

bool LawnApp::NeedRegister()
{
	return false;
}

void LawnApp::UpdateRegisterInfo()
{

}


