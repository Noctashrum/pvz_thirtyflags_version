#include "ThirtyFlags.h"
#include "Board.h"
#include "Challenge.h"
#include "GridItem.h"
#include "LawnApp.h"
#include "LawnDialog.h"
#include "Plant.h"
#include "Projectile.h"
#include "LawnMower.h"
#include <time.h>
#include "PoolEffect.h"
#include "Resources.h"
#include "../SexyAppFramework/Common.h"
#include "../SexyAppFramework/Graphics.h"
#include "../TodLib/EffectSystem.h"
static Board* gTFSaveBoard = nullptr;   // ThirtyFlags: board snapshot for saving plants/sun
static bool gTFJustLoaded = false;      // ThirtyFlags: suppress autosave right after load (plants not restored yet)
static void ThirtyFlagsSaveProgress();
static bool ThirtyFlagsLoadProgress();


#include <math.h>
#include <string.h>

using namespace Sexy;

// ====================================================================================================
// 《植物大战僵尸：三十旗》实现
//
// 场地采用「日间布局」（5 行 x 100px）+ 局部水路：
//   * 日间布局才能复用原版的草皮滚动动画（IMAGE_BACKGROUND1UNSODDED + IMAGE_SOD1ROW）；
//   * 水路不再是整张泳池背景，而是按格从 IMAGE_POOL 取水格绘制（该图是 15x5 的水格阵列）；
//   * 行解锁时播放草皮滚出动画；未开放的行绘制黑幕，形成原版左侧草坪缺口那种观感。
//
// 注意：GridToPixelY 的行高由 StageHasPool() 全局二选一（日间 100px / 泳池 85px），
// 无法行级混用。日间布局下第 6 行 y=580 已越界，因此三十旗按 5 行设计。
// ====================================================================================================

ThirtyFlags gThirtyFlags;

static inline int TFClampI(int theValue, int theMin, int theMax)
{
    return theValue <= theMin ? theMin : (theValue >= theMax ? theMax : theValue);
}

bool ThirtyFlagsMode()
{
    return gThirtyFlags.mActive && gLawnApp && gLawnApp->mGameMode == GAMEMODE_THIRTY_FLAGS;
}

// ----------------------------------------------------------------------------------------------------
// 30 旗波次表（策划案第 2 / 4 章，按日间 5 行布局重新编排）
//
// mUnlockedRows / mWaterRows 为 5 位掩码（bit0=第 1 行 … bit4=第 5 行）。
// 解锁节奏：开局只给中间 1 道（真 1-1 观感）→ 逐步扩到 4 道 → 两端转水路。
// ----------------------------------------------------------------------------------------------------
#define TF_R2       (1 << 2)
#define TF_R1_3     ((1 << 1) | (1 << 3) | TF_R2)
#define TF_R0_4     ((1 << 0) | (1 << 4) | TF_R1_3)

// 日间布局只有 5 行可用（第 6 行 y=580 已出画面），因此按 5 条道编排：
//   中路 1 道开局 -> 逐旗扩到 4 道陆地 -> 再让两端转水路。
// 水面按「整行满水」绘制（不拉伸贴图），因此 mWaterCols 仅作「该行是否为水」的开关，
// 取值为 MAX_GRID_SIZE_X 表示整行皆水。
static const ThirtyFlagsFlagDef gThirtyFlagsFlagDefs[TF_TOTAL_FLAGS] = {
    // 旗次  主题            点数  水路行        水列      雾列  开放行
    {  1,   "独木难支",      1,    0,            0,        0,    TF_R2 },
    {  2,   "独木难支",      1,    0,            0,        0,    TF_R2 },
    {  3,   "独木难支",      1,    0,            0,        0,    TF_R2 },
    {  4,   "场地打开",      2,    0,            0,        0,    TF_R2 | (1 << 1) },
    {  5,   "场地打开",      2,    0,            0,        0,    TF_R2 | (1 << 1) },
    {  6,   "场地打开",      3,    0,            0,        0,    TF_R2 | (1 << 1) },
    {  7,   "场地打开",      3,    0,            0,        0,    TF_R2 | (1 << 1) },
    {  8,   "三线成形",      4,    0,            0,        0,    TF_R1_3 },
    {  9,   "三线成形",      4,    0,            0,        0,    TF_R1_3 },
    { 10,   "三线成形",      5,    0,            0,        0,    TF_R1_3 },
    { 11,   "三线成形",      5,    0,            0,        0,    TF_R1_3 },
    { 12,   "三线成形",      6,    0,            0,        0,    TF_R1_3 },
    { 13,   "四线铺开",      6,    0,            0,        0,    (1 << 0) | TF_R1_3 },
    { 14,   "四线铺开",      7,    0,            0,        0,    (1 << 0) | TF_R1_3 },
    { 15,   "四线铺开",      7,    0,            0,        3,    (1 << 0) | TF_R1_3 },
    { 16,   "四线铺开",      8,    0,            0,        3,    (1 << 0) | TF_R1_3 },
    { 17,   "四线铺开",      8,    0,            0,        3,    (1 << 0) | TF_R1_3 },
    { 18,   "水路登场",      9,    (1 << 4),      MAX_GRID_SIZE_X, 3,  TF_R0_4 },
    { 19,   "水路登场",      9,    (1 << 4),      MAX_GRID_SIZE_X, 3,  TF_R0_4 },
    { 20,   "双向水路",      10,   (1 << 4) | (1 << 0), MAX_GRID_SIZE_X, 3, TF_R0_4 },
    { 21,   "双向水路",      11,   (1 << 4) | (1 << 0), MAX_GRID_SIZE_X, 3, TF_R0_4 },
    { 22,   "双向水路",      12,   (1 << 4) | (1 << 0), MAX_GRID_SIZE_X, 3, TF_R0_4 },
    { 23,   "双向水路",      13,   (1 << 4) | (1 << 0), MAX_GRID_SIZE_X, 3, TF_R0_4 },
    { 24,   "双向水路",      14,   (1 << 4) | (1 << 0), MAX_GRID_SIZE_X, 3, TF_R0_4 },
    { 25,   "双方超模",      15,   (1 << 4) | (1 << 0), MAX_GRID_SIZE_X, 3, TF_R0_4 },
    { 26,   "双方超模",      17,   (1 << 4) | (1 << 0), MAX_GRID_SIZE_X, 3, TF_R0_4 },
    { 27,   "双方超模",      19,   (1 << 4) | (1 << 0), MAX_GRID_SIZE_X, 3, TF_R0_4 },
    { 28,   "双方超模",      22,   (1 << 4) | (1 << 0), MAX_GRID_SIZE_X, 3, TF_R0_4 },
    { 29,   "双方超模",      25,   (1 << 4) | (1 << 0), MAX_GRID_SIZE_X, 3, TF_R0_4 },
    { 30,   "僵王终战",      0,    (1 << 4) | (1 << 0), MAX_GRID_SIZE_X, 3, TF_R0_4 }
};

// ----------------------------------------------------------------------------------------------------
// 每行草皮滚出的动画进度（0..1000）。行解锁时置 0 并逐帧推进到 1000。
// ----------------------------------------------------------------------------------------------------
static int gThirtyFlagsSodProgress[MAX_GRID_SIZE_Y];
static int gThirtyFlagsPrevUnlockedRows = 0;

// 每格是否已变成水（用于水格淡入，避免换旗时水突然出现）
static int gThirtyFlagsWaterFade[MAX_GRID_SIZE_X][MAX_GRID_SIZE_Y];

// ----------------------------------------------------------------------------------------------------
// 当前旗次
//   mSurvivalStage 全程等于「已完成旗数」，任何调用时机都成立；
//   不能只依赖 mFlag（Board::InitSurvivalStage 会在阶段自增前调用 InitZombieWaves）。
// ----------------------------------------------------------------------------------------------------
int ThirtyFlagsCurrentFlag(Board* theBoard)
{
    if (!theBoard || !theBoard->mChallenge)
        return 1;

    return TFClampI(theBoard->mChallenge->mSurvivalStage + 1, 1, TF_TOTAL_FLAGS);
}

// ----------------------------------------------------------------------------------------------------
// 僵尸辅助
// ----------------------------------------------------------------------------------------------------
static inline void TFSetElite(Zombie* theZombie, int theKind)
{
    theZombie->mBossMode |= (1 << theKind);
}

static inline bool TFHasElite(Zombie* theZombie, int theKind)
{
    return (theZombie->mBossMode & (1 << theKind)) != 0;
}

// 【三十旗】处决特效（策划案 7.7）：精英 / 巨人僵尸阵亡时的爆发演出。
// 定义在文件后段的表现层区，此处先声明，供 ThirtyFlagsZombieKilled 调用。
static void TFSpawnExecution(Board* theBoard, Zombie* theZombie, bool theGiant);

// 尸毒毒潭：每格剩余帧数
static int gThirtyFlagsPoison[MAX_GRID_SIZE_X][MAX_GRID_SIZE_Y];

static void TFAddPoison(int theCol, int theRow, int theFrames)
{
    if (theCol < 0 || theCol >= MAX_GRID_SIZE_X || theRow < 0 || theRow >= MAX_GRID_SIZE_Y)
        return;

    gThirtyFlagsPoison[theCol][theRow] = max(gThirtyFlagsPoison[theCol][theRow], theFrames);
}

// ----------------------------------------------------------------------------------------------------
// 肉鸽强化静态表（策划案 7.2 / 7.3 / 7.4，共 36 条，全部可叠加）
// ----------------------------------------------------------------------------------------------------
static const ThirtyFlagsUpgradeDef gThirtyFlagsUpgradeDefs[TF_UPG_COUNT] = {
    // ---- 普通池 ----
    { TF_UPG_HASTE,                TF_RARITY_COMMON,    _S("急速"),         _S("全植物攻速 +12%") },
    { TF_UPG_BARRAGE,              TF_RARITY_COMMON,    _S("弹幕"),         _S("射手类额外发射 1 发弹丸") },
    { TF_UPG_REINFORCE,            TF_RARITY_COMMON,    _S("加固"),         _S("全植物生命 +35%") },
    { TF_UPG_ARMORPIERCE,          TF_RARITY_COMMON,    _S("穿甲"),         _S("无视 25% 护甲") },
    { TF_UPG_MASS_PRODUCE,         TF_RARITY_COMMON,    _S("量产"),         _S("种植费用 -20%") },
    { TF_UPG_ABUNDANCE,            TF_RARITY_COMMON,    _S("丰饶"),         _S("自然阳光掉落间隔 -25%") },

    // ---- 稀有池 ----
    { TF_UPG_CRIT,                 TF_RARITY_RARE,      _S("暴击"),         _S("15% 概率造成 2 倍伤害") },
    { TF_UPG_COOLDOWN,             TF_RARITY_RARE,      _S("冷却"),         _S("全植物冷却 -20%") },
    { TF_UPG_SIPHON,               TF_RARITY_RARE,      _S("虹吸"),         _S("击杀回 3 阳光") },
    { TF_UPG_THORNS,               TF_RARITY_RARE,      _S("荆棘"),         _S("反弹 20% 所受伤害") },
    { TF_UPG_HIGH_YIELD,           TF_RARITY_RARE,      _S("高产"),         _S("向日葵产出 +60%") },
    { TF_UPG_DRAIN,                TF_RARITY_RARE,      _S("汲取"),         _S("植物阵亡返还 50% 种植费") },

    // ---- 史诗池：联动与转化 ----
    { TF_UPG_RESONANCE_HASTE,      TF_RARITY_EPIC,      _S("共鸣·攻速"),    _S("每持有 1 条「急速」，全植物攻速额外 +8%") },
    { TF_UPG_RESONANCE_BARRAGE,    TF_RARITY_EPIC,      _S("共鸣·弹幕"),    _S("每持有 1 条「弹幕」，射手类伤害 +15%") },
    { TF_UPG_RESONANCE_WEALTH,     TF_RARITY_EPIC,      _S("共鸣·财富"),    _S("每持有 1 条经济类强化，全植物攻击力 +10%") },
    { TF_UPG_BLOOD_RAGE,           TF_RARITY_EPIC,      _S("血怒转化"),     _S("全植物当前生命的 30% 转化为攻击力") },
    { TF_UPG_COMPOUND_SUN,         TF_RARITY_EPIC,      _S("复利·阳光"),    _S("每旗结束时，当前阳光储备 x1.15") },
    { TF_UPG_CHAIN,                TF_RARITY_EPIC,      _S("连锁"),         _S("爆炸类命中后触发一次半额二次爆炸") },
    { TF_UPG_OVERLOAD,             TF_RARITY_EPIC,      _S("临界·超载"),    _S("攻速加成每达到 +100%，射手追加 1 发弹丸") },
    { TF_UPG_SYMBIOSIS_WATER,      TF_RARITY_EPIC,      _S("共生·水路"),    _S("睡莲上的植物攻速 +30%，且睡莲不再占卡槽") },
    { TF_UPG_SCAVENGE,             TF_RARITY_EPIC,      _S("拾荒"),         _S("每累计击杀 500 只僵尸，全植物攻击力 +10%") },
    { TF_UPG_DESPERATION,          TF_RARITY_EPIC,      _S("绝境"),         _S("每失去 1 点防线（一格被占），全植物攻速 +5%") },

    // ---- 传说池：指数级与规则改写 ----
    { TF_UPG_GOD_NUT,              TF_RARITY_LEGENDARY, _S("神格·坚果"),    _S("所有坚果类生命 x3，阵亡时对整行造成巨额伤害") },
    { TF_UPG_GOD_ICE,              TF_RARITY_LEGENDARY, _S("神格·寒冰"),    _S("所有减速效果附加真实伤害") },
    { TF_UPG_NEURO_CRIT,           TF_RARITY_LEGENDARY, _S("神经·暴击"),    _S("暴击不再有概率，每次攻击必暴，倍率 +1") },
    { TF_UPG_INFINITE,             TF_RARITY_LEGENDARY, _S("无限"),         _S("卡槽位 +2（可超过 12）") },
    { TF_UPG_NIRVANA,              TF_RARITY_LEGENDARY, _S("涅槃"),         _S("每旗开始时复活上一旗阵亡的植物") },
    { TF_UPG_DEVOUR,               TF_RARITY_LEGENDARY, _S("吞噬"),         _S("植物阵亡时把其全部属性分给相邻植物") },
    { TF_UPG_LEFT_FOOT_RIGHT_FOOT, TF_RARITY_LEGENDARY, _S("左脚踩右脚"),   _S("每持有 1 条传说强化，所有强化效果 +30%（自身也计入）") },
    { TF_UPG_OVERLOADED,           TF_RARITY_LEGENDARY, _S("超载"),         _S("全植物攻速 +50%，生命 -30%") },
    { TF_UPG_TIME_STOP,            TF_RARITY_LEGENDARY, _S("时间停滞"),     _S("每旗的前 10 秒，所有僵尸无法移动") },
    { TF_UPG_OVERCLOCK,            TF_RARITY_LEGENDARY, _S("超频核心"),     _S("全植物攻速 +60%") },
    { TF_UPG_WARHEAD,              TF_RARITY_LEGENDARY, _S("毁灭弹头"),     _S("全植物伤害 +50%") },
    { TF_UPG_GOLD_HARVEST,         TF_RARITY_LEGENDARY, _S("黄金收割"),     _S("击杀必掉阳光，每层面额 +25") },
    { TF_UPG_BURST_SEED,           TF_RARITY_LEGENDARY, _S("暴烈种子"),     _S("植物种下立即开火") }
};

// ----------------------------------------------------------------------------------------------------
// 僵尸登场表
// ----------------------------------------------------------------------------------------------------
// 【三十旗】流程日志：排查“改动没生效”类问题的定位工具（低频节点才调用）。
// 输出到 Release\thirtyflags_flow.log（绝对路径，避免工作目录不一致）。
// ThirtyFlags: flag-zombie aura query (cached once per frame)
bool ThirtyFlagsHasFlagZombieAlive(Board* theBoard)
{
    if (!ThirtyFlagsMode() || theBoard == nullptr)
        return false;
    static bool aCached = false;
    static int aCachedFrame = -1;
    static Board* aCachedBoard = nullptr;
    if (theBoard != aCachedBoard || theBoard->mMainCounter != aCachedFrame)
    {
        aCachedFrame = theBoard->mMainCounter;
        aCachedBoard = theBoard;
        aCached = false;
        Zombie* aZombie = nullptr;
        while (theBoard->IterateZombies(aZombie))
        {
            if (!aZombie->IsDeadOrDying() && aZombie->mZombieType == ZombieType::ZOMBIE_FLAG)
            {
                aCached = true;
                break;
            }
        }
    }
    return aCached;
}

void TFLog(const char* theLine)
{
#ifdef TF_PLAYER_BUILD
    (void)theLine;
    return;
#else
    FILE* f = fopen("D:////dsh-project////LawnProject////Release////thirtyflags_flow.log", "a");
    if (f)
    {
        fprintf(f, "[%d] %s\n", (int)time(NULL), theLine);
        fclose(f);
    }

#endif
}

void ThirtyFlags::BuildZombieMask(int theFlag, bool* theAllowed)
{
    for (int i = 0; i < NUM_ZOMBIE_TYPES; i++)
    {
        theAllowed[i] = false;
    }

    theAllowed[(int)ZOMBIE_NORMAL] = true;
    theAllowed[(int)ZOMBIE_FLAG] = true;
    if (theFlag <= 1)
        return;

    if (theFlag >= 1)  theAllowed[(int)ZOMBIE_TRAFFIC_CONE] = true;
    if (theFlag >= 2)  theAllowed[(int)ZOMBIE_PAIL] = true;
    if (theFlag >= 2)
    {
        theAllowed[(int)ZOMBIE_POLEVAULTER] = true;
        theAllowed[(int)ZOMBIE_NEWSPAPER] = true;
    }
    if (theFlag >= 3)
    {
        theAllowed[(int)ZOMBIE_DOOR] = true;
        theAllowed[(int)ZOMBIE_DUCKY_TUBE] = true;
    }
    if (theFlag >= 5) theAllowed[(int)ZOMBIE_SNORKEL] = true;
    if (theFlag >= 4) theAllowed[(int)ZOMBIE_FOOTBALL] = true;
    if (theFlag >= 4) theAllowed[(int)ZOMBIE_DANCER] = true;
    if (theFlag >= 6)
    {
        theAllowed[(int)ZOMBIE_DOLPHIN_RIDER] = true;
        theAllowed[(int)ZOMBIE_BALLOON] = true;
    }
    if (theFlag >= 6) theAllowed[(int)ZOMBIE_DIGGER] = true;
    if (theFlag >= 6)
    {
        theAllowed[(int)ZOMBIE_JACK_IN_THE_BOX] = true;
        theAllowed[(int)ZOMBIE_LADDER] = true;
    }
    if (theFlag >= 7) theAllowed[(int)ZOMBIE_CATAPULT] = true;
    if (theFlag >= 7) theAllowed[(int)ZOMBIE_POGO] = true;
    if (theFlag >= 5) theAllowed[(int)ZOMBIE_GARGANTUAR] = true;
    if (theFlag >= 6) theAllowed[(int)ZOMBIE_ZAMBONI] = true;
    if (theFlag >= 7)
    {
        theAllowed[(int)ZOMBIE_PEA_HEAD] = true;
        theAllowed[(int)ZOMBIE_WALLNUT_HEAD] = true;
    }
    if (theFlag >= 7) theAllowed[(int)ZOMBIE_REDEYE_GARGANTUAR] = true;
    if (theFlag >= 8)
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

// ----------------------------------------------------------------------------------------------------
// 构造 / 生命周期
// ----------------------------------------------------------------------------------------------------
ThirtyFlags::ThirtyFlags()
{
    mActive = false;
    mFlag = 0;
    mUnlockedRows = TF_R2;
    mWaterRows = 0;
    mWaterCols = 0;
    mFogCols = 0;
    memset(mMutations, 0, sizeof(mMutations));
    mMutationPicksLeft = 0;
    memset(mUpgradeStacks, 0, sizeof(mUpgradeStacks));
    mUpgradeCount = 0;
    mLegendaryCount = 0;
    mIntermission = false;
    mUpgradeChosen = false;
    mUpgradeDialogShown = false;
    mPendingUpgradeChoices[0] = TF_UPG_HASTE;
    mPendingUpgradeChoices[1] = TF_UPG_HASTE;
    mPendingUpgradeChoices[2] = TF_UPG_HASTE;
    mPrepTimeLeft = 0;
    mPrepTimeStart = 1;
    mSunAtFlagStart = 0;
    mTotalKills = 0;
    mKillStreak = 0;
    mKillStreakTimer = 0;
    mLostLanes = 0;

    mBossPhase = 0;
    mBossPtr = NULL;
    mBossSkillTimer = 0;
    mBossWarnTimer = 0;
    mBossWarnSkill = 0;
    mBossWarnRow = 0;
    mBossSelfDestruct = 0;
    mBossEmpTimer = 0;
    mBossRoarTimer = 0;
    mBossDomainTimer = 0;
}

void ThirtyFlags::StartRun()
{
    *this = ThirtyFlags();
    mActive = true;
    mMutationPicksLeft = TF_MUTATION_PICKS;
    ApplyFlag(1);

    memset(gThirtyFlagsPoison, 0, sizeof(gThirtyFlagsPoison));
    memset(gThirtyFlagsWaterFade, 0, sizeof(gThirtyFlagsWaterFade));

    // 开局：已开放的行直接显示为草皮（不播动画），其余行等解锁时再滚出
    for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
    {
        gThirtyFlagsSodProgress[aRow] = 0;
    }
    gThirtyFlagsPrevUnlockedRows = 0;
}

void ThirtyFlags::EndRun()
{
    mActive = false;
    mIntermission = false;
}

void ThirtyFlags::ApplyFlag(int theFlag)
{
    mFlag = TFClampI(theFlag, 1, TF_TOTAL_FLAGS);
    const ThirtyFlagsFlagDef& aDef = GetFlagDef(mFlag);
    mUnlockedRows = aDef.mUnlockedRows;
    mWaterRows = aDef.mWaterRows;
    mWaterCols = aDef.mWaterCols;
    mFogCols = aDef.mFogCols;
}

const ThirtyFlagsFlagDef& ThirtyFlags::GetFlagDef(int theFlag)
{
    return gThirtyFlagsFlagDefs[TFClampI(theFlag, 1, TF_TOTAL_FLAGS) - 1];
}

const ThirtyFlagsUpgradeDef& ThirtyFlags::GetUpgradeDef(ThirtyFlagsUpgrade theUpgrade)
{
    return gThirtyFlagsUpgradeDefs[TFClampI((int)theUpgrade, 0, TF_UPG_COUNT - 1)];
}

// ----------------------------------------------------------------------------------------------------
// 场地
// ----------------------------------------------------------------------------------------------------
bool ThirtyFlags::IsWaterCell(int theRow, int theCol) const
{
    if (!IsWaterRow(theRow) || mWaterCols <= 0)
        return false;

    return theCol >= MAX_GRID_SIZE_X - mWaterCols;
}

int ThirtyFlags::GetWalkableRowCount() const
{
    int aCount = 0;
    for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
    {
        if (IsRowUnlocked(aRow))
            aCount++;
    }
    return aCount;
}

// ----------------------------------------------------------------------------------------------------
// 尸潮进化：成长
// ----------------------------------------------------------------------------------------------------
bool ThirtyFlags::HasMutation(int theMutation) const
{
    if (theMutation < 0 || theMutation >= NUM_MUTATIONS)
        return false;
    return mMutations[theMutation];
}

float ThirtyFlags::GetHealthScale() const
{
    // 【三十旗】全局僵尸同步增强（用户需求）：基础血量 x1.3，与植物超模等比上涨
    float aScale = powf(1.0f + TF_HP_GROWTH_PER_FLAG, (float)(mFlag - 1)) * 1.3f;
    return min(aScale, TF_HP_GROWTH_CAP * 1.3f);
}

float ThirtyFlags::GetSpeedScale() const
{
    float aBonus = TF_SPEED_GROWTH_PER_FLAG * (float)(mFlag - 1);
    aBonus = min(aBonus, TF_SPEED_GROWTH_CAP);
    if (HasMutation(MUTATION_SWIFT))
        aBonus += 0.25f;
    return 1.0f + aBonus;
}

float ThirtyFlags::GetCountScale() const
{
    return 1.0f + 0.03f * (float)max(0, mFlag - 9);
}

float ThirtyFlags::GetZombieDamageScale(Board* theBoard) const
{
    float aScale = 1.0f;

    if (HasMutation(MUTATION_RESONANCE) && theBoard)
    {
        int aCount = 0;
        Zombie* aZombie = nullptr;
        while (theBoard->IterateZombies(aZombie))
        {
            if (!aZombie->IsDeadOrDying())
                aCount++;
        }
        aScale += 0.01f * (float)min(aCount, 100);
    }

    return aScale;
}

float ThirtyFlags::GetZombieDamageReduction() const
{
    return HasMutation(MUTATION_ARMOR) ? 0.15f : 0.0f;
}

void ThirtyFlags::RollMutation()
{
    if (mMutationPicksLeft <= 0)
        return;

    int aCandidates[NUM_MUTATIONS];
    int aCount = 0;
    for (int i = 0; i < NUM_MUTATIONS; i++)
    {
        if (!mMutations[i])
            aCandidates[aCount++] = i;
    }

    if (aCount == 0)
    {
        mMutationPicksLeft = 0;
        return;
    }

    mMutations[aCandidates[Rand(aCount)]] = true;
    mMutationPicksLeft--;
}

int ThirtyFlags::GetEliteChancePercent() const
{
    if (mFlag < TF_ELITE_START_FLAG)
        return 0;

    return min(30 + (mFlag - TF_ELITE_START_FLAG) * 5, 90);
}

// ----------------------------------------------------------------------------------------------------
// 肉鸽强化
// ----------------------------------------------------------------------------------------------------
float ThirtyFlags::GetLegendaryAmplifier() const
{
    int aStacks = GetStacks(TF_UPG_LEFT_FOOT_RIGHT_FOOT);
    if (aStacks <= 0)
        return 1.0f;

    return 1.0f + 0.30f * (float)(mLegendaryCount * aStacks);
}

int ThirtyFlags::CountRarity(ThirtyFlagsRarity theRarity) const
{
    int aCount = 0;
    for (int i = 0; i < TF_UPG_COUNT; i++)
    {
        if (mUpgradeStacks[i] > 0 && gThirtyFlagsUpgradeDefs[i].mRarity == theRarity)
            aCount += mUpgradeStacks[i];
    }
    return aCount;
}

int ThirtyFlags::CountEconomyUpgrades() const
{
    int aCount = 0;
    aCount += GetStacks(TF_UPG_MASS_PRODUCE);
    aCount += GetStacks(TF_UPG_ABUNDANCE);
    aCount += GetStacks(TF_UPG_SIPHON);
    aCount += GetStacks(TF_UPG_HIGH_YIELD);
    aCount += GetStacks(TF_UPG_DRAIN);
    aCount += GetStacks(TF_UPG_COMPOUND_SUN);
    return aCount;
}

int ThirtyFlags::GetBonusSeedSlots() const
{
    return 2 * GetStacks(TF_UPG_INFINITE);
}

void ThirtyFlags::RollUpgradeChoices()
{
    ThirtyFlagsRarity aAllowed[4];
    int aWeights[4];
    int aAllowedCount = 0;

    aAllowed[aAllowedCount] = TF_RARITY_COMMON;     aWeights[aAllowedCount++] = 40;
    aAllowed[aAllowedCount] = TF_RARITY_RARE;       aWeights[aAllowedCount++] = 30;
    if (mFlag >= 10)
    {
        aAllowed[aAllowedCount] = TF_RARITY_EPIC;
        aWeights[aAllowedCount++] = (mFlag >= 15) ? 30 : 12;
    }
    if (mFlag >= 20)
    {
        aAllowed[aAllowedCount] = TF_RARITY_LEGENDARY;
        aWeights[aAllowedCount++] = 12;
    }

    for (int aChoice = 0; aChoice < 3; aChoice++)
    {
        int aTotal = 0;
        for (int i = 0; i < aAllowedCount; i++)
            aTotal += aWeights[i];

        int aRoll = Rand(aTotal);
        int aAccum = 0;
        ThirtyFlagsRarity aRarity = aAllowed[0];
        for (int i = 0; i < aAllowedCount; i++)
        {
            aAccum += aWeights[i];
            if (aRoll < aAccum)
            {
                aRarity = aAllowed[i];
                break;
            }
        }

        int aCandidates[TF_UPG_COUNT];
        int aCount = 0;
        // 【三十旗】去重（用户反馈三选一重复）：已选条目从候选池剔除；
        // 该稀有度候选耗尽则回退到全体未选条目。
        for (int i = 0; i < TF_UPG_COUNT; i++)
        {
            if (gThirtyFlagsUpgradeDefs[i].mRarity != aRarity)
                continue;
            bool aDup = false;
            for (int p = 0; p < aChoice; p++)
            {
                if (mPendingUpgradeChoices[p] == (ThirtyFlagsUpgrade)i)
                {
                    aDup = true;
                    break;
                }
            }
            if (!aDup)
                aCandidates[aCount++] = i;
        }
        if (aCount == 0)
        {
            for (int i = 0; i < TF_UPG_COUNT; i++)
            {
                bool aDup = false;
                for (int p = 0; p < aChoice; p++)
                {
                    if (mPendingUpgradeChoices[p] == (ThirtyFlagsUpgrade)i)
                    {
                        aDup = true;
                        break;
                    }
                }
                if (!aDup)
                    aCandidates[aCount++] = i;
            }
        }

        mPendingUpgradeChoices[aChoice] = (aCount > 0) ? (ThirtyFlagsUpgrade)aCandidates[Rand(aCount)] : TF_UPG_HASTE;
    }
}

void ThirtyFlags::GrantUpgrade(ThirtyFlagsUpgrade theUpgrade)
{
    if ((int)theUpgrade < 0 || (int)theUpgrade >= TF_UPG_COUNT)
        return;

    mUpgradeStacks[theUpgrade]++;
    mUpgradeCount++;

    if (gThirtyFlagsUpgradeDefs[theUpgrade].mRarity == TF_RARITY_LEGENDARY)
    {
        mLegendaryCount++;
    }
}

// ----------------------------------------------------------------------------------------------------
// 综合战斗系数
// ----------------------------------------------------------------------------------------------------
float ThirtyFlags::GetAttackSpeedBonus() const
{
    if (!mActive)
        return 1.0f;

    float aBonus = 0.0f;
    aBonus += 0.12f * (float)GetStacks(TF_UPG_HASTE);
    aBonus += 0.08f * (float)(GetStacks(TF_UPG_RESONANCE_HASTE) * GetStacks(TF_UPG_HASTE));
    aBonus += 0.05f * (float)(GetStacks(TF_UPG_DESPERATION) * mLostLanes);
    aBonus += 0.50f * (float)GetStacks(TF_UPG_OVERLOADED);
    aBonus += 0.60f * (float)GetStacks(TF_UPG_OVERCLOCK);

    aBonus *= GetLegendaryAmplifier();

    return 1.0f + aBonus;
}

float ThirtyFlags::GetDamageBonus() const
{
    if (!mActive)
        return 1.0f;

    float aBonus = 0.0f;
    aBonus += 0.15f * (float)(GetStacks(TF_UPG_RESONANCE_BARRAGE) * GetStacks(TF_UPG_BARRAGE));
    aBonus += 0.10f * (float)(GetStacks(TF_UPG_RESONANCE_WEALTH) * CountEconomyUpgrades());
    aBonus += 0.10f * (float)(GetStacks(TF_UPG_SCAVENGE) * (mTotalKills / 500));
    aBonus += 0.50f * (float)GetStacks(TF_UPG_WARHEAD);

    int aBloodRage = GetStacks(TF_UPG_BLOOD_RAGE);
    if (aBloodRage > 0)
    {
        aBonus += 0.30f * (float)aBloodRage * max(0.0f, GetHealthBonus() - 1.0f);
    }

    aBonus *= GetLegendaryAmplifier();
    return 1.0f + aBonus;
}

float ThirtyFlags::GetHealthBonus() const
{
    if (!mActive)
        return 1.0f;

    float aBonus = 0.35f * (float)GetStacks(TF_UPG_REINFORCE);
    aBonus -= 0.30f * (float)GetStacks(TF_UPG_OVERLOADED);
    aBonus *= GetLegendaryAmplifier();

    float aMultiplier = powf(3.0f, (float)GetStacks(TF_UPG_GOD_NUT));
    return max(0.1f, (1.0f + aBonus) * aMultiplier);
}

float ThirtyFlags::GetCostScale() const
{
    if (!mActive)
        return 1.0f;

    float aReduction = 0.20f * (float)GetStacks(TF_UPG_MASS_PRODUCE);
    aReduction *= GetLegendaryAmplifier();
    return max(0.0f, 1.0f - aReduction);
}

int ThirtyFlags::GetExtraProjectiles() const
{
    if (!mActive)
        return 0;

    int aExtra = GetStacks(TF_UPG_BARRAGE);
    int aOverloads = GetStacks(TF_UPG_OVERLOAD);
    if (aOverloads > 0)
    {
        int aHundreds = (int)((GetAttackSpeedBonus() - 1.0f) / 1.0f);
        aExtra += max(0, aHundreds) * aOverloads;
    }
    return aExtra;
}

float ThirtyFlags::GetArmorPierce() const
{
    if (!mActive)
        return 0.0f;
    return 0.25f * (float)GetStacks(TF_UPG_ARMORPIERCE) * GetLegendaryAmplifier();
}

float ThirtyFlags::GetThornsReflect() const
{
    if (!mActive)
        return 0.0f;
    return 0.20f * (float)GetStacks(TF_UPG_THORNS) * GetLegendaryAmplifier();
}

float ThirtyFlags::GetSunDropIntervalScale() const
{
    if (!mActive)
        return 1.0f;
    float aReduction = 0.25f * (float)GetStacks(TF_UPG_ABUNDANCE) * GetLegendaryAmplifier();
    return max(0.05f, 1.0f - aReduction);
}

float ThirtyFlags::GetSunflowerYieldBonus() const
{
    if (!mActive)
        return 1.0f;
    return 1.0f + 0.60f * (float)GetStacks(TF_UPG_HIGH_YIELD) * GetLegendaryAmplifier();
}

int ThirtyFlags::GetKillSunReward() const
{
    if (!mActive)
        return 0;

    float aReward = 3.0f * (float)GetStacks(TF_UPG_SIPHON);
    if (mKillStreak > 0)
    {
        float aStreakBonus = min(0.015f * (float)mKillStreak, 0.15f);
        aReward *= (1.0f + aStreakBonus);
    }
    return (int)aReward;
}

float ThirtyFlags::GetDeathRefundRate() const
{
    if (!mActive)
        return 0.0f;
    return 0.50f * (float)GetStacks(TF_UPG_DRAIN) * GetLegendaryAmplifier();
}

float ThirtyFlags::GetLilypadSpeedBonus() const
{
    return (mActive && GetStacks(TF_UPG_SYMBIOSIS_WATER) > 0) ? 0.30f : 0.0f;
}

int ThirtyFlags::GetTimeStopSeconds() const
{
    if (!mActive)
        return 0;
    return 10 * GetStacks(TF_UPG_TIME_STOP);
}

// ----------------------------------------------------------------------------------------------------
// 表现层 / 统计
// ----------------------------------------------------------------------------------------------------
void ThirtyFlags::UpdateKillStreak()
{
    if (mKillStreakTimer > 0)
    {
        mKillStreakTimer--;
        if (mKillStreakTimer == 0)
        {
            mKillStreak = 0;
        }
    }
}

void ThirtyFlags::OnZombieKilled(Board* theBoard, int theX, int theY, int theRow)
{
    if (!mActive)
        return;

    mTotalKills++;
    mKillStreak++;
    mKillStreakTimer = 100;

    int aSun = GetKillSunReward();
    if (aSun > 0 && theBoard)
    {
        theBoard->AddSunMoney(aSun);
    }

    // 【三十旗】「黄金收割」：击杀必掉阳光，每层面额 +25（用户需求）
    int aHarvestSun = 25 * GetStacks(TF_UPG_GOLD_HARVEST);
    if (aHarvestSun > 0 && theBoard)
    {
        theBoard->AddSunMoney(aHarvestSun);
    }

    if (!theBoard)
        return;

    if (HasMutation(MUTATION_ZOMBIE_EXPLODE))
    {
        theBoard->KillAllPlantsInRadius(theX, theY, 90);
    }

    if (HasMutation(MUTATION_ZOMBIE_POISON))
    {
        TFAddPoison(theBoard->PixelToGridXKeepOnBoard(theX, theY),
                    theBoard->PixelToGridYKeepOnBoard(theX, theY), 450);
    }

    if (HasMutation(MUTATION_PROSPER) && Rand(100) < 30)
    {
        int aGridY = theRow;
        if (aGridY < 0 || aGridY >= MAX_GRID_SIZE_Y)
            aGridY = TFClampI(theBoard->PixelToGridYKeepOnBoard(theX, theY), 0, MAX_GRID_SIZE_Y - 1);

        Zombie* aImp = theBoard->AddZombieInRow(ZOMBIE_IMP, aGridY, theBoard->mCurrentWave);
        if (aImp)
        {
            aImp->mPosX = (float)theX;
        }
    }

    if (HasMutation(MUTATION_DIG))
    {
        int aGridX = theBoard->PixelToGridXKeepOnBoard(theX, theY);
        int aGridY = theBoard->PixelToGridYKeepOnBoard(theX, theY);
        if (aGridX >= 0 && aGridX < MAX_GRID_SIZE_X && aGridY >= 0 && aGridY < MAX_GRID_SIZE_Y)
        {
            if (!theBoard->GetCraterAt(aGridX, aGridY) && !theBoard->GetGraveStoneAt(aGridX, aGridY))
            {
                GridItem* aCrater = theBoard->AddACrater(aGridX, aGridY);
                if (aCrater)
                {
                    aCrater->mGridItemCounter = 150;
                }
            }
        }
    }
}

// ====================================================================================================
// 钩子实现
// ====================================================================================================

void ThirtyFlagsInitRun(Board* theBoard)
{
    gThirtyFlags.StartRun();

    // ThirtyFlags v5: restore saved progress (flag + upgrades) and jump straight there
    if (ThirtyFlagsLoadProgress() && theBoard)
    {
        ThirtyFlagsFlagChanged(theBoard);
    }
    ThirtyFlagsSetupBoard(theBoard);

    // 开局已开放的行直接呈现为草皮，不留滚动动画
    for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
    {
        gThirtyFlagsSodProgress[aRow] = gThirtyFlags.IsRowUnlocked(aRow) ? 1000 : 0;
    }
    gThirtyFlagsPrevUnlockedRows = gThirtyFlags.mUnlockedRows;

    for (int x = 0; x < MAX_GRID_SIZE_X; x++)
    {
        for (int y = 0; y < MAX_GRID_SIZE_Y; y++)
        {
            gThirtyFlagsWaterFade[x][y] = gThirtyFlags.IsWaterCell(y, x) ? 100 : 0;
        }
    }
}

// ----------------------------------------------------------------------------------------------------
// 场地：按当前旗配置水陆格
// ----------------------------------------------------------------------------------------------------
void ThirtyFlagsSetupBoard(Board* theBoard)
{
    if (!theBoard)
        return;

    if (!gThirtyFlags.mActive)
    {
        gThirtyFlags.StartRun();
    }

    for (int y = 0; y < MAX_GRID_SIZE_Y; y++)
    {
        bool aUnlocked = gThirtyFlags.IsRowUnlocked(y);
        bool aWaterRow = gThirtyFlags.IsWaterRow(y);

        if (!aUnlocked)
        {
            // 未开放的行：整行不可种植（DIRT 仅作内部标记，视觉由 DrawBackdrop 画黑幕）
            theBoard->mPlantRow[y] = PLANTROW_DIRT;
            for (int x = 0; x < MAX_GRID_SIZE_X; x++)
            {
                theBoard->mGridSquareType[x][y] = GRIDSQUARE_DIRT;
            }
            continue;
        }

        if (aWaterRow)
        {
            theBoard->mPlantRow[y] = PLANTROW_POOL;
            for (int x = 0; x < MAX_GRID_SIZE_X; x++)
            {
                theBoard->mGridSquareType[x][y] = gThirtyFlags.IsWaterCell(y, x) ? GRIDSQUARE_POOL : GRIDSQUARE_GRASS;
            }
        }
        else
        {
            theBoard->mPlantRow[y] = PLANTROW_NORMAL;
            for (int x = 0; x < MAX_GRID_SIZE_X; x++)
            {
                theBoard->mGridSquareType[x][y] = GRIDSQUARE_GRASS;
            }
        }
    }

    // 迷雾：右侧 mFogCols 列
    for (int x = 0; x < MAX_GRID_SIZE_X; x++)
    {
        for (int y = 0; y < MAX_GRID_SIZE_Y + 1; y++)
        {
            bool aFog = (y < MAX_GRID_SIZE_Y) && gThirtyFlags.IsRowUnlocked(y) && gThirtyFlags.IsFogCell(x);
            theBoard->mGridCelFog[x][y] = aFog ? 255 : 0;
        }
    }
}

void ThirtyFlagsFlagChanged(Board* theBoard)
{
    if (!theBoard)
        return;

    ThirtyFlagsSetupBoard(theBoard);

    // 【三十旗】新解锁行补生成小推车 —— InitLawnMowers 只在关卡开始跑一次，
    // 之后解锁的行（mPlantRow 从 DIRT 变为可种植）没有推车（用户报 bug）。
    // 逐行检查：已解锁、可种植、且该行还没有推车 → 补一台。
    for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
    {
        if (!gThirtyFlags.IsRowUnlocked(aRow) || theBoard->mPlantRow[aRow] == PlantRowType::PLANTROW_DIRT)
            continue;

        bool aHasMower = false;
        LawnMower* aMowerScan = nullptr;
        while (theBoard->mLawnMowers.IterateNext(aMowerScan))
        {
            if (aMowerScan->mRow == aRow)
            {
                aHasMower = true;
                break;
            }
        }
        if (!aHasMower)
        {
            LawnMower* aNewMower = theBoard->mLawnMowers.DataArrayAlloc();
            aNewMower->LawnMowerInitialize(aRow);
            aNewMower->mVisible = false;
        }
    }

    // 整张 180 波表在关卡开始时由 Board::PickZombieWaves 一次建成
    // （每波白名单按该波所属旗次取），换旗只需把「当前波」推进到下一面旗的起点。
    theBoard->mCurrentWave = (gThirtyFlags.mFlag - 1) * TF_WAVES_PER_FLAG;
    {
        char aBuf[64];
        sprintf(aBuf, "FlagChanged -> flag %d (rows=%d)", gThirtyFlags.mFlag, gThirtyFlags.mUnlockedRows);
        TFLog(aBuf);
    }
    // 【三十旗】换旗中央大字（7.7 花活）
    ThirtyFlagsShowCenterText(
        StrFormat(_S("------ 第 %d 面旗 ------"), gThirtyFlags.mFlag),
        Sexy::Color(255, 60, 30), 180);
    theBoard->mZombieCountDown = 1800;
    theBoard->mZombieCountDownStart = theBoard->mZombieCountDown;
    theBoard->mZombieHealthToNextWave = -1;
    theBoard->mZombieHealthWaveStart = 0;

    // ThirtyFlags v5: save progress on every flag change (board snapshot included)
    gTFSaveBoard = theBoard;
    ThirtyFlagsSaveProgress();
}

void ThirtyFlagsIntermissionBegin(Board* theBoard)
{
    TFLog("intermission BEGIN");
    gThirtyFlags.mIntermission = true;
    gThirtyFlags.mUpgradeChosen = false;
    gThirtyFlags.mUpgradeDialogShown = false;
    gThirtyFlags.mPrepTimeStart = TF_PREP_TIME;
    gThirtyFlags.mPrepTimeLeft = TF_PREP_TIME;
    gThirtyFlags.mSunAtFlagStart = theBoard ? theBoard->mSunMoney : 0;

    if (gThirtyFlags.mFlag % 5 == 0)
    {
        gThirtyFlags.RollMutation();
    }

    gThirtyFlags.RollUpgradeChoices();

    if (theBoard)
    {
        int aStacks = gThirtyFlags.GetStacks(TF_UPG_COMPOUND_SUN);
        if (aStacks > 0)
        {
            int aBonus = (int)((float)theBoard->mSunMoney * 0.15f * (float)aStacks);
            if (aBonus > 0)
            {
                theBoard->AddSunMoney(aBonus);
            }
        }
    }
}

void ThirtyFlagsAdvanceFlag(Board* theBoard)
{
    // 唯一数据源是 mChallenge->mSurvivalStage + 1；不能用 gThirtyFlags.mFlag，否则用 Tab 直接跳旗时两者脱节，场地会按错旗次铺开。
    gThirtyFlags.ApplyFlag(ThirtyFlagsCurrentFlag(theBoard));
    // 【三十旗】修复：这里不能关 mIntermission —— CheckForGameEnd 的顺序是
    // Begin -> AdvanceFlag，Begin 刚置 true 就被这里抹掉，弹窗条件永远为假。
    // intermission 改由 TFDialog 选卡回调结束。

    if (theBoard)
    {
        theBoard->AddSunMoney(TF_STARTING_SUN);
    }

    ThirtyFlagsFlagChanged(theBoard);
}

void ThirtyFlagsIntermissionFinish(Board* theBoard)
{
    if (theBoard && gThirtyFlags.mPrepTimeLeft > 0 && gThirtyFlags.mPrepTimeStart > 0)
    {
        int aSun = (int)((float)TF_EARLY_SUN_MAX * (float)gThirtyFlags.mPrepTimeLeft / (float)gThirtyFlags.mPrepTimeStart);
        if (aSun > 0)
        {
            theBoard->AddSunMoney(aSun);
        }
    }

    gThirtyFlags.mPrepTimeLeft = 0;
}

float ThirtyFlagsZombiePointsScale(Board* theBoard)
{
    if (!ThirtyFlagsMode())
        return 1.0f;

    return gThirtyFlags.GetCountScale();
}

int ThirtyFlagsPickZombieWaves(Board* theBoard, int& theWaveCount)
{
    if (!theBoard || gLawnApp->mGameMode != GAMEMODE_THIRTY_FLAGS)
        return 0;

    // 自举：Board::InitLevel 的调用顺序是 PickBackground -> InitZombieWaves(内部会调本函数)
    // -> ThirtyFlagsInitRun，因此本函数可能早于 InitRun 执行；此时先补一次 StartRun，
    // 否则 mActive 仍为 false，波次接管会被跳过、mNumWaves 保持未初始化值。
    if (!gThirtyFlags.mActive)
    {
        gThirtyFlags.StartRun();
    }

    theWaveCount = TF_TOTAL_FLAGS * TF_WAVES_PER_FLAG;
    return 1;
}

// ----------------------------------------------------------------------------------------------------
// 僵尸侧：初始化（数值成长 + 精英词条）
// ----------------------------------------------------------------------------------------------------
void ThirtyFlagsZombieInit(Zombie* theZombie)
{
    if (!ThirtyFlagsMode() || !theZombie)
        return;

    if (!theZombie->IsOnBoard())
        return;

    // Final boss: shared three-phase health pool (plan 8.1).
    if (theZombie->mZombieType == ZOMBIE_BOSS)
    {
        theZombie->mBodyHealth = TF_BOSS_HEALTH;
        theZombie->mBodyMaxHealth = TF_BOSS_HEALTH;
        return;
    }

    float aHealthScale = gThirtyFlags.GetHealthScale();
    if (theZombie->mBodyHealth > 0)
    {
        theZombie->mBodyHealth = (int)((float)theZombie->mBodyHealth * aHealthScale);
        theZombie->mBodyMaxHealth = theZombie->mBodyHealth;
    }
    if (theZombie->mHelmHealth > 0)
    {
        theZombie->mHelmHealth = (int)((float)theZombie->mHelmHealth * aHealthScale);
        theZombie->mHelmMaxHealth = theZombie->mHelmHealth;
    }
    if (theZombie->mShieldHealth > 0)
    {
        theZombie->mShieldHealth = (int)((float)theZombie->mShieldHealth * aHealthScale);
        theZombie->mShieldMaxHealth = theZombie->mShieldHealth;
    }
    if (theZombie->mFlyingHealth > 0)
    {
        theZombie->mFlyingHealth = (int)((float)theZombie->mFlyingHealth * aHealthScale);
        theZombie->mFlyingMaxHealth = theZombie->mFlyingHealth;
    }

    theZombie->mVelZ = 0.0f;
    theZombie->mBossMode = 0;
    // 【三十旗】旗帜僵尸 = 战争之王（用户需求改版）：三件套全部走原版原生防具系统 ——
    // 高坚果颅甲（本体 +4000，头换高坚果裂纹脸）；铁桶（原生 helm：凹陷/击飞动画）；
    // 铁门（原生 shield：弹孔/破损动画）。外加战旗光环与死亡召兵（指挥官机制）。
    if (theZombie->mZombieType == ZombieType::ZOMBIE_FLAG)
    {
        theZombie->mBodyHealth *= 2;
        // 高坚果颅甲：本体厚血（原版高坚果就是高本体血量的定位）
        theZombie->mBodyHealth += 4000;
        theZombie->mBodyMaxHealth = theZombie->mBodyHealth;
        // 橄榄球帽防具：原生 helm 系统（用户需求：铁桶换成橄榄球头盔）
        theZombie->mHelmType = HelmType::HELMTYPE_FOOTBALL;
        theZombie->mHelmHealth = 1400;
        theZombie->mHelmMaxHealth = 1400;
        // 铁门防具：原生 shield 系统（受击弹孔/破损帧）
        theZombie->mShieldType = ShieldType::SHIELDTYPE_DOOR;
        theZombie->mShieldHealth = 1100;
        Reanimation* aFlagReanim = theZombie->mApp->ReanimationTryToGet(theZombie->mBodyReanimID);
        if (aFlagReanim)
        {
            aFlagReanim->SetImageOverride("anim_head1", Sexy::IMAGE_REANIM_TALLNUT_CRACKED1);
            theZombie->ReanimShowPrefix("anim_screen_door", RENDER_GROUP_HIDDEN);
        }
        // 【三十旗】铁门换皮小推车（用户需求）：门轨道隐藏，手持位挂原版小推车 reanim
        //（AddAttachedReanim 同缠绕海草挂藤蔓机制，附件随僵尸移动/销毁）
        Reanimation* aMowerReanim = theZombie->AddAttachedReanim(-35, 10, ReanimationType::REANIM_LAWNMOWER);
        if (aMowerReanim)
        {
            aMowerReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
            aMowerReanim->mAnimRate = 12.0f;
        }
    }

    if (Rand(100) >= gThirtyFlags.GetEliteChancePercent())
        return;

    int aWanted = 1 + Rand(TF_ELITE_MAX);
    for (int i = 0; i < aWanted; i++)
    {
        TFSetElite(theZombie, Rand(TF_ELITE_KIND_COUNT));
    }

    if (TFHasElite(theZombie, ELITE_SHIELD))
    {
        theZombie->mBossBungeeCounter = 480;
    }
    if (TFHasElite(theZombie, ELITE_SPEAR))
    {
        theZombie->mBossStompCounter = 300;
    }


    // 【三十旗】精英僵尸全员加强（用户需求）：血量 +60%
    theZombie->mBodyHealth = (int)(theZombie->mBodyHealth * 1.6f);
    theZombie->mBodyMaxHealth = theZombie->mBodyHealth;

    // 【三十旗】精英出没警告（7.7 花活）：冷却 10 秒，避免刷屏
    {
        static int aLastWarn = -10000;
        if (theZombie->mBoard && theZombie->mBoard->mMainCounter - aLastWarn > 600)
        {
            aLastWarn = theZombie->mBoard->mMainCounter;
            ThirtyFlagsShowCenterText(SexyString(_S("!! 精英出没 !!")), Sexy::Color(255, 40, 20), 150);
        }
    }

    // 【三十旗】精英视觉表现（用户需求）：按主词条给僵尸染发光色 —— 远看一眼是精英
    //（原版无现成"换头"资源可复用，用 reanim 加色叠加，与魅惑/冰冻染色同一机制）
    Reanimation* aEliteReanim = theZombie->mApp->ReanimationTryToGet(theZombie->mBodyReanimID);
    if (aEliteReanim)
    {
        aEliteReanim->mEnableExtraAdditiveDraw = true;
        Sexy::Color aGlow(150, 0, 0);
        if (TFHasElite(theZombie, ELITE_BERSERK))         aGlow = Sexy::Color(170, 0, 0);
        else if (TFHasElite(theZombie, ELITE_IRONWALL))      aGlow = Sexy::Color(0, 70, 180);
        else if (TFHasElite(theZombie, ELITE_SWIFT))      aGlow = Sexy::Color(170, 170, 0);
        else if (TFHasElite(theZombie, ELITE_REGEN)) aGlow = Sexy::Color(0, 150, 0);
        else if (TFHasElite(theZombie, ELITE_SHIELD))     aGlow = Sexy::Color(140, 0, 190);
        else if (TFHasElite(theZombie, ELITE_SPLIT))      aGlow = Sexy::Color(200, 100, 0);
        else if (TFHasElite(theZombie, ELITE_SPEAR))      aGlow = Sexy::Color(0, 150, 160);
        aEliteReanim->mExtraAdditiveColor = aGlow;

        // 【三十旗】精英换头（用户需求）：按主词条换头 —— 狂暴=狂态扭曲头，其余按词条配墨镜 1-4 号。
        // 旗帜僵尸（战争之王）保留高坚果裂纹头，不在此覆盖。
        if (theZombie->mZombieType != ZombieType::ZOMBIE_FLAG)
        {
            Sexy::Image* aHeadImg = Sexy::IMAGE_REANIM_ZOMBIE_HEAD_SUNGLASSES4;
            if (TFHasElite(theZombie, ELITE_BERSERK))       aHeadImg = Sexy::IMAGE_REANIM_ZOMBIE_HEAD_GROSSOUT;
            else if (TFHasElite(theZombie, ELITE_SWIFT))    aHeadImg = Sexy::IMAGE_REANIM_ZOMBIE_HEAD_SUNGLASSES1;
            else if (TFHasElite(theZombie, ELITE_IRONWALL)) aHeadImg = Sexy::IMAGE_REANIM_ZOMBIE_HEAD_SUNGLASSES2;
            else if (TFHasElite(theZombie, ELITE_REGEN))    aHeadImg = Sexy::IMAGE_REANIM_ZOMBIE_HEAD_SUNGLASSES3;
            aEliteReanim->SetImageOverride("anim_head1", aHeadImg);
        }
    }
}

int ThirtyFlagsZombieTakeDamage(Zombie* theZombie, int theDamage, unsigned int theDamageFlags)
{
    if (!ThirtyFlagsMode() || !theZombie)
        return theDamage;

    if (theZombie->mZombieType == ZOMBIE_BOSS)
        return theDamage;

    int aDamage = theDamage;

    if (TestBit(theDamageFlags, (int)TF_DAMAGE_FROM_PLANT))
    {
        aDamage = (int)((float)aDamage * gThirtyFlags.GetDamageBonus());
        // 【三十旗·平衡 v5】破绽标记：每层 +5% 受伤（最多 5 层 = +25%）
        int aMark = ThirtyFlagsGetMark(theZombie, theZombie->mBoard);
        if (aMark > 0)
            aDamage = (int)((float)aDamage * (1.0f + 0.05f * (float)aMark));

        float aCritChance = 0.15f * (float)gThirtyFlags.GetStacks(TF_UPG_CRIT) * gThirtyFlags.GetLegendaryAmplifier();
        bool aForceCrit = (gThirtyFlags.GetStacks(TF_UPG_NEURO_CRIT) > 0);
        if (aForceCrit || (aCritChance > 0.0f && Rand(10000) < (int)(aCritChance * 10000.0f)))
        {
            float aCritMul = 2.0f + (float)gThirtyFlags.GetStacks(TF_UPG_NEURO_CRIT);
            aDamage = (int)((float)aDamage * aCritMul);
        }
    }

    float aPierce = gThirtyFlags.GetArmorPierce();
    if (aPierce > 0.0f)
    {
        aDamage = (int)((float)aDamage * (1.0f + min(aPierce, 1.0f)));
    }

    if (TFHasElite(theZombie, ELITE_IRONWALL))
    {
        aDamage = (int)((float)aDamage * 0.6f);
    }

    float aReduction = gThirtyFlags.GetZombieDamageReduction();
    if (aReduction > 0.0f)
    {
        aDamage = (int)((float)aDamage * (1.0f - aReduction));
    }

    return max(1, aDamage);
}

void ThirtyFlagsZombieKilled(Zombie* theZombie)
{
    if (!ThirtyFlagsMode() || !theZombie || !theZombie->mBoard)
        return;

    if (theZombie->mDead)
        return;

    // 【三十旗】整备期间（换旗清场）的死亡不计击杀：否则残余僵尸的 DieNoLoot
    // 会灌爆连杀数、概率掉阳光（用户反馈“换旗莫名多出很多连杀”）
    if (gThirtyFlags.mIntermission)
        return;


    int aRow = theZombie->mRow;

    if (ThirtyFlagsBossIsDomainActive())
    {
        int aCol = ClampInt((int)(theZombie->mPosX - BOARD_OFFSET) / 80, 0, MAX_GRID_SIZE_X - 1);
        TFAddPoison(aCol, aRow, 300);
    }

    if (TFHasElite(theZombie, ELITE_SPLIT))
    {
        for (int i = 0; i < 2; i++)
        {
            Zombie* aImp = theZombie->mBoard->AddZombieInRow(ZOMBIE_IMP, aRow, theZombie->mBoard->mCurrentWave);
            if (aImp)
            {
                aImp->mPosX = theZombie->mPosX + (float)(i * 30);
                aImp->mBodyHealth = max(1, aImp->mBodyMaxHealth / 2);
            }
        }
    }

    // 【三十旗】处决特效（策划案 7.7）：击杀精英僵尸或巨人僵尸时，在死亡位置放一圈爆发特效。
    bool aGiant = (theZombie->mZombieType == ZOMBIE_GARGANTUAR ||
                   theZombie->mZombieType == ZOMBIE_REDEYE_GARGANTUAR ||
                   theZombie->mZombieType == ZOMBIE_BOSS);
    bool aElite = false;
    for (int aKind = 0; aKind < TF_ELITE_KIND_COUNT; aKind++)
    {
        if (TFHasElite(theZombie, aKind))
        {
            aElite = true;
            break;
        }
    }
    if (aGiant || aElite)
    {
        TFSpawnExecution(theZombie->mBoard, theZombie, aGiant);
    }

    gThirtyFlags.OnZombieKilled(theZombie->mBoard, (int)theZombie->mPosX, (int)theZombie->mPosY, aRow);
}

void ThirtyFlagsZombieAtePlant(Zombie* theZombie, Plant* thePlant)
{
    if (!ThirtyFlagsMode() || !theZombie)
        return;

    // 反噬统一走 TakeDamage 结算，所以铁壁/尸甲等减伤会自动正确生效。
    int aThorns = 0;

    // 「高坚果反伤」（策划案 6.2）：固定反噬伤害
    if (thePlant && thePlant->mSeedType == SeedType::SEED_TALLNUT)
    {
        aThorns += TF_TALLNUT_THORNS;
    }

    // 「荆棘」强化（策划案 5.4）：按僵尸最大生命百分比反弹。
    // 原版 GetThornsReflect() 定义了却从未被调用，是死代码，这里接上。
    float aReflect = gThirtyFlags.GetThornsReflect();
    if (aReflect > 0.0f)
    {
        aThorns += (int)((float)theZombie->mBodyMaxHealth * aReflect);
    }

    if (aThorns > 0)
    {
        theZombie->TakeDamage(aThorns, 0);
    }

    // 【三十旗】僵?P3「尸王领域」：领域期间全体僵尸啃食均吸血?
    if (gThirtyFlags.HasMutation(MUTATION_LIFESTEAL) || ThirtyFlagsBossIsDomainActive())
    {
        float aRate = ThirtyFlagsBossIsDomainActive() ? 0.04f : 0.02f;
        int aHeal = max(1, (int)((float)theZombie->mBodyMaxHealth * aRate));
        theZombie->mBodyHealth = min(theZombie->mBodyHealth + aHeal, theZombie->mBodyMaxHealth);
    }
}

bool ThirtyFlagsZombieIsBerserk(Zombie* theZombie)
{
    if (!ThirtyFlagsMode() || !theZombie || theZombie->mZombieType == ZOMBIE_BOSS)
        return false;

    if (!TFHasElite(theZombie, ELITE_BERSERK))
        return false;

    return (theZombie->mBodyHealth * 10 < theZombie->mBodyMaxHealth * 3);
}

void ThirtyFlagsZombieUpdate(Zombie* theZombie)
{
    if (!ThirtyFlagsMode() || !theZombie || theZombie->mZombieType == ZOMBIE_BOSS)
        return;

    if (theZombie->IsDeadOrDying() || !theZombie->IsOnBoard())
        return;

    if (TFHasElite(theZombie, ELITE_REGEN) && (theZombie->mZombieAge % 10) == 0)
    {
        int aHeal = max(1, (int)((float)theZombie->mBodyMaxHealth * 0.02f));
        if (theZombie->mBodyHealth < theZombie->mBodyMaxHealth)
        {
            theZombie->mBodyHealth = min(theZombie->mBodyHealth + aHeal, theZombie->mBodyMaxHealth);
        }
    }

    if (TFHasElite(theZombie, ELITE_SHIELD))
    {
        if (theZombie->mBossBungeeCounter > 0)
        {
            theZombie->mBossBungeeCounter--;
        }
        else
        {
            theZombie->mBossBungeeCounter = 480;
            int aShield = 300 + (int)((float)theZombie->mBodyMaxHealth * 0.10f);
            theZombie->mBodyHealth = min(theZombie->mBodyHealth + aShield, theZombie->mBodyMaxHealth);
        }
    }

    if (TFHasElite(theZombie, ELITE_SPEAR))
    {
        if (theZombie->mBossStompCounter != 0)
        {
            theZombie->mBossStompCounter += (theZombie->mBossStompCounter > 0) ? -1 : 1;
            if (theZombie->mBossStompCounter == 0)
            {
                theZombie->mBossStompCounter = 300;
            }
        }
        else
        {
            Plant* aTarget = theZombie->FindPlantTarget(ZombieAttackType::ATTACKTYPE_CHEW);
            if (aTarget)
            {
                theZombie->mBossStompCounter = -15;

                aTarget->mPlantHealth -= 100;
                if (aTarget->mPlantHealth <= 0)
                {
                    theZombie->mBoard->mPlantsEaten++;
                    aTarget->Die();
                }

                int aRenderOrder = Board::MakeRenderOrder(RENDER_LAYER_PROJECTILE, theZombie->mRow, 0);
                Projectile* aSpear = theZombie->mBoard->AddProjectile(
                    (int)theZombie->mPosX, (int)theZombie->mPosY + 10, aRenderOrder, theZombie->mRow,
                    PROJECTILE_CACTUS_SPEAR);
                if (aSpear)
                {
                    aSpear->mWidth = 20;
                    aSpear->mHeight = 20;
                    aSpear->mRotation = 0.0f;
                    aSpear->mRotationSpeed = 0.0f;
                }
            }
            else
            {
                theZombie->mBossStompCounter = 60;
            }
        }
    }

}

// ----------------------------------------------------------------------------------------------------
// 植物侧
// ----------------------------------------------------------------------------------------------------
void ThirtyFlagsPlantInit(Plant* thePlant)
{
    if (!ThirtyFlagsMode() || !thePlant)
        return;

    // ---- 基础值增强（策划案第 6 章）----
    switch (thePlant->mSeedType)
    {
    case SeedType::SEED_WALLNUT:
    case SeedType::SEED_EXPLODE_O_NUT:
    case SeedType::SEED_GIANT_WALLNUT:
        thePlant->mPlantHealth *= 2;          // 4000 -> 8000
        break;
    case SeedType::SEED_TALLNUT:
        thePlant->mPlantHealth *= 2;          // 8000 -> 16000
        break;
    case SeedType::SEED_PUMPKINSHELL:
        thePlant->mPlantHealth *= 2;          // 护甲 4000 -> 8000
        break;
    case SeedType::SEED_UMBRELLA:
        thePlant->mPlantHealth *= 2;
        break;
    default:
        break;
    }

    // ---- 通用倍率通道 ----
    if (thePlant->mPlantHealth > 0)
    {
        thePlant->mPlantHealth = (int)((float)thePlant->mPlantHealth * gThirtyFlags.GetHealthBonus());
        thePlant->mPlantMaxHealth = thePlant->mPlantHealth;
    }

    // 「共生·水路」：睡莲不再挤占卡槽
    if (thePlant->mSeedType == SeedType::SEED_LILYPAD && gThirtyFlags.GetStacks(TF_UPG_SYMBIOSIS_WATER) > 0)
    {
        if (thePlant->mBoard && thePlant->mBoard->mSeedBank)
        {
            thePlant->mBoard->mSeedBank->mNumPackets = min(TF_SEED_SLOTS_MAX, thePlant->mBoard->mSeedBank->mNumPackets + 1);
            thePlant->mBoard->mSeedBank->UpdateWidth();
        }
    }

    int aCooldown = gThirtyFlags.GetStacks(TF_UPG_COOLDOWN);
    if (aCooldown > 0 && thePlant->mLaunchRate > 0)
    {
        float aScale = max(0.15f, 1.0f - 0.20f * (float)aCooldown);
        thePlant->mLaunchRate = max(5, (int)((float)thePlant->mLaunchRate * aScale));
    }
    // 免咖啡豆（策划案 6.2 / 6.3）：三十旗全程连续作战，夜行植物不再睡觉。
    // 必须放在这里 —— ThirtyFlagsPlantInit 比 PlantInitialize 里的
    // "IsNocturnal && !StageIsNight -> SetSleeping(true)" 晚执行，能覆盖掉睡眠。
    if (Plant::IsNocturnal(thePlant->mSeedType) && thePlant->mIsAsleep)
    {
        thePlant->SetSleeping(false);
    }
    // 【三十旗】小喷菇改造（用户需求）：种下 5 秒后消失并产出一个小阳光。
    if (thePlant->mSeedType == SeedType::SEED_PUFFSHROOM)
    {
        thePlant->mPuffLife = 300;   // 300 帧 = 5 秒（专属成员，勿用 mWakeUpCounter）
    }
    // 【三十旗】「暴烈种子」：植物种下立即开火（射手类 mLaunchCounter 归零即触发首射）
    if (gThirtyFlags.GetStacks(TF_UPG_BURST_SEED) > 0 && thePlant->mLaunchRate > 0)
    {
        thePlant->mLaunchCounter = 0;
    }

    // 【三十旗·平衡 v5】叠种视觉错位：同格已有植物时向左偏移 6px ——
    // 原版同格第二株会与第一株完全重叠（看不出叠种生效），错开后两株都可辨。
    if (thePlant->mBoard && thePlant->IsOnBoard())
    {
        Plant* aScanPlant = nullptr;
        while (thePlant->mBoard->IteratePlants(aScanPlant))
        {
            if (aScanPlant != thePlant && aScanPlant->mPlantCol == thePlant->mPlantCol &&
                aScanPlant->mRow == thePlant->mRow && aScanPlant->mPlantHealth > 0)
            {
                thePlant->mX -= 6.0f;
                break;
            }
        }
    }
}

float ThirtyFlagsPlantLaunchDecrement(Plant* thePlant)
{
    if (!ThirtyFlagsMode() || !thePlant)
        return 1.0f;

    float aSpeed = gThirtyFlags.GetAttackSpeedBonus();

    if (thePlant->IsOnBoard() && thePlant->mBoard)
    {
        Plant* aUnder = thePlant->mBoard->GetTopPlantAt(thePlant->mPlantCol, thePlant->mRow, TOPPLANT_ONLY_UNDER_PLANT);
        if (aUnder && (aUnder->mSeedType == SeedType::SEED_LILYPAD || aUnder->mSeedType == SeedType::SEED_FLOWERPOT))
        {
            aSpeed *= (1.0f + gThirtyFlags.GetLilypadSpeedBonus());
        }
    }

    // 策划案 6.3：小喷菇攻速 x3
    if (thePlant->mSeedType == SeedType::SEED_PUFFSHROOM)
    {
        aSpeed *= 3.0f;
    }

    // 【三十旗】投掷家族攻速 x1.5（与伤害加成配套，让投掷系有携带价值）
    // 【三十旗·平衡 v5】投掷系时态拆分：卷心菜 = 高频小溅射（快小），
    // 西瓜/冰瓜 = 低频重炮（保持 1.5 直到其溅射强化轮落地）
    if (thePlant->mSeedType == SeedType::SEED_CABBAGEPULT)
    {
        aSpeed *= 2.0f;
    }
    else if (thePlant->mSeedType == SeedType::SEED_KERNELPULT ||
             thePlant->mSeedType == SeedType::SEED_MELONPULT ||
             thePlant->mSeedType == SeedType::SEED_WINTERMELON)
    {
        aSpeed *= 1.5f;
    }
    return aSpeed;
}

int ThirtyFlagsPlantShotBonus(int theSeedType)
{
    if (!ThirtyFlagsMode())
        return 0;

    switch ((SeedType)theSeedType)
    {
    case SeedType::SEED_PEASHOOTER:     return TF_SHOT_BONUS_PEA;
    case SeedType::SEED_SNOWPEA:        return TF_SHOT_BONUS_PEA;
    case SeedType::SEED_REPEATER:       return TF_SHOT_BONUS_REPEATER;
    case SeedType::SEED_GATLINGPEA:     return TF_SHOT_BONUS_REPEATER;
    case SeedType::SEED_THREEPEATER:    return TF_SHOT_BONUS_THREEPEA;
    case SeedType::SEED_SPLITPEA:       return TF_SHOT_BONUS_SPLITPEA;
    case SeedType::SEED_STARFRUIT:      return TF_SHOT_BONUS_STARFRUIT;
    case SeedType::SEED_CACTUS:         return TF_SHOT_BONUS_CACTUS;
    // 【三十旗】投掷家族强化（用户需求：射击系有机枪+火炬，投掷系要有带的必要）：
    // 卷心菜 40->70 / 玉米 20->45 / 西瓜 80->130 / 冰瓜 80->120（额外伤害走 DoImpact 公共路径补结算）
    case SeedType::SEED_CABBAGEPULT:    return 55;   // v5-flat: 40 -> 95
    case SeedType::SEED_KERNELPULT:     return 45;   // v5-flat: 20 -> 65
    case SeedType::SEED_MELONPULT:      return 85;   // v5-flat: 80 -> 165
    case SeedType::SEED_WINTERMELON:    return 70;   // v5-flat: 80 -> 150
    default:                            return 0;
    }
}

// -----------------------------------------------------------------------------------------
// 7.7 表现层：伤害跳字 + 连杀反馈
// -----------------------------------------------------------------------------------------
namespace
{
struct TFDamageNumber
{
    float   mX;
    float   mY;
    int     mValue;
    int     mLife;
    int     mColorKind;   // 0普通 1冰 2毒 3火 4暴击金
};



// 【三十旗】中央公告（7.7 花活）：换旗大字 / 精英出没警告，居中大字淡出
static int          gTFCenterLife = 0;
static int          gTFCenterLifeMax = 1;
static SexyString   gTFCenterText;
static Sexy::Color  gTFCenterColor(255, 255, 255);


TFDamageNumber  gTFNumbers[TF_DAMAGE_NUM_MAX];
int             gTFNumberCount = 0;

int             gTFStreakShow   = 0;    // 连杀条剩余显示帧
int             gTFStreakValue  = 0;    // 快照下来的连杀数
int             gTFStreakSeen   = 0;    // 上一帧看到的连杀数，用于检测变化

// 【三十旗】处决爆发环（策划案 7.7）
struct TFBurstRing
{
    float   mX;
    float   mY;
    int     mLife;
    bool    mBig;          // 巨人 / 僵王：更大更重
};

TFBurstRing     gTFBursts[TF_BURST_MAX];
int             gTFBurstCount   = 0;

// 【三十旗】顿帧（策划案 7.7）：只冻结表现层推进，不干预 Board::Update 时序
int             gTFFreezeFrames = 0;
int             gTFHitFlash     = 0;

// 【三十旗】阳光滚动（策划案 7.7）
int             gTFSunSeen      = -1;
int             gTFSunGain      = 0;
int             gTFSunRollValue = 0;
int             gTFSunRollLife  = 0;

// 【三十旗】表现层帧计数（呼吸高亮用）
int             gTFTick         = 0;

void TFPushBurst(float theX, float theY, bool theBig)
{
    int aSlot;
    if (gTFBurstCount < TF_BURST_MAX)
    {
        aSlot = gTFBurstCount++;
    }
    else
    {
        aSlot = 0;
        for (int i = 1; i < TF_BURST_MAX; i++)
        {
            if (gTFBursts[i].mLife < gTFBursts[aSlot].mLife)
                aSlot = i;
        }
    }

    gTFBursts[aSlot].mX = theX;
    gTFBursts[aSlot].mY = theY;
    gTFBursts[aSlot].mLife = TF_BURST_LIFE;
    gTFBursts[aSlot].mBig = theBig;
}

// 用线段近似画一个圆（Graphics 无线圈接口），当作冲击波 / 爆发环
void TFDrawRing(Sexy::Graphics* g, int theCX, int theCY, float theRadius, const Sexy::Color& theColor)
{
    const int aSegments = 28;
    g->SetColor(theColor);

    float aPrevX = (float)theCX + theRadius;
    float aPrevY = (float)theCY;
    for (int i = 1; i <= aSegments; i++)
    {
        float aAngle = (float)i * 6.2831853f / (float)aSegments;
        float aX = (float)theCX + theRadius * cosf(aAngle);
        float aY = (float)theCY + theRadius * sinf(aAngle);
        g->DrawLine((int)aPrevX, (int)aPrevY, (int)aX, (int)aY);
        aPrevX = aX;
        aPrevY = aY;
    }
}
}

// 【三十旗】处决特效（策划案 7.7）：爆发环 + 粒子 + 震屏 + 顿帧，全部复用原版资源。
static void TFSpawnExecution(Board* theBoard, Zombie* theZombie, bool theGiant)
{
    if (!theBoard || !theZombie)
        return;

    int aX = (int)theZombie->mPosX + 20;
    int aY = (int)theZombie->mPosY;
    int aRow = theZombie->mRow;

    TFPushBurst((float)aX, (float)aY, theGiant);

    if (gLawnApp)
    {
        int aRenderOrder = Board::MakeRenderOrder(RENDER_LAYER_TOP, aRow, 0);
        gLawnApp->AddTodParticle((float)aX, (float)aY, aRenderOrder,
            theGiant ? ParticleEffect::PARTICLE_JACKEXPLODE : ParticleEffect::PARTICLE_STARBURST);
    }

    // 震屏：巨人更重（复用原版 Board::ShakeBoard）
    theBoard->ShakeBoard(theGiant ? 4 : 2, theGiant ? -6 : -3);

    // 顿帧：巨人 10 帧、精英 6 帧。只冻结表现层推进，不动 Board::Update 的时序逻辑。
    gTFFreezeFrames = min(TF_FREEZE_MAX_FRAMES, theGiant ? 10 : 6);
    gTFHitFlash = theGiant ? 4 : 0;      // 白闪仅巨人，避免后期精英击杀频繁闪屏
}

// 【三十旗】顿帧查询（策划案 7.7）：由调用方决定如何使用（例如跳过本帧的表现层插值）。
bool ThirtyFlagsShouldFreezeFrame()
{
    return ThirtyFlagsMode() && gTFFreezeFrames > 0;
}

void ThirtyFlagsAddDamageNumber(int theX, int theY, int theDamage, int theColorKind)
{
    if (!ThirtyFlagsMode() || theDamage <= 0)
        return;

    int aSlot;
    if (gTFNumberCount < TF_DAMAGE_NUM_MAX)
    {
        aSlot = gTFNumberCount++;
    }
    else
    {
        // 池满：复用剩余寿命最短的那条
        aSlot = 0;
        for (int i = 1; i < TF_DAMAGE_NUM_MAX; i++)
        {
            if (gTFNumbers[i].mLife < gTFNumbers[aSlot].mLife)
                aSlot = i;
        }
    }

    gTFNumbers[aSlot].mX = (float)theX;
    gTFNumbers[aSlot].mY = (float)theY;
    gTFNumbers[aSlot].mValue = theDamage;
    gTFNumbers[aSlot].mLife = TF_DAMAGE_NUM_LIFE;
    gTFNumbers[aSlot].mColorKind = theColorKind;
}

void ThirtyFlagsUpdateVisuals()
{
    if (!ThirtyFlagsMode())
        return;

    gTFTick++;

    // 【三十旗】顿帧（策划案 7.7）：冻结期间跳字 / 爆发环 / 连杀条 / 阳光滚动全部定格，
    // 只推进计数器本身，用来制造打击瞬间的"顿感"。不触碰 Board::Update 的时序逻辑。
    if (gTFHitFlash > 0)
        gTFHitFlash--;

    if (gTFFreezeFrames > 0)
    {
        gTFFreezeFrames--;
        return;
    }

    // 【三十旗】处决爆发环推进（策划案 7.7）
    for (int i = 0; i < gTFBurstCount; i++)
    {
        if (gTFBursts[i].mLife > 0)
            gTFBursts[i].mLife--;
    }
    for (int i = 0; i < gTFBurstCount; i++)
    {
        if (gTFBursts[i].mLife <= 0 && i < gTFBurstCount - 1)
        {
            gTFBursts[i] = gTFBursts[gTFBurstCount - 1];
            gTFBurstCount--;
            i--;
        }
    }
    if (gTFBurstCount > 0 && gTFBursts[gTFBurstCount - 1].mLife <= 0)
        gTFBurstCount--;

    for (int i = 0; i < gTFNumberCount; i++)
    {
        if (gTFNumbers[i].mLife > 0)
        {
            gTFNumbers[i].mLife--;
            gTFNumbers[i].mY -= 0.9f;      // 缓缓上飘
        }
    }

    // 把已经消失的条目挤到数组尾部，保持前段紧凑
    for (int i = 0; i < gTFNumberCount; i++)
    {
        if (gTFNumbers[i].mLife <= 0 && i < gTFNumberCount - 1)
        {
            gTFNumbers[i] = gTFNumbers[gTFNumberCount - 1];
            gTFNumberCount--;
            i--;
        }
    }
    if (gTFNumberCount > 0 && gTFNumbers[gTFNumberCount - 1].mLife <= 0)
        gTFNumberCount--;

    if (gTFCenterLife > 0)
        gTFCenterLife--;

    // 连杀条：连杀数一变就重新计时
    if (gThirtyFlags.mKillStreak != gTFStreakSeen)
    {
        gTFStreakSeen = gThirtyFlags.mKillStreak;
        if (gTFStreakSeen > 1)
        {
            gTFStreakValue = gTFStreakSeen;
            gTFStreakShow = TF_STREAK_SHOW_LIFE;
        }
    }
    if (gTFStreakShow > 0)
        gTFStreakShow--;

    // 【三十旗】阳光滚动（策划案 7.7）：阳光增加时把增量做成滚动数字，从 0 平滑逼近实际增量。
    // 说明：真正的阳光计数器绘制在 SeedPacket.cpp::SeedBank::Draw（不在本任务文件范围内），
    // 这里用一枚贴近阳光栏的滚动"+N"指示器实现同样的数字滚动过渡。
    Board* aBoard = gLawnApp ? gLawnApp->mBoard : nullptr;
    if (aBoard)
    {
        int aSun = max(aBoard->mSunMoney, 0);
        if (gTFSunSeen < 0)
            gTFSunSeen = aSun;

        if (aSun > gTFSunSeen)
        {
            gTFSunGain += (aSun - gTFSunSeen);
            gTFSunRollValue = 0;
            gTFSunRollLife = TF_SUN_ROLL_LIFE;
        }
        gTFSunSeen = aSun;
    }

    if (gTFSunRollLife > 0)
    {
        gTFSunRollLife--;
        if (gTFSunRollValue < gTFSunGain)
        {
            int aStep = max(1, gTFSunGain / 8 + 1);
            gTFSunRollValue = min(gTFSunGain, gTFSunRollValue + aStep);
        }
        if (gTFSunRollLife <= 0)
        {
            gTFSunGain = 0;
            gTFSunRollValue = 0;
        }
    }
}

void ThirtyFlagsDrawVisuals(Sexy::Graphics* g)
{
    if (!ThirtyFlagsMode() || !g)
        return;

    // 【三十旗】处决爆发环（策划案 7.7）：精英 / 巨人僵尸阵亡处的扩散冲击环
    for (int i = 0; i < gTFBurstCount; i++)
    {
        if (gTFBursts[i].mLife <= 0)
            continue;

        int aElapsed = TF_BURST_LIFE - gTFBursts[i].mLife;
        int anAlpha = ClampInt(gTFBursts[i].mLife * 220 / TF_BURST_LIFE, 0, 220);
        float aMaxRadius = gTFBursts[i].mBig ? 90.0f : 55.0f;
        float aRadius = 8.0f + aMaxRadius * ((float)aElapsed / (float)TF_BURST_LIFE);

        TFDrawRing(g, (int)gTFBursts[i].mX, (int)gTFBursts[i].mY, aRadius,
            gTFBursts[i].mBig ? Sexy::Color(255, 150, 40, anAlpha) : Sexy::Color(255, 235, 120, anAlpha));
        TFDrawRing(g, (int)gTFBursts[i].mX, (int)gTFBursts[i].mY, aRadius * 0.6f,
            Sexy::Color(255, 255, 220, anAlpha / 2));
    }

    // 【三十旗】顿帧高光（策划案 7.7）：顿帧起始一两帧的极淡白闪，强化打击瞬间
    if (gTFHitFlash > 0)
    {
        g->SetColor(Sexy::Color(255, 255, 255, ClampInt(gTFHitFlash * 12, 0, 48)));
        g->FillRect(0, 0, BOARD_WIDTH, BOARD_HEIGHT);
    }

    // 伤害跳字
    for (int i = 0; i < gTFNumberCount; i++)
    {
        if (gTFNumbers[i].mLife <= 0)
            continue;

        SexyString aText = StrFormat(_S("%d"), gTFNumbers[i].mValue);
        int anAlpha = ClampInt(gTFNumbers[i].mLife * 300 / TF_DAMAGE_NUM_LIFE, 0, 255);

        // 【三十旗】7.7 跳字分色：普通黄白 / 冰蓝 / 毒绿 / 火橙 / 暴击金（>=100 大伤害或暴击）
        Sexy::Color aMain;
        switch (gTFNumbers[i].mColorKind)
        {
        case 1:   aMain = Sexy::Color(130, 200, 255, anAlpha); break;
        case 2:   aMain = Sexy::Color(130, 255, 130, anAlpha); break;
        case 3:   aMain = Sexy::Color(255, 150, 60,  anAlpha); break;
        case 4:   aMain = Sexy::Color(255, 215, 0,   anAlpha); break;
        default:  aMain = Sexy::Color(255, 240, 150, anAlpha); break;
        }

        int aPasses = (gTFNumbers[i].mColorKind == 4) ? 9 : 5;   // 大伤害描边更厚
        for (int aPass = 0; aPass < aPasses; aPass++)
        {
            static const int aOff[9][2] = { {0,0},{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1} };
            Sexy::Color aCol = (aPass == 0) ? aMain : Sexy::Color(0, 0, 0, anAlpha);
            TodDrawString(g, aText, (int)gTFNumbers[i].mX + aOff[aPass][0], (int)gTFNumbers[i].mY + aOff[aPass][1],
                Sexy::FONT_HOUSEOFTERROR20, aCol,
                DrawStringJustification::DS_ALIGN_CENTER);
        }
    }

    // 【三十旗】中央公告（换旗大字 / 精英出没）
    if (gTFCenterLife > 0)
    {
        int anAlpha = ClampInt(gTFCenterLife * 400 / gTFCenterLifeMax, 0, 255);
        for (int aPass = 0; aPass < 9; aPass++)
        {
            static const int aOff[9][2] = { {0,0},{2,0},{-2,0},{0,2},{0,-2},{2,2},{2,-2},{-2,2},{-2,-2} };
            Sexy::Color aCol = (aPass == 0) ? Sexy::Color(gTFCenterColor.mRed, gTFCenterColor.mGreen, gTFCenterColor.mBlue, anAlpha)
                                            : Sexy::Color(0, 0, 0, anAlpha);
            TodDrawString(g, gTFCenterText, BOARD_WIDTH / 2 + aOff[aPass][0], 210 + aOff[aPass][1],
                Sexy::FONT_HOUSEOFTERROR20, aCol,
                DrawStringJustification::DS_ALIGN_CENTER);
        }
    }

    // 连杀反馈条
    if (gTFStreakShow > 0 && gTFStreakValue > 1)
    {
        int anAlpha = ClampInt(gTFStreakShow * 300 / TF_STREAK_SHOW_LIFE, 0, 255);
        SexyString aText = StrFormat(_S("%d 连杀"), gTFStreakValue);

        for (int aPass = 0; aPass < 5; aPass++)
        {
            int aOffX = (aPass == 1) - (aPass == 2);
            int aOffY = (aPass == 3) - (aPass == 4);
            Sexy::Color aCol = (aPass == 0) ? Sexy::Color(255, 70, 40, anAlpha)
                                            : Sexy::Color(0, 0, 0, anAlpha);
            TodDrawString(g, aText, BOARD_WIDTH / 2 + aOffX, 150 + aOffY,
                Sexy::FONT_HOUSEOFTERROR20, aCol,
                DrawStringJustification::DS_ALIGN_CENTER);
        }
    }

    // 【三十旗】阳光滚动（策划案 7.7）：滚动"+N"指示器，紧贴阳光栏下方
    if (gTFSunRollLife > 0 && gTFSunGain > 0)
    {
        int anAlpha = ClampInt(gTFSunRollLife * 255 / TF_SUN_ROLL_LIFE, 0, 255);
        SexyString aText = StrFormat(_S("+%d"), gTFSunRollValue);
        TodDrawString(g, aText, 34, 104, Sexy::FONT_DWARVENTODCRAFT12,
            Sexy::Color(255, 240, 120, anAlpha), DrawStringJustification::DS_ALIGN_CENTER);
    }

    // 【三十旗】强化抽取出场演出（策划案 7.7）：抽取阶段给屏幕最外沿加一圈呼吸金色高亮边框。
    // 三选一弹窗本体在 TFDialog.cpp（不在本任务文件范围内），此处用边框高亮烘托"抽取中"氛围；
    // 边框位于屏幕最外沿，不会被居中的弹窗遮住。
    if (gThirtyFlags.mIntermission && !gThirtyFlags.mUpgradeChosen)
    {
        int aPulse = (int)((sinf((float)gTFTick * 0.12f) + 1.0f) * 0.5f * 80.0f) + 120;
        aPulse = ClampInt(aPulse, 0, 255);
        g->SetColor(Sexy::Color(255, 210, 90, aPulse));

        const int aThick = 6;
        g->FillRect(0, 0, BOARD_WIDTH, aThick);
        g->FillRect(0, BOARD_HEIGHT - aThick, BOARD_WIDTH, aThick);
        g->FillRect(0, 0, aThick, BOARD_HEIGHT);
        g->FillRect(BOARD_WIDTH - aThick, 0, aThick, BOARD_HEIGHT);
    }
}

void ThirtyFlagsShowCenterText(const SexyString& theText, const Sexy::Color& theColor, int theLife)
{
    if (!ThirtyFlagsMode())
        return;
    gTFCenterText = theText;
    gTFCenterColor = theColor;
    gTFCenterLife = theLife;
    gTFCenterLifeMax = theLife;
}

// -------------------------------------------------------------------------------------------
// 【三十旗·平衡 v5】破绽标记系统（豌豆射手 = 施加者 / 双发、机枪 = 引爆者）
// 存储用文件级 static 表按僵尸 ID 索引（不得给 Zombie 加成员——SyncBoard 按 sizeof(Board) 写盘）
// -------------------------------------------------------------------------------------------
static int  gTFMarkCount[512];
static int  gTFMarkTimer[512];
static int  gTFMarkID[512];

void ThirtyFlagsAddMark(Zombie* theZombie, Board* theBoard)
{
    if (!ThirtyFlagsMode() || !theZombie || !theBoard)
        return;
    int aZid = theBoard->ZombieGetID(theZombie);
    if (aZid < 0 || aZid >= 512)
        return;
    if (gTFMarkID[aZid] != aZid)
    {
        gTFMarkID[aZid] = aZid;
        gTFMarkCount[aZid] = 0;
    }
    if (gTFMarkCount[aZid] < 5)
        gTFMarkCount[aZid]++;
    gTFMarkTimer[aZid] = 240;   // 4 秒有效期
}

int ThirtyFlagsGetMark(Zombie* theZombie, Board* theBoard)
{
    if (!ThirtyFlagsMode() || !theZombie || !theBoard)
        return 0;
    int aZid = theBoard->ZombieGetID(theZombie);
    if (aZid < 0 || aZid >= 512 || gTFMarkID[aZid] != aZid)
        return 0;
    if (gTFMarkTimer[aZid] <= 0)
        return 0;
    return gTFMarkCount[aZid];
}

static void ThirtyFlagsTickMarks()
{
    for (int i = 0; i < 512; i++)
    {
        if (gTFMarkTimer[i] > 0)
        {
            gTFMarkTimer[i]--;
            if (gTFMarkTimer[i] == 0)
                gTFMarkCount[i] = 0;
        }
    }
}

// -------------------------------------------------------------------------------------------
// 【三十旗】旗间进度存档（用户需求：不用每次从头打）——
// 换旗时自动保存（旗次 + 强化池），三十旗开局时自动恢复并直接从存档旗次开始。
// 文件：Release/thirtyflags_save.ini；删除该文件 = 从第 1 面旗重新开始。
// -------------------------------------------------------------------------------------------
static void ThirtyFlagsSaveProgress()
{
    FILE* f = fopen("thirtyflags_save.ini", "w");
    if (!f) return;
    fprintf(f, "flag=%d\n", gThirtyFlags.mFlag);
    fprintf(f, "upgcount=%d\n", gThirtyFlags.mUpgradeCount);
    for (int i = 0; i < TF_UPG_COUNT; i++)
    {
        if (gThirtyFlags.mUpgradeStacks[i] > 0)
            fprintf(f, "u %d %d\n", i, gThirtyFlags.mUpgradeStacks[i]);
    }
    fclose(f);
}

static bool ThirtyFlagsLoadProgress()
{
    FILE* f = fopen("thirtyflags_save.ini", "r");
    if (!f) return false;
    char aLine[128];
    bool aFound = false;
    gThirtyFlags.mUpgradeCount = 0;
    for (int i = 0; i < TF_UPG_COUNT; i++)
        gThirtyFlags.mUpgradeStacks[i] = 0;
    while (fgets(aLine, sizeof(aLine), f))
    {
        int aFlag, aCount, aIdx, aStacks;
        if (sscanf(aLine, "flag=%d", &aFlag) == 1)      { gThirtyFlags.mFlag = aFlag; aFound = true; }
        else if (sscanf(aLine, "upgcount=%d", &aCount) == 1) gThirtyFlags.mUpgradeCount = aCount;
        else if (sscanf(aLine, "u %d %d", &aIdx, &aStacks) == 2)
        {
            if (aIdx >= 0 && aIdx < TF_UPG_COUNT) gThirtyFlags.mUpgradeStacks[aIdx] = aStacks;
        }
    }
    fclose(f);
    if (gThirtyFlags.mFlag < 1) gThirtyFlags.mFlag = 1;
    if (gThirtyFlags.mFlag > 30) gThirtyFlags.mFlag = 30;
    return aFound;
}

int ThirtyFlagsRollFireballElement(int theFrame)
{
    if (!ThirtyFlagsMode())
        return TF_ELEM_NONE;

    // 【三十旗·平衡 v5】同轮多发弹的元素概率递减：同一帧内第 2/3/4 发弹的元素概率
    // 依次 ×0.6 / ×0.4 / ×0.25 —— 精准削「多弹 build × 火炬」的乘算收益。
    static int aLastFrame = -1;
    static int aCountInFrame = 0;
    if (theFrame != aLastFrame)
    {
        aLastFrame = theFrame;
        aCountInFrame = 0;
    }
    aCountInFrame++;
    int aDecay = (aCountInFrame <= 1) ? 100 : (aCountInFrame == 2 ? 60 : (aCountInFrame == 3 ? 40 : 25));

    int aRoll = Rand(100);
    int aElemGate = 56 * aDecay / 100;   // 元素总概率 56%，受递减衰减
    if (aRoll >= aElemGate)  return TF_ELEM_NONE;   // 普通火球

    // 元素内部分布（按原比例缩放）
    int aSub = Rand(aElemGate);
    int aAcc = 0;
    aAcc += 12 * aDecay / 100;  if (aSub < aAcc) return TF_ELEM_ICE;
    aAcc +=  8 * aDecay / 100;  if (aSub < aAcc) return TF_ELEM_DEEPFREEZE;
    aAcc += 10 * aDecay / 100;  if (aSub < aAcc) return TF_ELEM_POISON;
    aAcc +=  4 * aDecay / 100;  if (aSub < aAcc) return TF_ELEM_CHARM;
    aAcc += 12 * aDecay / 100;  if (aSub < aAcc) return TF_ELEM_BUTTER;
    return TF_ELEM_GIANT;
}

void ThirtyFlagsAddPoisonAt(int theCol, int theRow, int theFrames)
{
    TFAddPoison(theCol, theRow, theFrames);
}

int ThirtyFlagsPlantPierce(int theSeedType)
{
    if (!ThirtyFlagsMode())
        return 0;

    switch ((SeedType)theSeedType)
    {
    case SeedType::SEED_PEASHOOTER:
    case SeedType::SEED_SNOWPEA:
    case SeedType::SEED_REPEATER:
    case SeedType::SEED_GATLINGPEA:
    case SeedType::SEED_THREEPEATER:
    case SeedType::SEED_SPLITPEA:
        return TF_PIERCE_PEA;
    default:
        return 0;
    }
}

int ThirtyFlagsPlantExtraProjectiles(Plant* thePlant)
{
    if (!ThirtyFlagsMode() || !thePlant)
        return 0;

    int aCount = gThirtyFlags.GetExtraProjectiles();

    // 【三十旗】海蘑菇 / 小喷菇：每次射击额外打一发（策划案 6.3）。
    // 真正的"上行额外子弹"在 Plant::UpdateShooter 里单独处理。
    if (thePlant->mSeedType == SeedType::SEED_PUFFSHROOM ||
        thePlant->mSeedType == SeedType::SEED_SEASHROOM)
    {
        aCount += 1;
    }

    return aCount;
}

void ThirtyFlagsPlantDied(Plant* thePlant)
{
    if (!ThirtyFlagsMode() || !thePlant || !thePlant->mBoard)
        return;

    float aRefundRate = gThirtyFlags.GetDeathRefundRate();
    if (aRefundRate > 0.0f)
    {
        int aCost = Plant::GetCost(thePlant->mSeedType, thePlant->mImitaterType);
        int aRefund = (int)((float)aCost * aRefundRate);
        if (aRefund > 0)
        {
            thePlant->mBoard->AddSunMoney(aRefund);
        }
    }

    // 坚果墙：被啃死后魅惑所有正在啃食它的僵尸
    if (thePlant->mSeedType == SeedType::SEED_WALLNUT || thePlant->mSeedType == SeedType::SEED_GIANT_WALLNUT ||
        thePlant->mSeedType == SeedType::SEED_EXPLODE_O_NUT)
    {
        int aCharmed = 0;
        Zombie* aZombie = nullptr;
        while (thePlant->mBoard->IterateZombies(aZombie))
        {
            if (aZombie->IsDeadOrDying() || aZombie->mMindControlled || aZombie->mRow != thePlant->mRow)
                continue;

            if (fabsf(aZombie->mPosX - (float)thePlant->mX) <= 90.0f)
            {
                aZombie->mMindControlled = true;
                aCharmed++;
            }
        }

        if (aCharmed > 0)
        {
            thePlant->mBoard->mApp->AddTodParticle((float)thePlant->mX + 40.0f, (float)thePlant->mY + 40.0f,
                Board::MakeRenderOrder(RENDER_LAYER_PARTICLE, thePlant->mRow, 0), PARTICLE_MIND_CONTROL);
            thePlant->mBoard->mApp->PlaySample(SOUND_MINDCONTROLLED);
        }
    }

    // 叶子保护伞：范围内植物阵亡时消耗自身生命弹开啃食者
    if (thePlant->mSeedType != SeedType::SEED_UMBRELLA && thePlant->mBoard)
    {
        Plant* aUmbrella = thePlant->mBoard->FindUmbrellaPlant(thePlant->mPlantCol, thePlant->mRow);
        if (aUmbrella && aUmbrella->mPlantHealth > 5)
        {
            int aPushed = 0;
            Zombie* aZombie = nullptr;
            while (thePlant->mBoard->IterateZombies(aZombie))
            {
                if (aZombie->IsDeadOrDying() || aZombie->mRow != thePlant->mRow)
                    continue;

                if (fabsf(aZombie->mPosX - (float)thePlant->mX) <= 90.0f)
                {
                    aZombie->mPosX += 80.0f;
                    aZombie->TakeDamage(100, 0U);
                    aZombie->ApplyChill(false);
                    aPushed++;
                }
            }

            if (aPushed > 0)
            {
                aUmbrella->mPlantHealth -= 5 * aPushed;
                if (aUmbrella->mPlantHealth <= 0)
                {
                    aUmbrella->Die();
                }
            }
        }
    }

    int aGodNut = gThirtyFlags.GetStacks(TF_UPG_GOD_NUT);
    if (aGodNut > 0)
    {
        if (thePlant->mSeedType == SeedType::SEED_WALLNUT || thePlant->mSeedType == SeedType::SEED_TALLNUT ||
            thePlant->mSeedType == SeedType::SEED_PUMPKINSHELL || thePlant->mSeedType == SeedType::SEED_EXPLODE_O_NUT)
        {
            thePlant->mBoard->KillAllZombiesInRadius(thePlant->mRow, thePlant->mX, thePlant->mY, 200, 0, false, 0);
        }
    }
}

int ThirtyFlagsSunflowerExtraSun(Plant* thePlant)
{
    if (!ThirtyFlagsMode() || !thePlant)
        return 0;

    float aBonus = gThirtyFlags.GetSunflowerYieldBonus();
    int aExtra = (int)aBonus - 1;
    if (aExtra < 0)
        return 0;

    float aFraction = aBonus - (float)(int)aBonus;
    if (aFraction > 0.0f && Rand(1000) < (int)(aFraction * 1000.0f))
    {
        aExtra++;
    }

    return aExtra;
}

// ----------------------------------------------------------------------------------------------------
// 场地绘制：草皮滚出动画 + 局部水面 + 未开放行黑幕
// ----------------------------------------------------------------------------------------------------
// 三十旗场地绘制：未开放行 = 裸土，已开放行 = 草皮（含滚动铺开动画），水行 = 整行水面。
// 全部使用不拉伸的 DrawImage(img, x, y, srcRect)，绕开 DDImage::StretchBlt。
//
// 坐标说明：DrawBackdrop 在 Board 的坐标系里调用，原点被平移了 -BOARD_OFFSET，
// 因此「客户区 x=0」对应此处的 x=-BOARD_OFFSET。草坪左界 LAWN_XMIN=40 在此处是 260。
static const int TF_BOARD_OFFSET = 220;
static const int TF_LAWN_LEFT = 260;          // 草坪左界在 Board 局部坐标里的 x
static const int TF_ROW_H = 100;              // 日间布局行高


// ====================================================================================================
// Final boss - "Doom Mech" (plan chapter 8)
//
// The stock ZOMBIE_BOSS already provides stomp / bungee / head-spit skills driven by DamageIndex,
// so this layer only adds what the plan asks for on top of it:
//   * a shared 3-phase health pool with an invulnerable phase transition,
//   * a telegraph-then-strike skill loop keyed to the current phase,
//   * P3 self destruct countdown.
// All effects reuse vanilla primitives (KillAllPlantsInRadius / AddZombieInRow / plant row rewrite),
// so no new art, animation or sound is required.
// ====================================================================================================

enum ThirtyFlagsBossSkill
{
    TF_BOSS_SKILL_NONE = 0,
    TF_BOSS_SKILL_SLAM,             // P1: crush a 2x2 block of plants
    TF_BOSS_SKILL_HORDE,            // P1: summon a wave of elites down two lanes
    TF_BOSS_SKILL_TERRAIN,          // P2: turn a whole row into water
    TF_BOSS_SKILL_EMP,              // P2: disable all shooters for 10s
    TF_BOSS_SKILL_ROAR,             // P2: board-wide zombie speed +50% for 8s
    TF_BOSS_SKILL_ROAR_ULTIMATE,    // P3: destroy every plant in the frontmost column
    TF_BOSS_SKILL_DOMAIN            // P3: zombies lifesteal and leave poison
};

constexpr int TF_BOSS_VANILLA_PHASE_HEALTH = TF_BOSS_HEALTH / 3;

bool ThirtyFlagsBossWantsBoss(int theFlag)
{
    return theFlag == TF_BOSS_FLAG;
}

bool ThirtyFlagsBossIsEmpActive()
{
    return ThirtyFlagsMode() && gThirtyFlags.mBossEmpTimer > 0;
}

bool ThirtyFlagsBossIsDomainActive()
{
    return ThirtyFlagsMode() && gThirtyFlags.mBossDomainTimer > 0;
}

static int TFBossPhaseForHealth(Zombie* theBoss)
{
    if (!theBoss || theBoss->mBodyMaxHealth <= 0)
        return 1;

    int aPercent = theBoss->mBodyHealth * 100 / theBoss->mBodyMaxHealth;
    if (aPercent <= TF_BOSS_PHASE3_PERCENT)
        return 3;
    if (aPercent <= TF_BOSS_PHASE2_PERCENT)
        return 2;
    return 1;
}

// Frames until the next skill for the given phase.
static int TFBossSkillInterval(int thePhase, int theSkill)
{
    switch (theSkill)
    {
    case TF_BOSS_SKILL_SLAM:     return TF_BOSS_SLAM_INTERVAL;
    case TF_BOSS_SKILL_HORDE:    return TF_BOSS_HORDE_INTERVAL;
    case TF_BOSS_SKILL_TERRAIN:  return TF_BOSS_TERRAIN_INTERVAL;
    case TF_BOSS_SKILL_EMP:      return 1200;
    case TF_BOSS_SKILL_ROAR:     return TF_BOSS_ROAR_INTERVAL;
    case TF_BOSS_SKILL_ROAR_ULTIMATE: return TF_BOSS_ROAR_INTERVAL;
    case TF_BOSS_SKILL_DOMAIN:   return TF_BOSS_DOMAIN_INTERVAL;
    default:                     return 600;
    }
}

// Pick the next skill for a phase (alternates its two signature skills).
static int TFBossPickSkill(int thePhase, int theLastSkill)
{
    if (thePhase == 1)
        return (theLastSkill == TF_BOSS_SKILL_SLAM) ? TF_BOSS_SKILL_HORDE : TF_BOSS_SKILL_SLAM;
    if (thePhase == 2)
    {
        if (theLastSkill == TF_BOSS_SKILL_TERRAIN) return TF_BOSS_SKILL_EMP;
        if (theLastSkill == TF_BOSS_SKILL_EMP)     return TF_BOSS_SKILL_ROAR;
        return TF_BOSS_SKILL_TERRAIN;
    }
    if (theLastSkill == TF_BOSS_SKILL_ROAR_ULTIMATE) return TF_BOSS_SKILL_DOMAIN;
    if (theLastSkill == TF_BOSS_SKILL_DOMAIN)        return TF_BOSS_SKILL_ROAR_ULTIMATE;
    return TF_BOSS_SKILL_ROAR_ULTIMATE;
}

static void TFBossSummonBoss(Board* theBoard)
{
    Zombie* aBoss = theBoard->AddZombieInRow(ZOMBIE_BOSS, Rand(TF_LAWN_ROWS), theBoard->mCurrentWave);
    if (!aBoss)
        return;

    aBoss->mBodyHealth = TF_BOSS_HEALTH;
    aBoss->mBodyMaxHealth = TF_BOSS_HEALTH;
    gThirtyFlags.mBossPtr = aBoss;
    gThirtyFlags.mBossPhase = 1;
    gThirtyFlags.mBossSkillTimer = TF_BOSS_SLAM_INTERVAL;
    gThirtyFlags.mBossSelfDestruct = 0;



    gLawnApp->TfLog("boss summoned hp=%d row=%d", aBoss->mBodyMaxHealth, aBoss->mRow);

}

// P1 slam: crush a 2x2 block of plants just in front of the boss.
static void TFBossDoSlam(Board* theBoard, Zombie* theBoss, int theRow)
{
    int aCol = TFClampI(theBoard->PixelToGridX((int)theBoss->mPosX, (int)theBoss->mPosY), 0, MAX_GRID_SIZE_X - 1);
    int aMinCol = max(0, aCol - 2);
    for (int aC = aMinCol; aC <= aMinCol + 1 && aC < MAX_GRID_SIZE_X; aC++)
    {
        for (int aR = theRow; aR <= theRow + 1 && aR < TF_LAWN_ROWS; aR++)
        {
            Plant* aPlant = theBoard->GetTopPlantAt(aC, aR, TOPPLANT_ANY);
            if (aPlant && !aPlant->mDead)
                aPlant->Die();
        }
    }

    int aPixelX = theBoard->GridToPixelX(aMinCol, theRow);
    int aPixelY = theBoard->GridToPixelY(aMinCol, theRow);
    theBoard->KillAllPlantsInRadius(aPixelX, aPixelY, 60);
    theBoard->mApp->PlayFoley(FOLEY_THUNDER);
}

// P1 horde: six elites split across two lanes.
static void TFBossDoHorde(Board* theBoard, int theRow)
{
    static const ZombieType aPool[] = { ZOMBIE_NORMAL, ZOMBIE_PAIL, ZOMBIE_FOOTBALL, ZOMBIE_GARGANTUAR, ZOMBIE_DOOR, ZOMBIE_LADDER };
    int aOtherRow = (theRow + 2) % TF_LAWN_ROWS;

    for (int i = 0; i < 6; i++)
    {
        int aRow = (i % 2 == 0) ? theRow : aOtherRow;
        Zombie* aZombie = theBoard->AddZombieInRow(aPool[Rand(6)], aRow, theBoard->mCurrentWave);
        if (!aZombie)
            continue;

        // reuse the elite path so summoned zombies carry the same modifiers
        ThirtyFlagsZombieInit(aZombie);
        aZombie->mPosX = (float)(MAX_GRID_SIZE_X * 80 + 40 + i * 20);
    }
}

// P2 terrain: turn a row into water; anything without a lilypad drowns.
static void TFBossDoTerrain(Board* theBoard, int theRow)
{
    theBoard->mPlantRow[theRow] = PLANTROW_POOL;
    for (int aC = 0; aC < MAX_GRID_SIZE_X; aC++)
    {
        theBoard->mGridSquareType[aC][theRow] = GRIDSQUARE_POOL;

        Plant* aPlant = theBoard->GetTopPlantAt(aC, theRow, TOPPLANT_ANY);
        if (aPlant && !aPlant->mDead && aPlant->mSeedType != SEED_LILYPAD && aPlant->mSeedType != SEED_TANGLEKELP)
            aPlant->Die();
    }

    gThirtyFlags.mWaterRows |= (1 << theRow);
    gThirtyFlags.mWaterCols = MAX_GRID_SIZE_X;
}

// P3 ultimate: destroy every plant in the frontmost occupied column.
static void TFBossDoRoarUltimate(Board* theBoard)
{
    int aFrontCol = -1;
    for (int aC = 0; aC < MAX_GRID_SIZE_X && aFrontCol < 0; aC++)
    {
        for (int aR = 0; aR < TF_LAWN_ROWS; aR++)
        {
            Plant* aPlant = theBoard->GetTopPlantAt(aC, aR, TOPPLANT_ANY);
            if (aPlant && !aPlant->mDead)
            {
                aFrontCol = aC;
                break;
            }
        }
    }

    if (aFrontCol < 0)
        return;

    for (int aR = 0; aR < TF_LAWN_ROWS; aR++)
    {
        Plant* aPlant = theBoard->GetTopPlantAt(aFrontCol, aR, TOPPLANT_ANY);
        if (aPlant && !aPlant->mDead)
            aPlant->Die();
    }
    theBoard->mApp->PlayFoley(FOLEY_THUNDER);
}

// Execute a telegraphed skill.
static void TFBossExecuteSkill(Board* theBoard, Zombie* theBoss, int theSkill, int theRow)
{
    switch (theSkill)
    {
    case TF_BOSS_SKILL_SLAM:
        TFBossDoSlam(theBoard, theBoss, theRow);
        break;

    case TF_BOSS_SKILL_HORDE:
        TFBossDoHorde(theBoard, theRow);
        break;

    case TF_BOSS_SKILL_TERRAIN:
        TFBossDoTerrain(theBoard, theRow);
        break;

    case TF_BOSS_SKILL_EMP:
        gThirtyFlags.mBossEmpTimer = TF_BOSS_EMP_FRAMES;
        break;

    case TF_BOSS_SKILL_ROAR:
        gThirtyFlags.mBossRoarTimer = TF_BOSS_ROAR_FRAMES;
        break;

    case TF_BOSS_SKILL_ROAR_ULTIMATE:
        TFBossDoRoarUltimate(theBoard);
        break;

    case TF_BOSS_SKILL_DOMAIN:
        gThirtyFlags.mBossDomainTimer = TF_BOSS_ROAR_FRAMES;
        break;

    default:
        break;
    }
}

// Zombie slots are recycled, so a cached pointer is only trusted while the zombie is
// still present in the board array and alive.
static Zombie* TFBossResolve(Board* theBoard)
{
    Zombie* aCached = gThirtyFlags.mBossPtr;
    if (!aCached)
        return NULL;

    for (unsigned int i = 0; i < theBoard->mZombies.mMaxUsedCount; i++)
    {
        Zombie* aSlot = &theBoard->mZombies.mBlock[i].mItem;
        if (aSlot == aCached && (theBoard->mZombies.mBlock[i].mID & DATA_ARRAY_INDEX_MASK) != DATA_ARRAY_INDEX_MASK)
            return aCached->mDead ? NULL : aCached;
    }
    return NULL;
}

void ThirtyFlagsBossUpdate(Board* theBoard)
{
    if (!ThirtyFlagsMode() || !theBoard)
        return;

    int aFlag = ThirtyFlagsCurrentFlag(theBoard);

    // spawn on the final flag
    if (gThirtyFlags.mBossPhase == 0)
    {
        if (!ThirtyFlagsBossWantsBoss(aFlag))
            return;
        // 僵王的美术资源不在常规预加载清单里（白名单只对第 30 旗放开），显式补一次。

        Zombie::PreloadZombieResources(ZOMBIE_BOSS);

        TFBossSummonBoss(theBoard);

        return;
    }

    Zombie* aBoss = TFBossResolve(theBoard);
    if (!aBoss || aBoss->mDead)
    {
        gThirtyFlags.mBossPhase = 0;
        gThirtyFlags.mBossPtr = NULL;
        return;
    }

    // phase transition (with a brief invulnerable window)
    int aWantPhase = TFBossPhaseForHealth(aBoss);
    if (aWantPhase > gThirtyFlags.mBossPhase)
    {
        gThirtyFlags.mBossPhase = aWantPhase;
        gThirtyFlags.mBossSkillTimer = TFBossSkillInterval(aWantPhase, TF_BOSS_SKILL_NONE);
        gThirtyFlags.mBossWarnTimer = 0;

        theBoard->mApp->PlayFoley(FOLEY_THUNDER);

        gLawnApp->TfLog("boss phase -> %d", aWantPhase);
    }

    // P3 self destruct countdown
    if (gThirtyFlags.mBossPhase == 3)
    {
        int aPercent = aBoss->mBodyHealth * 100 / max(1, aBoss->mBodyMaxHealth);
        if (aPercent <= TF_BOSS_SELF_DESTRUCT_PERCENT && gThirtyFlags.mBossSelfDestruct == 0)
            gThirtyFlags.mBossSelfDestruct = TF_BOSS_SELF_DESTRUCT_FRAMES;

        if (gThirtyFlags.mBossSelfDestruct > 0)
        {
            gThirtyFlags.mBossSelfDestruct--;
            if (gThirtyFlags.mBossSelfDestruct == 0)
            {
                // The mech detonates: it dies outright, which ends the fight as a win.
                aBoss->mBodyHealth = 0;
                aBoss->DieNoLoot();
                theBoard->mApp->PlayFoley(FOLEY_BOSS_EXPLOSION_SMALL);
            }
        }
    }

    // skill timers
    if (gThirtyFlags.mBossEmpTimer > 0)
        gThirtyFlags.mBossEmpTimer--;
    if (gThirtyFlags.mBossRoarTimer > 0)
        gThirtyFlags.mBossRoarTimer--;
    if (gThirtyFlags.mBossDomainTimer > 0)
        gThirtyFlags.mBossDomainTimer--;

    // telegraph resolves first
    if (gThirtyFlags.mBossWarnTimer > 0)
    {
        gThirtyFlags.mBossWarnTimer--;
        if (gThirtyFlags.mBossWarnTimer == 0)
        {
            TFBossExecuteSkill(theBoard, aBoss, gThirtyFlags.mBossWarnSkill, gThirtyFlags.mBossWarnRow);
            gThirtyFlags.mBossSkillTimer = TFBossSkillInterval(gThirtyFlags.mBossPhase, gThirtyFlags.mBossWarnSkill);
            gThirtyFlags.mBossWarnSkill = TF_BOSS_SKILL_NONE;
        }
        return;
    }

    if (gThirtyFlags.mBossSkillTimer > 0)
    {
        gThirtyFlags.mBossSkillTimer--;
        return;
    }

    // start a new telegraph
    int aSkill = TFBossPickSkill(gThirtyFlags.mBossPhase, gThirtyFlags.mBossWarnSkill);
    gThirtyFlags.mBossWarnSkill = aSkill;
    gThirtyFlags.mBossWarnRow = TFClampI(aBoss->mRow, 0, TF_LAWN_ROWS - 1);
    gThirtyFlags.mBossWarnTimer = TF_BOSS_WARN_FRAMES;
}

void ThirtyFlagsDrawBackdrop(Board* theBoard, Graphics* g)
{
    if (!ThirtyFlagsMode() || !theBoard || !g)
        return;

    const int aBgLeft = -BOARD_OFFSET;

    // ---- 1) 未开放的行：用「未铺草皮」底图的对应条带覆盖，呈现裸土 ----
    // 两块贴图（BACKGROUND1 / BACKGROUND1UNSODDED）的草坪区域构图一致，唯一差别就是有没有草皮，
    // 因此按同行同列取条带即可精确对齐，且**不需要拉伸**。
    if (Sexy::IMAGE_BACKGROUND1UNSODDED != nullptr)
    {
        for (int aRow = 0; aRow < MAX_GRID_SIZE_Y && aRow < TF_LAWN_ROWS; aRow++)
        {
            if (gThirtyFlags.IsRowUnlocked(aRow))
                continue;

            int aY = theBoard->GridToPixelY(0, aRow);
            int aSrcX = TF_LAWN_LEFT + BOARD_OFFSET;        // 转回贴图坐标
            Rect aSrc(aSrcX, aY, MAX_GRID_SIZE_X * 80, TF_ROW_H);
            g->DrawImage(Sexy::IMAGE_BACKGROUND1UNSODDED, TF_LAWN_LEFT, aY, aSrc);
        }
    }

    // ---- 2) 草皮滚动铺开：按进度从左向右揭示 IMAGE_SOD1ROW ----
    for (int aRow = 0; aRow < MAX_GRID_SIZE_Y && aRow < TF_LAWN_ROWS; aRow++)
    {
        if (!gThirtyFlags.IsRowUnlocked(aRow))
            continue;

        int aProgress = gThirtyFlagsSodProgress[aRow];
        if (aProgress <= 0 || aProgress >= 1000)
            continue;                       // 未开始 / 已铺完（背景自带草皮）

        if (Sexy::IMAGE_SOD1ROW == nullptr)
            continue;

        int aY = theBoard->GridToPixelY(0, aRow);
        int aFullWidth = Sexy::IMAGE_SOD1ROW->GetWidth();
        int aWidth = (int)((float)aFullWidth * (float)aProgress / 1000.0f);
        if (aWidth <= 0)
            continue;

        Rect aSrcRect(0, 0, aWidth, Sexy::IMAGE_SOD1ROW->GetHeight());
        g->DrawImage(Sexy::IMAGE_SOD1ROW, TF_LAWN_LEFT - 40, aY, aSrcRect);
    }

    // ---- 3) 水路：整行铺 IMAGE_POOL ----
    if (gThirtyFlags.mWaterRows != 0 && Sexy::IMAGE_POOL != nullptr &&
        Sexy::IMAGE_POOL->GetWidth() > 0 && Sexy::IMAGE_POOL->GetHeight() > 0)
    {
        int aPoolW = Sexy::IMAGE_POOL->GetWidth();
        int aPoolH = Sexy::IMAGE_POOL->GetHeight();

        for (int aRow = 0; aRow < MAX_GRID_SIZE_Y && aRow < TF_LAWN_ROWS; aRow++)
        {
            if (!gThirtyFlags.IsWaterRow(aRow))
                continue;

            int aY = theBoard->GridToPixelY(0, aRow);
            g->DrawImage(Sexy::IMAGE_POOL, TF_LAWN_LEFT - 30, aY - 5, Rect(0, 0, aPoolW, aPoolH));
        }
    }

#ifndef TF_PLAYER_BUILD
    {
        SexyString aLine1 = StrFormat(_S("FLAG %d/30"), gThirtyFlags.mFlag);
        SexyString aLine2 = StrFormat(_S("unlock %d%d%d%d%d"),
            gThirtyFlags.IsRowUnlocked(0) ? 1 : 0, gThirtyFlags.IsRowUnlocked(1) ? 1 : 0,
            gThirtyFlags.IsRowUnlocked(2) ? 1 : 0, gThirtyFlags.IsRowUnlocked(3) ? 1 : 0,
            gThirtyFlags.IsRowUnlocked(4) ? 1 : 0);
        SexyString aLine3 = StrFormat(_S("water %d%d%d%d%d"),
            gThirtyFlags.IsRowUnlocked(0) ? (gThirtyFlags.IsWaterRow(0) ? 1 : 0) : 9,
            gThirtyFlags.IsRowUnlocked(1) ? (gThirtyFlags.IsWaterRow(1) ? 1 : 0) : 9,
            gThirtyFlags.IsRowUnlocked(2) ? (gThirtyFlags.IsWaterRow(2) ? 1 : 0) : 9,
            gThirtyFlags.IsRowUnlocked(3) ? (gThirtyFlags.IsWaterRow(3) ? 1 : 0) : 9,
            gThirtyFlags.IsRowUnlocked(4) ? (gThirtyFlags.IsWaterRow(4) ? 1 : 0) : 9);
        SexyString aLine4 = StrFormat(_S("upg %d  elite %d%%  kill %d"),
            gThirtyFlags.mUpgradeCount, gThirtyFlags.GetEliteChancePercent(), gThirtyFlags.mTotalKills);

        g->SetFont(Sexy::FONT_DWARVENTODCRAFT12);
        g->SetColor(Color(0, 0, 0, 190));
        g->FillRect(2, 2, 152, 64);
        g->SetColor(Color(255, 255, 80));
        g->DrawString(aLine1, 8, 5);
        g->DrawString(aLine2, 8, 20);
        g->DrawString(aLine3, 8, 35);
        g->SetColor(Color(140, 255, 140));
        g->DrawString(aLine4, 8, 50);
    }
#endif
}
void ThirtyFlagsUpdateSod(Board* theBoard)
{
    if (!ThirtyFlagsMode() || !theBoard)
        return;

    // 检测新解锁的行 -> 启动滚动
    int aUnlocked = gThirtyFlags.mUnlockedRows;
    int aNewly = aUnlocked & ~gThirtyFlagsPrevUnlockedRows;
    if (aNewly != 0)
    {
        for (int aRow = 0; aRow < MAX_GRID_SIZE_Y && aRow < 5; aRow++)
        {
            if (aNewly & (1 << aRow))
            {
                gThirtyFlagsSodProgress[aRow] = 1;

                // 【三十旗】新解锁的行补放小推车 —— 只在解锁那一帧补一次。
                // 不能用“每帧幂等补齐”：那样玩家丢车后下一帧就会凭空补回来。
                if (theBoard->mPlantRow[aRow] != PlantRowType::PLANTROW_DIRT &&
                    theBoard->FindLawnMowerInRow(aRow) == nullptr)
                {
                    LawnMower* aMower = theBoard->mLawnMowers.DataArrayAlloc();
                    aMower->LawnMowerInitialize(aRow);
                    aMower->mVisible = false;
                }
            }
        }
        gThirtyFlagsPrevUnlockedRows = aUnlocked;
    }

    // 推进动画
    for (int aRow = 0; aRow < MAX_GRID_SIZE_Y && aRow < 5; aRow++)
    {
        if (gThirtyFlagsSodProgress[aRow] > 0 && gThirtyFlagsSodProgress[aRow] < 1000)
        {
            gThirtyFlagsSodProgress[aRow] = min(1000, gThirtyFlagsSodProgress[aRow] + 25);
        }
    }

    // 水格淡入
    for (int x = 0; x < MAX_GRID_SIZE_X; x++)
    {
        for (int y = 0; y < MAX_GRID_SIZE_Y; y++)
        {
            int aTarget = gThirtyFlags.IsWaterCell(y, x) ? 100 : 0;
            if (gThirtyFlagsWaterFade[x][y] < aTarget)
            {
                gThirtyFlagsWaterFade[x][y] = min(aTarget, gThirtyFlagsWaterFade[x][y] + 5);
            }
            else if (gThirtyFlagsWaterFade[x][y] > aTarget)
            {
                gThirtyFlagsWaterFade[x][y] = max(aTarget, gThirtyFlagsWaterFade[x][y] - 5);
            }
        }
    }
}

// ----------------------------------------------------------------------------------------------------
// 场地每帧
// ----------------------------------------------------------------------------------------------------
void ThirtyFlagsBoardUpdate(Board* theBoard)
{
    ThirtyFlagsTickMarks();   // ThirtyFlags v5: break-mark timer
    if (!ThirtyFlagsMode() || !theBoard)
        return;

    // 【三十旗】关卡结束判定修正（策划案第 2 章）：
    // 波次表一次建满 180 波，但“一面旗”只有 6 波。把 mNumWaves 校正为**本旗末波**，
    // 这样每打满 6 波 → 场面清空 → FadeOutLevel → CheckForGameEnd →
    // mSurvivalStage++ / AdvanceFlag → InitSurvivalStage → ShowSeedChooserScreen（选卡）。
    // 原先 mNumWaves 恒为 180，导致要打满 180 波才第一次进选卡，旗次/草皮/强化全部不推进。
    theBoard->mNumWaves = gThirtyFlags.mFlag * TF_WAVES_PER_FLAG;

    // 【三十旗】流程日志：旗次/波号变化时记录一次（排查实测问题用）
    {
        static int aLastFlag = -1;
        static int aLastWave = -1;
        if (theBoard->mCurrentWave != aLastWave || gThirtyFlags.mFlag != aLastFlag)
        {
            aLastWave = theBoard->mCurrentWave;
            aLastFlag = gThirtyFlags.mFlag;
            char aBuf[128];
            sprintf(aBuf, "flag=%d wave=%d numWaves=%d stage=%d", gThirtyFlags.mFlag,
                theBoard->mCurrentWave, theBoard->mNumWaves, theBoard->mChallenge->mSurvivalStage);
            TFLog(aBuf);
        }
    }

    ThirtyFlagsUpdateSod(theBoard);
    gThirtyFlags.UpdateKillStreak();
    ThirtyFlagsBossUpdate(theBoard);

    if (gThirtyFlags.mIntermission && gThirtyFlags.mPrepTimeLeft > 0)
    {
        gThirtyFlags.mPrepTimeLeft--;
    }

    // 整备阶段且场上已清空时，弹出肉鸽强化三选一
    if (gThirtyFlags.mIntermission && !gThirtyFlags.mUpgradeChosen && !gThirtyFlags.mUpgradeDialogShown)
    {
        bool aAnyZombie = false;
        Zombie* aCheck = nullptr;
        while (theBoard->IterateZombies(aCheck))
        {
            if (!aCheck->IsDeadOrDying())
            {
                aAnyZombie = true;
                break;
            }
        }

        if (!aAnyZombie)
        {
            gThirtyFlags.mUpgradeDialogShown = true;
            TFLog("upgrade dialog SHOWN");
            theBoard->mApp->DoDialog(Dialogs::DIALOG_THIRTY_FLAGS, true, _S(""), _S(""), _S(""), Dialog::BUTTONS_NONE);
        }
    }

    int aStopFrames = gThirtyFlags.GetTimeStopSeconds() * 10;
    bool aTimeStopped = (aStopFrames > 0) && (theBoard->mMainCounter < aStopFrames);

    float aGlobalSpeed = gThirtyFlags.GetSpeedScale();

    Zombie* aZombie = nullptr;
    while (theBoard->IterateZombies(aZombie))
    {
        if (aZombie->IsDeadOrDying() || aZombie->mZombieType == ZOMBIE_BOSS)
            continue;

        float aMul = aGlobalSpeed;

        if (TFHasElite(aZombie, ELITE_SWIFT))

        {

            aMul *= 1.60f;

        }



        // 【三十旗】僵王 P2「尸王号令」：号令期间全体僵尸移速 +50%。

        if (gThirtyFlags.mBossRoarTimer > 0)

        {

            aMul *= (1.0f + (float)TF_BOSS_ROAR_SPEED_PERCENT / 100.0f);

        }

        float aPrev = aZombie->mVelZ;
        if (aPrev <= 0.01f)
            aPrev = 1.0f;

        if (aZombie->mVelX > 0.0f)
        {
            aZombie->mVelX = (aZombie->mVelX / aPrev) * aMul;
        }
        aZombie->mVelZ = aMul;

        if (aTimeStopped)
        {
            aZombie->mVelX = 0.0f;
        }

        ThirtyFlagsZombieUpdate(aZombie);
    }

    // 投矛清理
    {
        Projectile* aProjectile = nullptr;
        while (theBoard->IterateProjectiles(aProjectile))
        {
            if (aProjectile->mProjectileType == PROJECTILE_CACTUS_SPEAR && aProjectile->mPosX < 20.0f)
            {
                aProjectile->mDead = true;
            }
        }
    }

    // 尸毒
    bool aHasPoison = false;
    for (int x = 0; x < MAX_GRID_SIZE_X; x++)
    {
        for (int y = 0; y < MAX_GRID_SIZE_Y; y++)
        {
            if (gThirtyFlagsPoison[x][y] > 0)
            {
                gThirtyFlagsPoison[x][y]--;
                aHasPoison = true;
            }
        }
    }

    if (aHasPoison && (theBoard->mMainCounter % 25) == 0)
    {
        Plant* aPlant = nullptr;
        while (theBoard->IteratePlants(aPlant))
        {
            if (aPlant->mPlantCol < 0 || aPlant->mPlantCol >= MAX_GRID_SIZE_X ||
                aPlant->mRow < 0 || aPlant->mRow >= MAX_GRID_SIZE_Y)
                continue;

            if (gThirtyFlagsPoison[aPlant->mPlantCol][aPlant->mRow] > 0)
            {
                aPlant->mPlantHealth -= 4;
                if (aPlant->mPlantHealth <= 0)
                {
                    theBoard->mPlantsEaten++;
                    aPlant->Die();
                }
            }
        }
    }
}
