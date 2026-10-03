// ==================================================================================================
// 【三十旗】构建开关
//   TF_PLAYER_BUILD —— 游玩版：关闭全部调试输出（TFLog 文件日志 / 左上角调试 HUD / TfLogWrite）。
//                     构建游玩版：cl ... /DTF_PLAYER_BUILD /DPVZ_DEBUG_ENABLED=0
//                     开发调试时：都不定义即可（保留全部日志）
//   PVZ_DEBUG_ENABLED=0 —— PvzDebug 模块整体编译移除（见 TodLib/PvzDebug.h 的 master switch）
// ==================================================================================================
// #define TF_PLAYER_BUILD

#ifndef __THIRTY_FLAGS_H__
#define __THIRTY_FLAGS_H__

// ====================================================================================================
// 《植物大战僵尸：三十旗》—— 1-1 单关卡地狱改版
// 策划案实现模块。本文件只声明「规则与数据」，玩法钩子分散接入 Board / LawnApp / Zombie / Plant。
//
// 设计原则（对应策划案 10.2「明确不做」）：
//   * 不改动植物与僵尸的基础机制、AI、渲染与种植交互，只改数值与触发规则；
//   * 不引入新贴图 / 新动画 / 新音效，全部复用原版资源。
//
// 接入点总览：
//   Board::PickBackground / InitLevel / PickZombieWaves / UpdateGameObjects / UpdateSunSpawning
//   LawnApp::CheckForGameEnd / HasSeedType / GetNumSeedsInBank
//   Zombie::ZombieInitialize / TakeDamage / DieNoLoot / EatPlant / Update
//   Plant::PlantInitialize / UpdateShooter / Die
// ====================================================================================================

#include "LawnCommon.h"
#include "Zombie.h"

class Board;
class Plant;

// ----------------------------------------------------------------------------------------------------
// 常量
// ----------------------------------------------------------------------------------------------------
constexpr int   TF_TOTAL_FLAGS          = 30;    // 总旗数（第 30 旗为僵王终战）
constexpr int   TF_WAVES_PER_FLAG       = 6;     // 每面旗包含 6 小波（旗帜波落在每面旗末波）
constexpr int   TF_SEED_SLOTS           = 10;    // 固定 10 卡槽
constexpr int   TF_SEED_SLOTS_MAX       = 14;    // 「无限」强化后的卡槽上限
constexpr int   TF_STARTING_SUN         = 100;   // 开局阳光（策划案 9.2）
constexpr int   TF_EARLY_SUN_MAX        = 150;   // 提前开波最高阳光奖励
constexpr int   TF_PREP_TIME            = 1200;  // 整备阶段基准计时（用于提前开波奖励换算）
constexpr int   TF_MUTATION_PICKS       = 5;     // 第 5/10/15/20/25 旗后各抽 1 条
constexpr int   TF_ELITE_START_FLAG     = 8;     //ThirtyFlags: elite from flag 8 (user request)    // 第 16 旗起僵尸开始精英化
constexpr int   TF_ELITE_MAX            = 3;     // 单只僵尸最多叠加的精英词条数
constexpr int   TF_ELITE_KIND_COUNT     = 7;     // 精英词条数
constexpr int   TF_BOSS_FLAG            = 30;    // 僵王终战旗次
constexpr int   TF_TALLNUT_THORNS       = 1;    // 「高坚果反伤」每次被啃食对僵尸造成的反噬伤害（策划案 6.2）
constexpr int   TF_TORCH_MULTI_CHANCE   = 30;    // 「随机多倍火球」触发概率（%，策划案 6.2）
constexpr int   TF_TORCH_MULTI_BONUS    = 40;    // 触发时额外附加的伤害（= 火球基础伤害）
constexpr int   TF_FROZEN_VULN_PERCENT  = 20;    // 「冻结易伤」冰冻期间受到的额外伤害（%，策划案 6.2）
constexpr int   TF_GRAVEBUSTER_SUN      = 50;    // 「墓碑吞噬者返阳光」（策划案 6.2）
constexpr int   TF_GARLIC_POISON        = 60;    // 「大蒜毒伤」改道时的一次性伤害（策划案 6.3）
// ---- 第 6.1 章「逐株投射物伤害加成」：投射物基础伤害一律查表，这里叠加额外值 ----
constexpr int   TF_SHOT_BONUS_PEA       = 15;    // 豌豆射手 20 -> 35
constexpr int   TF_SHOT_BONUS_REPEATER  = 15;    // 双发
constexpr int   TF_SHOT_BONUS_THREEPEA  = 16;    // 三线 +80%
constexpr int   TF_SHOT_BONUS_SPLITPEA  = 10;    // 裂荚
constexpr int   TF_SHOT_BONUS_STARFRUIT = 10;    // 杨桃 +50%
constexpr int   TF_SHOT_BONUS_CACTUS    = 20;    // 仙人掌 +100%
// 【三十旗·平衡】忧郁菇 / 猫尾草 专项加强
//
// 这两个植物的强度主要来自机制（忧郁菇「孢子腐化」、猫尾草「收割」，见下方常量），
// 所以基础数值只给 3~4 倍，避免机制叠数值叠过头：
//   忧郁菇：原版每次孢子 20 伤害、200 帧一轮 4 次（≈40 DPS，范围 5 行）-> ≈120 DPS + 腐化
//   猫尾草：原版每发 20 伤害、每 50 帧一发（≈40 DPS 单体追踪）-> ≈160 DPS + 破绽标记 + 收割
// 想再调强度，只改这两个数字即可。
constexpr int   TF_GLOOM_PUFF_DAMAGE    = 60;   // 忧郁菇单次孢子 20 -> 60（≈120 DPS 范围伤害）
constexpr int   TF_SHOT_BONUS_CATTAIL   = 60;   // 猫尾草额外弹伤 20 -> 80（≈160 DPS 单体追踪）

constexpr int   TF_PIERCE_PEA           = 1;
// ---- 元素火球（策划案 6.2「随机多倍火球」扩展版）：火炬点燃豌豆时随机附加元素 ----
constexpr int   TF_ELEM_NONE            = 0;
constexpr int   TF_ELEM_ICE             = 1;    // 寒冰火球：命中减速
constexpr int   TF_ELEM_DEEPFREEZE      = 2;    // 极寒火球：命中强冻结
constexpr int   TF_ELEM_POISON          = 3;    // 毒伤火球：命中处生成尸毒潭
constexpr int   TF_ELEM_CHARM           = 4;    // 魅惑火球：命中直接魅惑
constexpr int   TF_ELEM_MARK            = 5;    // break-mark projectile (peashooter fuse)
constexpr int   TF_ELEM_BUTTER          = 5;    // 黄油火球：命中定身
constexpr int   TF_ELEM_GIANT           = 6;    // 巨大火球：伤害 3 倍     // 「连续命中穿透」豌豆类可多打穿 1 只僵尸
// ---- 7.7 表现层 ----
constexpr int   TF_DAMAGE_NUM_MAX       = 48;    // 跳字并发上限
constexpr int   TF_DAMAGE_NUM_LIFE      = 45;    // 跳字存活帧数
constexpr int   TF_STREAK_SHOW_LIFE     = 120;   // 连杀条显示帧数
constexpr int   TF_BIG_DAMAGE_THRESHOLD = 100;
constexpr int   TF_TALLNUT_SPIKE_DAMAGE = 20;    // vanilla spikeweed per-hit damage   // base value; x2 internal spike multiplier + plant bonus apply on top   //Tallnut crush-revenge: spike-flagged damage
constexpr unsigned int TF_DMGFLAG_POISON = (1u << 10);
constexpr unsigned int TF_DMGFLAG_ICEELEM = (1u << 11);
constexpr unsigned int TF_DMGFLAG_FIRE   = (1u << 13);

// 【三十旗·平衡】忧郁菇「孢子腐化」+ 猫尾草「收割」
// 用户需求：这两个植物不能只靠数值，要各有一条真机制，而且要能处理精英「再生」。
constexpr int   TF_CORRUPT_MAX_STACKS   = 4;    // 腐化最大层数
constexpr int   TF_CORRUPT_DURATION     = 600;  // 每层持续帧数（6 秒，可刷新）
constexpr int   TF_CORRUPT_SLOW_PERCENT = 12;   // 每层减速 %（4 层 = -48%）
constexpr int   TF_HARVEST_CORRUPT_PCT  = 200;  // 猫尾草对「腐化」目标的伤害 %（×2）
constexpr int   TF_HARVEST_ELITE_PCT    = 150;  // 猫尾草对精英僵尸的伤害 %（×1.5）
constexpr int   TF_BURST_MAX            = 12;    // 处决爆发环并发上限
constexpr int   TF_BURST_LIFE           = 30;    // 处决爆发环存活帧数
constexpr int   TF_FREEZE_MAX_FRAMES    = 12;    // 单次顿帧上限（帧，只冻结表现层）
constexpr int   TF_SUN_ROLL_LIFE        = 45;    // 阳光滚动指示存活帧数
constexpr int   TF_LAWN_ROWS            = 5;     // 日间布局可用行数（第 6 行 y=580 已越界）

// ---- Final boss (plan 8.1 / 8.2) ----
constexpr int   TF_BOSS_HEALTH                 = 300000;
constexpr int   TF_BOSS_PHASE2_PERCENT         = 70;
constexpr int   TF_BOSS_PHASE3_PERCENT         = 30;
constexpr int   TF_BOSS_SELF_DESTRUCT_PERCENT  = 10;
constexpr int   TF_BOSS_SELF_DESTRUCT_FRAMES   = 3600;
constexpr int   TF_BOSS_WARN_FRAMES            = 90;
constexpr int   TF_BOSS_HORDE_INTERVAL         = 1200;
constexpr int   TF_BOSS_SLAM_INTERVAL          = 900;
constexpr int   TF_BOSS_EMP_FRAMES             = 600;
constexpr int   TF_BOSS_ROAR_FRAMES            = 480;
constexpr int   TF_BOSS_ROAR_SPEED_PERCENT     = 50;
constexpr int   TF_BOSS_ROAR_INTERVAL          = 1500;
constexpr int   TF_BOSS_DOMAIN_INTERVAL        = 900;
constexpr int   TF_BOSS_TERRAIN_INTERVAL       = 1800;

// ----------------------------------------------------------------------------------------------------
// 数值成长参数（策划案 4.1 + 9.1）
// 调优旋钮只有一个：TF_HP_GROWTH_PER_FLAG。策划案原则是「所有旗次数值由公式生成，
// 改这一个数字整体缩放，切勿逐旗手改」。
// ----------------------------------------------------------------------------------------------------
constexpr float TF_HP_GROWTH_PER_FLAG   = 0.07f;    // 生命：每旗 +7%（乘算）
constexpr float TF_HP_GROWTH_CAP        = 6.80f;    // 生命成长上限（第 29 旗 ≈ 基准 6.8 倍）
constexpr float TF_SPEED_GROWTH_PER_FLAG = 0.015f;  // 移速：每旗 +1.5%
constexpr float TF_SPEED_GROWTH_CAP     = 0.45f;    // 移速成长上限 +45%
constexpr float TF_BALANCE_KNOB         = 1.25f;    // ★ 全局调优旋钮（策划案 9.2）

// ----------------------------------------------------------------------------------------------------
// 突变（每 5 旗随机抽 1 条，永久生效；改变规则而非数值，是每局差异的主要来源）
// ----------------------------------------------------------------------------------------------------
enum ThirtyFlagsMutation
{
    MUTATION_ZOMBIE_EXPLODE,        // 尸爆：僵尸死亡时原地爆炸，对周围植物造成伤害
    MUTATION_ZOMBIE_POISON,         // 尸毒：僵尸死亡留下毒潭，持续伤害该格植物
    MUTATION_LIFESTEAL,             // 吸血：僵尸攻击植物时回复自身生命
    MUTATION_ARMOR,                 // 尸甲：全体僵尸获得 15% 减伤
    MUTATION_SWIFT,                 // 疾行：全体移速 +25%，击退效果减半
    MUTATION_PROSPER,               // 增殖：僵尸死亡时 30% 概率原地留下一只小鬼
    MUTATION_DIG,                   // 掘地：僵尸死亡后在原地留下坑洞，该格 15 秒不可种植
    MUTATION_RESONANCE,             // 共鸣：场上每有一只僵尸，全体僵尸攻击力 +1%
    NUM_MUTATIONS
};

// ----------------------------------------------------------------------------------------------------
// 精英词条（第 16 旗起挂到部分僵尸身上）
// ----------------------------------------------------------------------------------------------------
enum ThirtyFlagsEliteKind
{
    ELITE_SWIFT,                    // 迅捷：移速 +60%
    ELITE_SHIELD,                   // 护盾：每 8 秒生成 300 点护盾
    ELITE_REGEN,                    // 再生：每秒回 2% 生命
    ELITE_IRONWALL,                 // 铁壁：受伤 -40%
    ELITE_SPLIT,                    // 分裂：死亡分裂 2 个半血小鬼
    ELITE_SPEAR,                    // 投矛：远程攻击前排植物
    ELITE_BERSERK                   // 狂暴：生命低于 30% 后攻速翻倍
};

// ----------------------------------------------------------------------------------------------------
// 肉鸽强化品级与条目
// ----------------------------------------------------------------------------------------------------
enum ThirtyFlagsRarity
{
    TF_RARITY_COMMON,
    TF_RARITY_RARE,
    TF_RARITY_EPIC,
    TF_RARITY_LEGENDARY
};

enum ThirtyFlagsUpgrade
{
    // —— 普通池 ——
    TF_UPG_HASTE,                   // 急速：全植物攻速 +12%
    TF_UPG_BARRAGE,                 // 弹幕：射手类额外发射 1 发弹丸
    TF_UPG_REINFORCE,               // 加固：全植物生命 +35%
    TF_UPG_ARMORPIERCE,             // 穿甲：无视 25% 护甲
    TF_UPG_MASS_PRODUCE,            // 量产：种植费用 -20%
    TF_UPG_ABUNDANCE,               // 丰饶：自然阳光掉落间隔 -25%

    // —— 稀有池 ——
    TF_UPG_CRIT,                    // 暴击：15% 概率造成 2 倍伤害
    TF_UPG_COOLDOWN,                // 冷却：全植物冷却 -20%
    TF_UPG_SIPHON,                  // 虹吸：击杀回 3 阳光
    TF_UPG_THORNS,                  // 荆棘：反弹 20% 所受伤害
    TF_UPG_HIGH_YIELD,              // 高产：向日葵产出 +60%
    TF_UPG_DRAIN,                   // 汲取：植物阵亡返还 50% 种植费

    // —— 史诗池：联动与转化（左脚踩右脚） ——
    TF_UPG_RESONANCE_HASTE,         // 共鸣·攻速：每持有 1 条「急速」，全植物攻速额外 +8%
    TF_UPG_RESONANCE_BARRAGE,       // 共鸣·弹幕：每持有 1 条「弹幕」，射手类伤害 +15%
    TF_UPG_RESONANCE_WEALTH,        // 共鸣·财富：每持有 1 条经济类强化，全植物攻击力 +10%
    TF_UPG_BLOOD_RAGE,              // 血怒转化：全植物当前生命的 30% 转化为攻击力
    TF_UPG_COMPOUND_SUN,            // 复利·阳光：每旗结束时，当前阳光储备 ×1.15
    TF_UPG_CHAIN,                   // 连锁：爆炸类命中后触发一次半额二次爆炸
    TF_UPG_OVERLOAD,                // 临界·超载：攻速加成每达到 +100%，射手追加 1 发弹丸
    TF_UPG_SYMBIOSIS_WATER,         // 共生·水路：睡莲上的植物攻速 +30%，且睡莲不再占卡槽
    TF_UPG_SCAVENGE,                // 拾荒：每累计击杀 500 只僵尸，全植物攻击力 +10%
    TF_UPG_DESPERATION,             // 绝境：每失去 1 点防线（一格被占），全植物攻速 +5%

    // —— 传说池：指数级与规则改写 ——
    TF_UPG_GOD_NUT,                 // 神格·坚果：所有坚果类生命 ×3，阵亡时对整行造成巨额伤害
    TF_UPG_GOD_ICE,                 // 神格·寒冰：所有减速效果附加真实伤害
    TF_UPG_NEURO_CRIT,              // 神经·暴击：暴击不再有概率，每次攻击必暴，倍率 +1
    TF_UPG_INFINITE,                // 无限：卡槽位 +2
    TF_UPG_NIRVANA,                 // 涅槃：每旗开始时复活上一旗阵亡的植物
    TF_UPG_DEVOUR,                  // 吞噬：植物阵亡时把其全部属性分给相邻植物
    TF_UPG_LEFT_FOOT_RIGHT_FOOT,    // 左脚踩右脚：每持有 1 条传说强化，所有强化效果 +30%
    TF_UPG_OVERLOADED,              // 超载：全植物攻速 +50%，生命 -30%
    TF_UPG_TIME_STOP,               // 时间停滞：每旗的前 10 秒，所有僵尸无法移动

    // —— 传说池·第二轮（用户需求） ——
    TF_UPG_OVERCLOCK,               // 超频核心：全植物攻速 +60%/层
    TF_UPG_WARHEAD,                 // 毁灭弹头：全植物伤害 +50%/层
    TF_UPG_GOLD_HARVEST,            // 黄金收割：击杀必掉阳光，每层面额 +25
    TF_UPG_BURST_SEED,              // 暴烈种子：植物种下立即开火

    TF_UPG_COUNT
};

// ----------------------------------------------------------------------------------------------------
// 静态定义结构
// ----------------------------------------------------------------------------------------------------
struct ThirtyFlagsUpgradeDef
{
    ThirtyFlagsUpgrade      mUpgrade;
    ThirtyFlagsRarity       mRarity;
    const SexyChar*         mName;
    const SexyChar*         mDescription;
};

struct ThirtyFlagsFlagDef
{
    int                     mFlag;                  // 旗次 1..30
    const char*             mTheme;                 // 主题名（整备界面显示）
    int                     mZombiePoints;          // 本旗僵尸点数基数
    int                     mWaterRows;             // 水路行位掩码（bit0=第 1 行 … bit5=第 6 行）
    int                     mWaterCols;             // 水路占右侧列数（0 = 全陆地）
    int                     mFogCols;               // 右侧迷雾列数（0 = 无雾）
    int                     mUnlockedRows;          // 本旗开放的行走位掩码
};

// ----------------------------------------------------------------------------------------------------
// 《三十旗》单局运行时状态
// ----------------------------------------------------------------------------------------------------
class ThirtyFlags
{
public:
    bool                        mActive;                    // 是否处于三十旗模式
    int                         mFlag;                      // 当前旗次 1..30

    // ---- 场地 ----
    int                         mUnlockedRows;
    int                         mWaterRows;
    int                         mWaterCols;
    int                         mFogCols;

    // ---- 尸潮进化 ----
    bool                        mMutations[NUM_MUTATIONS];
    int                         mMutationPicksLeft;

    // ---- 肉鸽强化 ----
    int                         mUpgradeStacks[TF_UPG_COUNT];
    int                         mUpgradeCount;
    int                         mLegendaryCount;

    // ---- 整备阶段 ----
    bool                        mIntermission;
    ThirtyFlagsUpgrade          mPendingUpgradeChoices[3];
    bool                        mUpgradeChosen;
    bool                        mUpgradeDialogShown;
    int                         mPrepTimeLeft;
    int                         mPrepTimeStart;
    int                         mSunAtFlagStart;

    // ---- 统计 ----
    int                         mTotalKills;
    int                         mKillStreak;
    int                         mKillStreakTimer;
    int                         mLostLanes;

public:
    ThirtyFlags();

    // ---- 生命周期 ----
    void                        StartRun();
    void                        ApplyFlag(int theFlag);
    void                        EndRun();

    // ---- 静态配置查询 ----
    static const ThirtyFlagsFlagDef& GetFlagDef(int theFlag);
    static const ThirtyFlagsUpgradeDef& GetUpgradeDef(ThirtyFlagsUpgrade theUpgrade);
    static void                 BuildZombieMask(int theFlag, bool* theAllowed);

    // ---- 场地 ----
    bool                        IsRowUnlocked(int theRow) const { return (mUnlockedRows & (1 << theRow)) != 0; }
    bool                        IsWaterRow(int theRow) const { return (mWaterRows & (1 << theRow)) != 0; }
    bool                        IsWaterCell(int theRow, int theCol) const;
    bool                        IsFogCell(int theCol) const { return mFogCols > 0 && theCol >= MAX_GRID_SIZE_X - mFogCols; }
    int                         GetWalkableRowCount() const;

    // ---- 尸潮进化 ----
    bool                        HasMutation(int theMutation) const;
    void                        RollMutation();
    float                       GetHealthScale() const;
    float                       GetSpeedScale() const;
    float                       GetCountScale() const;
    float                       GetZombieDamageScale(Board* theBoard) const;
    float                       GetZombieDamageReduction() const;

    // ---- 精英化 ----
    int                         GetEliteChancePercent() const;

    // ---- 肉鸽强化 ----
    int                         GetStacks(ThirtyFlagsUpgrade theUpgrade) const { return mUpgradeStacks[theUpgrade]; }
    int                         CountRarity(ThirtyFlagsRarity theRarity) const;
    int                         CountEconomyUpgrades() const;
    void                        RollUpgradeChoices();
    void                        GrantUpgrade(ThirtyFlagsUpgrade theUpgrade);
    int                         GetBonusSeedSlots() const;

    // ---- 综合战斗系数 ----
    float                       GetAttackSpeedBonus() const;
    float                       GetDamageBonus() const;
    float                       GetHealthBonus() const;
    float                       GetCostScale() const;
    int                         GetExtraProjectiles() const;
    float                       GetArmorPierce() const;
    float                       GetThornsReflect() const;
    float                       GetSunDropIntervalScale() const;
    float                       GetSunflowerYieldBonus() const;
    int                         GetKillSunReward() const;
    float                       GetDeathRefundRate() const;
    float                       GetLilypadSpeedBonus() const;
    int                         GetTimeStopSeconds() const;
    float                       GetLegendaryAmplifier() const;      // 「左脚踩右脚」总放大系数

    // ---- 表现层 / 统计 ----
    void                        OnZombieKilled(Board* theBoard, int theX, int theY, int theRow);
    void                        UpdateKillStreak();

    int                         GetKillStreak() const { return mKillStreak; }

    // ---- Final boss (flag 30) ----
    int                         mBossPhase;
    Zombie*                     mBossPtr;
    int                         mBossSkillTimer;
    int                         mBossWarnTimer;
    int                         mBossWarnSkill;
    int                         mBossWarnRow;
    int                         mBossSelfDestruct;
    int                         mBossEmpTimer;
    int                         mBossRoarTimer;
    int                         mBossDomainTimer;
};

extern ThirtyFlags gThirtyFlags;

// 植物侧伤害标记（复用 DamageFlags 未占用的 bit 8）：
// Zombie::TakeDamage 见到该位即视为「植物来源伤害」，据三十旗规则放大与判定暴击。
constexpr unsigned int TF_DAMAGE_FROM_PLANT = 1u << 8;

// ----------------------------------------------------------------------------------------------------
// 钩子函数（由 Board / Plant / Zombie 调用；内部自行判断是否处于三十旗模式）
// ----------------------------------------------------------------------------------------------------
bool    ThirtyFlagsMode();
void    ThirtyFlagsDrawBackdrop(Board* theBoard, Sexy::Graphics* g);   // 草皮滚动 + 局部水面 + 未开放行黑幕
int     ThirtyFlagsCurrentFlag(Board* theBoard);   // 当前旗次（单一数据源：Board::mChallenge->mSurvivalStage + 1）
void    ThirtyFlagsInitRun(Board* theBoard);
void    ThirtyFlagsSetupBoard(Board* theBoard);
void    ThirtyFlagsFlagChanged(Board* theBoard);
void    ThirtyFlagsIntermissionBegin(Board* theBoard);
void    ThirtyFlagsIntermissionFinish(Board* theBoard);
void    ThirtyFlagsAdvanceFlag(Board* theBoard);

// --------------------------------------------------------------------------------------------
// Run save -- built on the ORIGINAL LawnSaveGame / LawnLoadGame pipeline (see "RUN SAVE" in .cpp)
// --------------------------------------------------------------------------------------------
void    ThirtyFlagsOnSaveGame(Board* theBoard);   // called right after LawnSaveGame()
void    ThirtyFlagsOnLoadGame(Board* theBoard);   // called right after a successful Board::LoadGame()
void    ThirtyFlagsClearSave();                   // delete the board save + the run-state sidecar
int     ThirtyFlagsPickZombieWaves(Board* theBoard, int& theWaveCount);
float   ThirtyFlagsZombiePointsScale(Board* theBoard);
int     ThirtyFlagsWavePoints(int theWaveIndex);

void    ThirtyFlagsZombieInit(Zombie* theZombie);
int     ThirtyFlagsZombieTakeDamage(Zombie* theZombie, int theDamage, unsigned int theDamageFlags);
void    ThirtyFlagsZombieKilled(Zombie* theZombie);
void    ThirtyFlagsZombieUpdate(Zombie* theZombie);
void    TFLog(const char* theLine);   // flow log (thirtyflags_flow.log)
void    ThirtyFlagsShowCenterText(const SexyString& theText, const Sexy::Color& theColor, int theLife);   // 中央公告（换旗大字/精英警告）
void    ThirtyFlagsZombieAtePlant(Zombie* theZombie, Plant* thePlant);

// 第 6.1 章：逐株植物投射物伤害加成（返回"额外伤害"，0 表示无加成）
int     ThirtyFlagsPlantShotBonus(int theSeedType);

// 第 6.1 章：逐株植物投射物可穿透的僵尸数（0 表示命中即消失）
int     ThirtyFlagsPlantPierce(int theSeedType);

// -----------------------------------------------------------------------------------------
// 表现层（策划案 7.7）
// 数据全部放在 ThirtyFlags.cpp 的文件级静态数组里，**不进 Board** ——
// Board 的布局受 SyncBoard 存档约束，不能追加成员。
// -----------------------------------------------------------------------------------------
void    ThirtyFlagsAddDamageNumber(int theX, int theY, int theDamage, int theColorKind = 0);   // 伤害跳字（kind: 0普通 1冰 2毒 3火 4暴击金）
void    ThirtyFlagsUpdateVisuals();                                      // 每帧推进（Board::Update 调用）
void    ThirtyFlagsDrawVisuals(Sexy::Graphics* g);
void    ThirtyFlagsUpdateSod(Board* theBoard);

// 元素火球：随机卷一个元素（TF_ELEM_*）
int     ThirtyFlagsRollFireballElement(int theFrame);
bool    ThirtyFlagsHasFlagZombieAlive(Board* theBoard);
void    ThirtyFlagsAddMark(Zombie* theZombie, Board* theBoard);
void    ThirtyFlagsAddCorrupt(Zombie* theZombie, Board* theBoard);
bool    ThirtyFlagsIsPlantingIntoPumpkin(Board* theBoard, int theGridX, int theGridY, int theSeedType);
int     ThirtyFlagsGetCorrupt(Zombie* theZombie, Board* theBoard);
int     ThirtyFlagsHarvestPercent(Zombie* theZombie, Board* theBoard);
int     ThirtyFlagsGetMark(Zombie* theZombie, Board* theBoard);   // 战旗光环：场上是否有旗帜僵尸
// 在 (col,row) 处放置尸毒潭（毒伤火球用；包装 ThirtyFlags.cpp 内部毒表）
void    ThirtyFlagsAddPoisonAt(int theCol, int theRow, int theFrames);   // 动态草皮滚动 + 水格淡入（每帧推进）                       // 绘制（Board::Draw 调用）
bool    ThirtyFlagsShouldFreezeFrame();                                  // 顿帧查询（策划案 7.7）
bool    ThirtyFlagsZombieIsBerserk(Zombie* theZombie);

void    ThirtyFlagsPlantInit(Plant* thePlant);
float   ThirtyFlagsPlantLaunchDecrement(Plant* thePlant);
void    ThirtyFlagsPlantDied(Plant* thePlant);
int     ThirtyFlagsSunflowerExtraSun(Plant* thePlant);
int     ThirtyFlagsPlantExtraProjectiles(Plant* thePlant);

void    ThirtyFlagsBoardUpdate(Board* theBoard);

// ---- Final boss hooks ----
void    ThirtyFlagsBossUpdate(Board* theBoard);
bool    ThirtyFlagsBossWantsBoss(int theFlag);
bool    ThirtyFlagsBossIsEmpActive();
bool    ThirtyFlagsBossIsDomainActive();

#endif // __THIRTY_FLAGS_H__
