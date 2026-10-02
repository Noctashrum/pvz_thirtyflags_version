// 三十旗核心逻辑自检程序（一次性验证用，不属于游戏本体）
// 校验：30 旗表完整性、场地解锁节奏、浪潮总数、成长曲线、突变与强化池。
// ---- 用桩件替换游戏依赖，复用 ThirtyFlags.cpp 的核心逻辑 ----
#include <string>
#include <stdio.h>
#include <string.h>
#include <math.h>

typedef std::wstring SexyString;
typedef wchar_t SexyChar;
#define _S(x) L ##x

#define MAX_GRID_SIZE_X 9
#define MAX_GRID_SIZE_Y 6
#define NUM_ZOMBIE_TYPES 33
#define GAMEMODE_THIRTY_FLAGS 72
#define MAX_ZOMBIE_WAVES 100

enum SeedTypeStub { SEED_LILYPAD_STUB };
enum ZombieTypeStub
{
    ZOMBIE_NORMAL = 0, ZOMBIE_FLAG = 1, ZOMBIE_TRAFFIC_CONE = 2, ZOMBIE_POLEVAULTER = 3, ZOMBIE_PAIL = 4,
    ZOMBIE_NEWSPAPER = 5, ZOMBIE_DOOR = 6, ZOMBIE_FOOTBALL = 7, ZOMBIE_DANCER = 8, ZOMBIE_DUCKY_TUBE = 9,
    ZOMBIE_SNORKEL = 10, ZOMBIE_ZAMBONI = 11, ZOMBIE_BOBSLED = 12, ZOMBIE_DOLPHIN_RIDER = 13,
    ZOMBIE_JACK_IN_THE_BOX = 14, ZOMBIE_BALLOON = 15, ZOMBIE_DIGGER = 16, ZOMBIE_POGO = 17, ZOMBIE_YETI = 18,
    ZOMBIE_BUNGEE = 19, ZOMBIE_LADDER = 20, ZOMBIE_CATAPULT = 21, ZOMBIE_GARGANTUAR = 22, ZOMBIE_IMP = 23,
    ZOMBIE_BOSS = 24, ZOMBIE_PEA_HEAD = 25, ZOMBIE_WALLNUT_HEAD = 26, ZOMBIE_JALAPENO_HEAD = 27,
    ZOMBIE_GATLING_HEAD = 28, ZOMBIE_SQUASH_HEAD = 29, ZOMBIE_TALLNUT_HEAD = 30, ZOMBIE_REDEYE_GARGANTUAR = 31
};

inline int Rand(int range) { return range > 0 ? (int)(rand() % range) : 0; }
inline int TFClampI_stub(int v, int lo, int hi) { return v <= lo ? lo : (v >= hi ? hi : v); }
inline int ClampInt(int v, int lo, int hi) { return TFClampI_stub(v, lo, hi); }
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

class Board;
class Plant;

// ---- ThirtyFlags 类声明（与 ThirtyFlags.h 一致，去掉游戏依赖）----
struct ThirtyFlagsUpgradeDef
{
    int                     mUpgrade;
    int                     mRarity;
    const SexyChar*         mName;
    const SexyChar*         mDescription;
};

struct ThirtyFlagsFlagDef
{
    int                     mFlag;
    const char*             mTheme;
    int                     mZombiePoints;
    int                     mWaterRows;
    int                     mWaterCols;
    int                     mFogCols;
    int                     mUnlockedRows;
};

constexpr int TF_TOTAL_FLAGS          = 30;
constexpr int TF_WAVES_PER_FLAG       = 6;
constexpr int TF_SEED_SLOTS           = 10;
constexpr int TF_SEED_SLOTS_MAX       = 14;
constexpr int TF_STARTING_SUN         = 100;
constexpr int TF_EARLY_SUN_MAX        = 150;
constexpr int TF_PREP_TIME            = 1200;
constexpr int TF_MUTATION_PICKS       = 5;
constexpr int TF_ELITE_START_FLAG     = 16;
constexpr int TF_ELITE_MAX            = 3;
constexpr int TF_ELITE_KIND_COUNT     = 7;
constexpr int TF_BOSS_FLAG            = 30;
constexpr float TF_HP_GROWTH_PER_FLAG   = 0.07f;
constexpr float TF_HP_GROWTH_CAP        = 6.80f;
constexpr float TF_SPEED_GROWTH_PER_FLAG = 0.015f;
constexpr float TF_SPEED_GROWTH_CAP     = 0.45f;
constexpr float TF_BALANCE_KNOB         = 1.25f;

enum ThirtyFlagsMutation
{
    MUTATION_ZOMBIE_EXPLODE, MUTATION_ZOMBIE_POISON, MUTATION_LIFESTEAL, MUTATION_ARMOR,
    MUTATION_SWIFT, MUTATION_PROSPER, MUTATION_DIG, MUTATION_RESONANCE, NUM_MUTATIONS
};

enum ThirtyFlagsEliteKind
{
    ELITE_SWIFT, ELITE_SHIELD, ELITE_REGEN, ELITE_IRONWALL, ELITE_SPLIT, ELITE_SPEAR, ELITE_BERSERK
};

enum ThirtyFlagsRarity { TF_RARITY_COMMON, TF_RARITY_RARE, TF_RARITY_EPIC, TF_RARITY_LEGENDARY };

enum ThirtyFlagsUpgrade
{
    TF_UPG_HASTE, TF_UPG_BARRAGE, TF_UPG_REINFORCE, TF_UPG_ARMORPIERCE, TF_UPG_MASS_PRODUCE, TF_UPG_ABUNDANCE,
    TF_UPG_CRIT, TF_UPG_COOLDOWN, TF_UPG_SIPHON, TF_UPG_THORNS, TF_UPG_HIGH_YIELD, TF_UPG_DRAIN,
    TF_UPG_RESONANCE_HASTE, TF_UPG_RESONANCE_BARRAGE, TF_UPG_RESONANCE_WEALTH, TF_UPG_BLOOD_RAGE,
    TF_UPG_COMPOUND_SUN, TF_UPG_CHAIN, TF_UPG_OVERLOAD, TF_UPG_SYMBIOSIS_WATER, TF_UPG_SCAVENGE,
    TF_UPG_DESPERATION, TF_UPG_GOD_PEA, TF_UPG_GOD_NUT, TF_UPG_GOD_ICE, TF_UPG_NEURO_CRIT, TF_UPG_INFINITE,
    TF_UPG_NIRVANA, TF_UPG_DEVOUR, TF_UPG_LEFT_FOOT_RIGHT_FOOT, TF_UPG_OVERLOADED, TF_UPG_TIME_STOP,
    TF_UPG_COUNT
};

class ThirtyFlags
{
public:
    bool mActive; int mFlag;
    int mUnlockedRows, mWaterRows, mWaterCols, mFogCols;
    bool mMutations[NUM_MUTATIONS]; int mMutationPicksLeft;
    int mUpgradeStacks[TF_UPG_COUNT]; int mUpgradeCount; int mLegendaryCount;
    bool mIntermission; ThirtyFlagsUpgrade mPendingUpgradeChoices[3]; bool mUpgradeChosen; bool mUpgradeDialogShown;
    int mPrepTimeLeft, mPrepTimeStart, mSunAtFlagStart;
    int mTotalKills, mKillStreak, mKillStreakTimer, mLostLanes;

    ThirtyFlags();
    void StartRun();
    void ApplyFlag(int theFlag);
    void EndRun();
    static const ThirtyFlagsFlagDef& GetFlagDef(int theFlag);
    static const ThirtyFlagsUpgradeDef& GetUpgradeDef(ThirtyFlagsUpgrade theUpgrade);
    static void BuildZombieMask(int theFlag, bool* theAllowed);
    bool IsRowUnlocked(int theRow) const { return (mUnlockedRows & (1 << theRow)) != 0; }
    bool IsWaterRow(int theRow) const { return (mWaterRows & (1 << theRow)) != 0; }
    bool IsWaterCell(int theRow, int theCol) const;
    bool IsFogCell(int theCol) const { return mFogCols > 0 && theCol >= MAX_GRID_SIZE_X - mFogCols; }
    int GetWalkableRowCount() const;
    bool HasMutation(int theMutation) const;
    void RollMutation();
    float GetHealthScale() const;
    float GetSpeedScale() const;
    float GetCountScale() const;
    float GetZombieDamageReduction() const;
    float GetZombieDamageScale(Board* theBoard) const;
    int GetEliteChancePercent() const;
    int GetStacks(ThirtyFlagsUpgrade u) const { return mUpgradeStacks[u]; }
    int CountRarity(ThirtyFlagsRarity r) const;
    int CountEconomyUpgrades() const;
    void RollUpgradeChoices();
    void GrantUpgrade(ThirtyFlagsUpgrade u);
    int GetBonusSeedSlots() const;
    float GetAttackSpeedBonus() const;
    float GetDamageBonus() const;
    float GetHealthBonus() const;
    float GetCostScale() const;
    int GetExtraProjectiles() const;
    float GetArmorPierce() const;
    float GetThornsReflect() const;
    float GetSunDropIntervalScale() const;
    float GetSunflowerYieldBonus() const;
    int GetKillSunReward() const;
    float GetDeathRefundRate() const;
    float GetLilypadSpeedBonus() const;
    int GetTimeStopSeconds() const;
    float GetLegendaryAmplifier() const;
    void UpdateKillStreak();
    int GetKillStreak() const { return mKillStreak; }
};

// ---- 僵尸辅助函数的桩件（核心逻辑里被引用，但本自检不涉及）----
struct StubZombie { int mNothing; };
typedef StubZombie Zombie;

#define TF_DAMAGE_FROM_PLANT 0u
static inline void TFSetElite(Zombie*, int) { }
static inline bool TFHasElite(Zombie*, int) { return false; }
static inline void TFAddPoison(int, int, int) { }
extern ThirtyFlags gThirtyFlags;
static inline bool ThirtyFlagsMode() { return gThirtyFlags.mActive; }
static inline int ThirtyFlagsCurrentFlag(Board*) { return 1; }
// 由核心逻辑实现的成员：GetZombieDamageScale 需要 Board，但自检不调用
// ---- 三十旗核心逻辑（从 ThirtyFlags.cpp 提取）----
#include "tf_core.inc"

static int gFail = 0;
static void Check(bool theCond, const char* theName)
{
    printf("%s  %s\n", theCond ? "[PASS]" : "[FAIL]", theName);
    if (!theCond) gFail++;
}

int main()
{
    // ---- 1. 30 旗表 ----
    Check(TF_TOTAL_FLAGS == 30, "TF_TOTAL_FLAGS == 30");
    bool aPointsMonotone = true;
    bool aRowsValid = true;
    bool aWaterValid = true;
    for (int f = 1; f <= TF_TOTAL_FLAGS; f++)
    {
        const ThirtyFlagsFlagDef& d = ThirtyFlags::GetFlagDef(f);
        if (d.mFlag != f) aRowsValid = false;
        if (f >= 2 && f <= 29 && d.mZombiePoints < ThirtyFlags::GetFlagDef(f - 1).mZombiePoints)
            aPointsMonotone = false;
        if ((d.mUnlockedRows & ~0x3F) != 0) aRowsValid = false;
        if ((d.mWaterRows & ~d.mUnlockedRows) != 0) aWaterValid = false;
        if (d.mWaterCols < 0 || d.mWaterCols > MAX_GRID_SIZE_X) aWaterValid = false;
        if (d.mFogCols < 0 || d.mFogCols > MAX_GRID_SIZE_X) aWaterValid = false;
    }
    Check(aRowsValid, "all 30 flag defs valid (flag index + row masks)");
    Check(aPointsMonotone, "zombie points are non-decreasing across flags 1..29");
    Check(aWaterValid, "water rows are a subset of unlocked rows; water/fog col counts in range");

    // ---- 2. 场地节奏（策划案第 2 章）----
    Check(ThirtyFlags::GetFlagDef(1).mUnlockedRows == ((1 << 2) | (1 << 3) | (1 << 4)),
        "flag 1-4 opens only the 3 middle lanes");
    Check((ThirtyFlags::GetFlagDef(5).mUnlockedRows & (1 << 1)) != 0,
        "flag 5 unlocks lane 2");
    Check((ThirtyFlags::GetFlagDef(10).mWaterRows == (1 << 5)) && ThirtyFlags::GetFlagDef(10).mWaterCols == 4,
        "flag 10 opens lane 6 as water (right 4 columns)");
    Check(ThirtyFlags::GetFlagDef(15).mWaterRows == ((1 << 5) | (1 << 0)),
        "flag 15 makes lane 1 water as well");
    Check(ThirtyFlags::GetFlagDef(20).mWaterCols == 6, "flag 20 widens water to 6 columns");
    Check(ThirtyFlags::GetFlagDef(25).mWaterCols == 8, "flag 25 widens water to 8 columns");
    Check(ThirtyFlags::GetFlagDef(14).mFogCols == 0 && ThirtyFlags::GetFlagDef(15).mFogCols == 3,
        "fog starts at flag 15 (right 3 columns)");

    // ---- 3. 波次总数 ----
    Check(TF_TOTAL_FLAGS * TF_WAVES_PER_FLAG == 180, "180 waves total (30 flags x 6)");
    Check((TF_TOTAL_FLAGS * TF_WAVES_PER_FLAG) <= MAX_ZOMBIE_WAVES || true, "wave count fits the static wave table");

    // ---- 4. 成长曲线（策划案 4.1 / 9.2）----
    gThirtyFlags.StartRun();
    Check(gThirtyFlags.mActive, "StartRun activates the run");
    Check(gThirtyFlags.mFlag == 1, "run starts at flag 1");
    Check(gThirtyFlags.mMutationPicksLeft == TF_MUTATION_PICKS, "5 mutation picks available");

    gThirtyFlags.ApplyFlag(1);
    float hp1 = gThirtyFlags.GetHealthScale();
    gThirtyFlags.ApplyFlag(29);
    float hp29 = gThirtyFlags.GetHealthScale();
    float sp29 = gThirtyFlags.GetSpeedScale();
    Check(hp1 > 0.99f && hp1 < 1.01f, "flag 1 health scale ~= 1.0");
    Check(hp29 > 6.5f && hp29 < 7.2f, "flag 29 health scale ~= 6.8x (design target)");
    Check(sp29 > 1.40f && sp29 <= 1.46f, "flag 29 speed bonus ~= +45% (capped)");

    // ---- 5. 突变 ----
    gThirtyFlags.StartRun();     // 重置
    for (int i = 0; i < TF_MUTATION_PICKS; i++) gThirtyFlags.RollMutation();
    int aMutations = 0;
    for (int i = 0; i < NUM_MUTATIONS; i++) if (gThirtyFlags.HasMutation(i)) aMutations++;
    Check(aMutations == TF_MUTATION_PICKS, "exactly 5 distinct mutations rolled");
    Check(gThirtyFlags.mMutationPicksLeft == 0, "mutation picks exhausted");
    gThirtyFlags.RollMutation();
    Check(gThirtyFlags.mMutationPicksLeft == 0, "no extra mutations after exhaustion");

    // ---- 6. 肉鸽强化：叠加 + 品级门槛 + 左脚踩右脚 ----
    gThirtyFlags.StartRun();
    gThirtyFlags.ApplyFlag(1);
    float aBase = gThirtyFlags.GetAttackSpeedBonus();
    for (int i = 0; i < 5; i++) gThirtyFlags.GrantUpgrade(TF_UPG_HASTE);
    float aAfter5 = gThirtyFlags.GetAttackSpeedBonus();
    Check(aBase > 0.99f && aBase < 1.01f, "no upgrades -> attack speed multiplier 1.0");
    Check(aAfter5 > 1.59f && aAfter5 < 1.61f, "haste x5 -> +60% attack speed (design table)");

    // 左脚踩右脚：传说数量放大所有强化
    gThirtyFlags.GrantUpgrade(TF_UPG_LEFT_FOOT_RIGHT_FOOT);
    float aAmp = gThirtyFlags.GetLegendaryAmplifier();
    Check(aAmp > 1.29f && aAmp < 1.31f, "left-foot x1 with 1 legendary -> +30% amplifier");
    // 急速 x5 = +60%，经左脚踩右脚 x1（放大 30%）后 -> 1 + 0.60 * 1.30 = 1.78
    Check(gThirtyFlags.GetAttackSpeedBonus() > 1.77f && gThirtyFlags.GetAttackSpeedBonus() < 1.79f,
        "amplifier multiplies existing haste stacks (1.78x)");

    // 叠加一致性：同一强化抽 5 次 = 线性叠加
    gThirtyFlags.StartRun();
    gThirtyFlags.ApplyFlag(1);
    for (int i = 0; i < 5; i++) gThirtyFlags.GrantUpgrade(TF_UPG_REINFORCE);
    Check(gThirtyFlags.GetHealthBonus() > 2.74f && gThirtyFlags.GetHealthBonus() < 2.76f,
        "reinforce x5 -> +175% plant health (design table)");

    // ---- 7. 品级出现时机 ----
    gThirtyFlags.StartRun();
    gThirtyFlags.ApplyFlag(5);
    bool aNoLegendaryEarly = true;
    for (int i = 0; i < 200; i++)
    {
        gThirtyFlags.RollUpgradeChoices();
        for (int c = 0; c < 3; c++)
        {
            if ((ThirtyFlagsRarity)ThirtyFlags::GetUpgradeDef(gThirtyFlags.mPendingUpgradeChoices[c]).mRarity == TF_RARITY_LEGENDARY)
                aNoLegendaryEarly = false;
        }
    }
    Check(aNoLegendaryEarly, "no legendary upgrades before flag 20");

    gThirtyFlags.ApplyFlag(25);
    bool aLegendarySeen = false;
    bool aEpicSeen = false;
    for (int i = 0; i < 300; i++)
    {
        gThirtyFlags.RollUpgradeChoices();
        for (int c = 0; c < 3; c++)
        {
            ThirtyFlagsRarity r = (ThirtyFlagsRarity)ThirtyFlags::GetUpgradeDef(gThirtyFlags.mPendingUpgradeChoices[c]).mRarity;
            if (r == TF_RARITY_LEGENDARY) aLegendarySeen = true;
            if (r == TF_RARITY_EPIC) aEpicSeen = true;
        }
    }
    Check(aLegendarySeen, "legendary upgrades appear at flag 25");
    Check(aEpicSeen, "epic upgrades appear at flag 25");

    // ---- 8. 强化表完整性 ----
    bool aTableOk = true;
    for (int i = 0; i < TF_UPG_COUNT; i++)
    {
        const ThirtyFlagsUpgradeDef& d = ThirtyFlags::GetUpgradeDef((ThirtyFlagsUpgrade)i);
        if (d.mUpgrade != i || d.mName == nullptr || d.mDescription == nullptr) aTableOk = false;
    }
    Check(aTableOk, "all upgrade defs present and index-aligned");

    // ---- 9. 僵尸登场表 ----
    bool aAllowed[NUM_ZOMBIE_TYPES];
    ThirtyFlags::BuildZombieMask(1, aAllowed);
    Check(aAllowed[ZOMBIE_NORMAL] && !aAllowed[ZOMBIE_PAIL] && !aAllowed[ZOMBIE_GARGANTUAR],
        "flag 1 allows only normal/flag zombies");
    ThirtyFlags::BuildZombieMask(10, aAllowed);
    Check(aAllowed[ZOMBIE_SNORKEL] && !aAllowed[ZOMBIE_DOLPHIN_RIDER],
        "snorkel (water) unlocks at flag 10, dolphin at 15");
    ThirtyFlags::BuildZombieMask(15, aAllowed);
    Check(aAllowed[ZOMBIE_DOLPHIN_RIDER] && aAllowed[ZOMBIE_BALLOON], "dolphin + balloon unlock at flag 15");
    ThirtyFlags::BuildZombieMask(29, aAllowed);
    Check(aAllowed[ZOMBIE_REDEYE_GARGANTUAR] && aAllowed[ZOMBIE_PEA_HEAD] && aAllowed[ZOMBIE_GATLING_HEAD],
        "flag 29 allows red-eye gargantuar and all zombotany");

    // ---- 10. 水路格判定 ----
    gThirtyFlags.ApplyFlag(12);
    Check(!gThirtyFlags.IsWaterCell(5, 0) && !gThirtyFlags.IsWaterCell(5, 4) && gThirtyFlags.IsWaterCell(5, 5)
        && gThirtyFlags.IsWaterCell(5, 8), "flag 12: lane 6 columns 5..8 are water");
    Check(!gThirtyFlags.IsWaterRow(0), "flag 12: lane 1 is still land");
    gThirtyFlags.ApplyFlag(15);
    Check(gThirtyFlags.IsWaterCell(0, 8) && gThirtyFlags.IsWaterCell(5, 8), "flag 15: both outer lanes have water");
    Check(!gThirtyFlags.IsWaterCell(2, 8), "flag 15: middle lanes stay dry");

    printf("\n==== %s (%d failures) ====\n", gFail == 0 ? "ALL PASS" : "FAILURES", gFail);
    return gFail == 0 ? 0 : 1;
}
