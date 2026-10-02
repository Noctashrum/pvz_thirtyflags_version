# Board 核心类模块文档
> 所属: src/Lawn | 分析范围: Board.h / Board.cpp（Board.cpp 共 9924 行，Board.h 共 748 行）

## 1. 模块职责

`Board` 是《植物大战僵尸》的**主战场（草坪）类**，继承自 `Sexy::Widget` 并实现 `Sexy::ButtonListener`。它负责：

- **战场网格**：维护 9×6（`MAX_GRID_SIZE_X × MAX_GRID_SIZE_Y`）草坪网格的地形类型、格子外观、雾浓度。
- **实体管理**：持有并管理僵尸 `mZombies`、植物 `mPlants`、投影物 `mProjectiles`、钱币/阳光 `mCoins`、割草机 `mLawnMowers`、格子物品 `mGridItems` 六类实体的增删与遍历。
- **游戏循环**：作为主 Update/Draw 入口，驱动所有游戏对象的逐帧更新、分层绘制、进度条、阳光/僵尸波次生成。
- **交互**：处理鼠标（种植、铲除、工具、点选）、键盘（作弊码、暂停）、光标与提示。
- **关卡流程**：关卡初始化（背景、卡槽、波次表、墓碑、割草机）、波次推进、胜负判定、关卡结束淡出、生存模式阶段切换。
- **关卡变体判定**：白天/夜晚/泳池/浓雾/屋顶/屋顶Boss 等背景特性，供种植限制、出怪限制、僵尸行选择等复用。

该文件同时含若干**全局自由函数**（碰撞检测、僵尸波次选择器初始化、渲染排序）和少量全局变量/静态常量。

---

## 2. Board 类成员变量（分组表格）

> 成员变量均带 `+0x…` 反编译偏移注释，下文按用途分组。类型中的 `DataArray<T>` 为可分配对象数组容器（见 `../TodLib/DataArray.h`）。

### 2.1 应用/框架指针

| 名称 | 类型 | 说明 |
|---|---|---|
| mApp | `LawnApp*` | 全局应用对象，几乎所有资源/场景/存档都通过它访问 |

### 2.2 实体容器（DataArray）

| 名称 | 类型 | 说明 |
|---|---|---|
| mZombies | `DataArray<Zombie>` | 僵尸数组（容量 1024），含普通/Boss 全部僵尸 |
| mPlants | `DataArray<Plant>` | 植物数组（容量 1024） |
| mProjectiles | `DataArray<Projectile>` | 投影物数组（豌豆、篮球、刺等，容量 1024） |
| mCoins | `DataArray<Coin>` | 阳光/钱币/掉落物数组（容量 1024） |
| mLawnMowers | `DataArray<LawnMower>` | 割草机数组（容量 32） |
| mGridItems | `DataArray<GridItem>` | 格子物品数组（墓碑、弹坑、梯子、钉耙、花盆罐、禅境工具等，容量 128） |

### 2.3 UI 控件 / 子对象

| 名称 | 类型 | 说明 |
|---|---|---|
| mCursorObject | `CursorObject*` | 光标（种植/工具手持状态） |
| mCursorPreview | `CursorPreview*` | 种植预览 |
| mAdvice | `MessageWidget*` | 游戏提示字幕 |
| mSeedBank | `SeedBank*` | 卡槽（种子包） |
| mMenuButton | `GameButton*` | 菜单按钮 |
| mStoreButton | `GameButton*` | 商店按钮（禅境花园/最终之树/最后一次模式） |
| mToolTip | `ToolTipWidget*` | 悬浮提示 |
| mDebugFont | `Sexy::Font*` | 调试字体 |
| mCutScene | `CutScene*` | 过场动画（关卡介绍、僵尸获胜等） |
| mChallenge | `Challenge*` | 挑战/小游戏玩法逻辑（大量玩法委托给它） |

### 2.4 网格 / 地形

| 名称 | 类型 | 说明 |
|---|---|---|
| mGridSquareType | `GridSquareType[9][6]` | 每格地形：GRASS/DIRT/POOL/HIGH_GROUND/NONE |
| mGridCelLook | `int[9][6]` | 每格草的随机外观索引（用于草地/雾格外观变化） |
| mGridCelOffset | `int[9][6][2]` | 每格草地贴图的 x/y 偏移 |
| mGridCelFog | `int[9][7]` | 每格雾浓度（0-255，第 7 列用于雾边缘） |
| mPlantRow | `PlantRowType[6]` | 每行种植类型：DIRT/NORMAL/POOL/HIGH_GROUND |

### 2.5 墓碑

| 名称 | 类型 | 说明 |
|---|---|---|
| mEnableGraveStones | `bool` | 是否启用墓碑（夜晚关卡） |
| mSpecialGraveStoneX | `int` | 特殊墓碑列（含宝藏/植物，-1 表示无） |
| mSpecialGraveStoneY | `int` | 特殊墓碑行 |

### 2.6 雾

| 名称 | 类型 | 说明 |
|---|---|---|
| mFogOffset | `float` | 浓雾水平偏移（三叶草吹雾/回退动画） |
| mFogBlownCountDown | `int` | 雾被吹散后的回退倒计时 |

### 2.7 割草机 / 冰道 / 行选择

| 名称 | 类型 | 说明 |
|---|---|---|
| mWaveRowGotLawnMowered | `int[6]` | 每行上一次被割草机扫过的波数（用于出怪行权重保护） |
| mBonusLawnMowersRemaining | `int` | 剩余奖励割草机数 |
| mIceMinX | `int[6]` | 每行冰道最左像素 x（默认 `BOARD_ICE_START=800`） |
| mIceTimer | `int[6]` | 每行冰道剩余持续时间 |
| mIceParticleID | `ParticleSystemID[6]` | 每行冰道粒子系统 ID |
| mRowPickingArray | `TodSmoothArray[6]` | 平滑加权行选择数组（出怪行选择） |

### 2.8 波次表 / 出怪

| 名称 | 类型 | 说明 |
|---|---|---|
| mZombiesInWave | `ZombieType[100][50]` | 每波僵尸类型表（`MAX_ZOMBIE_WAVES×MAX_ZOMBIES_IN_WAVE`） |
| mZombieAllowed | `bool[100]` | 每种僵尸类型本关是否允许出现（`NUM_ZOMBIE_TYPES` 内） |

### 2.9 阳光

| 名称 | 类型 | 说明 |
|---|---|---|
| mSunCountDown | `int` | 阳光自然掉落倒计时 |
| mNumSunsFallen | `int` | 已自然掉落的阳光数（决定掉落间隔递增） |
| mSunMoney | `int` | 当前阳光数（上限 9990） |
| mOutOfMoneyCounter | `int` | 阳光不足提示计时 |
| mMaxSunPlants | `int` | 场上向日葵/阳光菇数量峰值（统计用） |

### 2.10 屏幕震动

| 名称 | 类型 | 说明 |
|---|---|---|
| mShakeCounter | `int` | 屏幕震动剩余帧数 |
| mShakeAmountX | `int` | 震动 x 幅度 |
| mShakeAmountY | `int` | 震动 y 幅度 |

### 2.11 背景 / 关卡

| 名称 | 类型 | 说明 |
|---|---|---|
| mBackground | `BackgroundType` | 当前背景类型（白天/夜晚/泳池/雾/屋顶/Boss/花园…） |
| mLevel | `int` | 当前关卡号（冒险模式 1~50） |
| mSodPosition | `int` | 开场铺草皮动画进度 |

### 2.12 鼠标

| 名称 | 类型 | 说明 |
|---|---|---|
| mPrevMouseX / mPrevMouseY | `int` | 上一帧鼠标坐标 |
| mIgnoreMouseUp | `bool` | 忽略下一次 MouseUp |

### 2.13 计时器 / 计数器

| 名称 | 类型 | 说明 |
|---|---|---|
| mMainCounter | `int` | 主循环帧计数（雾动画、闪烁等） |
| mEffectCounter | `int` | 特效计数 |
| mDrawCount | `int` | 绘制帧计数（FPS 统计） |
| mRiseFromGraveCounter | `int` | 最后一波后从墓碑爬出倒计时 |
| mFinalWaveSoundCounter | `int` | 最终波音效计时 |
| mTimeStopCounter | `int` | 时停计时（樱桃炸弹等） |
| mIceTrapCounter | `int` | 冰陷阱计时（冻结水面） |
| mFwooshCountDown | `int` | 火爆辣椒火焰动画计时 |
| mCobCannonCursorDelayCounter | `int` | 玉米加农炮瞄准延迟（误点检测） |
| mCobCannonMouseX / mCobCannonMouseY | `int` | 玉米加农炮瞄准点 |
| mTutorialTimer | `int` | 教程计时 |
| mCoinBankFadeCount | `int` | 金币银行显示淡出计时 |
| mBoardFadeOutCounter | `int` | 关卡结束淡出倒计时（-1 未开始） |
| mNextSurvivalStageCounter | `int` | 生存/无尽下一阶段倒计时 |
| mScoreNextMowerCounter | `int` | 结算割草机奖励计时 |
| mFlagRaiseCounter | `int` | 旗帜波旗帜升起计时 |
| mProgressMeterWidth | `int` | 进度条当前宽度（0~150） |

### 2.14 波次推进状态

| 名称 | 类型 | 说明 |
|---|---|---|
| mNumWaves | `int` | 本关总波数 |
| mCurrentWave | `int` | 当前波数（即将刷出的波） |
| mTotalSpawnedWaves | `int` | 已刷出的总波数 |
| mLastBungeeWave | `int` | 上一次蹦极僵尸波 |
| mZombieHealthToNextWave | `int` | 触发下一波的僵尸剩余血量阈值 |
| mZombieHealthWaveStart | `int` | 本波开始时的僵尸总血量 |
| mZombieCountDown | `int` | 下一波倒计时 |
| mZombieCountDownStart | `int` | 下一波倒计时初值（进度条比例） |
| mHugeWaveCountDown | `int` | 大波（旗帜波）预告倒计时 |

### 2.15 教程 / 帮助字幕

| 名称 | 类型 | 说明 |
|---|---|---|
| mTutorialState | `TutorialState` | 教程状态机（第 1/2 关、更多阳光、铲子等） |
| mTutorialParticleID | `ParticleSystemID` | 教程箭头粒子 ID |
| mHelpDisplayed | `bool[NUM_ADVICE_TYPES]` | 各类提示是否已显示过 |
| mHelpIndex | `AdviceType` | 当前提示类型 |

### 2.16 状态标志

| 名称 | 类型 | 说明 |
|---|---|---|
| mPaused | `bool` | 暂停标志 |
| mFinalBossKilled | `bool` | 最终 Boss 是否已击杀 |
| mShowShovel | `bool` | 是否显示铲子按钮 |
| mLevelComplete | `bool` | 关卡是否完成 |
| mLevelAwardSpawned | `bool` | 关卡奖励是否已掉落 |
| mDroppedFirstCoin | `bool` | 是否已掉落第一枚金币 |
| mKilledYeti | `bool` | 是否击杀过雪人僵尸 |
| mPrevBoardResult | `BoardResult` | 上一局结果 |

### 2.17 随机种子

| 名称 | 类型 | 说明 |
|---|---|---|
| mBoardRandSeed | `int` | 棋盘随机种子（生存模式每次重开刷新） |

### 2.18 特效 ID

| 名称 | 类型 | 说明 |
|---|---|---|
| mPoolSparklyParticleID | `ParticleSystemID` | 泳池闪光粒子 ID |
| mFwooshID | `ReanimationID[6][12]` | 每行火爆辣椒火焰动画 ID |

### 2.19 彩蛋 / 作弊模式

| 名称 | 类型 | 说明 |
|---|---|---|
| mMustacheMode | `bool` | 胡子模式 |
| mSuperMowerMode | `bool` | 超级割草机模式 |
| mFutureMode | `bool` | 未来模式 |
| mPinataMode | `bool` | 彩罐模式 |
| mDanceMode | `bool` | 舞王模式 |
| mDaisyMode | `bool` | 雏菊模式 |
| mSukhbirMode | `bool` | Sukhbir 模式 |

### 2.20 性能统计 / 数据统计

| 名称 | 类型 | 说明 |
|---|---|---|
| mTriggeredLawnMowers | `int` | 已触发割草机数 |
| mPlayTimeActiveLevel | `int` | 本关活跃游戏时长 |
| mPlayTimeInactiveLevel | `int` | 本关非活跃时长 |
| mStartDrawTime / mIntervalDrawTime | `DWORD` | FPS 统计时间戳 |
| mIntervalDrawCountStart | `int` | FPS 统计起始帧 |
| mMinFPS | `float` | 最低 FPS |
| mPreloadTime | `int` | 预加载时间 |
| mGameID | `int` | 游戏实例 ID（time32） |
| mGravesCleared | `int` | 已清除墓碑数 |
| mPlantsEaten | `int` | 被吃掉的植物数 |
| mPlantsShoveled | `int` | 被铲除的植物数 |
| mCoinsCollected | `int` | 收集金币数 |
| mDiamondsCollected | `int` | 收集钻石数 |
| mPottedPlantsCollected | `int` | 收集盆栽数 |
| mChocolateCollected | `int` | 收集巧克力数 |

---

## 3. Board 成员函数索引（签名 | 行号 | 功能 | 可复用性）

> 行号除标注 `h.` 者（在 Board.h 内联定义）外均为 Board.cpp 行号。共 **252** 个成员函数（含构造/析构；其中 7 个在头文件内联）。可复用性：★★★ 独立通用；★★ 可借鉴算法；★ 业务耦合。

### 3.1 构造 / 析构 / 生命周期

| 签名 | 行号 | 功能 | 可复用性 |
|---|---|---|---|
| `Board(LawnApp* theApp)` | 48 | 构造函数：初始化六类实体数组、网格地形、UI 控件、随机种子 | ★ |
| `~Board()` | 211 | 析构：释放子对象与实体数组 | ★ |
| `void DisposeBoard()` | 249 | 离开花园/智慧树、停雨声、释放特效 | ★ |
| `void InitLevel()` | 1368 | 关卡初始化：背景、波次、初始阳光、卡槽、墓碑、行数据 | ★ |
| `void StartLevel()` | 1693 | 开始关卡：重置统计、启动音乐 | ★ |
| `void InitLawnMowers()` | 1651 | 按行创建割草机（排除无车关卡） | ★ |
| `void InitSurvivalStage()` | 1281 | 生存模式下一阶段初始化（刷新卡牌、重开出怪） | ★ |
| `void LoadBackgroundImages()` | 817 | 按背景加载延迟资源 | ★ |
| `void PickBackground()` | 874 | 按游戏模式选背景，并据此设定行类型/格子地形/墓碑分布 | ★ |

### 3.2 实体遍历（Iterate*）

| 签名 | 行号 | 功能 | 可复用性 |
|---|---|---|---|
| `bool IterateZombies(Zombie*&)` | 9260 | 迭代存活僵尸（跳过 mDead） | ★★ |
| `bool IteratePlants(Plant*&)` | 9273 | 迭代存活植物 | ★★ |
| `bool IterateProjectiles(Projectile*&)` | 9286 | 迭代存活投影物 | ★★ |
| `bool IterateCoins(Coin*&)` | 9299 | 迭代存活钱币 | ★★ |
| `bool IterateLawnMowers(LawnMower*&)` | 9312 | 迭代存活割草机 | ★★ |
| `bool IterateGridItems(GridItem*&)` | 9325 | 迭代存活格子物品 | ★★ |
| `bool IterateParticles(TodParticleSystem*&)` | 9338 | 迭代存活粒子系统 | ★★ |
| `bool IterateReanimations(Reanimation*&)` | 9350 | 迭代存活 Reanimation 动画 | ★★ |
| `void ProcessDeleteQueue()` | 8719 | 删除所有 mDead 实体（延迟删除队列） | ★★ |

### 3.3 实体增删（Add* / Remove* / New*）

| 签名 | 行号 | 功能 | 可复用性 |
|---|---|---|---|
| `Plant* NewPlant(int,int,SeedType,SeedType)` | 2086 | 分配并初始化植物（不含特效/统计） | ★ |
| `Plant* AddPlant(int,int,SeedType,SeedType)` | 2140 | 种植物：NewPlant + 种植特效 + 挑战回调 + 统计 | ★ |
| `void DoPlantingEffects(int,int,Plant*)` | 2095 | 播放种植音效/粒子（水陆区分） | ★ |
| `Projectile* AddProjectile(int,int,int,int,ProjectileType)` | 2377 | 分配并初始化投影物 | ★ |
| `Coin* AddCoin(int,int,CoinType,CoinMotion)` | 2018 | 分配并初始化钱币/阳光 | ★ |
| `Zombie* AddZombie(ZombieType,int)` | 2704 | 选择随机行后 AddZombieInRow | ★ |
| `Zombie* AddZombieInRow(ZombieType,int,int)` | 2683 | 指定行分配并初始化僵尸（雪橇小队自动生成 4 只） | ★ |
| `void RemoveAllZombies()` | 2710 | 杀死场上所有僵尸（无掉落） | ★ |
| `void RemoveZombiesForRepick()` | 2723 | 删除右侧被策反的僵尸（换卡阶段） | ★ |
| `void RemoveCutsceneZombies()` | 2736 | 删除过场动画僵尸 | ★ |
| `GridItem* AddAGraveStone(int,int)` | 473 | 分配并初始化墓碑 | ★ |
| `GridItem* AddACrater(int,int)` | 463 | 分配并初始化弹坑 | ★ |
| `GridItem* AddALadder(int,int)` | 452 | 分配并初始化梯子 | ★ |
| `void AddGraveStones(int,int,MTRand&)` | 485 | 在指定列随机生成若干墓碑（含修正防死循环） | ★ |
| `void PlaceRake()` | 1597 | 放置钉耙（随机行，扣除购买数） | ★ |
| `Reanimation* CreateRakeReanim(float,float,int)` | 1587 | 创建钉耙 Reanimation | ★ |

### 3.4 实体查询（Get* / Count* / Find*）

| 签名 | 行号 | 功能 | 可复用性 |
|---|---|---|---|
| `GridItem* GetGridItemAt(GridItemType,int,int)` | 367 | 按类型+坐标取格子物品 | ★★ |
| `GridItem* GetRake()` | 381 | 取钉耙 | ★ |
| `GridItem* GetCraterAt(int,int)` | 394 | 取弹坑 | ★★ |
| `GridItem* GetGraveStoneAt(int,int)` | 399 | 取墓碑 | ★★ |
| `GridItem* GetLadderAt(int,int)` | 404 | 取梯子 | ★★ |
| `GridItem* GetScaryPotAt(int,int)` | 409 | 取破罐 | ★★ |
| `GridItem* GetSquirrelAt(int,int)` | 414 | 取松鼠 | ★★ |
| `GridItem* GetZenToolAt(int,int)` | 419 | 取禅境工具 | ★★ |
| `Plant* GetPumpkinAt(int,int)` | 2156 | 取南瓜头 | ★ |
| `Plant* GetFlowerPotAt(int,int)` | 2170 | 取花盆 | ★ |
| `void GetPlantsOnLawn(int,int,PlantsOnLawn*)` | 2184 | 分类统计格内植物（底座/南瓜/飞行/普通） | ★★ |
| `Plant* GetTopPlantAt(int,int,PlantPriority)` | 2254 | 按优先级取格内“顶层”植物 | ★★ |
| `Plant* FindUmbrellaPlant(int,int)` | 9598 | 找 1 格范围内的叶子保护伞 | ★★ |
| `LawnMower* FindLawnMowerInRow(int)` | 9860 | 取某行割草机 | ★★ |
| `LawnMower* GetBottomLawnMower()` | 1718 | 取最下方未触发割草机 | ★ |
| `Zombie* GetBossZombie()` | 9584 | 取 Boss 僵尸 | ★★ |
| `Zombie* GetWinningZombie()` | 9873 | 取获胜动画僵尸 | ★ |
| `Zombie* ZombieHitTest(int,int)` | 3103 | 鼠标命中的僵尸 | ★ |
| `Plant* ToolHitTest(int,int)` | 4080 | 工具命中的植物 | ★ |
| `Plant* ToolHitTestHelper(HitResult*)` | 4073 | 工具命中辅助（排除墓碑吞噬者） | ★ |
| `Plant* SpecialPlantHitTest(int,int)` | 4152 | 特殊植物（南瓜/飞行）命中测试 | ★ |
| `int CountZombiesOnScreen()` | 277 | 统计在屏敌人僵尸数 | ★ |
| `int CountZombieByType(ZombieType)` | 9886 | 统计某类僵尸数 | ★★ |
| `int CountUntriggerLawnMowers()` | 292 | 统计未触发割草机 | ★ |
| `int CountSunFlowers()` | 2295 | 统计产阳光植物 | ★ |
| `int CountPlantByType(SeedType)` | 2310 | 统计某类植物数 | ★★ |
| `int CountEmptyPotsOrLilies(SeedType)` | 2325 | 统计空的花盆/睡莲数 | ★ |
| `int CountCoinByType(CoinType)` | 9412 | 统计某类钱币数 | ★★ |
| `int CountSunBeingCollected()` | 8669 | 统计正在被收集的阳光值 | ★ |
| `int CountCoinsBeingCollected()` | 8684 | 统计正在被收集的金币值 | ★ |
| `int GetGraveStoneCount()` | 9428 | 统计墓碑数 | ★★ |
| `int GetGraveStonesCount()` | 4794 | 统计墓碑数（与上一个同名功能重复） | ★★ |
| `bool AreEnemyZombiesOnScreen()` | 263 | 场上是否有敌方（未被策反）僵尸 | ★ |
| `bool BungeeIsTargetingCell(int,int)` | 9570 | 是否有蹦极僵尸正瞄准该格 | ★★ |

### 3.5 阳光 / 金币经济

| 签名 | 行号 | 功能 | 可复用性 |
|---|---|---|---|
| `void AddSunMoney(int)` | 8659 | 增加阳光（上限 9990） | ★ |
| `bool TakeSunMoney(int)` | 8699 | 扣阳光（不足则蜂鸣+计时） | ★ |
| `bool CanTakeSunMoney(int)` | 8713 | 是否付得起（含收集中的阳光） | ★ |
| `void ShowCoinBank(int=1000)` | 4769 | 显示金币银行 | ★ |
| `int GetCurrentPlantCost(SeedType,SeedType)` | 9784 | 计算植物当前价格（无尽模式涨价） | ★ |
| `bool PlantUsesAcceleratedPricing(SeedType)` | 9778 | 是否加速涨价（无尽+升级卡） | ★ |
| `void DropLootPiece(int,int,int)` | 9445 | 掉落战利品（金币/钻石/巧克力/盆栽/礼物） | ★ |
| `bool CanDropLoot()` | 9564 | 是否可掉落战利品 | ★ |

### 3.6 种植判定 / 种植流程

| 签名 | 行号 | 功能 | 可复用性 |
|---|---|---|---|
| `PlantingReason CanPlantAt(int,int,SeedType)` | 2759 | 判断某格能否种植某植物（返回原因枚举） | ★★ |
| `bool PlantingRequirementsMet(SeedType)` | 9689 | 紫卡植物是否满足升级条件 | ★ |
| `bool IsValidCobCannonSpot(int,int)` | 2354 | 玉米加农炮合法落点（需横向两格） | ★ |
| `bool IsValidCobCannonSpotHelper(int,int)` | 2340 | 玉米加农炮落点辅助判断 | ★ |
| `bool HasValidCobCannonSpot()` | 2363 | 场上是否有玉米加农炮落点 | ★ |
| `void OffsetYForPlanting(int&,SeedType)` | 8959 | 按植物类型修正种植 y（飞行/地刺/温室） | ★★ |
| `int PlantingPixelToGridX(int,int,SeedType)` | 8976 | 种植像素→网格列（含 y 修正） | ★★ |
| `int PlantingPixelToGridY(int,int,SeedType)` | 8983 | 种植像素→网格行（含咖啡豆特殊吸附） | ★★ |
| `void MouseDownWithPlant(int,int,int)` | 3695 | 种植流程：判定→扣阳光→销毁被替换植物→AddPlant→教程 | ★ |
| `void MouseDownWithTool(int,int,int,CursorType)` | 4107 | 工具使用流程（铲子铲植物等） | ★ |
| `void MouseDownCobcannonFire(int,int,int)` | 3673 | 玉米加农炮发射（含误点检测） | ★ |
| `bool IsPlantInCursor()` | 2030 | 光标是否手持植物 | ★ |
| `SeedType GetSeedTypeInCursor()` | 2041 | 取光标中的种子类型 | ★ |
| `void RefreshSeedPacketFromCursor()` | 2060 | 放回光标中的种子包/金币 | ★ |
| `void ClearCursor()` | 4621 | 清空光标手持状态 | ★ |

### 3.7 坐标系统

| 签名 | 行号 | 功能 | 可复用性 |
|---|---|---|---|
| `int PixelToGridX(int,int)` | 9022 | 像素 x→网格列（`(x-40)/80`） | ★★★ |
| `int PixelToGridY(int,int)` | 9048 | 像素 y→网格行（屋顶带坡度修正） | ★★★ |
| `int GridToPixelX(int,int)` | 9090 | 网格列→像素 x（`x*80+40`） | ★★★ |
| `int GridToPixelY(int,int)` | 9125 | 网格行→像素 y（屋顶坡度/高地修正） | ★★★ |
| `int PixelToGridXKeepOnBoard(int,int)` | 9041 | 像素→列并钳制到板上（≥0） | ★★★ |
| `int PixelToGridYKeepOnBoard(int,int)` | 9083 | 像素→行并钳制到板上（≥0） | ★★★ |
| `float GetPosYBasedOnRow(float,int)` | 9108 | 按行与 x 计算 y（屋顶斜坡） | ★★★ |
| `int MakeRenderOrder(RenderLayer,int,int)` | 446 | 计算渲染排序值（行×层偏移+层+偏移） | ★★★ |

### 3.8 波次 / 出怪

| 签名 | 行号 | 功能 | 可复用性 |
|---|---|---|---|
| `void InitZombieWaves()` | 1216 | 初始化波次表与倒计时 | ★ |
| `void InitZombieWavesForLevel(int)` | 1169 | 按关卡填充 mZombieAllowed | ★ |
| `void PickZombieWaves()` | 579 | 生成完整出怪表（点数分配、旗帜波、固定怪、随机补充） | ★★ |
| `void PutZombieInWave(ZombieType,int,ZombiePicker*)` | 553 | 向某波出怪表添加一只僵尸并扣点数 | ★★ |
| `void PutInMissingZombies(int,ZombiePicker*)` | 567 | 最后一波补齐本关可能出现但未出现的僵尸 | ★ |
| `bool CanZombieSpawnOnLevel(ZombieType,int)` | 2385 | 某僵尸是否可在某关出现（静态逻辑） | ★★ |
| `ZombieType GetIntroducedZombieType()` | 2403 | 本关新引入的僵尸类型 | ★ |
| `ZombieType PickZombieType(int,int,ZombiePicker*)` | 2451 | 按剩余点数加权随机选一种僵尸 | ★★ |
| `ZombieType PickGraveRisingZombieType(int)` | 2422 | 从墓碑爬出的僵尸加权随机（普通/路障/铁桶） | ★★ |
| `int PickRowForNewZombie(ZombieType)` | 2610 | 加权随机选出怪行（钉耙/传送门/丢车保护） | ★★ |
| `bool RowCanHaveZombies(int)` | 5963 | 某行是否可有僵尸 | ★ |
| `bool RowCanHaveZombieType(int,ZombieType)` | 2550 | 某行是否可刷某类僵尸（水路/高地/巨人行等） | ★★ |
| `bool IsZombieTypePoolOnly(ZombieType)` | 2544 | 是否仅水路僵尸（潜水/海豚） | ★★ |
| `bool IsZombieTypeSpawnedOnly(ZombieType)` | 9918 | 是否仅派生僵尸（伴舞/雪橇/小鬼） | ★★ |
| `bool CanAddBobSled()` | 2670 | 是否可刷雪橇僵尸（需冰道） | ★ |
| `void SpawnZombieWave()` | 5032 | 刷出当前波全部僵尸并推进波数 | ★ |
| `void SpawnZombiesFromGraves()` | 4971 | 从墓碑/泳池/天空刷出僵尸 | ★ |
| `void SpawnZombiesFromPool()` | 4833 | 从泳池刷出僵尸 | ★ |
| `void SpawnZombiesFromSky()` | 4926 | 从天空（蹦极）刷出僵尸 | ★ |
| `void SetupBungeeDrop(BungeeDropGrid*)` | 4895 | 初始化蹦极落点网格 | ★ |
| `void BungeeDropZombie(BungeeDropGrid*,ZombieType)` | 4913 | 蹦极空投一只僵尸 | ★ |
| `void UpdateZombieSpawning()` | 5378 | 逐帧推进出怪倒计时与大波预告 | ★ |
| `void NextWaveComing()` | 5356 | 大波来临音效/预告 | ★ |
| `int TotalZombiesHealthInWave(int)` | 5016 | 计算某波僵尸总血量（含护盾/头盔/飞行） | ★★ |
| `int NumberZombiesInWave(int)` | 9902 | 某波僵尸数量 | ★★ |
| `int GetNumWavesPerFlag()` | 524 | 每旗帜波包含的小波数 | ★ |
| `int GetNumWavesPerSurvivalStage()` | 9750 | 每生存阶段波数（普通 10/困难无尽 20） | ★ |
| `bool IsFlagWave(int)` | 530 | 是否旗帜波 | ★★ |
| `bool IsZombieWaveDistributionOk()` | 1183 | 校验出怪表是否覆盖全部应有僵尸 | ★ |
| `int GetSurvivalFlagsCompleted()` | 5143 | 已完成的旗帜数 | ★ |
| `void UpdateSunSpawning()` | 5318 | 自然阳光掉落倒计时与生成 | ★ |

### 3.9 游戏循环 / 更新

| 签名 | 行号 | 功能 | 可复用性 |
|---|---|---|---|
| `void Update()` | 5847 | 主更新：控件/光标/教程/震动/特效/实体/挑战/结束序列 | ★ |
| `void UpdateGame()` | 5784 | 游戏逻辑更新：实体+雾+阳光+出怪+冰+进度条 | ★ |
| `void UpdateGameObjects()` | 5091 | 更新全部实体对象（植物/僵尸/投影物/币/割草机/卡槽） | ★ |
| `void UpdateLayers()` | 5947 | 将对话框置于最前并标记脏 | ★ |
| `void UpdateCursor()` | 2991 | 更新光标形状（手/手指/隐藏） | ★ |
| `void UpdateMousePosition()` | 3192 | 更新光标/提示/植物高亮 | ★ |
| `void UpdateToolTip()` | 3292 | 更新悬浮提示 | ★ |
| `void UpdateIce()` | 5525 | 更新冰道计时与粒子 | ★ |
| `void UpdateFog()` | 7491 | 更新雾浓度（灯笼草/火把清雾） | ★ |
| `void UpdateGridItems()` | 9663 | 更新格子物品（墓碑成长、弹坑消退） | ★ |
| `void UpdateProgressMeter()` | 5564 | 更新进度条宽度（血量/倒计时双比例） | ★★ |
| `void UpdateLevelEndSequence()` | 1736 | 关卡结束序列（割草机奖励、淡出） | ★ |
| `void UpdateTutorial()` | 5634 | 教程状态推进 | ★ |
| `void SetTutorialState(TutorialState)` | 5683 | 设置教程状态并执行对应动作 | ★ |
| `void UpdateFwoosh()` | 9640 | 更新火爆辣椒火焰动画 | ★ |
| `void DoFwoosh(int)` | 9612 | 生成一行火爆辣椒火焰 | ★ |
| `void ClearFogAroundPlant(Plant*,int)` | 7438 | 清除植物周围雾 | ★★ |
| `void ShakeBoard(int,int)` | 9853 | 触发屏幕震动 | ★ |
| `void ResetFPSStats()` | 344 | 重置 FPS 统计 | ★ |
| `void FreezeEffectsForCutscene(bool)` | 1256 | 冻结/恢复过场特效 | ★ |

### 3.10 鼠标 / 键盘输入

| 签名 | 行号 | 功能 | 可复用性 |
|---|---|---|---|
| `void MouseMove(int,int)` | 3089 | 鼠标移动 | ★ |
| `void MouseDrag(int,int)` | 3096 | 鼠标拖动 | ★ |
| `void MouseDown(int,int,int)` | 4488 | 主鼠标按下分发（工具/种植/按钮/币/罐） | ★ |
| `void MouseUp(int,int,int)` | 4703 | 鼠标抬起（菜单/商店按钮） | ★ |
| `bool MouseHitTest(int,int,HitResult*)` | 4236 | 鼠标命中测试（按钮/卡槽/币/植物/罐/老虎机） | ★ |
| `bool MouseHitTestPlant(int,int,HitResult*)` | 4178 | 植物命中测试 | ★ |
| `bool CanInteractWithBoardButtons()` | 4686 | 是否可交互（暂停/对话/疯狂戴夫） | ★ |
| `void PickUpTool(GameObjectType)` | 4385 | 拿起工具（铲子/水壶/肥料等） | ★ |
| `void HighlightPlantsForMouse(int,int)` | 3155 | 高亮鼠标作用植物 | ★ |
| `bool IsPlantInGoldWateringCanRange(int,int,Plant*)` | 3131 | 金水壶范围判断 | ★ |
| `void KeyDown(KeyCode)` | 7853 | 键盘按下（作弊码检测/暂停/ESC） | ★ |
| `void KeyChar(SexyChar)` | 7898 | 调试/作弊键处理 | ★ |
| `void DoTypingCheck(KeyCode)` | 7772 | 彩蛋序列检测（konami/mustache 等） | ★ |
| `void KeyUp(KeyCode)` | h.520 | 键盘抬起（空实现） | ★ |

### 3.11 绘制

| 签名 | 行号 | 功能 | 可复用性 |
|---|---|---|---|
| `void Draw(Graphics*)` | 7658 | 主绘制：FPS 统计 + DrawGameObjects | ★ |
| `void DrawGameObjects(Graphics*)` | 6231 | 生成渲染列表、按 z 排序、逐项绘制 | ★★ |
| `void AddBossRenderItem(RenderItem*,int&,Zombie*)` | 6095 | 添加 Boss 分体渲染项 | ★ |
| `void DrawBackdrop(Graphics*)` | 6007 | 绘制背景（含铺草皮动画） | ★ |
| `void DrawLevel(Graphics*)` | 6809 | 绘制关卡名 | ★ |
| `void DrawShovel(Graphics*)` | 7034 | 绘制铲子/禅境工具按钮 | ★ |
| `void DrawZenButtons(Graphics*)` | 6900 | 绘制禅境花园工具按钮 | ★ |
| `void DrawZenWheelBarrowButton(Graphics*,int)` | 6864 | 绘制手推车按钮 | ★ |
| `void DrawProgressMeter(Graphics*)` | 6686 | 绘制进度条/旗帜/文字 | ★ |
| `void DrawUIBottom(Graphics*)` | 7363 | 绘制底部 UI（卡槽/铲子/菜单） | ★ |
| `void DrawUITop(Graphics*)` | 7592 | 绘制顶部 UI（进度/关卡/光标/提示） | ★ |
| `void DrawTopRightUI(Graphics*)` | 7326 | 绘制右上菜单/商店按钮 | ★ |
| `void DrawUICoinBank(Graphics*)` | 7410 | 绘制金币银行 | ★ |
| `void DrawFog(Graphics*)` | 7535 | 绘制浓雾（周期流动） | ★ |
| `void DrawIce(Graphics*,int)` | 5977 | 绘制一行冰道 | ★ |
| `void DrawFadeOut(Graphics*)` | 7308 | 绘制关卡结束淡出 | ★ |
| `void DrawHouseDoorTop(Graphics*)` | 6794 | 绘制房门掩码（上） | ★ |
| `void DrawHouseDoorBottom(Graphics*)` | 6781 | 绘制房门内饰（下） | ★ |
| `void DrawDebugText(Graphics*)` | 7063 | 绘制调试文本 | ★ |
| `void DrawDebugObjectRects(Graphics*)` | 7243 | 绘制碰撞矩形（调试） | ★★ |
| `int GetIceZPos(int)` | 5971 | 冰道渲染层级 | ★★ |

### 3.12 关卡特性判断（Stage*/Is*）

| 签名 | 行号 | 功能 | 可复用性 |
|---|---|---|---|
| `bool StageIsNight()` | 8863 | 是否夜晚（含雾/Boss/蘑菇园/水族馆） | ★★ |
| `bool StageHasPool()` | 8896 | 是否有泳池 | ★★ |
| `bool StageHas6Rows()` | 8901 | 是否有 6 行（泳池/雾） | ★★ |
| `bool StageHasFog()` | 8924 | 是否有雾 | ★★ |
| `bool StageHasRoof()` | 8890 | 是否屋顶 | ★★ |
| `bool StageHasGraveStones()` | 8874 | 是否有墓碑 | ★★ |
| `bool StageHasZombieWalkInFromRight()` | 8907 | 僵尸是否从右侧走入 | ★★ |
| `int LeftFogColumn()` | 8930 | 雾起始列 | ★★ |
| `bool HasConveyorBeltSeedBank()` | 8784 | 是否传送带卡槽 | ★ |
| `bool HasProgressMeter()` | 6645 | 是否有进度条 | ★ |
| `bool ProgressMeterHasFlags()` | 6668 | 进度条是否标旗帜 | ★ |
| `bool HasLevelAwardDropped()` | 5312 | 关卡奖励是否已掉落 | ★ |
| `bool IsPoolSquare(int,int)` | 2075 | 某格是否泳池 | ★★ |
| `bool IsIceAt(int,int)` | 2749 | 某格是否结冰 | ★★ |
| `bool CanAddGraveStoneAt(int,int)` | 425 | 某格能否生成墓碑 | ★ |
| `unsigned SeedNotRecommendedForLevel(SeedType)` | 9377 | 返回某植物在本关的“不推荐”位标志 | ★★ |
| `bool CanUseGameObject(GameObjectType)` | 9795 | 禅境/智慧树工具是否可用 | ★ |
| `bool CanDropLoot()` | 9564 | 是否可掉落战利品 | ★ |

### 3.13 胜负 / 结束 / 存档

| 签名 | 行号 | 功能 | 可复用性 |
|---|---|---|---|
| `void ZombiesWon(Zombie*)` | 5186 | 僵尸获胜（游戏结束/失败界面） | ★ |
| `void FadeOutLevel()` | 1867 | 关卡结束淡出 | ★ |
| `void CompleteEndLevelSequenceForSaving()` | 1834 | 存档前结算割草机奖励与金币 | ★ |
| `void UpdateLevelEndSequence()` | 1736 | 关卡结束序列更新 | ★ |
| `bool IsFinalSurvivalStage()` | 5276 | 是否生存模式最终阶段 | ★ |
| `bool IsFinalScaryPotterStage()` | 5262 | 是否破罐者最终阶段 | ★ |
| `bool IsLastStandFinalStage()` | 5294 | 是否最后一次最终阶段 | ★ |
| `bool IsSurvivalStageWithRepick()` | 5300 | 生存模式是否还有换卡阶段 | ★ |
| `bool IsLastStandStageWithRepick()` | 5306 | 最后一次模式是否还有换卡阶段 | ★ |
| `bool IsScaryPotterDaveTalking()` | 7586 | 破罐者疯狂戴夫是否在说话 | ★ |
| `void SurvivalSaveScore()` | 5156 | 保存生存模式最高旗帜数 | ★ |
| `void PuzzleSaveStreak()` | 5171 | 保存无尽模式最高连击 | ★ |
| `bool TryToSaveGame()` | 307 | 尝试存档 | ★ |
| `bool NeedSaveGame()` | 328 | 是否需要存档 | ★ |
| `void SaveGame(const std::string&)` | 339 | 存档（委托 LawnSaveGame） | ★ |
| `bool LoadGame(const std::string&)` | 354 | 读档 | ★ |

### 3.14 提示字幕

| 签名 | 行号 | 功能 | 可复用性 |
|---|---|---|---|
| `void DisplayAdvice(const SexyString&,MessageStyle,AdviceType)` | 1976 | 显示提示（去重） | ★ |
| `void DisplayAdviceAgain(const SexyString&,MessageStyle,AdviceType)` | 1991 | 强制再次显示提示 | ★ |
| `void ClearAdvice(AdviceType)` | 2008 | 清除提示 | ★ |
| `void ClearAdviceImmediately()` | 2001 | 立即清除提示 | ★ |
| `void TutorialArrowShow(int,int)` | 4092 | 显示教程箭头 | ★ |
| `void TutorialArrowRemove()` | 4100 | 移除教程箭头 | ★ |

### 3.15 工具 / 布局 / 杂项

| 签名 | 行号 | 功能 | 可复用性 |
|---|---|---|---|
| `Sexy::Rect GetShovelButtonRect()` | 1311 | 铲子按钮矩形 | ★ |
| `void GetZenButtonRect(GameObjectType,Sexy::Rect&)` | 1322 | 禅境工具按钮矩形 | ★ |
| `int GetSeedPacketPositionX(int)` | 8941 | 卡槽第 i 张卡的 x 坐标 | ★★ |
| `int GetSeedBankExtraWidth()` | 8953 | 卡槽额外宽度 | ★ |
| `int GetNumSeedsInBank()` | 8800 | 本关卡槽数量 | ★ |
| `bool ChooseSeedsOnCurrentLevel()` | 1675 | 本关是否玩家选卡 | ★ |
| `void Pause(bool)` | 4775 | 暂停/恢复 | ★ |
| `void StopAllZombieSounds()` | 5133 | 停止全部僵尸音效 | ★ |
| `void KillAllPlantsInRadius(int,int,int)` | 9363 | 圆形范围杀死植物 | ★★ |
| `void KillAllZombiesInRadius(int,int,int,int,int,bool,int)` | 9706 | 范围杀僵尸/烧僵尸/清梯子 | ★★ |
| `void RemoveParticleByType(ParticleEffect)` | 9765 | 按类型移除粒子 | ★ |
| `ZombieID ZombieGetID(Zombie*)` | 9170 | 僵尸指针→ID | ★★ |
| `Zombie* ZombieGet(ZombieID)` | 9175 | 僵尸 ID→指针 | ★★ |
| `Zombie* ZombieTryToGet(ZombieID)` | 9181 | 僵尸 ID→指针（可空） | ★★ |
| `int GetLevelRandSeed()` | 802 | 关卡随机种子 | ★ |
| `void PickSpecialGraveStone()` | 4811 | 随机选定特殊墓碑 | ★ |
| `void SetMustacheMode(bool)` | 7691 | 开关胡子模式 | ★ |
| `void SetFutureMode(bool)` | 7705 | 开关未来模式 | ★ |
| `void SetPinataMode(bool)` | 7719 | 开关彩罐模式 | ★ |
| `void SetDanceMode(bool)` | 7727 | 开关舞王模式 | ★ |
| `void SetSuperMowerMode(bool)` | 7744 | 开关超级割草机模式 | ★ |
| `void SetDaisyMode(bool)` | 7757 | 开关雏菊模式 | ★ |
| `void SetSukhbirMode(bool)` | 7764 | 开关 Sukhbir 模式 | ★ |
| `bool SyncState(DataSync&)` | h.496 | 存档同步（内联占位，未实现） | ★ |
| `bool MakeEasyZombieType()` | h.533 | 内联占位（未实现） | ★ |
| `void MouseDownNormal(int,int,int)` | h.555 | 内联占位（未实现） | ★ |
| `void ButtonMouseEnter(int)` | h.526 | 按钮进入（空） | ★ |
| `void ButtonMouseLeave(int)` | h.527 | 按钮离开（空） | ★ |
| `void ButtonPress(int)` | h.528 | 按钮按下（空） | ★ |

---

## 4. 全局函数与变量

### 4.1 全局变量

| 名称 | 行号 | 类型 | 说明 |
|---|---|---|---|
| `gShownMoreSunTutorial` | 33 | `bool` | 全局标志：是否已显示“更多向日葵”教程（`extern` 声明于 Board.h:742） |

### 4.2 静态常量（文件内，行 35~45）

| 名称 | 值 | 说明 |
|---|---|---|
| `FLAG_RAISE_TIME` | 100 | 旗帜升起时长 |
| `ZOMBIE_COUNTDOWN_FIRST_WAVE` | 1800 | 首波倒计时 |
| `ZOMBIE_COUNTDOWN` | 2497 | 常规波间隔 |
| `ZOMBIE_COUNTDOWN_RANGE` | 600 | 波间隔随机范围 |
| `ZOMBIE_COUNTDOWN_BEFORE_FLAG` | 4497 | 旗帜波前间隔 |
| `ZOMBIE_COUNTDOWN_BEFORE_REPICK` | 5496 | 换卡阶段前间隔 |
| `ZOMBIE_COUNTDOWN_MIN` | 400 | 波间隔下限 |
| `FOG_BLOW_RETURN_TIME` | 2000 | 雾吹散回退时间 |
| `SUN_COUNTDOWN` | 425 | 阳光掉落基础间隔 |
| `SUN_COUNTDOWN_RANGE` | 275 | 阳光掉落随机范围 |
| `SUN_COUNTDOWN_MAX` | 950 | 阳光掉落间隔上限 |

### 4.3 自由函数（非成员）

| 签名 | 行号 | 功能 | 可复用性 |
|---|---|---|---|
| `void BoardInitForPlayer()` | 243 | 玩家级初始化：重置 `gShownMoreSunTutorial` | ★ |
| `void ZombiePickerInitForWave(ZombiePicker*)` | 540 | 清空单波僵尸选择器 | ★★ |
| `void ZombiePickerInit(ZombiePicker*)` | 546 | 清空整个僵尸选择器（含全波统计） | ★★ |
| `bool RenderItemSortFunc(const RenderItem&,const RenderItem&)` | 6084 | 渲染项按 zPos（同 z 按指针）排序比较 | ★★★ |
| `static inline void AddGameObjectRenderItem(RenderItem*,int&,RenderObjectType,GameObject*)` | 6153 | 向渲染列表追加通用游戏对象渲染项 | ★★ |
| `static inline void AddGameObjectRenderItemCursorPreview(...)` | 6163 | 追加光标预览渲染项 | ★★ |
| `static inline void AddGameObjectRenderItemPlant(...)` | 6175 | 追加植物渲染项 | ★★ |
| `static inline void AddGameObjectRenderItemZombie(...)` | 6187 | 追加僵尸渲染项 | ★★ |
| `static inline void AddGameObjectRenderItemProjectile(...)` | 6198 | 追加投影物渲染项 | ★★ |
| `static inline void AddGameObjectRenderItemCoin(...)` | 6209 | 追加钱币渲染项 | ★★ |
| `static inline void AddUIRenderItem(RenderItem*,int&,RenderObjectType,int)` | 6220 | 追加 UI 渲染项 | ★★ |
| `static void TodCrash()` | 7892 | 调试强制断言崩溃 | ★ |
| `int GetRectOverlap(const Rect&,const Rect&)` | 9187 | 两矩形水平重叠宽度 | ★★★ |
| `bool GetCircleRectOverlap(int,int,int,const Rect&)` | 9213 | 圆与矩形碰撞检测 | ★★★ |

---

## 5. 核心流程说明

### 5.1 游戏主循环

入口为 `Board::Update()`（cpp:5847），每帧执行顺序：

1. `Widget::Update()` + `MarkDirty()`：框架基类更新、标记重绘。
2. `mCutScene->Update()`：过场动画（关卡介绍/僵尸获胜）。
3. `UpdateMousePosition()`：光标、悬浮提示、植物高亮。
4. 禅境花园 / 疯狂戴夫对话更新。
5. 若 `mPaused`：仅更新 Challenge 与光标隐藏后提前返回。
6. 更新菜单/商店按钮、特效系统、字幕、教程、震动、金币银行。
7. `UpdateLayers()`：对话框置顶。
8. 若 `mTimeStopCounter > 0`（时停）提前返回（冻结游戏世界，但 UI 仍响应）。
9. `UpdateGridItems()` → `UpdateFwoosh()` → **`UpdateGame()`** → `UpdateFog()` → `mChallenge->Update()` → `UpdateLevelEndSequence()`。

`Board::UpdateGame()`（cpp:5784）：
1. `UpdateGameObjects()`：逐个调用植物/僵尸/投影物/钱币/割草机/卡槽的 `Update()`（这是所有实体逻辑的核心分发点）。
2. 雾偏移动画更新。
3. 非 `SCENE_PLAYING` 且非推销界面则返回。
4. `mMainCounter++` → `UpdateSunSpawning()` → `UpdateZombieSpawning()` → `UpdateIce()`。
5. `UpdateProgressMeter()`。

> 注：源码中并无名为 `UpdateFrame` 的函数；游戏循环即 `Update → UpdateGame → UpdateGameObjects`。实体删除采用**延迟删除**：实体 `Update()` 内将 `mDead=true`，再由 `ProcessDeleteQueue()`（cpp:8719，通常在 LawnApp 层调用）统一 `DataArrayFree`。

### 5.2 实体增删查接口

- **植物**：`AddPlant(x,y,seed,imitate)`（cpp:2140）→ `NewPlant`（分配 + `PlantInitialize`）→ `DoPlantingEffects`（音效粒子）→ `mChallenge->PlantAdded` 回调 + 统计。删除靠 `Plant::Die()` 置 `mDead`，无显式 RemovePlant（同原版设计）。
- **僵尸**：`AddZombie(type,fromWave)`（cpp:2704）→ `PickRowForNewZombie` 选行 → `AddZombieInRow`（分配 + `ZombieInitialize`，雪橇小队自动补 4 只）。删除：`RemoveAllZombies`/`RemoveZombiesForRepick`/`RemoveCutsceneZombies` 均为遍历置 `DieNoLoot`。
- **投影物**：`AddProjectile(x,y,renderOrder,row,type)`（cpp:2377）。
- **钱币/阳光**：`AddCoin(x,y,type,motion)`（cpp:2018）。
- **格子物品**：`AddAGraveStone/AddACrater/AddALadder`（均为 `DataArrayAlloc` + 字段赋值 + `MakeRenderOrder`）。
- **查询**：各类 `Get*At` 均通过 `GetGridItemAt` 或直接 `Iterate*` 遍历比对坐标/类型；`GetPlantsOnLawn` 将同格植物按“底座(花盆/睡莲)/南瓜/飞行/普通”四类归档，`GetTopPlantAt` 再按 `PlantPriority` 优先级取其一。
- **ID 映射**：僵尸用 `ZombieGetID / ZombieGet / ZombieTryToGet`（DataArray 内 ID），用于植物/投影物跨帧持有目标引用。

### 5.3 网格坐标系统

- 常量：`LAWN_XMIN = 40`、`LAWN_YMIN = 80`；格宽 **80** px；行高：普通/泳池/雾 **85** px（泳池），普通陆地 **100** px（无泳池场景）；屋顶 `GRID` 行高 85 + 坡度偏移。
- **像素→网格**：
  - `PixelToGridX = (x - 40) / 80`（`x < 40` 返回 -1）。
  - `PixelToGridY`：先由 x 算列；屋顶场景当列 <5 时按 `(4-列)*20` 修正 y（斜坡），随后 `(y-80)/85`（屋顶/泳池）或 `(y-80)/100`（普通）。
- **网格→像素**：
  - `GridToPixelX = x*80 + 40`。
  - `GridToPixelY`：屋顶 `y*85 + 坡度偏移 + 80 - 10`，泳池 `y*85+80`，普通 `y*100+80`；高地格再 `-HIGH_GROUND_HEIGHT(30)`。
- **种植专用**：`PlantingPixelToGridX/Y` 先经 `OffsetYForPlanting`（飞行植物/墓碑吞噬者 +15、地刺 -15、温室 -25）修正 y，咖啡豆还会吸附到睡眠植物所在行。
- **坡面行 y 插值**：`GetPosYBasedOnRow(x,row)` 供僵尸/物体沿屋顶斜坡移动（x<440 时线性抬升 0.25）。
- **渲染层级**：`MakeRenderOrder(layer,row,offset) = row*RENDER_LAYER_ROW_OFFSET + layer + offset`，是贯穿全工程的分层排序公式。

### 5.4 种植 / 铲除 / 判定

- **判定**：`CanPlantAt(x,y,seed)`（cpp:2759）返回 `PlantingReason`，逐层检查：越界 → 挑战玩法 `mChallenge->CanPlantAt` → 墓碑吞噬者/咖啡豆特判 → 墓碑/弹坑/破罐/冰道阻挡 → 地形（土/无）→ 水生/飞行/地刺/睡莲/花盆/屋顶花盆/南瓜/土豆雷/香蒲/紫卡升级/玉米加农炮等。是种植规则最集中的函数。
- **种植**：`MouseDownWithPlant`（cpp:3695）：右击放回卡牌 → 网格换算 → `CanPlantAt` 不通过则播提示字幕 → 通过则扣阳光 → 升级/包扎/加农炮时先销毁被替换植物 → 依据光标来源（金币/卡槽/手套/手推车）`AddPlant` → 教程状态推进 → 清光标。
- **铲除**：`MouseDownWithTool`（cpp:4107）中 `CURSOR_TYPE_SHOVEL` 分支：`ToolHitTest` 命中植物 → `mPlantsShoveled++` → `Plant::Die()`；铲香蒲会补种睡莲。

### 5.5 波次推进（SpawnZombiesFromWave 相关）

> 源码中实际函数名为 `SpawnZombieWave()`（cpp:5032）；出怪表生成在 `PickZombieWaves()`。

1. **确定总波数** `PickZombieWaves`（cpp:579）：冒险模式按 `gZombieWaves[level-1]`（非初见+10），各挑战模式各有固定波数。
2. **逐波填充**：每波算“僵尸点数” `aZombiePoints`（`wave/3+1` 起步，旗帜波×2.5，部分关卡再翻倍），先固定塞入旗帜波普通僵尸+旗帜僵尸、新引入僵尸、Boss/柱子关卡特殊怪，剩余点数循环 `PickZombieType`（加权随机）补充。
3. **出怪许可** `InitZombieWavesForLevel` 用 `CanZombieSpawnOnLevel`（按 `gZombieAllowedLevels` + `mStartingLevel`）填充 `mZombieAllowed[]`。
4. **逐帧推进** `UpdateZombieSpawning`（cpp:5378）：递减 `mZombieCountDown`；到 5 时若旗帜波则播“大波”预告（`mHugeWaveCountDown=750`），到 0 调 `SpawnZombieWave()`。相邻波间隔受上一波剩余血量动态缩短（`mZombieHealthToNextWave`）。
5. **刷出一波** `SpawnZombieWave`（cpp:5032）：遍历 `mZombiesInWave[mCurrentWave]`，逐只 `AddZombie`；最后一波后 210 帧触发 `SpawnZombiesFromGraves`（墓碑/泳池/天空出怪）；旗帜波触发旗升；`mCurrentWave++`。
6. **行选择** `PickRowForNewZombie`（cpp:2610）：优先钉耙所在行，否则 `TodPickFromSmoothArray` 平滑加权（丢车保护使刚被割草机扫过的行权重降低）。
7. **胜利判定**：`UpdateZombieSpawning` 中当 `mCurrentWave == mNumWaves` 且场上清空 → `FadeOutLevel()`；僵尸走到屋左侧触发 `ZombiesWon`（失败）。

---

## 6. 可复用性评估

### 6.1 ★★★ 独立通用（可直接移植，无游戏耦合）

| 函数 | 说明 |
|---|---|
| `GetRectOverlap(const Rect&,const Rect&)` | 两矩形水平重叠像素宽度，纯几何 |
| `GetCircleRectOverlap(int,int,int,const Rect&)` | 圆-矩形碰撞，纯几何 |
| `RenderItemSortFunc(const RenderItem&,const RenderItem&)` | 稳定排序比较器（zPos 优先、指针兜底），可作通用渲染排序 |
| `MakeRenderOrder(RenderLayer,int,int)` | 分层 + 行 + 偏移的 z 值合成，通用绘制排序 |
| `PixelToGridX / PixelToGridY / GridToPixelX / GridToPixelY` | 网格↔像素换算（仅依赖关卡背景常量，边界清晰） |
| `PixelToGridXKeepOnBoard / PixelToGridYKeepOnBoard` | 带钳制的坐标换算 |
| `GetPosYBasedOnRow(float,int)` | 斜坡行 y 插值 |

### 6.2 ★★ 可借鉴算法（逻辑清晰，稍改即可复用）

| 函数 | 说明 |
|---|---|
| `PickZombieType` / `PickGraveRisingZombieType` / `PickRowForNewZombie` | 加权随机（`TodWeightedArray`/`TodSmoothArray`）选型/选行，是“按权重抽奖+冷却/保护”的通用范式 |
| `PutZombieInWave` | “点数预算制”生成器：按价值消耗预算填充列表 |
| `UpdateProgressMeter` | 双指标（时间比例 + 血量比例）取大值推进进度条，含平滑增量 |
| `GetPlantsOnLawn` / `GetTopPlantAt` | 同格多实体分层归档 + 优先级取顶层，可推广到任意“多层占位”系统 |
| `Iterate*` 系列 / `ProcessDeleteQueue` | “标记死亡 + 延迟删除”的容器遍历范式 |
| `TotalZombiesHealthInWave` | 按波次聚合实体剩余血量（含护盾/头盔/飞行多部位） |
| `CanZombieSpawnOnLevel` / `RowCanHaveZombieType` / `IsFlagWave` | 关卡/行/波次约束判定链，可借鉴其“多条件过滤”结构 |
| `KillAllPlantsInRadius` / `KillAllZombiesInRadius` | 范围 AOE（圆-矩形重叠 + 行距 + 伤害类型标志位） |
| `SeedNotRecommendedForLevel` | 位标志聚合多种“不推荐”条件 |
| `DrawGameObjects` + `Add*RenderItem` 静态函数 | 渲染列表收集 + 按 z 排序 + 分派绘制，是通用 2D 渲染管线模板 |
| `ClearFogAroundPlant` | 基于网格的圆形可视区域清除（迷雾/LOS 通用） |
| `GetSeedPacketPositionX / GetSeedBankExtraWidth` | 卡槽布局计算 |

### 6.3 ★ 业务耦合（依赖 PvZ 实体与玩法，不宜直接复用）

- 构造/析构、`InitLevel`/`StartLevel`/`InitSurvivalStage`/`PickBackground` 等关卡装配流程。
- 全部 `Draw*` UI 绘制函数、`Mouse*`/`Key*` 交互分发、`Update*` 游戏循环流程。
- `AddPlant`/`AddZombie`/`AddProjectile`/`AddCoin` 等实体工厂（依赖各自 `*Initialize`）。
- `CanPlantAt`/`MouseDownWithPlant`（大量植物种类硬编码）、波次生成的具体数值配置、教程/存档/胜负序列、彩蛋模式开关。

### 6.4 总体结论

- **纯逻辑可复用点**集中在：几何碰撞（`GetRectOverlap`/`GetCircleRectOverlap`）、坐标换算（Pixel/Grid 互转）、渲染排序（`MakeRenderOrder`/`RenderItemSortFunc`）、加权随机选型（`PickZombieType`/`PickRowForNewZombie`）、延迟删除容器遍历（`Iterate*`/`ProcessDeleteQueue`）、进度条推进（`UpdateProgressMeter`）。
- 若要将 Board 整体剥离复用，需同时解耦 `LawnApp`、`Challenge`、`SeedBank`、`CutScene`、各实体类及 `TodLib` 工具库，成本高；建议按上表**局部抽取**算法函数复用。
