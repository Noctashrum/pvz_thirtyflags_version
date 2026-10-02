ThirtyFlags gThirtyFlags;

// 植物侧伤害标记（复用 DamageFlags 未占用的 bit 8）：
// Zombie::TakeDamage 见到该位即视为「植物来源伤害」，据三十旗规则放大。

static inline int TFClampI(int theValue, int theMin, int theMax)
{
    return theValue <= theMin ? theMin : (theValue >= theMax ? theMax : theValue);
}


// 当前旗次的唯一数据源。
// 注意：不能只依赖 gThirtyFlags.mFlag —— Board::InitSurvivalStage 会在 mSurvivalStage 自增之前
// 调用 InitZombieWaves，此时若按 mFlag 取旗次会拿到上一旗。mSurvivalStage 在整个流程里始终等于
// 「已完成的旗数」，因此「当前旗次 = mSurvivalStage + 1」在任何调用时机都成立。

// ----------------------------------------------------------------------------------------------------
// 僵尸身上可复用的「备用字段」
//
// Zombie 类没有空闲成员，而下列字段仅在 ZOMBIE_BOSS 分支被读写（非 Boss 僵尸恒为初始值），
// 因此三十旗模式把它们临时借用为精英词条位与专属计时：
//   mBossMode           -> 精英词条位掩码（1 << ELITE_xxx）
//   mBossBungeeCounter  -> 精英·护盾的生成计时
//   mBossStompCounter   -> 精英·投矛的投掷计时
//   mVelZ               -> 该僵尸上一帧施加的移速倍率
// 三十旗模式下所有精英逻辑都显式排除 ZOMBIE_BOSS，故不会与 Boss 自身机制冲突。
// ----------------------------------------------------------------------------------------------------


// 尸毒毒潭：每格独立的剩余帧数（0 = 无毒），不依赖坑洞


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
    { TF_UPG_GOD_PEA,              TF_RARITY_LEGENDARY, _S("神格·豌豆"),    _S("所有豌豆类变为加特林形态，攻速 x2（可叠加）") },
    { TF_UPG_GOD_NUT,              TF_RARITY_LEGENDARY, _S("神格·坚果"),    _S("所有坚果类生命 x3，阵亡时对整行造成巨额伤害") },
    { TF_UPG_GOD_ICE,              TF_RARITY_LEGENDARY, _S("神格·寒冰"),    _S("所有减速效果附加真实伤害") },
    { TF_UPG_NEURO_CRIT,           TF_RARITY_LEGENDARY, _S("神经·暴击"),    _S("暴击不再有概率，每次攻击必暴，倍率 +1") },
    { TF_UPG_INFINITE,             TF_RARITY_LEGENDARY, _S("无限"),         _S("卡槽位 +2（可超过 12）") },
    { TF_UPG_NIRVANA,              TF_RARITY_LEGENDARY, _S("涅槃"),         _S("每旗开始时复活上一旗阵亡的植物") },
    { TF_UPG_DEVOUR,               TF_RARITY_LEGENDARY, _S("吞噬"),         _S("植物阵亡时把其全部属性分给相邻植物") },
    { TF_UPG_LEFT_FOOT_RIGHT_FOOT, TF_RARITY_LEGENDARY, _S("左脚踩右脚"),   _S("每持有 1 条传说强化，所有强化效果 +30%（自身也计入）") },
    { TF_UPG_OVERLOADED,           TF_RARITY_LEGENDARY, _S("超载"),         _S("全植物攻速 +50%，生命 -30%") },
    { TF_UPG_TIME_STOP,            TF_RARITY_LEGENDARY, _S("时间停滞"),     _S("每旗的前 10 秒，所有僵尸无法移动") }
};

// ----------------------------------------------------------------------------------------------------
// 30 旗波次表（策划案第 2 / 4 章）
//
// 行走位掩码 bit0..bit5 对应 PvZ 的第 0..5 行。
// 策划案「1-4 旗只开放地上 3 行草皮」-> 常开行 = 第 2/3/4 行；
// 第 1 行于第 5 旗开放；第 6 行于第 10 旗开放并成为水路；
// 第 1 行于第 15 旗转为水路 —— 与「水陆平衡的 PvZ 泳池布局」一致，网格行高与坐标换算自洽。
// ----------------------------------------------------------------------------------------------------
#define TF_ROW2_4   ((1 << 2) | (1 << 3) | (1 << 4))
#define TF_ROW1_4   ((1 << 1) | TF_ROW2_4)
#define TF_ROW0_4   ((1 << 0) | TF_ROW1_4)
#define TF_ALLROWS  (TF_ROW0_4 | (1 << 5))
#define TF_ROW5     (1 << 5)
#define TF_ROW0     (1 << 0)

static const ThirtyFlagsFlagDef gThirtyFlagsFlagDefs[TF_TOTAL_FLAGS] = {
    // 旗次  主题            点数  水路行                  水列  雾列  开放行
    {  1,   "拮据求生",      1,    0,                      0,    0,    TF_ROW2_4 },
    {  2,   "拮据求生",      1,    0,                      0,    0,    TF_ROW2_4 },
    {  3,   "拮据求生",      2,    0,                      0,    0,    TF_ROW2_4 },
    {  4,   "拮据求生",      2,    0,                      0,    0,    TF_ROW2_4 },
    {  5,   "场地打开",      3,    0,                      0,    0,    TF_ROW1_4 },
    {  6,   "场地打开",      3,    0,                      0,    0,    TF_ROW1_4 },
    {  7,   "场地打开",      4,    0,                      0,    0,    TF_ROW1_4 },
    {  8,   "场地打开",      4,    0,                      0,    0,    TF_ROW1_4 },
    {  9,   "场地打开",      5,    0,                      0,    0,    TF_ROW1_4 },
    { 10,   "水路登场",      5,    TF_ROW5,                4,    0,    TF_ROW1_4 | TF_ROW5 },
    { 11,   "水路登场",      6,    TF_ROW5,                4,    0,    TF_ROW1_4 | TF_ROW5 },
    { 12,   "水路登场",      6,    TF_ROW5,                4,    0,    TF_ROW1_4 | TF_ROW5 },
    { 13,   "水路登场",      7,    TF_ROW5,                4,    0,    TF_ROW1_4 | TF_ROW5 },
    { 14,   "水路登场",      7,    TF_ROW5,                4,    0,    TF_ROW1_4 | TF_ROW5 },
    { 15,   "双向水路",      8,    TF_ROW5 | TF_ROW0,      4,    3,    TF_ALLROWS },
    { 16,   "双向水路",      8,    TF_ROW5 | TF_ROW0,      4,    3,    TF_ALLROWS },
    { 17,   "双向水路",      9,    TF_ROW5 | TF_ROW0,      4,    3,    TF_ALLROWS },
    { 18,   "双向水路",      9,    TF_ROW5 | TF_ROW0,      4,    3,    TF_ALLROWS },
    { 19,   "双向水路",      10,   TF_ROW5 | TF_ROW0,      4,    3,    TF_ALLROWS },
    { 20,   "双方超模",      11,   TF_ROW5 | TF_ROW0,      6,    3,    TF_ALLROWS },
    { 21,   "双方超模",      12,   TF_ROW5 | TF_ROW0,      6,    3,    TF_ALLROWS },
    { 22,   "双方超模",      13,   TF_ROW5 | TF_ROW0,      6,    3,    TF_ALLROWS },
    { 23,   "双方超模",      14,   TF_ROW5 | TF_ROW0,      6,    3,    TF_ALLROWS },
    { 24,   "双方超模",      15,   TF_ROW5 | TF_ROW0,      6,    3,    TF_ALLROWS },
    { 25,   "双方超模",      16,   TF_ROW5 | TF_ROW0,      8,    3,    TF_ALLROWS },
    { 26,   "双方超模",      18,   TF_ROW5 | TF_ROW0,      8,    3,    TF_ALLROWS },
    { 27,   "双方超模",      20,   TF_ROW5 | TF_ROW0,      8,    3,    TF_ALLROWS },
    { 28,   "双方超模",      23,   TF_ROW5 | TF_ROW0,      8,    3,    TF_ALLROWS },
    { 29,   "双方超模",      26,   TF_ROW5 | TF_ROW0,      8,    3,    TF_ALLROWS },
    { 30,   "僵王终战",      0,    TF_ROW5 | TF_ROW0,      8,    3,    TF_ALLROWS }
};

// ----------------------------------------------------------------------------------------------------
// 僵尸登场表（策划案 2.3「水路救活了哪批僵尸」+ 第 4 章尸潮进化节奏）
// ----------------------------------------------------------------------------------------------------
void ThirtyFlags::BuildZombieMask(int theFlag, bool* theAllowed)
{
    for (int i = 0; i < NUM_ZOMBIE_TYPES; i++)
    {
        theAllowed[i] = false;
    }

    // 第 1 旗永远只有普通僵尸与旗帜僵尸（保留 1-1 的开局观感）
    theAllowed[(int)ZOMBIE_NORMAL] = true;
    theAllowed[(int)ZOMBIE_FLAG] = true;
    if (theFlag <= 1)
        return;

    if (theFlag >= 2)  theAllowed[(int)ZOMBIE_TRAFFIC_CONE] = true;                 // 路障
    if (theFlag >= 4)  theAllowed[(int)ZOMBIE_PAIL] = true;                         // 铁桶
    if (theFlag >= 5)                                                                // 场地打开
    {
        theAllowed[(int)ZOMBIE_POLEVAULTER] = true;                                  // 撑杆
        theAllowed[(int)ZOMBIE_NEWSPAPER] = true;                                    // 读报
    }
    if (theFlag >= 7)                                                                // 铁门 / 鸭子救生圈
    {
        theAllowed[(int)ZOMBIE_DOOR] = true;
        theAllowed[(int)ZOMBIE_DUCKY_TUBE] = true;
    }
    if (theFlag >= 10) theAllowed[(int)ZOMBIE_SNORKEL] = true;                       // 水路登场：潜水
    if (theFlag >= 11) theAllowed[(int)ZOMBIE_FOOTBALL] = true;                      // 橄榄球
    if (theFlag >= 13) theAllowed[(int)ZOMBIE_DANCER] = true;                        // 舞王
    if (theFlag >= 15)                                                               // 双向水路
    {
        theAllowed[(int)ZOMBIE_DOLPHIN_RIDER] = true;                                // 海豚
        theAllowed[(int)ZOMBIE_BALLOON] = true;                                      // 气球
    }
    if (theFlag >= 16) theAllowed[(int)ZOMBIE_DIGGER] = true;                        // 矿工（精英化同步开始）
    if (theFlag >= 17)                                                               // 玩偶匣 / 梯子
    {
        theAllowed[(int)ZOMBIE_JACK_IN_THE_BOX] = true;
        theAllowed[(int)ZOMBIE_LADDER] = true;
    }
    if (theFlag >= 19) theAllowed[(int)ZOMBIE_CATAPULT] = true;                      // 投石车
    if (theFlag >= 20) theAllowed[(int)ZOMBIE_POGO] = true;                          // 蹦蹦
    if (theFlag >= 22) theAllowed[(int)ZOMBIE_GARGANTUAR] = true;                    // 巨人
    if (theFlag >= 25) theAllowed[(int)ZOMBIE_ZAMBONI] = true;                       // 冰车
    if (theFlag >= 26)                                                               // 植物僵尸：纯耐久检验
    {
        theAllowed[(int)ZOMBIE_PEA_HEAD] = true;
        theAllowed[(int)ZOMBIE_WALLNUT_HEAD] = true;
    }
    if (theFlag >= 28) theAllowed[(int)ZOMBIE_REDEYE_GARGANTUAR] = true;             // 红眼巨人
    if (theFlag >= 29)                                                               // 全植物僵尸
    {
        theAllowed[(int)ZOMBIE_JALAPENO_HEAD] = true;
        theAllowed[(int)ZOMBIE_GATLING_HEAD] = true;
        theAllowed[(int)ZOMBIE_SQUASH_HEAD] = true;
        theAllowed[(int)ZOMBIE_TALLNUT_HEAD] = true;
    }
}

// ----------------------------------------------------------------------------------------------------
// 构造 / 生命周期
// ----------------------------------------------------------------------------------------------------
ThirtyFlags::ThirtyFlags()
{
    mActive = false;
    mFlag = 0;
    mUnlockedRows = TF_ROW2_4;
    mWaterRows = 0;
    mWaterCols = 0;
    mFogCols = 0;
    memset(mMutations, 0, sizeof(mMutations));
    mMutationPicksLeft = 0;
    memset(mUpgradeStacks, 0, sizeof(mUpgradeStacks));
    mUpgradeCount = 0;
    mLegendaryCount = 0;
    mIntermission = false;
    mPendingUpgradeChoices[0] = TF_UPG_HASTE;
    mPendingUpgradeChoices[1] = TF_UPG_HASTE;
    mPendingUpgradeChoices[2] = TF_UPG_HASTE;
    mUpgradeChosen = false;
    mUpgradeDialogShown = false;
    mPrepTimeLeft = 0;
    mPrepTimeStart = 1;
    mSunAtFlagStart = 0;
    mTotalKills = 0;
    mKillStreak = 0;
    mKillStreakTimer = 0;
    mLostLanes = 0;
}

void ThirtyFlags::StartRun()
{
    *this = ThirtyFlags();
    mActive = true;
    mMutationPicksLeft = TF_MUTATION_PICKS;
    ApplyFlag(1);

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
    float aScale = powf(1.0f + TF_HP_GROWTH_PER_FLAG, (float)(mFlag - 1));
    return min(aScale, TF_HP_GROWTH_CAP);
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
    // 数量成长已内建在 30 旗波次表的 mZombiePoints 曲线里（第 9 旗 5 -> 第 29 旗 26）。
    // 这里只作为第 10 旗之后区段的补足系数。
    return 1.0f + 0.03f * (float)max(0, mFlag - 9);
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

// ----------------------------------------------------------------------------------------------------
// 精英化
// ----------------------------------------------------------------------------------------------------
int ThirtyFlags::GetEliteChancePercent() const
{
    if (mFlag < TF_ELITE_START_FLAG)
        return 0;

    // 第 16 旗 15% 起步，逐旗 +3%，上限 60%
    return min(15 + (mFlag - TF_ELITE_START_FLAG) * 3, 60);
}

// ----------------------------------------------------------------------------------------------------
// 肉鸽强化
// ----------------------------------------------------------------------------------------------------
float ThirtyFlags::GetLegendaryAmplifier() const
{
    int aStacks = GetStacks(TF_UPG_LEFT_FOOT_RIGHT_FOOT);
    if (aStacks <= 0)
        return 1.0f;

    // 每持有 1 条传说强化，所有强化效果 +30%（自身也计入）
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
    // 品级只决定出现时机，不决定能否叠加。
    // 前 10 旗以普通/稀有为主；第 15 旗起史诗概率提升；第 20 旗起传说出现。
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

        // 同名强化可重复抽取，因此三选一允许出现重复项
        int aCandidates[TF_UPG_COUNT];
        int aCount = 0;
        for (int i = 0; i < TF_UPG_COUNT; i++)
        {
            if (gThirtyFlagsUpgradeDefs[i].mRarity == aRarity)
                aCandidates[aCount++] = i;
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

    aBonus *= GetLegendaryAmplifier();

    // 神格·豌豆：每次再 ×2（可叠加）
    float aMultiplier = powf(2.0f, (float)GetStacks(TF_UPG_GOD_PEA));

    return (1.0f + aBonus) * aMultiplier;
}

float ThirtyFlags::GetDamageBonus() const
{
    if (!mActive)
        return 1.0f;

    float aBonus = 0.0f;
    aBonus += 0.15f * (float)(GetStacks(TF_UPG_RESONANCE_BARRAGE) * GetStacks(TF_UPG_BARRAGE));
    aBonus += 0.10f * (float)(GetStacks(TF_UPG_RESONANCE_WEALTH) * CountEconomyUpgrades());
    aBonus += 0.10f * (float)(GetStacks(TF_UPG_SCAVENGE) * (mTotalKills / 500));

    // 血怒转化：全植物当前生命的 30% 转化为攻击力（以生命加成为代理量）
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

    // 神格·坚果：生命 ×3（可叠加）
    float aMultiplier = powf(3.0f, (float)GetStacks(TF_UPG_GOD_NUT));

    return max(0.1f, (1.0f + aBonus) * aMultiplier);
}

float ThirtyFlags::GetCostScale() const
{
    if (!mActive)
        return 1.0f;

    float aReduction = 0.20f * (float)GetStacks(TF_UPG_MASS_PRODUCE);
    aReduction *= GetLegendaryAmplifier();

    return max(0.0f, 1.0f - aReduction);   // 下限免费
}

int ThirtyFlags::GetExtraProjectiles() const
{
    if (!mActive)
        return 0;

    int aExtra = GetStacks(TF_UPG_BARRAGE);

    // 临界·超载：攻速加成每达到 +100%，射手追加 1 发弹丸
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

    float aPierce = 0.25f * (float)GetStacks(TF_UPG_ARMORPIERCE);
    aPierce *= GetLegendaryAmplifier();

    return aPierce;    // 可超过 1.0（溢出部分转为真实伤害，这里以整体增伤近似）
}

float ThirtyFlags::GetThornsReflect() const
{
    if (!mActive)
        return 0.0f;

    float aReflect = 0.20f * (float)GetStacks(TF_UPG_THORNS);
    aReflect *= GetLegendaryAmplifier();

    return aReflect;
}

float ThirtyFlags::GetSunDropIntervalScale() const
{
    if (!mActive)
        return 1.0f;

    float aReduction = 0.25f * (float)GetStacks(TF_UPG_ABUNDANCE);
    aReduction *= GetLegendaryAmplifier();

    return max(0.05f, 1.0f - aReduction);   // 间隔趋近连续掉落
}

float ThirtyFlags::GetSunflowerYieldBonus() const
{
    if (!mActive)
        return 1.0f;

    float aBonus = 0.60f * (float)GetStacks(TF_UPG_HIGH_YIELD);
    aBonus *= GetLegendaryAmplifier();

    return 1.0f + aBonus;
}

int ThirtyFlags::GetKillSunReward() const
{
    if (!mActive)
        return 0;

    float aReward = 3.0f * (float)GetStacks(TF_UPG_SIPHON);

    // 连杀加成：10 秒内每连杀 1 只，掉落概率 +1.5%（上限 +15%）
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

    float aRate = 0.50f * (float)GetStacks(TF_UPG_DRAIN);
    aRate *= GetLegendaryAmplifier();

    return aRate;   // 可超过 1.0（净赚）
}

float ThirtyFlags::GetLilypadSpeedBonus() const
{
    return (mActive && GetStacks(TF_UPG_SYMBIOSIS_WATER) > 0) ? 0.30f : 0.0f;
}

int ThirtyFlags::GetTimeStopSeconds() const
{
    if (!mActive)
        return 0;

    return 10 * GetStacks(TF_UPG_TIME_STOP);    // 每旗的前 N 秒，所有僵尸无法移动
}

