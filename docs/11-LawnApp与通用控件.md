# LawnApp 与通用控件模块文档

> 所属: `src/Lawn`
> 编码: 源文件为 GBK(代码页 936)，本文档为 UTF-8。
> 分析范围: LawnApp.h/.cpp、LawnCommon.h/.cpp、GameButton.h/.cpp、CursorObject.h/.cpp、MessageWidget.h/.cpp、ToolTipWidget.h/.cpp、PoolEffect.h/.cpp、ImitaterDialog.h/.cpp（另附 TypingCheck.h/.cpp 作为作弊码系统的支撑类）。

---

## 1. 模块职责

本组文件构成了《植物大战僵尸》反编译工程的**程序主控与通用 UI/特效控件层**：

| 文件 | 职责 |
| --- | --- |
| `LawnApp.h/.cpp` | 程序主类。继承框架 `SexyApp`，负责窗口与启动、资源加载管线、场景(Widget)切换、游戏模式/场景状态、作弊码注册、注册/试玩 DRM、命令行参数、玩家档案、Crazy Dave 演出、全局资源表（音效/粒子/动画/拖尾/字符串格式）。 |
| `LawnCommon.h/.cpp` | 通用小工具：网格常量、`GameMode` 枚举、`LawnEditWidget` 输入框、范围判断、图片平铺、控件工厂、存档路径、日期计算。 |
| `GameButton.h/.cpp` | 游戏内按钮：轻量自绘 `GameButton`、石质按钮 `LawnStoneButton`、新版按钮 `NewLawnButton`，及全局按钮绘制/工厂函数。 |
| `CursorObject.h/.cpp` | 光标对象：关卡中跟随鼠标的“拖拽物”（植物/铲子/工具/推车等），及网格落点预览 `CursorPreview`。 |
| `MessageWidget.h/.cpp` | 消息显示控件：关卡内字幕/提示条，支持逐字动画(Reanimation)。 |
| `ToolTipWidget.h/.cpp` | 提示框控件：带标题/警告/换行文本的悬浮提示。 |
| `PoolEffect.h/.cpp` | 泳池水波效果：焦散(caustic)纹理实时双线性采样 + 3D 三角网格扭曲。 |
| `ImitaterDialog.h/.cpp` | 模仿者(变脸娃娃)选择对话框：在选卡界面挑选被模仿的植物。 |
| `TypingCheck.h/.cpp`（支撑） | 作弊码“按键序列匹配器”：滚动窗口记录最近按键，匹配目标短语。 |

---

## 2. 各类分析

### 2.1 LawnApp（程序主类）

- **继承关系**: `class LawnApp : public Sexy::SexyApp`
- **职责**: 全局单例（`gLawnApp`）。持有所有屏幕/界面 Widget 指针与全局子系统（音乐 `Music`、音效 `TodFoley`、特效 `EffectSystem`、动画缓存 `ReanimatorCache`、档案 `ProfileMgr`、泳池 `PoolEffect`、禅境花园 `ZenGarden`、DRM `PopDRMComm`）；驱动启动流程 `WinMain → Init → Start → UpdateFrames → Shutdown`；管理场景切换、关卡结束结算、作弊码注册与判定、注册/试玩 DRM。

#### 成员变量表（头部声明，含对象布局偏移）

| 成员 | 类型 | 偏移 | 说明 |
| --- | --- | --- | --- |
| `mBoard` | `Board*` | +0x768 | 当前关卡棋盘（最核心运行时对象） |
| `mTitleScreen` | `TitleScreen*` | +0x76C | 标题/加载画面 |
| `mGameSelector` | `GameSelector*` | +0x770 | 主菜单（冒险/迷你游戏/益智/生存/花园入口） |
| `mSeedChooserScreen` | `SeedChooserScreen*` | +0x774 | 选卡界面 |
| `mAwardScreen` | `AwardScreen*` | +0x778 | 奖励/奖杯界面 |
| `mCreditScreen` | `CreditScreen*` | +0x77C | 制作人员名单 |
| `mChallengeScreen` | `ChallengeScreen*` | +0x780 | 挑战(迷你/益智/生存)选择页 |
| `mSoundSystem` | `TodFoley*` | +0x784 | 音效系统 |
| `mControlButtonList` | `ButtonList` | +0x788 | 控件按钮链表 |
| `mCreatedImageList` | `ImageList` | +0x794 | 本 App 动态创建的图片链表 |
| `mReferId` / `mRegisterLink` / `mMod` | `std::string` | +0x7A0/+0x7BC/+0x7D8 | 推荐人 ID / 注册链接 / mod 标识 |
| `mRegisterResourcesLoaded` | `bool` | +0x7F4 | 注册资源是否已加载 |
| `mTodCheatKeys` | `bool` | +0x7F5 | 是否开启调试作弊键（`-tod` 参数） |
| `mGameMode` | `GameMode` | +0x7F8 | 当前游戏模式（见 §3） |
| `mGameScene` | `GameScenes` | +0x7FC | 当前场景状态（见 §3） |
| `mLoadingZombiesThreadCompleted` | `bool` | +0x800 | 僵尸资源加载线程完成标志 |
| `mFirstTimeGameSelector` | `bool` | +0x801 | 是否首次进入主菜单 |
| `mGamesPlayed` / `mMaxExecutions` / `mMaxPlays` / `mMaxTime` | `int` | +0x804..+0x810 | 已玩局数 / 试玩限制（执行次数、局数、时长） |
| `mEasyPlantingCheat` | `bool` | +0x814 | 简易种植作弊 |
| `mPoolEffect` / `mZenGarden` / `mEffectSystem` / `mReanimatorCache` | 指针 | +0x818..+0x824 | 泳池特效 / 禅境花园 / 特效系统 / 动画缓存 |
| `mProfileMgr` / `mPlayerInfo` / `mLastLevelStats` | 指针 | +0x828..+0x830 | 档案管理器 / 当前玩家 / 上局统计 |
| `mCloseRequest` | `bool` | +0x834 | 请求退出标志 |
| `mAppCounter` | `int` | +0x838 | 应用主循环帧计数 |
| `mMusic` | `Music*` | +0x83C | 音乐播放器 |
| `mCrazyDaveReanimID` | `ReanimationID` | +0x840 | 戴夫动画 ID |
| `mCrazyDaveState` | `CrazyDaveState` | +0x844 | 戴夫状态机（见 §3） |
| `mCrazyDaveBlinkCounter` / `mCrazyDaveBlinkReanimID` | `int`/`ReanimationID` | +0x848/+0x84C | 戴夫眨眼计时/动画 |
| `mCrazyDaveMessageIndex` / `mCrazyDaveMessageText` | `int`/`SexyString` | +0x850/+0x854 | 戴夫台词索引/文本 |
| `mAppRandSeed` | `int` | +0x872 | 应用随机种子 |
| `mBigArrowCursor` | `HICON` | +0x874 | 大箭头光标句柄 |
| `mDRM` | `PopDRMComm*` | +0x878 | 试玩/注册 DRM 通信对象 |
| `mSessionID` | `int` | +0x87C | 会话 ID |
| `mPlayTimeActiveSession` / `mPlayTimeInactiveSession` | `int` | +0x880/+0x884 | 本会话活跃/非活跃游玩时长 |
| `mBoardResult` | `BoardResult` | +0x888 | 本局结果（见 §3） |
| `mSawYeti` | `bool` | +0x88C | 是否见过雪人僵尸 |
| `mKonamiCheck` … `mSukhbirCheck` | `TypingCheck*`×10 | +0x890..+0x8B4 | 10 个作弊码匹配器（见 §5.4） |
| `mMustacheMode` … `mSukhbirMode` | `bool`×7 | +0x8B8..+0x8BE | 各作弊模式开关（胡子/超级割草机/未来/彩罐/跳舞/雏菊/Sukhbir） |
| `mTrialType` | `TrialType` | +0x8C0 | 试玩类型（见 §3） |
| `mDebugTrialLocked` | `bool` | +0x8C4 | 调试用强制试玩锁定 |
| `mMuteSoundsForCutscene` | `bool` | +0x8C5 | 过场动画期间静音标志 |

#### 成员函数索引表（签名 | 文件:行号 | 功能 | 可复用性）

> 行号均为实现处（`.cpp`）；构造/析构为 `LawnApp.cpp`。可复用性 ★★★=自包含通用 / ★★=弱耦合可移植 / ★=强耦合游戏状态。

**A. 生命周期、初始化与加载管线**

| 签名 | 文件:行号 | 功能 | 复用 |
| --- | --- | --- | --- |
| `LawnApp()` | LawnApp.cpp:463 | 构造：初始化所有指针/状态，设定窗口尺寸、注册表键、标题、档案管理器，加载大箭头光标 | ★ |
| `virtual ~LawnApp()` | LawnApp.cpp:543 | 析构：存档、销毁 Board/TitleScreen/各界面/音效/音乐/作弊码/档案管理器，释放资源 | ★ |
| `virtual void Init()` | LawnApp.cpp:1587 | 解析命令行→限制单实例→`SexyApp::Init()`→解析 resources.xml→加载 `Init` 资源组→加载档案→建 TitleScreen→建 Music/Foley/EffectSystem→注册全部作弊码→加载动画定义表 | ★ |
| `virtual void InitHook()` | LawnApp.cpp:3681 | 框架初始化钩子：Release 下创建 `PopDRMComm` 并 `Connect()`；`MarketingMode=="StageLocked"` 时设 `mTrialType=STAGELOCKED` 并 `EnableLocking()`；Debug 下 DRM 置空 | ★ |
| `virtual void Start()` | LawnApp.cpp:1711 | 加载失败则返回，否则 `SexyAppBase::Start()` 进入消息循环 | ★ |
| `virtual void Shutdown()` | LawnApp.cpp:645 | 关闭流程：清对话框→存盘→KillBoard→删除 Pool/ZenGarden/EffectSystem/ReanimatorCache→释放粒子/动画/拖尾定义→释放全局分配器→`UpdateRegisterInfo()`→基类 Shutdown→删 DRM | ★ |
| `virtual void LoadingThreadProc()` | LawnApp.cpp:2057 | **后台资源加载主流程**（见 §5.2） | ★ |
| `void LoadGroup(const char*,int)` | LawnApp.cpp:2031 | 加载一个资源组：`StartLoadResources` + 循环 `TodLoadNextResource()`，累计进度，错误时弹资源错误 | ★★ |
| `virtual void LoadingCompleted()` | LawnApp.cpp:2147 | 加载完成：移除 TitleScreen、删标题图、进入主菜单 `ShowGameSelector()` | ★ |
| `virtual void LoadingThreadCompleted()` | LawnApp.cpp:2142 | 加载线程完成回调（空实现） | ★ |
| `void FastLoad(GameMode)` | LawnApp.cpp:2130 | 快速加载：跳过标题画面直接 `PreNewGame`（Debug 快捷路径） | ★ |
| `void PreloadForUser()` | LawnApp.cpp:3454 | 按玩家进度预载植物/僵尸资源（支持标题画面按键取消） | ★★ |
| `int GetNumPreloadingTasks()` | LawnApp.cpp:3422 | 估算预载任务数（用于进度条） | ★ |
| `void TraceLoadGroup(...)` | LawnApp.cpp:4144 | 加载分组耗时跟踪（空实现） | ★ |
| `virtual void ShowResourceError(bool)` | LawnApp.cpp:1573 | 转发基类资源错误弹窗 | ★ |
| `virtual bool ChangeDirHook(const char*)` | LawnApp.cpp:1705 | 目录切换钩子（固定 false） | ★ |
| `virtual void SwitchScreenMode(bool,bool,bool)` | LawnApp.cpp:3799 | 切换窗口/全屏与 3D，并同步选项对话框勾选 | ★ |

**B. 场景/界面切换（Show/Kill 系列，WidgetManager 装配）**

| 签名 | 文件:行号 | 功能 | 复用 |
| --- | --- | --- | --- |
| `void PreNewGame(GameMode,bool)` | LawnApp.cpp:794 | 进入新游戏前：设模式→有存档则 `TryLoadGame`，否则删存档→`NewGame()` | ★ |
| `void NewGame()` | LawnApp.cpp:860 | 新建关卡：`MakeNewBoard()`→`InitLevel()`→场景=LEVEL_INTRO→`ShowSeedChooserScreen()`→播放入场过场 | ★ |
| `void MakeNewBoard()` | LawnApp.cpp:812 | `KillBoard()` 后 `new Board`，Resize/AddWidget/BringToBack/SetFocus | ★ |
| `void KillBoard()` | LawnApp.cpp:714 | 收尾无模式对话框、关选卡、`DisposeBoard()`、移除并延迟删除 Board | ★ |
| `void StartPlaying()` | LawnApp.cpp:823 | 关选卡→`mBoard->StartLevel()`→场景=PLAYING | ★ |
| `bool TryLoadGame()` | LawnApp.cpp:838 | 尝试读档：存在存档则 `MakeNewBoard()+LoadGame`，成功弹“继续”对话框 | ★ |
| `bool SaveFileExists()` | LawnApp.cpp:831 | 判断冒险模式存档文件是否存在 | ★★ |
| `void EndLevel()` | LawnApp.cpp:1017 | 结束本关：KillBoard 后重开新局 | ★ |
| `void ShowGameSelector()` / `KillGameSelector()` | LawnApp.cpp:874 / 898 | 显示/销毁主菜单（场景=MENU） | ★ |
| `void ShowAwardScreen(AwardType)` / `KillAwardScreen()` | LawnApp.cpp:909 / 920 | 显示/销毁奖励界面（场景=AWARD） | ★ |
| `void ShowCreditScreen()` / `KillCreditScreen()` | LawnApp.cpp:931 / 941 | 显示/销毁制作名单 | ★ |
| `void ShowChallengeScreen(ChallengePage)` / `KillChallengeScreen()` | LawnApp.cpp:952 / 963 | 显示/销毁挑战页（场景=CHALLENGE） | ★ |
| `StoreScreen* ShowStoreScreen()` / `KillStoreScreen()` | LawnApp.cpp:974 / 986 | 以对话框形式打开/关闭商店 | ★ |
| `void ShowSeedChooserScreen()` / `KillSeedChooserScreen()` | LawnApp.cpp:996 / 1007 | 显示/销毁选卡界面 | ★ |
| `void DoBackToMain()` | LawnApp.cpp:1029 | 返回主菜单：停音乐/音效→写配置→关选项→KillBoard→ShowGameSelector | ★ |
| `void DoConfirmBackToMain()` | LawnApp.cpp:1040 | 弹“是否返回主菜单”确认框 | ★ |
| `void CheckForGameEnd()` | LawnApp.cpp:1827 | 每帧检查关卡是否完成，按模式分发后续（奖励界面/挑战页/下一生存波/下一关） | ★ |
| `bool UpdatePlayerProfileForFinishingLevel()` | LawnApp.cpp:1742 | 通关后更新档案（推进关卡、冒险周目、挑战记录、解锁新挑战标记） | ★ |
| `void FinishZenGardenToturial()` | LawnApp.cpp:3661 | 禅境花园教程结束→回到冒险模式 | ★ |

**C. 对话框管理（基于框架 Dialog 体系）**

| 签名 | 文件:行号 | 功能 | 复用 |
| --- | --- | --- | --- |
| `Dialog* DoDialog(int,bool,...)` | LawnApp.cpp:1147 | 创建通用对话框（先 `TodStringTranslate` 再转基类） | ★★ |
| `Dialog* DoDialogDelay(...)` | LawnApp.cpp:1162 | 创建对话框并设按钮延迟 30 | ★★ |
| `Dialog* NewDialog(...)` | LawnApp.cpp:1468 | 新建 `LawnDialog` 并居中 | ★★ |
| `bool KillDialog(int)` | LawnApp.cpp:1545 | 销毁对话框并恢复焦点/取消暂停 | ★ |
| `void ModalOpen()` / `ModalClose()` | LawnApp.cpp:1532 / 1540 | 模态打开时暂停游戏 / 关闭（空） | ★ |
| `int LawnMessageBox(...)` | LawnApp.cpp:1124 | 模态消息框封装，`WaitForResult` 阻塞等待 | ★★ |
| `static void CenterDialog(Dialog*,int,int)` | LawnApp.cpp:2372 | 将对话框居中于 BOARD 尺寸 | ★★★ |
| `void DoNewOptions(bool)` / `bool KillNewOptionsDialog()` | LawnApp.cpp:1057 / 1485 | 打开/关闭选项对话框（关闭时应用窗口/3D 切换） | ★ |
| `AlmanacDialog* DoAlmanacDialog(...)` / `bool KillAlmanacDialog()` | LawnApp.cpp:1068 / 1501 | 打开/关闭图鉴（可直达某植物/僵尸） | ★ |
| `void DoUserDialog()` / `FinishUserDialog(bool)` | LawnApp.cpp:1170 / 1181 | 打开/确认“切换用户” | ★ |
| `void DoCreateUserDialog()` / `FinishCreateUserDialog(bool)` | LawnApp.cpp:1206 / 1216 | 新建用户及其校验（空名/重名） | ★ |
| `void DoConfirmDeleteUserDialog(...)` / `FinishConfirmDeleteUserDialog(bool)` | LawnApp.cpp:1282 / 1297 | 删除用户确认与执行 | ★ |
| `void DoRenameUserDialog(...)` / `FinishRenameUserDialog(bool)` | LawnApp.cpp:1341 / 1352 | 重命名用户确认与执行 | ★ |
| `void FinishNameError(int)` | LawnApp.cpp:1398 | 名称错误对话框关闭后回焦输入框 | ★ |
| `void DoCheatDialog()` / `FinishCheatDialog(bool)` | LawnApp.cpp:1421 / 1430 | 作弊对话框（`CheatDialog`）应用与重开本局 | ★ |
| `void DoContinueDialog()` | LawnApp.cpp:1095 | 读档后的“继续”对话框 | ★ |
| `void DoPauseDialog()` | LawnApp.cpp:1103 | 暂停对话框 | ★ |
| `void DoConfirmSellDialog(...)` / `DoConfirmPurchaseDialog(...)` | LawnApp.cpp:1453 / 1460 | 出售/购买确认框 | ★ |
| `void FinishTimesUpDialog()` | LawnApp.cpp:1448 | 时间到对话框关闭 | ★ |
| `void FinishRestartConfirmDialog()` | LawnApp.cpp:1410 | 重开本关确认后重开 | ★ |
| `void ConfirmQuit()` | LawnApp.cpp:2198 | 退出确认对话框 | ★ |
| `bool NeedPauseGame()` | LawnApp.cpp:1514 | 依据对话框列表判断是否需要暂停棋盘 | ★ |
| `void FinishModelessDialogs()` | LawnApp.cpp:4176 | 收尾无模式对话框（空实现） | ★ |
| `void DoHighScoreDialog()` | LawnApp.cpp:4151 | 高分榜对话框（空实现） | ★ |

**D. 帧循环 / 输入 / 系统回调**

| 签名 | 文件:行号 | 功能 | 复用 |
| --- | --- | --- | --- |
| `virtual void UpdateFrames()` | LawnApp.cpp:1967 | 主帧更新：慢/快放计数、删除队列、基类更新、音乐更新、`CheckForGameEnd` | ★ |
| `virtual bool UpdateApp()` | LawnApp.cpp:2643 | 应用更新：处理退出请求后转发基类 | ★ |
| `virtual void PreDisplayHook()` | LawnApp.cpp:2208 | 显示前钩子（转发基类） | ★ |
| `void GotFocus()` / `LostFocus()` | LawnApp.cpp:753 / 758 | 获得/失去焦点（失焦且未开作弊键时自动暂停） | ★ |
| `virtual void ButtonPress(int)` / `ButtonDepress(int)` | LawnApp.cpp:2213 / 2218 | 对话框按钮事件分发：按编号 `[2000,3000)`=是、`[3000,4000)`=否，路由到各 `Finish*` | ★ |
| `virtual bool DebugKeyDown(int)` | LawnApp.cpp:1720 | 调试键（转发基类） | ★ |
| `void HandleCmdLineParam(name,value)` | LawnApp.cpp:1726 | 命令行参数：`-tod` 在 Debug 下开 `mTodCheatKeys/mDebugKeysEnabled`，其余转基类 | ★ |
| `void ToggleSlowMo()` / `ToggleFastMo()` | LawnApp.cpp:2017 / 2024 | 慢放(1/4 帧率)/快放(20×)切换 | ★ |
| `virtual void EnforceCursor()` | LawnApp.cpp:3547 | 按 `mCursorNum` 设置系统光标（`SetCursor` 分支） | ★★ |
| `bool CanPauseNow()` | LawnApp.cpp:733 | 判断当前是否可暂停 | ★ |
| `virtual void CloseRequestAsync()` | LawnApp.cpp:2667 | 异步退出请求：清延迟消息、置 `mCloseRequest` | ★ |
| `virtual bool OpenURL(...)` / `URLOpenSucceeded(...)` / `URLOpenFailed(...)` | LawnApp.cpp:2181 / 2174 / 2159 | 打开 URL 的等待对话框与成败回调 | ★ |

**E. 模式/关卡判断（IsXxx / CanXxx）**

| 签名 | 文件:行号 | 功能 | 复用 |
| --- | --- | --- | --- |
| `bool IsAdventureMode()` | LawnApp.cpp:2403 | 是否冒险模式 | ★ |
| `bool IsSurvivalMode()` | LawnApp.cpp:2409 | 是否生存模式（含普通/困难/无尽） | ★ |
| `bool IsPuzzleMode()` | LawnApp.cpp:2415 | 是否益智模式（花瓶/我是僵尸） | ★ |
| `bool IsChallengeMode()` | LawnApp.cpp:2423 | 是否挑战（迷你游戏）模式 = 非冒险/益智/生存 | ★ |
| `static bool IsSurvivalNormal/Hard/Endless(GameMode)` | LawnApp.cpp:2428 / 2434 / 2440 | 判断生存模式难度段 | ★ |
| `static bool IsEndlessScaryPotter/IsEndlessIZombie(GameMode)` | LawnApp.cpp:2446 / 2451 | 无尽花瓶/无尽我是僵尸 | ★ |
| `bool IsContinuousChallenge()` | LawnApp.cpp:2457 | 是否“连续”型挑战（艺术/老虎机/BOSS/宝石迷阵等） | ★ |
| `bool IsArtChallenge()` | LawnApp.cpp:2469 | 艺术挑战（坚果/向日葵/看星星） | ★ |
| `bool IsSquirrelLevel()` / `IsIZombieLevel()` / `IsShovelLevel()` | LawnApp.cpp:2481 / 2487 / 2506 | 松鼠/我是僵尸/铲子关卡 | ★ |
| `bool IsWallnutBowlingLevel()` / `IsSlotMachineLevel()` / `IsWhackAZombieLevel()` | LawnApp.cpp:2512 / 2524 / 2530 | 保龄球/老虎机/打地鼠关卡 | ★ |
| `bool IsLittleTroubleLevel()` / `IsScaryPotterLevel()` | LawnApp.cpp:2542 / 2548 | 小小麻烦/花瓶关卡 | ★ |
| `bool IsStormyNightLevel()` / `IsBungeeBlitzLevel()` | LawnApp.cpp:2557 / 2569 | 雷雨夜/蹦极闪电关卡 | ★ |
| `bool IsMiniBossLevel()` / `IsFinalBossLevel()` | LawnApp.cpp:2581 / 2593 | 小BOSS(10/20/30)/最终BOSS(50)关卡 | ★ |
| `bool IsChallengeWithoutSeedBank()` | LawnApp.cpp:2605 | 无卡槽的关卡（雨天/打地鼠/松鼠/花瓶等） | ★ |
| `bool IsNight()` | LawnApp.cpp:2618 | 是否夜晚关卡（11-20、31-40、50） | ★ |
| `bool IsIceDemo()` | LawnApp.h:285（内联） | 是否冰面演示（恒 false） | ★ |
| `bool IsTrialStageLocked()` | LawnApp.cpp:3669 | 试玩是否锁定阶段（DRM 未授权且 StageLocked） | ★ |
| `bool IsRegistered()/IsExpired()/IsDRMConnected()` | LawnApp.h:324-326（内联） | 注册/过期/DRM 连接（均恒 false） | ★ |
| `bool HasSeedType(SeedType)` | LawnApp.cpp:2710 | 是否拥有某植物（商店购买 + 进度解锁，试玩锁拦截辣椒以上） | ★ |
| `bool SeedTypeAvailable(SeedType)` | LawnApp.cpp:2756 | 植物可用（机枪豌豆特殊处理 + HasSeedType） | ★ |
| `bool HasBeatenChallenge(GameMode)` | LawnApp.cpp:2890 | 是否已通关某挑战（生存按旗数阈值） | ★ |
| `bool HasFinishedAdventure()` | LawnApp.cpp:2913 | 是否完成过冒险模式 | ★ |
| `bool IsFirstTimeAdventureMode()` | LawnApp.cpp:2919 | 是否首次冒险 | ★ |
| `bool CanShowAlmanac()/CanShowStore()/CanShowZenGarden()` | LawnApp.cpp:2848 / 2860 / 2872 | 各功能解锁条件（进度门槛） | ★ |
| `bool CanSpawnYetis()` | LawnApp.cpp:2883 | 是否可刷雪人（通关且等级/周目达标） | ★ |
| `bool CanDoPinataMode()/CanDoDanceMode()/CanDoDaisyMode()` | LawnApp.cpp:3763 / 3772 / 3781 | 彩罐/跳舞/雏菊作弊是否可用（智慧树记录≥1000/500/100） | ★ |

**F. 进度 / 存档 / 统计**

| 签名 | 文件:行号 | 功能 | 复用 |
| --- | --- | --- | --- |
| `void WriteToRegistry()` / `ReadFromRegistry()` | LawnApp.cpp:767 / 779 | 写/读注册表（当前用户名 + 玩家详情） | ★ |
| `bool WriteCurrentUserConfig()` | LawnApp.cpp:785 | 保存当前玩家详情 | ★ |
| `static SexyString GetStageString(int)` | LawnApp.cpp:2396 | 关卡号转“大关-小关”字符串 | ★★★ |
| `static SeedType GetAwardSeedForLevel(int)` | LawnApp.cpp:2675 | 由关卡号推算应解锁的植物 | ★★ |
| `int GetSeedsAvailable()` | LawnApp.cpp:2697 | 当前可用植物种类数（进度上限） | ★ |
| `int GetCurrentChallengeIndex()` | LawnApp.cpp:2626 | 当前模式 → 挑战索引（减去生存模式基准） | ★ |
| `ChallengeDefinition& GetCurrentChallengeDef()` | LawnApp.cpp:2631 | 当前挑战定义 | ★ |
| `int GetNumTrophies(ChallengePage)` | LawnApp.cpp:3633 | 统计某挑战页已获奖杯数 | ★ |
| `int TrophiesNeedForGoldSunflower()` | LawnApp.cpp:3650 | 距金向日葵还差多少奖杯（48 - 已获） | ★ |
| `bool EarnedGoldTrophy()` | LawnApp.cpp:3656 | 是否已获金向日葵 | ★ |
| `PottedPlant* GetPottedPlantByIndex(int)` | LawnApp.cpp:2636 | 取盆栽 | ★ |
| `void UpdatePlayTimeStats()` | LawnApp.cpp:1913 | 累计活跃/非活跃游玩时长（含作弊标记） | ★ |
| `void BetaSubmit(bool)` / `BetaRecordLevelStats()` / `BetaAddFile(...)` | LawnApp.cpp:3812 / 3902 / 4129 | Beta 上报/统计（编译期 `_DEBUG && FALSE` 关闭） | ★ |

**G. 特效 / 动画 / 音效门面**

| 签名 | 文件:行号 | 功能 | 复用 |
| --- | --- | --- | --- |
| `Reanimation* AddReanimation(...)` | LawnApp.cpp:2762 | 经 EffectSystem 分配动画 | ★ |
| `TodParticleSystem* AddTodParticle(...)` | LawnApp.cpp:2768 | 经 EffectSystem 分配粒子 | ★ |
| `ParticleSystemID ParticleGetID(...)` / `ReanimationID ReanimationGetID(...)` | LawnApp.cpp:2773 / 2778 | 取 ID | ★ |
| `TodParticleSystem* ParticleGet/TryToGet(...)` | LawnApp.cpp:2783 / 2788 | 按 ID 取粒子（断言/容错） | ★ |
| `Reanimation* ReanimationGet/TryToGet(...)` | LawnApp.cpp:2793 / 2799 | 按 ID 取动画（断言/容错） | ★ |
| `void RemoveReanimation(ReanimationID)` / `RemoveParticle(ParticleSystemID)` | LawnApp.cpp:2805 / 2814 | 移除动画/粒子 | ★ |
| `void PlayFoley(FoleyType)` / `PlayFoleyPitch(FoleyType,float)` | LawnApp.cpp:2378 / 2387 | 播放音效（过场静音时忽略） | ★ |
| `void PlaySample(int)` | LawnApp.cpp:3790 | 播放采样（过场静音时忽略） | ★ |

**H. Crazy Dave（疯狂戴夫）演出**

| 签名 | 文件:行号 | 功能 | 复用 |
| --- | --- | --- | --- |
| `void CrazyDaveEnter()` | LawnApp.cpp:2925 | 戴夫进场（动画进入，雷雨夜改色） | ★ |
| `void CrazyDaveDie()` / `CrazyDaveLeave()` | LawnApp.cpp:2948 / 2965 | 戴夫销毁/离场 | ★ |
| `void CrazyDaveTalkIndex(int)` | LawnApp.cpp:2987 | 按索引播放台词 | ★ |
| `void CrazyDaveTalkMessage(const SexyString&)` | LawnApp.cpp:3014 | 解析台词控制字 `{HANDING}/{SHAKE}/{SCREAM}/{SHOW_*}/{MOUTH_*}` 并驱动动画+音效 | ★ |
| `void CrazyDaveStopTalking()` | LawnApp.cpp:3259 | 停止说话，回到待机/递物待机 | ★ |
| `void UpdateCrazyDave()` | LawnApp.cpp:3290 | 更新戴夫状态机、嘴型、眨眼 | ★ |
| `void DrawCrazyDave(Graphics*)` | LawnApp.cpp:3367 | 绘制戴夫与气泡台词 | ★ |
| `void CrazyDaveDoneHanding()` | LawnApp.cpp:2995 | 结束“递物”姿势，销毁手上附件 | ★ |
| `void CrazyDaveStopSound()` | LawnApp.cpp:3005 | 停止戴夫音效 | ★ |
| `bool AdvanceCrazyDaveText()` | LawnApp.cpp:2824 | 推进到下一条台词 | ★ |
| `SexyString GetCrazyDaveText(int)` | LawnApp.cpp:2837 | 取台词并替换 `{PLAYER_NAME}/{MONEY}/{UPGRADE_COST}` | ★ |

**I. 注册 / 试玩 DRM（大部分为空壳）**

| 签名 | 文件:行号 | 功能 | 复用 |
| --- | --- | --- | --- |
| `void DoRegister()` / `DoRegisterError()` | LawnApp.cpp:4156 / 4161 | 注册/注册错误（空） | ★ |
| `bool CanDoRegisterDialog()` / `NeedRegister()` | LawnApp.cpp:4166 / 4181 | 是否可/需要注册（恒 false） | ★ |
| `void DoNeedRegisterDialog()` / `UpdateRegisterInfo()` | LawnApp.cpp:4171 / 4186 | 需要注册对话框/更新注册信息（空） | ★ |

**J. 工具函数**

| 签名 | 文件:行号 | 功能 | 复用 |
| --- | --- | --- | --- |
| `static SexyString Pluralize(int,const SexyChar*,const SexyChar*)` | LawnApp.cpp:3622 | 单复数选择并替换 `{COUNT}` | ★★★ |
| `static SexyString GetMoneyString(int)` | LawnApp.cpp:3701 | 金额格式化（×10 显示，千分位） | ★★★ |
| `static inline SexyString GetCurrentLevelName()` | LawnApp.h:344（内联） | 当前关卡名（恒 "Unknown"，被全局函数覆盖） | ★ |

---

### 2.2 LevelStats（关卡统计）

- **继承关系**: 无基类。
- **职责**: 记录单局统计。当前仅一个成员。

| 成员 | 类型 | 说明 |
| --- | --- | --- |
| `mUnusedLawnMowers` | `int` | 未使用的割草机数 |

| 签名 | 文件:行号 | 功能 | 复用 |
| --- | --- | --- | --- |
| `LevelStats()` | LawnApp.h:53（内联） | 构造即 `Reset()` | ★ |
| `void Reset()` | LawnApp.h:54（内联） | 清零统计 | ★ |

---

### 2.3 LawnEditWidget（输入框）

- **继承关系**: `class LawnEditWidget : public Sexy::EditWidget`
- **职责**: 游戏内单行输入框扩展：绑定所属对话框，首字母自动大写，ESC 转交对话框。

| 成员 | 类型 | 说明 |
| --- | --- | --- |
| `mDialog` | `Sexy::Dialog*` | 所属对话框（用于 ESC 转发） |
| `mAutoCapFirstLetter` | `bool` | 首字母自动大写开关 |

| 签名 | 文件:行号 | 功能 | 复用 |
| --- | --- | --- | --- |
| `LawnEditWidget(int,EditListener*,Dialog*)` | LawnCommon.cpp:57 | 构造，`mAutoCapFirstLetter=true` | ★★ |
| `~LawnEditWidget()` | LawnCommon.cpp:64 | 空析构 | ★★ |
| `virtual void KeyDown(KeyCode)` | LawnCommon.cpp:69 | ESC 转发给对话框 | ★★ |
| `virtual void KeyChar(SexyChar)` | LawnCommon.cpp:77 | 首字母自动大写后转发基类 | ★★★ |

---

### 2.4 GameButton（游戏内轻量按钮）

- **继承关系**: 无基类（自绘、非框架 Widget 继承；坐标相对父 Widget 手动换算）。
- **职责**: 无框架依赖的按钮：4 态图片 + 文字 + 悬浮淡入淡出(alpha) + 石质按钮样式。

| 成员 | 类型 | 说明 |
| --- | --- | --- |
| `mApp` / `mParentWidget` | `LawnApp*` / `Sexy::Widget*` | App 指针 / 父控件（坐标换算用） |
| `mX/mY/mWidth/mHeight` | `int` | 位置尺寸 |
| `mIsOver/mIsDown/mDisabled` | `bool` | 悬浮/按下/禁用 |
| `mColors[NUM_COLORS]` | `Sexy::Color[6]` | 各状态文字/描边/底色 |
| `mId` / `mLabel` / `mLabelJustify` | `int`/`SexyString`/`int` | 按钮 ID / 文本 / 对齐(-1左0中1右) |
| `mFont` | `Sexy::Font*` | 字体（独立持有副本） |
| `mButtonImage/mOverImage/mDownImage/mDisabledImage/mOverOverlayImage` | `Sexy::Image*` | 4 态贴图 + 悬浮叠加层 |
| `mNormalRect/mOverRect/mDownRect/mDisabledRect` | `Sexy::Rect` | 各态精灵子矩形（cel） |
| `mInverted/mBtnNoDraw/mFrameNoDraw` | `bool` | 按下反相 / 整钮不绘 / 边框不绘 |
| `mOverAlpha/mOverAlphaSpeed/mOverAlphaFadeInSpeed` | `double` | 悬浮 alpha 过渡参数 |
| `mDrawStoneButton` | `bool` | 用石质样式绘制 |
| `mTextOffsetX/Y`、`mButtonOffsetX/Y` | `int` | 文本/贴图偏移 |

| 签名 | 文件:行号 | 功能 | 复用 |
| --- | --- | --- | --- |
| `GameButton(int theId)` | GameButton.cpp:56 | 构造，初始化 6 色表等 | ★★ |
| `~GameButton()` | GameButton.cpp:92 | 释放字体副本 | ★★ |
| `static bool HaveButtonImage(Image*,const Rect&)` | GameButton.cpp:98 | 是否存在有效贴图/cel | ★★★ |
| `void DrawButtonImage(Graphics*,Image*,const Rect&,int,int)` | GameButton.cpp:104 | 按偏移绘制贴图（cel 或整图） | ★★ |
| `void SetFont(Font*)` | GameButton.cpp:119 | 复制设置字体 | ★★ |
| `bool IsButtonDown()` | GameButton.cpp:127 | 是否处于按下态 | ★★ |
| `void Draw(Graphics*)` | GameButton.cpp:133 | 主绘制：4 态选择 + 文字 + 悬浮 alpha 叠加 + 高亮叠加层 | ★★ |
| `void SetDisabled(bool)` | GameButton.cpp:114 | 设置禁用 | ★★★ |
| `bool IsMouseOver()` | GameButton.cpp:225 | 是否悬浮（且未禁用/未隐藏） | ★★ |
| `void Update()` | GameButton.cpp:231 | 命中检测 + alpha 淡入淡出更新 | ★★ |
| `void Resize(int,int,int,int)` | GameButton.cpp:216 | 设置位置尺寸 | ★★★ |
| `void SetLabel(const SexyString&)` | GameButton.cpp:278 | 设置文本（翻译后） | ★★★ |

**LawnStoneButton（石质按钮）** — `: public Sexy::DialogButton`

| 签名 | 文件:行号 | 功能 | 复用 |
| --- | --- | --- | --- |
| `LawnStoneButton(Image*,int,ButtonListener*)` | GameButton.h:84（内联） | 转基类构造 | ★★ |
| `virtual void Draw(Graphics*)` | GameButton.cpp:296 | 用 `DrawStoneButton` 绘制 | ★★ |
| `void SetLabel(const SexyString&)` | GameButton.cpp:290 | 翻译设置文本 | ★★ |

**NewLawnButton（新版按钮）** — `: public Sexy::DialogButton`

| 成员 | 类型 | 说明 |
| --- | --- | --- |
| `mHiliteFont` | `Sexy::Font*` | 高亮字体 |
| `mTextDownOffsetX/Y` | `int` | 按下文字位移 |
| `mButtonOffsetX/Y` | `int` | 贴图位移 |
| `mUsePolygonShape` | `bool` | 使用多边形命中区 |
| `mPolygonShape[4]` | `Sexy::SexyVector2[4]` | 命中多边形 |

| 签名 | 文件:行号 | 功能 | 复用 |
| --- | --- | --- | --- |
| `NewLawnButton(Image*,int,ButtonListener*)` | GameButton.cpp:320 | 构造，初始化成员 | ★★ |
| `virtual ~NewLawnButton()` | GameButton.cpp:331 | 析构（字体释放被注释） | ★★ |
| `virtual void Draw(Graphics*)` | GameButton.cpp:338 | 主绘制（支持 colorize、4 态、高亮字体） | ★★ |
| `virtual bool IsPointVisible(int,int)` | GameButton.cpp:406 | 命中检测（支持多边形） | ★★ |
| `void SetLabel(const SexyString&)` | GameButton.cpp:284 | 翻译设置文本 | ★★ |

---

### 2.5 CursorObject（光标/拖拽对象）

- **继承关系**: `class CursorObject : public GameObject`
- **职责**: 关卡中跟随鼠标的对象，按 `CursorType` 绘制不同内容（植物拖拽、铲子、洒水壶、推车等）。

| 成员 | 类型 | 说明 |
| --- | --- | --- |
| `mSeedBankIndex` | `int` | 卡槽索引 |
| `mType` / `mImitaterType` | `SeedType` | 种子类型 / 模仿者类型 |
| `mCursorType` | `CursorType` | 光标种类（见 §3） |
| `mCoinID` | `CoinID` | 硬币 ID |
| `mGlovePlantID/mDuplicatorPlantID/mCobCannonPlantID` | `PlantID` | 手套/复制器/玉米炮关联植物 ID |
| `mHammerDownCounter` | `int` | 锤子按下计数 |
| `mReanimCursorID` | `ReanimationID` | 光标动画 ID（打地鼠锤子） |

| 签名 | 文件:行号 | 功能 | 复用 |
| --- | --- | --- | --- |
| `CursorObject()` | CursorObject.cpp:14 | 构造；打地鼠关卡预建锤子动画 | ★ |
| `void Update()` | CursorObject.cpp:43 | 非游玩场景/鼠标离开则隐藏，否则跟随鼠标 | ★ |
| `void Draw(Graphics*)` | CursorObject.cpp:74 | 按 `mCursorType` 分派绘制 | ★ |
| `void Die()` | CursorObject.cpp:68 | 移除光标动画 | ★ |

**CursorPreview（落点预览）** — `: public GameObject`

| 成员 | 类型 | 说明 |
| --- | --- | --- |
| `mGridX/mGridY` | `int` | 预览网格坐标 |

| 签名 | 文件:行号 | 功能 | 复用 |
| --- | --- | --- | --- |
| `CursorPreview()` | CursorObject.cpp:220 | 构造，初始隐藏 | ★ |
| `void Update()` | CursorObject.cpp:232 | 计算网格落点，仅在可种植时显示 | ★ |
| `void Draw(Graphics*)` | CursorObject.cpp:273 | 半透明绘制植物/盆栽预览；柱形关卡绘制整列 | ★ |

---

### 2.6 MessageWidget（消息/字幕控件）

- **继承关系**: 无基类。
- **职责**: 关卡内提示文字，支持多种 `MessageStyle`（教程/提示/巨大波次/老虎机/房屋名），可逐字 Reanimation 动画。

| 成员 | 类型 | 说明 |
| --- | --- | --- |
| `mApp` | `LawnApp*` | App 指针 |
| `mLabel[MAX_MESSAGE_LENGTH]` | `SexyChar[128]` | 当前文本 |
| `mDisplayTime` / `mDuration` | `int` | 显示总时长 / 剩余时长 |
| `mMessageStyle` | `MessageStyle` | 当前样式（见 §3） |
| `mTextReanimID[MAX_MESSAGE_LENGTH]` | `ReanimationID[128]` | 每个字的动画 ID |
| `mReanimType` | `ReanimationType` | 文字动画类型（如 `REANIM_TEXT_FADE_ON`） |
| `mSlideOffTime` | `int` | 滑出时间 |
| `mLabelNext[MAX_MESSAGE_LENGTH]` / `mMessageStyleNext` | 缓冲 | 排队中的下一条消息 |

| 签名 | 文件:行号 | 功能 | 复用 |
| --- | --- | --- | --- |
| `MessageWidget(LawnApp*)` | MessageWidget.cpp:11 | 构造 | ★ |
| `~MessageWidget()` | MessageWidget.cpp:26 | 清理动画 | ★ |
| `void SetLabel(const SexyString&,MessageStyle)` | MessageWidget.cpp:58 | 设置文本，按样式定显示时长，必要时排队下一条 | ★★ |
| `void ClearReanim()` | MessageWidget.cpp:32 | 清除所有文字动画 | ★ |
| `void ClearLabel()` | MessageWidget.cpp:45 | 缩短时长以快速退场 | ★ |
| `void LayoutReanimText()` | MessageWidget.cpp:131 | 为每个字符创建进入动画并排版 | ★ |
| `void Update()` | MessageWidget.cpp:181 | 倒计时、下一条切换、逐字动画速率/退场 | ★ |
| `void Draw(Graphics*)` | MessageWidget.cpp:307 | 按样式定位并绘制（背景条/描边/淡入淡出/波浪） | ★ |
| `void DrawReanimatedText(...)` | MessageWidget.cpp:238 | 逐字矩阵绘制动画文本 | ★ |
| `Sexy::Font* GetFont()` | MessageWidget.cpp:277 | 按样式选择字体 | ★ |
| `bool IsBeingDisplayed()` | MessageWidget.cpp:466 | 是否正在显示 | ★★ |

---

### 2.7 ToolTipWidget（提示框控件）

- **继承关系**: 无基类。
- **职责**: 悬浮提示框：标题 + 可选红色警告 + 自动换行正文，自动计算尺寸并约束在屏幕内。

| 成员 | 类型 | 说明 |
| --- | --- | --- |
| `mTitle` / `mLabel` / `mWarningText` | `SexyString` | 标题 / 正文 / 警告文本 |
| `mX/mY/mWidth/mHeight` | `int` | 位置与尺寸 |
| `mVisible` / `mCenter` | `bool` | 可见 / 水平居中 |
| `mMinLeft` / `mMaxBottom` | `int` | 屏幕边界约束 |
| `mGetsLinesWidth` | `int` | 换行宽度 |
| `mWarningFlashCounter` | `int` | 警告闪烁计数 |

| 签名 | 文件:行号 | 功能 | 复用 |
| --- | --- | --- | --- |
| `ToolTipWidget()` | ToolTipWidget.cpp:10 | 构造，默认 `mVisible=1`、`mMaxBottom=BOARD_HEIGHT` | ★★ |
| `void GetLines(std::vector<SexyString>&)` | ToolTipWidget.cpp:25 | 按宽度/空格/换行拆分文本 | ★★★ |
| `void CalculateSize()` | ToolTipWidget.cpp:65 | 依据字体度量计算宽高 | ★★ |
| `void SetLabel/SetTitle/SetWarningText(...)` | ToolTipWidget.cpp:98 / 105 / 112 | 翻译设置文本并重算尺寸 | ★★★ |
| `void Draw(Graphics*)` | ToolTipWidget.cpp:119 | 绘制背景/边框/标题/警告/正文（含边界修正与闪烁） | ★★ |
| `void FlashWarning()` | ToolTipWidget.cpp:192 | 触发警告闪烁(70 帧) | ★★ |
| `void Update()` | ToolTipWidget.cpp:197 | 闪烁计数递减 | ★★ |
| `void SetPosition(int,int)` | ToolTipWidget.cpp:203 | 设置坐标 | ★★★ |

---

### 2.8 PoolEffect（泳池水波效果）

- **继承关系**: 无基类。
- **职责**: 泳池/屋顶水面的焦散与波动渲染。预生成 128×64 焦散位图，每帧用双线性插值更新 alpha 并叠加到 3D 网格，实现水面折射闪烁。

| 成员 | 类型 | 说明 |
| --- | --- | --- |
| `mCausticGrayscaleImage` | `unsigned char*` | 256×256 焦散灰度源图 |
| `mCausticImage` | `Sexy::MemoryImage*` | 128×64 焦散目标位图 |
| `mApp` | `LawnApp*` | App 指针 |
| `mPoolCounter` | `int` | 波动相位计数器 |

| 签名 | 文件:行号 | 功能 | 复用 |
| --- | --- | --- | --- |
| `void PoolEffectInitialize()` | PoolEffect.cpp:15 | 分配焦散位图，从 `IMAGE_POOL_CAUSTIC_EFFECT` 拷贝灰度图 | ★ |
| `void PoolEffectDispose()` | PoolEffect.cpp:44 | 释放位图 | ★ |
| `void PoolEffectDraw(Graphics*,bool)` | PoolEffect.cpp:134 | 主绘制：软渲染画静态池面；3D 下构建 3 层三角网格并用 `DrawTrianglesTex` 扭曲叠加焦散 | ★ |
| `void UpdateWaterEffect(Graphics*)` | PoolEffect.cpp:92 | 逐像素双线性采样更新焦散 alpha | ★ |
| `unsigned char BilinearLookupFixedPoint(uint,uint)` | PoolEffect.cpp:51 | 定点数双线性采样（8.8/16.16） | ★★★ |
| `unsigned char BilinearLookup(float,float)` | PoolEffect.cpp:71 | 浮点双线性采样 | ★★★ |
| `void PoolEffectUpdate()` | PoolEffect.cpp:262 | 相位计数器自增 | ★★ |

---

### 2.9 ImitaterDialog（模仿者对话框）

- **继承关系**: `class ImitaterDialog : public LawnDialog`
- **职责**: 点击模仿者卡槽后弹出的植物选择框，让玩家指定模仿对象。

| 成员 | 类型 | 说明 |
| --- | --- | --- |
| `mToolTip` | `ToolTipWidget*` | 提示框（复用 §2.7） |
| `mToolTipSeed` | `SeedType` | 当前提示的种子 |

| 签名 | 文件:行号 | 功能 | 复用 |
| --- | --- | --- | --- |
| `ImitaterDialog()` | ImitaterDialog.cpp:15 | 构造，隐藏确定/取消按钮 | ★ |
| `virtual ~ImitaterDialog()` | ImitaterDialog.cpp:29 | 释放提示框 | ★ |
| `SeedType SeedHitTest(int,int)` | ImitaterDialog.cpp:35 | 命中检测返回种子类型 | ★ |
| `void UpdateCursor()` | ImitaterDialog.cpp:53 | 悬停种子时改手型光标 | ★ |
| `virtual void Update()` | ImitaterDialog.cpp:67 | 更新提示框与光标 | ★ |
| `void GetSeedPosition(int,int&,int&)` | ImitaterDialog.cpp:75 | 按索引计算种子格坐标（8 列网格） | ★ |
| `virtual void Draw(Graphics*)` | ImitaterDialog.cpp:82 | 绘制所有可模仿种子卡（灰显不可选）+ 提示框 | ★ |
| `void ShowToolTip()` | ImitaterDialog.cpp:100 | 生成提示（不可选/不推荐警告 + 名称/说明） | ★ |
| `void RemoveToolTip()` | ImitaterDialog.cpp:148 | 隐藏提示框 | ★ |
| `virtual void MouseDown(int,int,int)` | ImitaterDialog.cpp:155 | 点击选定模仿对象，写入 `ChosenSeed` 并关闭 | ★ |
| `virtual void MouseUp(int,int,int)` | ImitaterDialog.cpp:179 | 空 | ★ |

---

### 2.10 TypingCheck（作弊码匹配器，支撑类）

- **继承关系**: 无基类。
- **职责**: 滚动窗口记录最近按键（KeyCode），与目标短语比较；Konami 码用方向键，其余用字符。

| 成员 | 类型 | 说明 |
| --- | --- | --- |
| `mPhrase` | `std::string` | 目标序列（存储为 KeyCode 字节） |
| `mRecentTyping` | `std::string` | 最近按键窗口 |

| 签名 | 文件:行号 | 功能 | 复用 |
| --- | --- | --- | --- |
| `TypingCheck()` / `TypingCheck(const std::string&)` | TypingCheck.cpp:5 / 10 | 构造 | ★★★ |
| `void SetPhrase(const std::string&)` | TypingCheck.cpp:16 | 逐字符转 KeyCode 写入短语 | ★★★ |
| `void AddKeyCode(KeyCode)` | TypingCheck.cpp:22 | 追加一个 KeyCode | ★★★ |
| `void AddChar(char)` | TypingCheck.cpp:28 | 小写化后按名字转 KeyCode | ★★★ |
| `bool Check()` | TypingCheck.cpp:35 | 比较命中即清空并返回 true | ★★★ |
| `bool Check(KeyCode)` | TypingCheck.cpp:46 | 追加按键、裁剪到短语长度、比较 | ★★★ |

---

## 3. 枚举

### GameScenes（场景状态，LawnApp.h:59）
```
SCENE_LOADING, SCENE_MENU, SCENE_LEVEL_INTRO, SCENE_PLAYING,
SCENE_ZOMBIES_WON, SCENE_AWARD, SCENE_CREDIT, SCENE_CHALLENGE
```
由 `mGameScene` 持有，`LawnGetCurrentLevelName()` 据此汇报当前场景名。

### BoardResult（单局结果，LawnApp.h:71）
```
BOARDRESULT_NONE / WON / LOST / RESTART / QUIT / QUIT_APP / CHEAT
```

### CrazyDaveState（戴夫状态机，LawnApp.h:82）
```
CRAZY_DAVE_OFF / ENTERING / LEAVING / IDLING / TALKING / HANDING_TALKING / HANDING_IDLING
```

### TrialType（试玩类型，LawnApp.h:93）
```
TRIALTYPE_NONE, TRIALTYPE_STAGELOCKED
```

### GameMode（游戏模式，LawnCommon.h:17）
```
GAMEMODE_ADVENTURE,
GAMEMODE_SURVIVAL_NORMAL_STAGE_1..5, HARD_STAGE_1..5, ENDLESS_STAGE_1..5,
GAMEMODE_CHALLENGE_WAR_AND_PEAS, WALLNUT_BOWLING, SLOT_MACHINE, RAINING_SEEDS,
BEGHOULED, INVISIGHOUL, SEEING_STARS, ZOMBIQUARIUM, BEGHOULED_TWIST, LITTLE_TROUBLE,
PORTAL_COMBAT, COLUMN, BOBSLED_BONANZA, SPEED, WHACK_A_ZOMBIE, LAST_STAND,
WAR_AND_PEAS_2, WALLNUT_BOWLING_2, POGO_PARTY, FINAL_BOSS,
ART_CHALLENGE_WALLNUT, SUNNY_DAY, RESODDED, BIG_TIME, ART_CHALLENGE_SUNFLOWER,
AIR_RAID, ICE, ZEN_GARDEN, HIGH_GRAVITY, GRAVE_DANGER, SHOVEL, STORMY_NIGHT,
BUNGEE_BLITZ, SQUIRREL, TREE_OF_WISDOM,
GAMEMODE_SCARY_POTTER_1..9, SCARY_POTTER_ENDLESS,
GAMEMODE_PUZZLE_I_ZOMBIE_1..9, PUZZLE_I_ZOMBIE_ENDLESS,
GAMEMODE_UPSELL, GAMEMODE_INTRO, NUM_GAME_MODES
```
常量：`NUM_CHALLENGE_MODES = NUM_GAME_MODES - 1`、`ADVENTURE_AREAS = 5`、`LEVELS_PER_AREA = 10`、`NUM_LEVELS = 50`、`FINAL_LEVEL = 50`。

### FoleyType（音效枚举，LawnApp.h:374）
约 120 项（`FOLEY_SUN` … `NUM_FOLEY`），对应 `gLawnFoleyParamArray` 的 `FoleyParams`（音量/采样列表/随机权值）。

### CursorType（光标种类，CursorObject.h:11）
```
CURSOR_TYPE_NORMAL, PLANT_FROM_BANK, PLANT_FROM_USABLE_COIN, PLANT_FROM_GLOVE,
PLANT_FROM_DUPLICATOR, PLANT_FROM_WHEEL_BARROW, SHOVEL, HAMMER, COBCANNON_TARGET,
WATERING_CAN, FERTILIZER, BUG_SPRAY, PHONOGRAPH, CHOCOLATE, GLOVE, MONEY_SIGN,
WHEEELBARROW, TREE_FOOD
```
（注意原代码 `WHEEELBARROW` 三 e 拼写。）

### MessageStyle（消息样式，MessageWidget.h:18）
```
MESSAGE_STYLE_OFF, TUTORIAL_LEVEL1, TUTORIAL_LEVEL1_STAY, TUTORIAL_LEVEL2,
TUTORIAL_LATER, TUTORIAL_LATER_STAY, HINT_LONG, HINT_FAST, HINT_STAY,
HINT_TALL_FAST, HINT_TALL_UNLOCKMESSAGE, HINT_TALL_LONG, BIG_MIDDLE, BIG_MIDDLE_FAST,
HOUSE_NAME, HUGE_WAVE, SLOT_MACHINE, ZEN_GARDEN_LONG
```

### GameButton 内部枚举（GameButton.h:12）
- 对齐：`BUTTON_LABEL_LEFT=-1 / CENTER=0 / RIGHT=1`
- 颜色槽：`COLOR_LABEL / COLOR_LABEL_HILITE / COLOR_DARK_OUTLINE / COLOR_LIGHT_OUTLINE / COLOR_MEDIUM_OUTLINE / COLOR_BKG / NUM_COLORS`

### 常量（LawnCommon.h）
- `MAX_GRID_SIZE_X=9`、`MAX_GRID_SIZE_Y=6`（棋盘网格）
- `MAX_MESSAGE_LENGTH=128`、`MAX_REANIM_LINES=5`（MessageWidget.h）

---

## 4. 全局函数与变量

### 全局变量

| 符号 | 类型 | 定义处 | 说明 |
| --- | --- | --- | --- |
| `gIsPartnerBuild` | `bool` | LawnApp.cpp:38 | 是否合作伙伴构建 |
| `gSlowMo` / `gFastMo` | `bool` | LawnApp.cpp:39-40 | 慢放/快放开关 |
| `gLawnApp` | `LawnApp*` | LawnApp.cpp:41 | 全局单例指针 |
| `gSlowMoCounter` | `int` | LawnApp.cpp:42 | 慢放计数器 |
| `gLawnFoleyParamArray[NUM_FOLEY]` | `FoleyParams[]` | LawnApp.cpp:44 | 全部音效参数表（0x69FAD0） |
| `gLawnParticleArray[NUM_PARTICLES]` | `ParticleParams[]` | LawnApp.cpp:151 | 粒子定义表（0x6A0FF0） |
| `gLawnReanimationArray[NUM_REANIMS]` | `ReanimationParams[]` | LawnApp.cpp:260 | 动画定义表（0x6A1340） |
| `gLawnTrailArray[NUM_TRAILS]` | `TrailParams[]` | LawnApp.cpp:406 | 拖尾定义表（0x6A19F4） |
| `gLawnStringFormatCount` / `gLawnStringFormats[12]` | `int` / `TodStringListFormat[]` | LawnApp.cpp:410-411 | 字符串富文本格式表（0x6A5010） |
| `gGameButtonColors[][3]` | `static int[][]` | GameButton.cpp:9 | 按钮 6 色表 |
| `gLawnEditWidgetColors[][4]` | `static int[][]` | LawnCommon.cpp:12 | 输入框颜色表 |

### 全局函数

| 签名 | 文件:行号 | 功能 | 复用 |
| --- | --- | --- | --- |
| `int WINAPI WinMain(...)` | LawnApp.cpp:442 | 程序入口：设置字符串颜色/回调→new LawnApp→Init→Start→Shutdown | ★ |
| `bool LawnGetCloseRequest()` | LawnApp.cpp:427 | 供框架查询退出请求 | ★ |
| `bool LawnHasUsedCheatKeys()` | LawnApp.cpp:436 | 是否用过作弊键 | ★ |
| `void BetaSubmitFunc()` | LawnApp.cpp:1578 | Beta 上报回调 | ★ |
| `SexyString LawnGetCurrentLevelName()` | LawnApp.cpp:3719 | 返回当前关卡/场景名（挂在 `gGetCurrentLevelName`） | ★ |
| `bool ModInRange(int,int,int)` | LawnCommon.cpp:21 | 判断 `[n-r,n+r]` 内是否有 `mod` 的倍数 | ★★★ |
| `bool GridInRange(...)` | LawnCommon.cpp:30 | 判断点是否在另一点邻域 | ★★★ |
| `void TileImageHorizontally/Vertically(...)` | LawnCommon.cpp:35 / 46 | 图片水平/垂直平铺 | ★★★ |
| `LawnEditWidget* CreateEditWidget(...)` | LawnCommon.cpp:89 | 创建标准输入框 | ★★ |
| `void DrawEditBox(Graphics*,EditWidget*)` | LawnCommon.cpp:99 | 绘制输入框背景 | ★★ |
| `Sexy::Checkbox* MakeNewCheckbox(...)` | LawnCommon.cpp:106 | 创建标准复选框 | ★★ |
| `std::string GetSavedGameName(GameMode,int)` | LawnCommon.cpp:117 | 存档文件路径 | ★★ |
| `int GetCurrentDaysSince2000()` | LawnCommon.cpp:123 | 距 2000 年天数 | ★★★ |
| `void DrawStoneButton(Graphics*,int,int,int,int,bool,bool,const SexyString&)` | GameButton.cpp:19 | 绘制石质按钮（左中右三段 + 文字） | ★★ |
| `LawnStoneButton* MakeButton(...)` | GameButton.cpp:306 | 工厂：标准石质按钮 | ★★ |
| `NewLawnButton* MakeNewButton(...)` | GameButton.cpp:415 | 工厂：新版按钮（贴图+字体） | ★★ |

---

## 5. 关键流程

### 5.1 启动总流程

```
WinMain
 ├─ TodStringListSetColors(gLawnStringFormats)
 ├─ 挂接回调: gGetCurrentLevelName / gAppCloseRequest / gAppHasUsedCheatKeys / gExtractResourcesByName
 ├─ gLawnApp = new LawnApp()          // 构造函数(463)初始化所有状态
 ├─ gLawnApp->Init()                  // (1587)
 │    ├─ DoParseCmdLine()             // 处理 -tod 等
 │    ├─ SexyApp::Init()
 │    ├─ ParseResourcesFile("properties\\resources.xml")
 │    ├─ TodLoadResources("Init")     // 首屏最小资源
 │    ├─ mProfileMgr->Load()          // 读 users.dat，选当前玩家
 │    ├─ new TitleScreen / Music / TodFoley / EffectSystem
 │    ├─ 注册 10 个 TypingCheck 作弊码
 │    └─ ReanimatorLoadDefinitions(gLawnReanimationArray) + 预载加载条动画
 ├─ gLawnApp->Start()                 // (1711) 进入 SexyAppBase::Start 消息循环
 └─ gLawnApp->Shutdown()              // (645) 退出清理
```

### 5.2 InitHook 与 LoadingThreadProc 资源加载管线

**InitHook**(3681) 在框架初始化阶段被调用：Release 构建下创建 `PopDRMComm` 并 `Connect()`；若 `properties` 里 `MarketingMode == "StageLocked"`，则 `mTrialType = TRIALTYPE_STAGELOCKED` 且 `mDRM->EnableLocking()`，用于试玩阶段锁定（`IsTrialStageLocked` 据此拦截辣椒之后的植物、禅境花园等）。Debug 下 `mDRM` 置空以跳过 DRM。

**LoadingThreadProc**(2057) 是后台资源加载主流程（配合 `LoadGroup`）：

1. `TodLoadResources("LoaderBar")` 载入加载条资源；
2. `TodStringListLoad("Properties\\LawnStrings.txt")` 载入文本表；
3. 置 `mTitleScreen->mLoaderScreenIsLoaded = true`；
4. 计算 `mNumLoadingThreadTasks`（各资源组资源数 × 平均耗时权重 + 636 + 预载任务 + 音乐任务）；
5. `LoadGroup("LoadingImages", 9)` → `LoadGroup("LoadingFonts", 54)`（`LoadGroup` 内部 `StartLoadResources` + 循环 `TodLoadNextResource()`，出错则 `ShowResourceError` 并置 `mLoadingFailed`）；
6. `mMusic->MusicInit()`；
7. 建 `PoolEffect`/`ZenGarden`/`ReanimatorCache` 并初始化，`TodFoleyInitialize`；
8. `TrailLoadDefinitions` → `TodParticleLoadDefinitions`；
9. `PreloadForUser()`（按玩家已解锁的植物/僵尸逐个 `Plant::PreloadPlantResources` / `Zombie::PreloadZombieResources`，支持标题画面按键取消）；
10. `LoadGroup("LoadingSounds", 54)`。

加载完成后框架回调 `LoadingCompleted`(2147)：移除 TitleScreen → `ShowGameSelector()`。

> “Preload/Preserve/Kill 系列”说明：本工程**没有独立的 Preserve 接口**；场景/资源生命周期由三部分构成——①`PreloadForUser`/`GetNumPreloadingTasks`/`LoadGroup` 负责**预载**；②各 `Show*` 通过 `mWidgetManager->AddWidget + BringToBack + SetFocus` **装配**界面；③各 `Kill*`（`KillBoard/KillGameSelector/KillAwardScreen/...`）通过 `RemoveWidget + SafeDeleteWidget` **销毁**界面。`mResourceManager->DeleteImage`（如 `LoadingCompleted` 删除 `IMAGE_TITLESCREEN`）负责**释放**不再需要的资源。

### 5.3 场景切换机制

- 单一活动场景由 `mGameScene`（`GameScenes`）标注，界面实体由 `mWidgetManager` 统一管理，各界面以 `AddWidget/BringToBack/SetFocus` 进入、以 `RemoveWidget/SafeDeleteWidget` 退出。
- 进入关卡链：`PreNewGame(mode, lookForSave)` →（有存档 `TryLoadGame` 否则 `NewGame`）→ `NewGame` = `MakeNewBoard + InitLevel + SCENE_LEVEL_INTRO + ShowSeedChooserScreen + StartLevelIntro`。
- 开始游戏：`StartPlaying` = `KillSeedChooserScreen + Board::StartLevel + SCENE_PLAYING`。
- 关卡结算：每帧 `UpdateFrames` 末尾 `CheckForGameEnd`(1827) → 先 `UpdatePlayerProfileForFinishingLevel`(1742) 更新档案，再按模式分发：
  - 冒险：通关/大关里程碑 → `ShowAwardScreen`，否则 `PreNewGame` 下一关；
  - 生存：最终波 → 解锁则 `ShowAwardScreen` 否则 `ShowChallengeScreen`；非最终波 → `mSurvivalStage++` 继续；
  - 益智/挑战：解锁 → `ShowAwardScreen`，否则回 `ShowChallengeScreen`。
- 返回主菜单：`DoBackToMain` = 停音乐/音效 → `WriteCurrentUserConfig` → `KillNewOptionsDialog` → `KillBoard` → `ShowGameSelector`。

### 5.4 作弊码系统（TypingCheck/CheatKeys）

**注册**（`Init`，1666-1685）：构造 10 个 `TypingCheck`：
- `mKonamiCheck`：方向键序列 ↑↑↓↓←→←→ + `b` + `a`（Konami 码）；
- `mMustacheCheck`("mustache") / `mMoustacheCheck`("moustache")；
- `mSuperMowerCheck`("trickedout") / `mSuperMowerCheck2`("tricked out")；
- `mFutureCheck`("future")、`mPinataCheck`("pinata")、`mDanceCheck`("dance")、`mDaisyCheck`("daisies")、`mSukhbirCheck`("sukhbir")。

**匹配器**（`TypingCheck`，见 §2.10）：`AddChar` 将字符转 KeyCode 存入 `mPhrase`；`Check(KeyCode)` 把当前按键追加到 `mRecentTyping` 并裁剪到短语长度后与 `mPhrase` 比较，命中即清空并返回 true。

**触发点**（不在 LawnApp.cpp，而在各输入入口）：
- `Board::DoTypingCheck`(Board.cpp:7772) / `Board::KeyDown`(7853)：关卡内判定；
- `GameSelector::KeyDown`(GameSelector.cpp:929)：主菜单判定；
- `LawnDialog::KeyDown` / `SeedChooserScreen` / `NewOptionsDialog` 均转发到 `Board::DoTypingCheck`。

**效果**：`mustache`→`mMustacheMode`、`trickedout`→`mSuperMowerMode`、`future`→`mFutureMode`、`sukhbir`→`mSukhbirMode` 直接反转；`pinata`/`dance`/`daisies` 需 `CanDoPinataMode/DanceMode/DaisyMode`（智慧树记录 ≥1000/500/100）否则蜂鸣提示 `[CANT_USE_CODE]`；Konami 码仅播放 `FOLEY_DROP` 音效。

**调试作弊键**（`mTodCheatKeys`，`-tod` 参数，Debug 专用）：解锁 `ChallengeScreen` 直接进入、`CutScene` 跳过、`GridItem` Shift 加速、`TitleScreen` 快速加载（'T'）等；使用后 `mPlayerInfo->mHasUsedCheatKeys` 被标记（`UpdatePlayTimeStats`/`LawnHasUsedCheatKeys`）。

### 5.5 注册 / 试玩 DRM

- `InitHook`(3681) 创建 `PopDRMComm` 并连接；`MarketingMode=="StageLocked"` → `TRIALTYPE_STAGELOCKED` + `EnableLocking`。
- `IsTrialStageLocked`(3669)：`mDebugTrialLocked` 或（DRM 无数据且 `mTrialType==STAGELOCKED`）→ 锁定。
- 锁定影响：`HasSeedType`(2712) 拦截 `SEED_JALAPENO` 之后的植物、`CanShowZenGarden`(2872) 直接 false。
- 面向用户的注册流程（`DoRegister/DoRegisterError/CanDoRegisterDialog/NeedRegister/DoNeedRegisterDialog/UpdateRegisterInfo`、`IsRegistered/IsExpired/IsDRMConnected`）在当前反编译版中**均为空壳或恒返回 false**，实际注册逻辑已移除。

### 5.6 命令行参数

`HandleCmdLineParam`(1726)：识别 `-tod`（Debug 下 `mTodCheatKeys=true`、`mDebugKeysEnabled=true`）；其余参数转发 `SexyApp::HandleCmdLineParam`。由 `Init` 中的 `DoParseCmdLine()` 触发解析。

---

## 6. 可复用性评估

### 高可复用（★★★，自包含、无/极少游戏耦合）
- `TypingCheck` 全套：按键序列匹配器，逻辑独立，可直接抽作通用工具。
- `ModInRange` / `GridInRange` / `TileImageHorizontally` / `TileImageVertically`：纯算法/绘制小工具。
- `PoolEffect::BilinearLookupFixedPoint` / `BilinearLookup`：定点/浮点双线性采样，通用图像算法。
- `Pluralize` / `GetMoneyString` / `GetStageString` / `GetCurrentDaysSince2000`：字符串/日期格式化。
- `CenterDialog`、`ToolTipWidget::GetLines`/`SetLabel`/`SetPosition`、`GameButton::Resize`/`SetDisabled`/`SetLabel`/`HaveButtonImage`。

### 中可复用（★★，依赖框架但逻辑清晰、可移植）
- `GameButton` / `LawnStoneButton` / `NewLawnButton` / `DrawStoneButton` / `MakeButton` / `MakeNewButton`：依赖 `Sexy::Graphics/Image/Font`，去掉渲染层后可移植为通用 UI 按钮。
- `ToolTipWidget` 整体：依赖字体度量与 `Graphics`，提示框逻辑通用。
- `MessageWidget`：依赖 `Reanimation` 逐字动画，换静态文本后可通用。
- `CursorObject` / `CursorPreview`：依赖 `Board/Plant/ZenGarden` 与 `GameObject`，光标对象框架可参考。
- `LoadGroup`（资源组加载循环）、`MakeNewCheckbox` / `CreateEditWidget` / `DrawEditBox`：工厂化控件创建。

### 低可复用（★，强耦合游戏状态/业务）
- `LawnApp` 绝大部分成员函数（场景管理、结算、档案、Crazy Dave、DRM、`IsXxxLevel` 系列）：深度耦合 `Board/GameMode/PlayerInfo/ChallengeDefinition`，仅可作同类“App 主控”的架构参考。
- `PoolEffect::PoolEffectDraw`（3D 顶点扭曲 + 焦散叠加）虽算法通用，但强依赖 `D3DInterface/DrawTrianglesTex` 与游戏贴图，直接复用成本高。
- `ImitaterDialog`、`LawnEditWidget::KeyDown`（依赖 `SeedChooserScreen/Plant`）。

### 架构参考要点
1. **资源加载分层**：`Init` 只载最小资源 → 后台线程 `LoadingThreadProc` 按组加权加载 → `PreloadForUser` 按玩家进度增量预载，进度条用任务数加权而非资源数。
2. **场景即 Widget**：所有界面统一挂在 `mWidgetManager`，`Show*`/`Kill*` 成对，`mGameScene` 仅作状态标注，切换逻辑集中在 `CheckForGameEnd` 一类调度函数。
3. **作弊码解耦**：`TypingCheck` 序列匹配器与具体效果分离，注册在 `LawnApp::Init`，判定分散在 `Board/GameSelector` 等输入入口，模式开关以 `bool` 字段承载。
4. **DRM 抽象**：`InitHook` 集中初始化，`IsTrialStageLocked` 作为统一闸口供各功能查询，业务层不直接触碰 `PopDRMComm`。

---

## 附：文件与函数统计

| 文件 | 行数 | 说明 |
| --- | --- | --- |
| LawnApp.h | 491 | 主类声明 + GameScenes/BoardResult/CrazyDaveState/TrialType/FoleyType 枚举 |
| LawnApp.cpp | 4191（135253 字节） | 主类实现 + 4 张全局资源表 + WinMain |
| LawnCommon.h / .cpp | 143 / 131 | GameMode 枚举、LawnEditWidget、小工具 |
| GameButton.h / .cpp | 114 / 432 | 三类按钮 + 工厂/绘制函数 |
| CursorObject.h / .cpp | 68 / 337 | CursorObject + CursorPreview |
| MessageWidget.h / .cpp | 70 / 469 | MessageWidget |
| ToolTipWidget.h / .cpp | 41 / 207 | ToolTipWidget |
| PoolEffect.h / .cpp | 29 / 265 | PoolEffect |
| ImitaterDialog.h / .cpp | 28 / 181 | ImitaterDialog |
| TypingCheck.h / .cpp（支撑） | 24 / 57 | 作弊码匹配器 |

成员函数（`.cpp` 内定义，含构造/析构）约 **248** 个；全局函数 **17** 个；合计约 **265** 个函数实现（不含 `.h` 内联函数）。
