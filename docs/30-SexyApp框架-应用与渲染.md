# SexyAppFramework：应用与渲染核心 模块文档

> 所属: `src/SexyAppFramework`
>
> 本文档分析 PopCap 自研 C++ 游戏框架 **SexyApp Framework** 的「应用与渲染核心」部分：应用主循环与事件泵、控件树系统、字体系统、渲染管线（画布/图像/贴图/设备分层）与资源管理器。所有源文件为 **GBK(代码页936)** 编码；文中行号均来自真实源码（grep + pwsh 936 编码读取）。

## 1. 模块职责与整体分层

SexyAppFramework 是《植物大战僵尸》(Plants vs. Zombies) 使用的 PopCap 自研 2D 游戏框架，技术底座为 **Win32 + DirectDraw7 / Direct3D7 + GDI**。它向上提供「窗口 + 主循环 + 控件树 + 资源 + 2D 渲染」一整套引擎能力，向下封装 Windows 消息、DirectDraw/Direct3D 与 GDI。本模块（应用与渲染核心）是其中的枢纽，按职责分 6 层：

```
┌────────────────────────────────────────────────────────────────┐
│ 应用层    SexyApp ── SexyAppBase                                 │
│           (主循环 DoMainLoop/Process、消息泵、时钟、加载线程)    │
├────────────────────────────────────────────────────────────────┤
│ UI 控件层 WidgetManager ── WidgetContainer ── Widget            │
│           └─ Dialog / DialogButton / ButtonWidget / Checkbox /  │
│              EditWidget / ListWidget / ScrollbarWidget /        │
│              ScrollbuttonWidget / Slider / TextWidget /         │
│              HyperlinkWidget / CursorWidget (+ 各 Listener)     │
│ 字体层    Font ──┬─ SysFont   (GDI TrueType 现画)              │
│                 └─ ImageFont (图集 + .txt 描述, 多图层)         │
├────────────────────────────────────────────────────────────────┤
│ 渲染层    Graphics (画布/状态机, 只转发到 mDestImage)           │
│           Image(抽象) ── MemoryImage(软件) ── DDImage(贴图)     │
│           设备: DDInterface ── D3DInterface / D3D8Helper        │
│           基础: Color / Rect / Point / Insets / SexyMatrix /    │
│                 SexyVector / Ratio / TriVertex / SharedImage    │
├────────────────────────────────────────────────────────────────┤
│ 资源层    ResourceManager (XML 清单) ── SexyAppBase::GetSharedImage
│           └─ p_fopen → PakInterface (src/PakLib, 独立模块)      │
└────────────────────────────────────────────────────────────────┘
```

**各层职责**：

- **应用层**（`SexyApp` / `SexyAppBase`）：框架核心。`SexyAppBase` 统一管理主窗口与 DD 设备、Windows 消息泵与输入分发、帧/时钟循环、后台加载线程、资源/声音/光标/属性/注册表与 Demo 录制回放。`SexyApp` 在其上叠加 PopCap 商业层（注册校验、版本更新、Beta 上报、partner.xml）。
- **控件层**（`WidgetManager` / `WidgetContainer` / `Widget` + 控件族）：GUI 控件树。`WidgetContainer` 提供父子列表、定位、Z 序、脏区与模态；`Widget` 提供可见性/焦点/鼠标键盘事件与绘制；`WidgetManager` 是根容器，负责把输入事件按 Z 序路由到具体控件、把控件画进屏幕后备缓冲。
- **字体层**（`Font` / `SysFont` / `ImageFont`）：统一量度与绘制虚接口。`SysFont` 走 GDI 现画系统字；`ImageFont` 用「图集图片 + 描述文件」实现多图层、可缩放、带字距/着色/标签切换的位图字体。
- **渲染层**（`Graphics` / `Image` / `MemoryImage` / `DDImage` / `DDInterface` / `D3DInterface`）：核心是「画布 + 图像 + 设备」三层。`Graphics` 是状态机画布，`Image→MemoryImage→DDImage` 是图像继承链（抽象接口 / 软件参考实现 / DD 表面加速）；设备层由 `DDInterface`（继承 `NativeDisplay`，DirectDraw 设备封装）与 `D3DInterface`（独立类，被 `DDInterface` 组合持有，Direct3D7 渲染器）构成。
- **资源层**（`ResourceManager`）：解析 XML 资源清单，按 id 缓存并惰性加载图像/字体/声音，与 `PakInterface`（`src/PakLib`，独立模块）通过 `p_fopen` 等文件 I/O 抽象衔接。

**关键设计思想**：

1. **Image 三层继承**：`Image`(抽象接口) → `MemoryImage`(软件参考实现) → `DDImage`(DD 表面加速)。所有图像以 `Image*` 互操作，通过虚函数多态让上层（Graphics/控件）与具体像素存储方式解耦。
2. **Graphics 是「轻量画布代理/状态机」**：只负责平移/缩放/裁剪/颜色/字体/混色等状态管理与坐标换算，**不保存像素**，每次绘制都转发到绑定的 `mDestImage`（一个 `MemoryImage` 或 `DDImage`）。
3. **软件/硬件双路径透明切换**：一次贴图要么走 `MemoryImage` 软件算法（ARGB 混合、Bresenham、双线性），要么走 `DDImage` 的 DDSurface 硬件 blit 或 `D3DInterface` 的 3D 纹理；三条路径共用同一 `Image` 虚接口，按 `mIs3D`/`mHasAlpha`/`mVideoMemory` 等标志选择。
4. **逻辑帧与渲染帧分离**：`UpdateFrames` 按固定 `mFrameTime`(10ms) 步长推进逻辑，绘制按脏区按需进行；帧率由 `UpdateFTimeAcc` 累积真实时间 + `Sleep` 控制，欠帧用 `mPendingUpdatesAcc` 补跑。
5. **事件延迟分发**：`WindowProc` 只把输入消息压入 `mDeferredMessages` 队列，主循环在 `UpdateAppStep` 阶段统一转发给 `WidgetManager`，避免在消息回调里直接改控件树（防重入）。
6. **资源惰性加载 + 分组 + 后台线程**：`ResourceManager` 按 XML 清单登记「描述对象」而非实例，按组惰性实例化；加载可放到 `_beginthread` 起的后台线程，完成后用共享 bool 标志 `mLoadingThreadCompleted` 通知主线程收尾。
7. **文件 I/O 抽象**：框架经 `p_fopen/p_fread/...`（`PakInterface.h` 内联）读写文件，运行时这些调用被重定向到全局 `gPakInterface`（内存映射的 `.pak` 归档），实现「源码写磁盘、运行读 pak」的透明切换。

## 2. 应用层（SexyApp / SexyAppBase）

> **重要勘误**：本反编译工程的 `SexyAppBase` 版本与 PopCap 官方框架命名有差异——模板中提到的 `MainLoop`、`MainLoopHook`、`StartApp`、`Update`、`DrawFrame`、`DrawScreen`、`PumpMessage`、`DoMouseDown/DoKeyDown…`、`GetScreenWidth`、`MoveWindow/SetWindowPos` 等字面方法**在本版源码中并不存在**。实际等价物见下表映射，行号均为真实值（grep/pwsh 输出）。

| 模板中的名字 | 本版实际符号 | 位置 |
|---|---|---|
| MainLoop | `DoMainLoop()` | SexyAppBase.cpp:5518 |
| MainLoopHook | 无（旧版 `DoMainLoop` 被注释保留） | SexyAppBase.cpp:5484-5516（注释） |
| StartApp | `Start()`（内部直接进入 `DoMainLoop`） | SexyAppBase.cpp:5624 |
| Update（更新循环） | `Process()` / `UpdateApp()` / `UpdateAppStep()` / `UpdateFrames()` / `DoUpdateFrames()` | 5215 / 5590 / 5528 / 2219 / 2239 |
| DrawFrame / DrawScreen | `DrawDirtyStuff()` / `Redraw()`；真正画 UI 的是 `WidgetManager::DrawScreen()` | 2569 / 2289 / WidgetManager.cpp:398 |
| PumpMessage / MessagePump | `PeekMessage` 循环内联在 `UpdateAppStep()` | 5546-5550 |
| DoMouseDown/Up/Move/Wheel、DoKeyDown/Up/DoCharInput | 事件经 `WindowProc`(3380) 压入 `mDeferredMessages`，由 `ProcessDeferredMessages`(4168) 转发给 `WidgetManager::MouseDown/MouseUp/MouseMove/MouseWheel/KeyDown/KeyUp/KeyChar` | 见 2.2/2.3 |
| GetScreenWidth/Height、MoveWindow/SetWindowPos | 无成员方法；尺寸用成员 `mWidth/mHeight`（h:138/139），窗口操作用 `MakeWindow`(4636)/`SwitchScreenMode`(5012,5057,5062)/`RestoreScreenResolution`(2202) | — |
| LoadingThreadCompletedStub | 实为 `LoadingThreadProcStub`(线程入口桩) + `LoadingThreadCompleted`(完成回调) | 4916 / 4912 |

---

### 2.1 SexyApp 类（SexyApp.h/.cpp）

- **继承关系**：`class SexyApp : public SexyAppBase`（SexyApp.h:12），全局单例 `extern SexyApp* gSexyApp`（SexyApp.h:85，定义于 SexyApp.cpp:18）。
- **职责**：PopCap 产品级上层封装——在 SexyAppBase 之上叠加注册/校验、版本更新检查、Beta 测试支持、崩溃上报（SEH）、`partner.xml` 属性读取与命令行参数（`-version`）等商业化功能。

**关键成员变量表**

| 变量 | 类型 | 说明 |
|---|---|---|
| `mInternetManager` | `InternetManager*` | 联网管理器（本版置 `nullptr` 停用，.cpp:50） |
| `mBetaSupport` | `BetaSupport*` | Beta 测试支持（本版置空，.cpp:594） |
| `mBetaSupportSiteOverride` / `mBetaSupportProdNameOverride` | `std::string` | Beta 上报地址/产品名覆盖 |
| `mReferId` / `mVariation` | `std::string` | 渠道标识 / 版本变体 |
| `mDownloadId` / `mRegSource` | `ulong` / `std::string` | 下载 ID / 注册来源（默认 `"ingame"`） |
| `mLastVerCheckQueryTime` | `ulong` | 上次版本检查时间戳 |
| `mSkipAd` / `mDontUpdate` | `bool` | 跳过广告 / 禁止自动更新 |
| `mBuildNum` / `mBuildDate` | `int` / `std::string` | 构建号/构建日期（从 `DYNAMIC_DATA_BLOCK` 标记解析） |
| `mUserName` / `mRegUserName` / `mRegCode` | `std::string` | 用户名 / 注册名 / 注册码 |
| `mIsRegistered` / `mBuildUnlocked` | `bool` | 是否已注册 / 构建已解锁 |
| `mTimesPlayed` / `mTimesExecuted` | `int` | 游玩次数 / 运行次数 |
| `mTimedOut` | `bool` | 是否超时 |

**主要公共成员函数索引表**

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `SexyApp()` / `~SexyApp()` | SexyApp.cpp:29 / 64 | 初始化全局指针与营销字段、解析构建号；析构释放 mBetaSupport/mInternetManager | ★★ |
| `bool Validate(const std::string&, const std::string&)` | :70 | 注册码校验（原 RSA 大数校验已注释，恒返 true） | ★ |
| `void ReadFromRegistry()` / `WriteToRegistry()` | :118 / :232 | 读写注册表与 `%windir%\popcinfo.dat`（运行/游玩次数等） | ★ |
| `bool OpenHTMLTemplate(const std::string&, const DefinesMap&)` | :305 | 渲染 HTML 模板（宏替换后写临时文件并打开） | ★ |
| `bool OpenRegisterPage(DefinesMap)` / `()` | :348 / :431 | 打开注册页（拼统计串 + 模板或 URL） | ★ |
| `bool ShouldCheckForUpdate()` / `void UpdateCheckQueried()` | :396 / :415 | 判断是否该检查更新（7 天/非正常退出）；更新时间戳 | ★ |
| `bool CheckSignature(const Buffer&, const std::string&)` | :437 | 资源签名校验（Debug/本版直接返回 true） | ★ |
| `void PreTerminate()` | :494 | 退出前钩子（ZYLOM 版弹广告） | ★ |
| `void OpenUpdateURL()` | :507 | 打开更新页后 `Shutdown()` | ★ |
| `void HandleCmdLineParam(...)` | :517 | 处理 `-version`（弹版本框后 `DoExit(0)`），余下转交基类 | ★ |
| `std::string GetGameSEHInfo()` / `void GetSEHWebParams(DefinesMap*)` | :536 / :555 | 崩溃上报信息 / 网页参数 | ★ |
| `void PreDisplayHook()` | :563 | 显示前钩子（Beta 校验，已注释） | ★ |
| `void InitPropertiesHook()` | :573 | 读 `properties\partner.xml`、覆盖标题/窗口化等 | ★ |
| `void Init()` | :602 | 设置 SEH 崩溃文案后调用 `SexyAppBase::Init()`，累加运行次数 | ★ |
| `void UpdateFrames()` | :635 | 追加调用 `SexyAppBase::UpdateFrames()`（联网更新已注释） | ★ |

---

### 2.2 SexyAppBase 类（SexyAppBase.h/.cpp）

- **继承关系**：`class SexyAppBase : public ButtonListener, public DialogListener`（SexyAppBase.h:122），全局单例 `extern SexyAppBase* gSexyAppBase`（.h:577，定义 .cpp:49）。
- **职责**：框架核心应用基类——统一管理窗口与 DirectDraw 设备、Windows 消息泵与事件分发、帧/时钟循环、资源与线程加载、注册表读写、Demo 录制回放、声音/光标/属性等，是所有游戏 App 的直接基类。

**关键成员变量表（按用途分组，行号来自 SexyAppBase.h）**

| 分组 | 变量 | 类型 | 说明 |
|---|---|---|---|
| 主循环/帧 | `mFrameTime` | `int` (h:206) | 逻辑帧周期，默认 10ms（≈100FPS） |
| | `mUpdateCount` / `mDrawCount` / `mSleepCount` | `int` (h:219/218/217) | 逻辑更新/绘制/睡眠计数 |
| | `mUpdateAppState` | `int` (h:220) | 更新状态机（UPDATESTATE_*，h:112-118） |
| | `mUpdateAppDepth` | `int` (h:221) | 更新嵌套深度（防重入） |
| | `mUpdateMultiplier` | `double` (h:222) | 加速/减速倍率（demo 回放用） |
| | `mNonDrawCount` | `int` (h:205) | 保证每秒至少绘制 10 次的计数 |
| | `mPendingUpdatesAcc` / `mUpdateFTimeAcc` | `double` (h:211/212) | 待补更新累计 / 帧时间累计 |
| | `mLastTimeCheck` / `mLastTime` / `mLastUserInputTick` | `DWORD` (h:213/214/215) | 上次计时/循环时间/用户输入时间 |
| | `mStepMode` | `int` (h:229) | 单步调试模式 |
| 时钟/时间 | `mLastTimerTime` / `mLastBigDelayTime` | `DWORD` (h:193/194) | 定时器消息时间戳/大延迟标记 |
| | `mTimeLoaded` | `DWORD` (h:172) | 加载起始 tick（构造时 `GetTickCount()`） |
| | `mLastDrawTick` / `mNextDrawTick` | `DWORD` (h:227/228) | 上次/下次绘制时间 |
| | `mFPSStartTick` / `mFPSTime` / `mFPSCount` | `ulong`/`int` (h:244/247/248) | FPS 统计 |
| 事件/输入 | `mDeferredMessages` | `WindowsMessageList` (h:177) | 延迟消息队列（`std::list<MSG>`） |
| | `mNoDefer` | `bool` (h:178) | 是否关闭延迟分发（弹窗时） |
| | `mCtrlDown` / `mAltDown` / `mAllowAltEnter` | `bool` (h:296/297/298) | 修饰键状态 / 允许 Alt+Enter |
| | `mMouseIn` | `bool` (h:236) | 鼠标是否在窗口内 |
| 窗口/显示 | `mHWnd` / `mInvisHWnd` | `HWND` (h:173/174) | 主窗口 / 隐藏辅助窗口句柄 |
| | `mWidth` / `mHeight` / `mFullscreenBits` | `int` (h:138/139/140) | 逻辑分辨率 / 全屏色深 |
| | `mIsWindowed` / `mIsPhysWindowed` / `mFullScreenWindow` | `bool` (h:165/166/167) | 窗口化 / 物理窗口化 / 全屏窗口（ChangeDisplaySettings） |
| | `mForceFullscreen` / `mForceWindowed` | `bool` (h:168/169) | 强制全屏/窗口 |
| | `mPreferredX` / `mPreferredY` | `int` (h:136/137) | 窗口位置偏好 |
| | `mScreenBounds` / `mWindowAspect` / `mWidescreenAware` | `Rect`/`Ratio`/`bool` (h:315/317/314) | 屏幕范围 / 窗口宽高比 / 宽屏感知 |
| 线程/加载 | `mPrimaryThreadId` | `DWORD` (h:161) | 主线程 ID（跨线程 Shutdown 保护） |
| | `mAutoStartLoadingThread` | `bool` (h:252) | Start 时是否自动起加载线程 |
| | `mLoadingThreadStarted` / `mLoadingThreadCompleted` | `bool` (h:253/254) | 线程已启动 / 已完成（跨线程共享标志） |
| | `mLoaded` / `mLoadingFailed` | `bool` (h:255/257) | 资源已装载 / 加载失败 |
| | `mYieldMainThread` | `bool` (h:256) | 主线程让出 CPU 给加载线程 |
| | `mNumLoadingThreadTasks` / `mCompletedLoadingThreadTasks` | `int` (h:267/268) | 加载任务总数/完成数（进度条） |
| 状态标志 | `mRunning` / `mActive` | `bool` (h:237/238) | 主循环运行中 / 窗口激活 |
| | `mMinimized` / `mPhysMinimized` / `mIsDisabled` / `mHasFocus` | `bool` (h:239/240/241/242) | 最小化/物理最小化/禁用/有焦点 |
| | `mPaused` | `bool` (h:223) | 暂停（demo 回放 P 键） |
| | `mShutdown` / `mExitToTop` | `bool` (h:163/164) | 请求退出 / 退出主循环标志 |
| | `mInitialized` / `mProcessInTimer` | `bool` (h:170/171) | 已初始化 / 定时器内处理（已弃用） |
| 子系统句柄 | `mWidgetManager` | `WidgetManager*` (h:158) | UI 控件管理器（构造时 new） |
| | `mDDInterface` | `DDInterface*` (h:181) | DirectDraw 设备接口 |
| | `mMusicInterface` / `mSoundManager` | `MusicInterface*`/`SoundManager*` (h:183/232) | 音乐 / 音效 |
| | `mResourceManager` | `ResourceManager*` (h:324) | 资源清单管理器 |
| | `mMutex` | `HANDLE` (h:152) | 单实例互斥体 |
| 其他 | `mRandSeed` | `ulong` (h:126) | 随机种子（`SRand` 用） |
| | `mProdName` / `mTitle` / `mProductVersion` / `mCompanyName` | `string` (h:130/131/186/128) | 产品名/窗口标题/版本/公司名 |
| | `mDemoBuffer` 等 `mDemo*` 族 | (h:271-292) | Demo 录制/回放状态 |
| | `mStringProperties` 等属性表 | (h:319-323) | properties 键值缓存 |

**主要公共成员函数索引表**

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `SexyAppBase()` / `~SexyAppBase()` | .cpp:138 / 369 | 初始化全部字段/加载 DLL/创建 WidgetManager、ResourceManager；析构清理窗口、设备、线程、资源 | ★ |
| `virtual void Init()` | .cpp:6065 | 完整初始化：注册窗口类、建隐藏窗口、光标、PreDisplayHook、Resize、MakeWindow、声音/音乐、InitHook | ★ |
| `virtual void Start()` | .cpp:5624 | 启动入口：起光标/加载线程→ShowWindow→`timeBeginPeriod(1)`→`DoMainLoop()`→清理与统计 | ★ |
| `virtual void Shutdown()` | .cpp:2152 | 请求退出：置 `mExitToTop/mShutdown`、停音乐、恢复显示模式/分辨率、写注册表 | ★ |
| `virtual void DoMainLoop()` | .cpp:5518 | 主循环骨架：`while(!mShutdown) UpdateApp();` | ★★ |
| `virtual bool UpdateAppStep(bool*)` | .cpp:5528 | 单步：先泵消息（PeekMessage/DispatchMessage）→ProcessDemo→ProcessDeferredMessages，再 `Process()` | ★ |
| `virtual bool UpdateApp()` | .cpp:5590 | 循环调 UpdateAppStep 直到产生一次真实更新 | ★ |
| `virtual bool Process(bool allowSleep)` | .cpp:5215 | **帧循环核心**：帧率/VSync 控制、逻辑更新调度、绘制与 Sleep | ★ |
| `virtual void UpdateFrames()` | .cpp:2219 | 单次逻辑帧：`mUpdateCount++` + `WidgetManager::UpdateFrame()` + 音乐更新 | ★ |
| `virtual bool DoUpdateFrames()` | .cpp:2239 | 逻辑帧包装：先处理加载线程完成→再 `UpdateFrames()` | ★ |
| `virtual void DoUpdateFramesF(float)` | .cpp:2233 | 分数帧更新（VSync 插值用 `UpdateFrameF`） | ★ |
| `virtual bool DrawDirtyStuff()` | .cpp:2569 | 脏区绘制：FPS 统计→`WidgetManager::DrawScreen()`→`Redraw(NULL)` | ★ |
| `virtual void Redraw(Rect*)` | .cpp:2289 | 提交帧到屏幕（`mDDInterface->Redraw`，失败则重建 DD 接口） | ★ |
| `virtual void BeginPopup()` / `EndPopup()` | .cpp:2722 / 2734 | 弹窗配对：切回 GDI 表面+关延迟分发；恢复延迟、清按键/鼠标 | ★★ |
| `int MsgBox(...)` / `void Popup(...)` | .cpp:2749/2766/2783/2797 | 消息框/致命错误弹窗（内部用 Begin/EndPopup 包裹） | ★★ |
| `static LRESULT CALLBACK WindowProc(HWND,UINT,WPARAM,LPARAM)` | .cpp:3380 | 窗口过程：把输入类消息压入 `mDeferredMessages`（延迟分发），处理 WM_CLOSE/PAINT/SETCURSOR 等 | ★ |
| `bool ProcessDeferredMessages(bool)` | .cpp:4168 | 从延迟队列取消息，录制 demo 后转发 `WidgetManager::MouseDown/Up/Move/Wheel、KeyDown/Up/KeyChar`、WM_SIZE/WM_TIMER/WM_CLOSE | ★ |
| `void StartLoadingThread()` | .cpp:4929 | 起加载线程（`_beginthread(LoadingThreadProcStub,...)`），并让主线程让出 CPU | ★ |
| `virtual void LoadingThreadProc()` | .cpp:4908 | 后台加载主体（基类空实现，子类重写加载资源） | ★ |
| `static void LoadingThreadProcStub(void*)` | .cpp:4916 | 线程入口桩：调用 `LoadingThreadProc()` 后置 `mLoadingThreadCompleted=true` | ★ |
| `virtual void LoadingThreadCompleted()` | .cpp:4912 | 加载完成回调（基类空，子类收尾）；由 `DoUpdateFrames` 主线程调用 | ★ |
| `void WaitForLoadingThread()` / `double GetLoadingThreadProgress()` | .cpp:1127 / 1472 | 等待加载线程结束 / 按任务数返回进度 | ★ |
| `void UpdateFTimeAcc()` | .cpp:5195 | 累加帧时间（`min(acc+Δ,200)`），主循环计时核心 | ★ |
| `void ClearUpdateBacklog(bool)` | .cpp:526 | 清零更新积压（弹窗后防止追帧） | ★ |
| `void MakeWindow()` | .cpp:4636 | 创建主窗口并 `SetTimer(mHWnd,100,mFrameTime,NULL)`（.cpp:4870） | ★ |
| `void SwitchScreenMode(...)` | .cpp:5012 / 5057 / 5062 | 切换窗口/全屏模式 | ★ |
| `void RestoreScreenResolution()` / `void DoExit(int)` | .cpp:2202 / 2213 | 恢复分辨率 / 退出进程 | ★ |
| `void SafeDeleteWidget(Widget*)` / `ProcessSafeDeleteList()` | .cpp:2811 / 5177 | 延迟安全删除控件 | ★ |
| 注册表/文件/属性/声音/光标等访问族 | .cpp:1483-2110, 5690-5860, 6874-6960 | 大量辅助方法（详见 .h:504-564 声明） | ★ |

---

### 2.3 主循环与时钟流程（重点）

#### 启动链路：Init → Start

```
WinMain / 外部入口
  └─ new SexyApp()            // SexyApp.cpp:29（设置 gSexyApp，解析构建号）
  └─ app->Init()              // SexyApp.cpp:602 → SexyAppBase::Init() (.cpp:6065)
  └─ app->Start()             // SexyAppBase::Start() (.cpp:5624) → 进入主循环
```

`SexyAppBase::Init()`（.cpp:6065）依次完成：
1. 记录 `mPrimaryThreadId`，检查 ddraw/dsound DLL 存在性；
2. `InitPropertiesHook()`（读 properties / partner.xml）→ `ReadFromRegistry()`；
3. Vista 下设置 `AppData` 数据目录；
4. `DoParseCmdLine()`、`ChangeDirHook`/`chdir`，`AddPakFile("main.pak")`；
5. `RegisterWindowMessage("Notify+产品名")` + `CreateMutex`（单实例检测 `HandleGameAlreadyRunning`）；
6. 随机种子、demo 缓冲读取；注册窗口类 `MainWindow`/`InvisWindow`、创建隐藏窗口、手型/拖动光标；
7. `PreDisplayHook()` → `mWidgetManager->Resize(...)`（.cpp:6253）；
8. 检查窗口化可行性，全屏时 `ChangeDisplaySettings`；
9. `MakeWindow()`（.cpp:6294，内含 `SetTimer(mHWnd,100,mFrameTime,...)` 于 .cpp:4870）；
10. 创建 `DSoundManager`、`CreateMusicInterface`，设音量；`InitHook()`；置 `mInitialized=true`。

`SexyAppBase::Start()`（.cpp:5624）：
- `StartCursorThread()` →（若 `mAutoStartLoadingThread`）`StartLoadingThread()`；
- `ShowWindow` + `SetFocus`，`timeBeginPeriod(1)`（把系统定时器精度提到 1ms）；
- `mRunning=true`，记录起始 tick → **`DoMainLoop()`**；
- 退出后 `ProcessSafeDeleteList()`、`WaitForLoadingThread()`、输出统计（Seconds/Update/Draw Count、Avg FPS）、`timeEndPeriod(1)` → `PreTerminate()` → `WriteToRegistry()`。

#### 主循环结构（DoMainLoop → UpdateApp → UpdateAppStep → Process）

```
DoMainLoop()  (.cpp:5518)
  while (!mShutdown):
    if (mExitToTop) mExitToTop = false
    UpdateApp()                                   // .cpp:5590
        for(;;):
          UpdateAppStep(&updated)                 // .cpp:5528
          if (updated) return true                // 完成一次真实逻辑更新才返回

UpdateAppStep():
  if (mUpdateAppState == PROCESS_DONE) state = MESSAGES
  if (state == MESSAGES):                         // 阶段1：消息
      while (PeekMessage(...PM_REMOVE) && !mShutdown)   // .cpp:5546
          TranslateMessage(&msg); DispatchMessage(&msg)  // → WindowProc(3380) 压入延迟队列
      ProcessDemo()
      if (!ProcessDeferredMessages(true))         // 把延迟消息转给 WidgetManager
          state = PROCESS_1
  else:                                           // 阶段2：逻辑/绘制
      Process()                                   // .cpp:5215（真正干活）
```

**`Process()`（.cpp:5215，帧循环核心）的伪代码：**

```
1. 计算帧参数：
   isVSynched = (!demo && VSyncUpdates && ... )
   aFrameFTime     = mFrameTime / mUpdateMultiplier      // 逻辑帧周期
   anUpdatesPerUpdateF = (VSync 时 = 1000/(mFrameTime*refresh))
2. demo 快速前进分支（mFastForwardToUpdateNum/marker）：循环 DoUpdateFrames + 偶尔 DrawDirtyStuff
3. if (!mPaused && mUpdateMultiplier>0):
     if (!isVSynched) UpdateFTimeAcc()                    // 累加真实流逝时间
     if (state == PROCESS_1):
        if (++mNonDrawCount < ceil(10*multiplier) || !mLoaded):
           doUpdate = (isVSynched ? 刚画完且时间到 : mUpdateFTimeAcc >= aFrameFTime)
           if (doUpdate):
              DoUpdateFrames()                            // 逻辑帧 .cpp:2239
              state = PROCESS_2
              mHasPendingDraw = true
     elif (state == PROCESS_2):
        state = PROCESS_DONE
        mPendingUpdatesAcc += anUpdatesPerUpdateF - 1.0   // 补欠帧
        while (mPendingUpdatesAcc >= 1.0): DoUpdateFrames()  // 追帧
        DoUpdateFramesF(frac)                             // 插值帧
        mUpdateFTimeAcc -= aFrameFTime                    // 扣掉一个帧周期
     if (!didUpdate):
        state = PROCESS_DONE; mNonDrawCount = 0
        if (mHasPendingDraw): DrawDirtyStuff()            // .cpp:2569 画一帧
        else: Sleep(aFrameFTime - mUpdateFTimeAcc)        // 无活可干→睡到下一帧
4. if (mYieldMainThread): Sleep(...)                      // 加载期间让出 CPU（.cpp:5461）
```

**帧率控制要点**：`mFrameTime=10` 决定逻辑帧 10ms 一格；`UpdateFTimeAcc` 累积真实时间，每攒够一帧就做一次逻辑更新，欠帧用 `mPendingUpdatesAcc` 补跑（防止卡顿后永久掉帧）；`mNonDrawCount` 保证即便逻辑繁忙也每秒至少重绘 10 次；无更新时 `Sleep` 让出 CPU。

**`MainLoopHook` 钩子**：本版无此方法。旧版 `DoMainLoop`（含 `PeekMessage` 循环，.cpp:5484-5516）被整体注释，说明框架曾把"消息泵+Process"内联在 DoMainLoop 里；现拆成 `UpdateAppStep`（泵消息）+ `Process`（帧逻辑）。派生类可重载 `InitHook`(6061)/`ShutdownHook`(2148)/`PreDisplayHook`(6030)/`Pre/PostDDInterfaceInitHook`(6034/6038)/`ChangeDirHook`(6042)/`InitPropertiesHook`(6057)/`LoadingThreadProc`(4908)/`LoadingThreadCompleted`(4912) 来插入自定义逻辑。

#### 逻辑帧 vs 渲染帧（UpdateFrames / DoUpdateFrames / DrawDirtyStuff）

- **逻辑帧**：`DoUpdateFrames()`（.cpp:2239）→ `UpdateFrames()`（.cpp:2219）：`mUpdateCount++`，驱动 `WidgetManager::UpdateFrame()`（游戏世界推进）和音乐更新；VSync 模式下还经 `DoUpdateFramesF(frac)` 做插值渲染帧。逻辑帧按 `mFrameTime` 固定步长推进，与真实渲染率解耦。
- **渲染帧**：`DrawDirtyStuff()`（.cpp:2569）：调用 `WidgetManager::DrawScreen()`（WidgetManager.cpp:398）把 UI 画进后备缓冲，再 `Redraw(NULL)`（.cpp:2289）交给 `mDDInterface->Redraw` 翻到屏幕。只有 `mHasPendingDraw` 或屏幕脏时才真正翻页；VSync 时在 `Redraw` 后立刻 `UpdateFTimeAcc()` 校准计时。

#### 事件分发链路（消息泵 → 延迟队列 → WidgetManager）

```
系统消息 → WindowProc (.cpp:3380)
   输入类消息(WM_LBUTTONDOWN..WM_MOUSEWHEEL、WM_KEYDOWN/SYSKEYDOWN/KEYUP/SYSKEYUP/WM_CHAR、
              WM_SIZE/MOVE/TIMER/CLOSE/DISPLAYCHANGE/SYSCOLORCHANGE 等) 且 !mNoDefer:
     构造 MSG 压入 mDeferredMessages  (push_back, .cpp:3596-3604)
     (顺带维护 mCtrlDown/mAltDown；WM_CLOSE→CloseRequestAsync；Alt+Enter→SwitchScreenMode)
   非输入消息(WM_PAINT/ERASEBKGND/SETCURSOR/SYSCOMMAND/ENDSESSION/DESTROY 等)就地处理

主循环 UpdateAppStep 阶段1 调 ProcessDeferredMessages(true) (.cpp:4168):
   while (mDeferredMessages 非空):
     msg = pop_front()
     (若录制 demo) 把鼠标/按键/字符编码进 mDemoBuffer (.cpp:4180-4352)
     (若未回放 demo) 分发给 WidgetManager:
        WM_*BUTTONDOWN/UP/DBLCLK/MOUSEMOVE → RemapMouse + MouseMove + MouseDown/MouseUp (.cpp:4383-4446)
        WM_MOUSEWHEEL → MouseWheel(.cpp:4450)
        WM_KEYDOWN/SYSKEYDOWN → KeyDown(.cpp:4475)；WM_KEYUP → KeyUp(.cpp:4481)；WM_CHAR → KeyChar(.cpp:4485)
        WM_SIZE → mMinimized / 静音切换(.cpp:4504)；WM_TIMER → mLastTimerTime + 鼠标进/出检测 + URL 超时(.cpp:4532)
        WM_MOVE → 记录窗口位置；WM_SYSCOLORCHANGE/DISPLAYCHANGE → SysColorChangedAll+MarkAllDirty
        WM_CLOSE → mManualShutdown=true; Shutdown()(.cpp:4606)
```

> 注：`WM_TIMER` 由 `SetTimer(mHWnd,100,mFrameTime,NULL)`（.cpp:4870）每 10ms 触发，仅用于维护 `mLastTimerTime`、检测鼠标进出窗口与 URL 打开超时，**不**驱动帧更新（帧循环由 `DoMainLoop→Process` 驱动，`mProcessInTimer` 定时器内处理的旧逻辑已注释，.cpp:3439-3454）。

#### 线程加载机制（StartLoadingThread）

```
StartLoadingThread() (.cpp:4929)
   if (!mLoadingThreadStarted):
     mYieldMainThread = true                              // 主线程准备让出 CPU
     SetThreadPriority(当前线程, ABOVE_NORMAL)            // 主线程升优先级
     mLoadingThreadStarted = true
     _beginthread(LoadingThreadProcStub, 0, this)          // 起后台线程

LoadingThreadProcStub(void* arg) (.cpp:4916)   // 后台线程入口
   aSexyApp->LoadingThreadProc()              // 虚函数，基类空实现(.cpp:4908)，游戏重写来加载资源
   OutputDebugString("Resource Loading Time: %d ms")
   aSexyApp->mLoadingThreadCompleted = true    // 仅置一个共享 bool 标志

主线程如何得知完成（DoUpdateFrames .cpp:2239 / 2267-2272）:
   if (mLoadingThreadCompleted && !mLoaded):
     SetThreadPriority(当前线程, NORMAL)       // 主线程降回正常优先级
     mLoaded = true; mYieldMainThread = false
     LoadingThreadCompleted()                  // 虚回调(.cpp:4912)，游戏做收尾

配合：
   Process() 里 mYieldMainThread 分支(.cpp:5461-5477)：加载期间主线程 Sleep(min(250, 2*elapsed))，把 CPU 让给加载线程；
   GetLoadingThreadProgress()(.cpp:1472)：按 mCompletedLoadingThreadTasks/mNumLoadingThreadTasks 报告进度；
   WaitForLoadingThread()(.cpp:1127)：Start 结束时确保线程已结束；
   Shutdown 跨线程调用时置 mLoadingFailed(.cpp:2154-2157)。
```

**关键结论**：结果回投**不是**通过 Windows 消息/事件，而是极简的**共享布尔标志 `mLoadingThreadCompleted` 轮询**——后台线程末尾置位，主线程在每次 `DoUpdateFrames()` 里检查该标志后调用 `LoadingThreadCompleted()` 完成收尾。`LoadingThreadProcStub` 是静态线程入口（把 `this` 参数转回对象并调用虚函数），`LoadingThreadCompleted` 则是主线程侧的完成回调（基类空实现，供子类重载）。

## 3. 控件系统（WidgetManager / Widget / WidgetContainer / Dialog 及全部控件）

> 行号均为本仓库实测值（GBK 源码，编码 936）。与任务描述中的约数略有出入：`WidgetManager.cpp` 实为 806 行、`Widget.cpp` 489 行、`WidgetContainer.cpp` 627 行、`Dialog.cpp` 407 行、`EditWidget.cpp` 694 行等，下文一律按实测行号。

### 3.0 控件树架构总览

**类层次**：`WidgetManager : WidgetContainer`（根）；`Widget : WidgetContainer`（叶子功能单元）；其余控件 `ButtonWidget/Checkbox/EditWidget/ListWidget/ScrollbarWidget/Slider/TextWidget/CursorWidget : Widget`；`Dialog/ListWidget/TextWidget` 还多继承对应 Listener。

**树结构与父子关系**
- 根节点 `WidgetManager` 本身即一个 `WidgetContainer`，其 `mWidgetManager` 指向自身（`WidgetManager.cpp:19`），是唯一 `mParent == NULL` 的容器。
- 父指针类型是 `WidgetContainer* mParent`（不是 `Widget*`），子列表是 `std::list<Widget*> mWidgets`（`WidgetContainer.h:21-23`）。**没有独立的 `mChildren` 成员**，`mWidgets` 即子列表。
- `AddWidget()` 设 `theWidget->mWidgetManager = mWidgetManager; theWidget->mParent = this`，再 `AddedToManager()` 递归下发（`WidgetContainer.cpp:59-76`）。`RemoveWidget()` 调 `WidgetRemovedHelper()`（`WidgetContainer.cpp:83-99`，`Widget.cpp:34-63`），后者递归清理子节点、`DisableWidget` 解除事件引用、清理 pre-modal 记录、置 `mWidgetManager = NULL`。
- **WidgetId**：框架无统一 `mWidgetId` 字段；各控件自带 `int mId`（`ButtonWidget/Checkbox/EditWidget/ListWidget/ScrollbarWidget/Dialog/Slider` 均声明自己的 `mId`），作为 Listener 回调（`ButtonPress(theId)` 等）的路由键。WidgetManager 层的位标志是 `int mWidgetFlags`（`WidgetManager.h:36` 枚举 `WIDGETFLAGS_UPDATE/DRAW/CLIP/ALLOW_MOUSE/ALLOW_FOCUS`），配合 `WidgetContainer::mWidgetFlagsMod` 做逐级标志增删。
- **Z 序**：每个 `WidgetContainer` 有 `int mZOrder`（`WidgetContainer.h:38`）。`InsertWidgetHelper()` 在 `mWidgets` 链表内按 `mZOrder` 稳定插入（`WidgetContainer.cpp:200-238`）；链表顺序 = 绘制顺序 = 命中测试顺序。`BringToFront/BringToBack/PutBehind/PutInfront`（`WidgetContainer.cpp:240-314`）移动节点后调 `OrderInManagerChanged()`。
- **焦点管理**：`WidgetManager::mFocusWidget` 指向当前焦点控件，`SetFocus()` 负责 LostFocus/GotFocus 交接（`WidgetManager.cpp:317-334`）；`Widget::WantsFocus()`（缺省返回 `mWantsFocus`，`Widget.cpp:176-179`）决定鼠标按下是否抢焦点。键盘/字符/滚轮事件一律发给 `mFocusWidget`（`WidgetManager.cpp:748-806`）。
- **事件分发模型**：所有系统事件进 `WidgetManager`，经 `GetWidgetAt()`（命中测试）路由；命中测试从列表**尾部反向**（Z 序高→低）递归进行（`WidgetContainer.cpp:101-151`）；按下即捕获到 `mLastDownWidget`，此后移动/抬起全部发给它（"Option 2"，`WidgetManager.cpp:637-640`）。
- **绘制/更新遍历**：`DrawAll/UpdateAll` 均带 `ModalFlags` 参数递归（`WidgetContainer.cpp:479-597`），`mBaseModalWidget` 之后的节点标志被裁剪，形成模态屏蔽。

---

### 3.1 WidgetManager（WidgetManager.h / .cpp）
- **继承关系**：`class WidgetManager : public WidgetContainer`（WidgetManager.h:42）
- **职责**：控件的根容器与中央调度器——命中测试、焦点、事件分发、绘制/更新、模态与脏矩形管理。
- **关键成员变量表**

| 变量 | 类型 | 说明 |
|---|---|---|
| `mDefaultTab` | `Widget*` | Ctrl+Tab 时的默认接管控件 |
| `mApp` | `SexyAppBase*` | 宿主应用 |
| `mImage` / `mTransientImage` | `MemoryImage*` | 后台缓冲与临时缓冲 |
| `mFocusWidget` | `Widget*` | 当前键盘焦点控件 |
| `mLastDownWidget` | `Widget*` | 鼠标按下捕获的控件（capture 目标） |
| `mOverWidget` | `Widget*` | 当前鼠标悬停控件 |
| `mBaseModalWidget` | `Widget*` | 当前基准模态控件 |
| `mBelowModalFlagsMod` / `mDefaultBelowModalFlagsMod` | `FlagsMod` | 模态之下控件的标志裁剪 |
| `mPreModalInfoList` | `PreModalInfoList` | 模态栈（用于恢复焦点/基准模态） |
| `mDownButtons` / `mActualDownButtons` | `int` | 逻辑按下掩码 / 物理按下掩码（1 左、2 右、4 中） |
| `mLastMouseX/Y` | `int` | 最近鼠标坐标 |
| `mKeyDown[0xFF]` | `bool[]` | 键盘按下状态表 |
| `mWidgetFlags` | `int` | 根级能力位标志（UPDATE|DRAW|CLIP|ALLOW_MOUSE|ALLOW_FOCUS） |
| `mPopupCommandWidget` | `Widget*` | 弹出式命令控件 |

- **主要公共成员函数索引表**

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `GetAnyWidgetAt(int,int,int*,int*)` | WidgetManager.cpp:87 | 命中最上层控件（含禁用） | ★★★ |
| `GetWidgetAt(int,int,int*,int*)` | WidgetManager.cpp:93 | 命中最上层**非禁用**控件 | ★★★ |
| `SetFocus(Widget*)` | WidgetManager.cpp:317 | 切换键盘焦点（Lost/Got 交接） | ★★★ |
| `GotFocus() / LostFocus()` | WidgetManager.cpp:336/347 | 应用级焦点进/出，触发按键释放 | ★★★ |
| `SetBaseModal(Widget*,const FlagsMod&)` | WidgetManager.cpp:218 | 设基准模态，裁剪下方控件 | ★★ |
| `AddBaseModal / RemoveBaseModal` | WidgetManager.cpp:250/267 | 模态入/出栈（可恢复焦点） | ★★★ |
| `DrawScreen()` | WidgetManager.cpp:398 | 脏矩形驱动的全屏重绘 | ★★ |
| `DrawWidgetsTo(Graphics*)` | WidgetManager.cpp:372 | 无条件绘制全部到指定 g | ★★ |
| `UpdateFrame() / UpdateFrameF(float)` | WidgetManager.cpp:474/489 | 驱动全树 Update/UpdateF | ★★★ |
| `MouseDown / MouseUp / MouseMove / MouseDrag / MouseExit` | WidgetManager.cpp:606/571/673/686/733 | 鼠标事件入口与分发 | ★★★ |
| `MousePosition(int,int)` | WidgetManager.cpp:516 | 悬停切换（Enter/Leave/Move） | ★★★ |
| `RehupMouse()` | WidgetManager.cpp:551 | 布局变化后重算悬停 | ★★★ |
| `MouseWheel(int)` | WidgetManager.cpp:748 | 滚轮→焦点控件 | ★★★ |
| `KeyChar / KeyDown / KeyUp` | WidgetManager.cpp:756/779/792 | 键盘事件→焦点控件（Tab 特殊处理） | ★★★ |
| `DoMouseUps(Widget*,ulong)` | WidgetManager.cpp:181 | 强制抬起指定控件的按下按钮 | ★★ |
| `RemapMouse(int&,int&)` | WidgetManager.cpp:194 | 源矩形→目标矩形坐标映射 | ★★ |

**事件分发链要点**（均在 WidgetManager.cpp）：
- **鼠标按下**：`MouseDown`(606) → `MousePosition`(617) → 命中 `GetWidgetAt`(624) → 若已有 `mLastDownWidget` 则捕获不变(639) → 设 `mDownButtons` 掩码(644-658) → `mLastDownWidget = aWidget`(660) → 若 `WantsFocus()` 则 `SetFocus`(663-664) → `aWidget->MouseDown(...,theClickCount)`(667)。
- **鼠标移动**：`MouseMove`(673) 检测 `mDownButtons` 非零则转 `MouseDrag`(686)；否则 `MousePosition`(516) 做 Enter/Leave 切换与 `MouseMove` 转发。`MouseDrag` 把坐标换算成 `mLastDownWidget` 局部坐标（`GetAbsPos`）转发，并按命中的控件是否等于捕获控件决定 Enter/Leave（701-728）。
- **鼠标抬起**：`MouseUp`(571) 用 `theClickCount` 推断按钮掩码(577-582)，清除掩码后只对 `mLastDownWidget` 发 `MouseUp`(596)，随后 `MousePosition` 重算悬停(601)。
- **滚轮**：直接 `mFocusWidget->MouseWheel`（752-753）。**按键/字符**：`KeyDown/KeyChar/KeyUp` 一律 `mFocusWidget->...`（786/774/802）；`KeyChar` 对 Tab 有 Ctrl 分支转 `mDefaultTab`（760-771）。

---

### 3.2 Widget（Widget.h / .cpp）
- **继承关系**：`class Widget : public WidgetContainer`（Widget.h:18）
- **职责**：有外观、可见性、禁用/焦点/悬停/按下状态与完整事件虚接口的 UI 叶子节点基类。
- **关键成员变量表**

| 变量 | 类型 | 说明 |
|---|---|---|
| `mVisible` | `bool` | 是否可见（决定绘制与命中） |
| `mMouseVisible` | `bool` | 是否参与鼠标命中 |
| `mDisabled` | `bool` | 是否禁用（禁用则不被 `GetWidgetAt` 返回） |
| `mHasFocus` | `bool` | 是否拥有键盘焦点 |
| `mIsDown` / `mIsOver` | `bool` | 按下 / 悬停状态 |
| `mHasTransparencies` | `bool` | 是否有透明区域（影响脏矩形优化） |
| `mColors` | `ColorVector` | 调色板（按索引存取） |
| `mMouseInsets` | `Insets` | 命中/悬停区域的内缩 |
| `mDoFinger` | `bool` | 悬停时切换手型光标 |
| `mWantsFocus` | `bool` | 是否愿意获得焦点 |
| `mTabPrev` / `mTabNext` | `Widget*` | Tab 焦点导航链表 |
| `mWriteColoredString` | `static bool` | 是否启用 `^color^` 彩色字符串 |

- **主要公共成员函数索引表**

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `WidgetRemovedHelper()` | Widget.cpp:34 | 递归清理子节点+解除事件引用 | ★★ |
| `SetVisible(bool)` | Widget.cpp:74 | 切换可见并 RehupMouse | ★★★ |
| `SetDisabled(bool)` | Widget.cpp:181 | 禁用并解除 over/down/focus 引用 | ★★★ |
| `Resize/Move` | Widget.cpp:146/171 | 改几何并双向标脏 | ★★★ |
| `SetColors/SetColor/GetColor` | Widget.cpp:103/122/131 | 调色板管理 | ★★★ |
| `WantsFocus()` | Widget.cpp:176 | 返回 `mWantsFocus` | ★★ |
| `GotFocus/LostFocus` | Widget.cpp:198/203 | 置/清 `mHasFocus` | ★★ |
| `KeyDown(KeyCode)` | Widget.cpp:221 | Tab 在 `mTabPrev/mTabNext` 间移动焦点 | ★★★ |
| `MouseDown(x,y,clickCount)` | Widget.cpp:272 | 把 1/2/3 键 clickCount 分发到带按键号的版本 | ★★ |
| `Contains(int,int)` | Widget.cpp:412 | 局部坐标命中判断 | ★★★ |
| `GetInsetRect()` | Widget.cpp:418 | 应用 `mMouseInsets` 的命中矩形 | ★★★ |
| `DeferOverlay(int)` | Widget.cpp:425 | 把自身加入延迟 overlay 队列 | ★★ |
| `WriteString / WriteWordWrapped / WriteCenteredLine` | Widget.cpp:348/358/316 | 文本绘制包装（支持彩色） | ★★★ |
| `WriteNumberFromStrip` | Widget.cpp:386 | 用数字条图绘制数字 | ★★ |
| `Layout(int,Widget*,...)` | Widget.cpp:430 | 基于 LAY_* 标志的相对布局 | ★★★ |

---

### 3.3 WidgetContainer（WidgetContainer.h / .cpp）
- **继承关系**：`class WidgetContainer`（WidgetContainer.h:18，非虚根，仅数据+逻辑）
- **职责**：控件树节点的容器——子列表维护、Z 序插入、命中测试、脏矩形、绘制/更新递归遍历。
- **关键成员变量表**

| 变量 | 类型 | 说明 |
|---|---|---|
| `mWidgets` | `WidgetList`（`std::list<Widget*>`） | 子控件列表，顺序即 Z 序 |
| `mWidgetManager` | `WidgetManager*` | 所属管理器 |
| `mParent` | `WidgetContainer*` | 父容器 |
| `mX/mY/mWidth/mHeight` | `int` | 相对父的几何 |
| `mHasAlpha` / `mClip` | `bool` | 是否有 alpha / 是否裁剪绘制 |
| `mWidgetFlagsMod` | `FlagsMod` | 对子树的标志增删 |
| `mPriority` | `int` | overlay 优先级 |
| `mZOrder` | `int` | Z 序键值 |
| `mDirty` / `mUpdateCnt` | `bool` / `int` | 脏标记 / 更新计数 |
| `mUpdateIterator` | `WidgetList::iterator` | 更新遍历的安全迭代器 |

- **主要公共成员函数索引表**

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `AddWidget(Widget*)` | WidgetContainer.cpp:59 | 挂子节点并下发 AddedToManager | ★★★ |
| `RemoveWidget(Widget*)` | WidgetContainer.cpp:83 | 摘子节点并触发清理 | ★★★ |
| `RemoveAllWidgets(bool,bool)` | WidgetContainer.cpp:35 | 清空子树（可选 delete） | ★★★ |
| `GetWidgetAtHelper(int,int,int,bool*,int*,int*)` | WidgetContainer.cpp:101 | 反向递归命中测试（核心） | ★★★ |
| `IsBelowHelper / IsBelow` | WidgetContainer.cpp:153/181 | 判断两控件 Z 序上下 | ★★ |
| `InsertWidgetHelper(iterator,Widget*)` | WidgetContainer.cpp:200 | 按 `mZOrder` 稳定插入 | ★★★ |
| `BringToFront/BringToBack` | WidgetContainer.cpp:240/258 | 移到最前/最后 | ★★★ |
| `PutBehind/PutInfront` | WidgetContainer.cpp:276/295 | 移到参照控件后/前 | ★★★ |
| `GetAbsPos()` | WidgetContainer.cpp:316 | 递归求顶层绝对坐标 | ★★★ |
| `AddedToManager/RemovedFromManager` | WidgetContainer.cpp:324/339 | 挂载/卸载时的递归通知 | ★★★ |
| `MarkDirty/MarkDirtyFull` | WidgetContainer.cpp:354/362 | 标脏（向上冒泡/连带遮挡者） | ★★★ |
| `UpdateAll(ModalFlags*)` | WidgetContainer.cpp:479 | 带模态标志的递归更新 | ★★★ |
| `DrawAll(ModalFlags*,Graphics*)` | WidgetContainer.cpp:555 | 带模态/裁剪/overlay 的递归绘制 | ★★★ |

**命中测试核心**（`WidgetContainer.cpp:101-151`）：`reverse_iterator` 从列表尾（最高 Z）向前；`belowModal` 标志在越过 `mBaseModalWidget` 后置真，之后子节点标志被 `mBelowModalFlagsMod` 裁剪（模态之下不可点击）。命中顺序：先递归子节点，子节点未命中再用自身 `GetInsetRect().Contains` + `IsPointVisible` 判定。

---

### 3.4 Dialog（Dialog.h / .cpp）
- **继承关系**：`class Dialog : public Widget, public ButtonListener`（Dialog.h:22）
- **职责**：可拖动、可模态、带 1–2 个按钮的通用对话框，阻塞等待用户选择结果。
- **关键成员变量表**

| 变量 | 类型 | 说明 |
|---|---|---|
| `mDialogListener` | `DialogListener*` | 按钮回调目标（默认 `gSexyAppBase`） |
| `mComponentImage` | `Image*` | 背景 9 宫格图 |
| `mYesButton` / `mNoButton` | `DialogButton*` | 确定/取消按钮 |
| `mDialogHeader/Footer/Lines` | `SexyString` | 标题/脚注/正文 |
| `mButtonMode` | `int` | BUTTONS_YES_NO / OK_CANCEL / FOOTER / NONE |
| `mId` | `int` | 对话框 id |
| `mIsModal` | `bool` | 是否模态 |
| `mResult` | `int` | 结果（初值 `0x7FFFFFFF`） |
| `mDragging/mDragMouseX/Y` | `bool/int` | 拖动状态与偏移 |
| `mHeaderFont/mLinesFont` | `Font*` | 标题/正文字体 |
| `mBackgroundInsets/mContentInsets` | `Insets` | 背景/内容内边距 |
| `mButtonHeight/mButtonHorzSpacing/mButtonSidePadding` | `int` | 按钮几何参数 |

- **主要公共成员函数索引表**

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `Dialog(Image*,Image*,int,bool,...)` | Dialog.cpp:27 | 构造并创建按钮 | ★★ |
| `GetPreferredHeight(int)` | Dialog.cpp:159 | 按宽度推算所需高度 | ★★★ |
| `Draw(Graphics*)` | Dialog.cpp:200 | 绘制背景/标题/换行正文 | ★★ |
| `AddedToManager/RemovedFromManager` | Dialog.cpp:260/270 | 把按钮也挂到管理器 | ★★ |
| `OrderInManagerChanged()` | Dialog.cpp:280 | 保持按钮位于对话框之上 | ★★ |
| `Resize(...)` | Dialog.cpp:289 | 重算按钮几何 | ★★ |
| `MouseDown/MouseDrag/MouseUp` | Dialog.cpp:310/322/356 | 拖动对话框并夹取到屏幕内 | ★★ |
| `IsModal()` | Dialog.cpp:373 | 返回 `mIsModal` | ★★ |
| `WaitForResult(bool autoKill)` | Dialog.cpp:378 | 循环驱动主循环直到有结果 | ★★★ |
| `ButtonPress/ButtonDepress` | Dialog.cpp:390/396 | 转发 DialogListener，Depress 写入 `mResult` | ★★ |

**模态与 WaitForResult 流程**：`WaitForResult` 在 `mResult == 0x7FFFFFFF` 且仍在管理器时循环调用 `gSexyAppBase->UpdateAppStep(NULL)` 驱动帧循环（`Dialog.cpp:382`）；用户点按钮 → `ButtonDepress` 写 `mResult`（`Dialog.cpp:400`）→ 循环退出 → `autoKill` 时 `gSexyAppBase->KillDialog(mId)`（`Dialog.cpp:385`）→ 返回结果。模态本身由宿主经 `WidgetManager::AddBaseModal(this)` 实现（下方控件被屏蔽）。

---

### 3.5 DialogButton（DialogButton.h / .cpp）
- **继承关系**：`class DialogButton : public ButtonWidget`（DialogButton.h:9）
- **职责**：对话框专用按钮，用 9 宫格组件图 + 按压缩放/平移绘制。
- **关键成员变量表**

| 变量 | 类型 | 说明 |
|---|---|---|
| `mComponentImage` | `Image*` | 9 宫格背景图 |
| `mTranslateX/Y` | `int` | 按下时的平移量（默认 1） |
| `mTextOffsetX/Y` | `int` | 文字偏移 |

- **主要公共成员函数索引表**

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `DialogButton(Image*,int,ButtonListener*)` | DialogButton.cpp:15 | 构造并设 `mDoFinger=true` | ★★ |
| `Draw(Graphics*)` | DialogButton.cpp:27 | 按状态选矩形 9 宫格绘制+文字 | ★★ |

---

### 3.6 ButtonWidget（ButtonWidget.h / .cpp）
- **继承关系**：`class ButtonWidget : public Widget`（ButtonWidget.h:12）
- **职责**：通用按钮——四态贴图/纯色边框绘制，按下/抬起/长按 tick 回调 `ButtonListener`。
- **关键成员变量表**

| 变量 | 类型 | 说明 |
|---|---|---|
| `mId` | `int` | 按钮 id（回调参数） |
| `mLabel` / `mLabelJustify` | `SexyString`/`int` | 文字与对齐 |
| `mFont` | `Font*` | 文字字体 |
| `mButtonImage/mOverImage/mDownImage/mDisabledImage` | `Image*` | 四态图 |
| `mNormalRect/mOverRect/mDownRect/mDisabledRect` | `Rect` | 四态图的子矩形 |
| `mInverted/mBtnNoDraw/mFrameNoDraw` | `bool` | 反相/不画/不画边框 |
| `mButtonListener` | `ButtonListener*` | 事件回调 |
| `mOverAlpha/mOverAlphaSpeed/mOverAlphaFadeInSpeed` | `double` | hover 淡入淡出参数 |

- **主要公共成员函数索引表**

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `IsButtonDown()` | ButtonWidget.cpp:50 | `mIsDown && mIsOver && !mDisabled` | ★★ |
| `Draw(Graphics*)` | ButtonWidget.cpp:68 | 四态绘制（贴图或程序化边框） | ★★★ |
| `SetDisabled(bool)` | ButtonWidget.cpp:197 | 禁用并标脏 | ★★ |
| `MouseEnter/MouseLeave/MouseMove` | ButtonWidget.cpp:205/218/233 | 转发 `ButtonMouseEnter/Leave/Move` | ★★ |
| `MouseDown(x,y,btn,count)` | ButtonWidget.cpp:240 | 触发 `ButtonPress(mId,count)` | ★★ |
| `MouseUp(x,y,btn,count)` | ButtonWidget.cpp:249 | 悬停且应用有焦点时触发 `ButtonDepress` | ★★ |
| `Update()` | ButtonWidget.cpp:259 | 长按 `ButtonDownTick` + hover 淡入淡出 | ★★★ |

**鼠标语义**：`ButtonPress` 在按下瞬间触发；`ButtonDepress` 仅当抬起时鼠标仍在按钮上（`mIsOver && mWidgetManager->mHasFocus`）才触发（`ButtonWidget.cpp:253-254`），实现"按下移出再移回不误触"。长按按住期间每帧 `ButtonDownTick`（263-264）。

---

### 3.7 Checkbox（Checkbox.h / .cpp）
- **继承关系**：`class Checkbox : public Widget`（Checkbox.h:12）
- **职责**：双态勾选框，按下即翻转并回调 `CheckboxListener`。
- **关键成员变量表**

| 变量 | 类型 | 说明 |
|---|---|---|
| `mListener` | `CheckboxListener*` | 回调目标 |
| `mId` | `int` | id |
| `mChecked` | `bool` | 勾选状态 |
| `mUncheckedImage/mCheckedImage` | `Image*` | 两态图 |
| `mCheckedRect/mUncheckedRect` | `Rect` | 两态子矩形 |
| `mOutlineColor/mBkgColor/mCheckColor` | `Color` | 无图时的程序化绘制颜色 |

- **主要公共成员函数索引表**

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `SetChecked(bool,bool)` | Checkbox.cpp:20 | 设状态并可选回调 | ★★★ |
| `IsChecked()` | Checkbox.cpp:28 | 返回 `mChecked` | ★★ |
| `Draw(Graphics*)` | Checkbox.cpp:33 | 贴图或程序化勾画 | ★★ |
| `MouseDown(x,y,btn,count)` | Checkbox.cpp:73 | 翻转并回调 `CheckboxChecked` | ★★ |

---

### 3.8 EditWidget（EditWidget.h / .cpp）
- **继承关系**：`class EditWidget : public Widget`（EditWidget.h:12）
- **职责**：单行文本编辑框——光标、选区、撤销、剪贴板、密码遮罩、最大长度/像素限制。
- **关键成员变量表**

| 变量 | 类型 | 说明 |
|---|---|---|
| `mId` | `int` | id |
| `mString` / `mPasswordDisplayString` | `SexyString` | 文本 / 密码遮罩串 |
| `mFont` | `Font*` | 字体 |
| `mWidthCheckList` | `WidthCheckList` | 多字体宽度校验表 |
| `mEditListener` | `EditListener*` | 回调 |
| `mShowingCursor` / `mCursorPos` / `mHilitePos` | `bool`/`int`/`int` | 光标闪烁 / 光标位 / 选区锚点 |
| `mBlinkAcc` / `mBlinkDelay` | `int` | 闪烁计数 / 周期 |
| `mLeftPos` | `int` | 视口左端字符索引（横向滚动） |
| `mMaxChars` / `mMaxPixels` | `int` | 最大字符数 / 像素宽 |
| `mPasswordChar` | `SexyChar` | 遮罩字符（0=不遮） |
| `mUndoString/mUndoCursor/mUndoHilitePos/mLastModifyIdx` | 混合 | 撤销快照与连续编辑检测 |
| `mHadDoubleClick` / `mDrawSelOverride` | `bool` | 双击选词修复 / 无焦点也画选区 |

- **主要公共成员函数索引表**

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `SetText(const SexyString&,bool)` | EditWidget.cpp:64 | 设文本并定位光标 | ★★★ |
| `SetFont(Font*,Font*)` | EditWidget.cpp:105 | 设字体+宽度校验字体 | ★★★ |
| `Draw(Graphics*)` | EditWidget.cpp:115 | 背景/选区高亮/文本/光标/边框 | ★★★ |
| `WantsFocus()` | EditWidget.cpp:93 | 恒返回 true | ★★ |
| `GotFocus/LostFocus` | EditWidget.cpp:182/199 | 平板 IME 光标 + 闪烁开关 | ★★ |
| `Update()` | EditWidget.cpp:213 | 光标闪烁 | ★★ |
| `ProcessKey(KeyCode,SexyChar)` | EditWidget.cpp:270 | 完整的编辑状态机（光标/选区/剪切粘贴撤销） | ★★ |
| `KeyDown/KeyChar` | EditWidget.cpp:544/552 | 入口转 `ProcessKey` | ★★ |
| `GetCharAt(int,int)` | EditWidget.cpp:560 | 坐标→字符索引 | ★★★ |
| `FocusCursor(bool)` | EditWidget.cpp:580 | 调整 `mLeftPos` 使光标可见 | ★★★ |
| `MouseDown/MouseUp/MouseDrag` | EditWidget.cpp:606/624/661 | 定位光标/选词/拖拽选区 | ★★★ |
| `HiliteWord()` | EditWidget.cpp:642 | 双击选中整词 | ★★ |
| `EnforceMaxPixels()` | EditWidget.cpp:233 | 按像素截断超宽文本 | ★★★ |
| `AddWidthCheckFont/ClearWidthCheckFonts` | EditWidget.cpp:56/48 | 宽度校验字体管理 | ★★ |

**文本编辑与光标**：光标状态由 `mCursorPos`（插入点）+ `mHilitePos`（选区锚，-1 表示无选区）+ `mLeftPos`（水平滚动）表达。`ProcessKey` 处理左右/Home/End/Back/Delete/回车及 Ctrl 组合（Ctrl+C/X/V/Z 通过 `theChar==3/24/22/26` 识别，`EditWidget.cpp:287-367`）；每次改动后 `EnforceMaxPixels` 截断、`FocusCursor` 滚动、并保存撤销快照（534-539）。选区绘制见 `Draw` 的两次裁剪 pass（125-162）。

---

### 3.9 ListWidget（ListWidget.h / .cpp）
- **继承关系**：`class ListWidget : public Widget, public ScrollListener`（ListWidget.h:17）
- **职责**：可排序、可多列联动（`mParent/mChild`）、带滚动条的行列表，高亮/选中/点击回调。
- **关键成员变量表**

| 变量 | 类型 | 说明 |
|---|---|---|
| `mId` | `int` | id |
| `mFont` | `Font*` | 字体 |
| `mScrollbar` | `ScrollbarWidget*` | 关联滚动条 |
| `mJustify` | `int` | 左/中/右对齐 |
| `mLines` / `mLineColors` | `SexyStringVector`/`ColorVector` | 行文本 / 行颜色 |
| `mPosition` / `mPageSize` | `double` | 滚动位置 / 每页行数 |
| `mHiliteIdx` / `mSelectIdx` | `int` | 悬停高亮 / 选中行 |
| `mListListener` | `ListListener*` | 回调 |
| `mParent` / `mChild` | `ListWidget*` | 多列列表联动链 |
| `mSortFromChild` / `mMaxNumericPlaces` | `bool`/`int` | 排序键拼接方向 / 数字补零位 |
| `mItemHeight` | `int` | 行高 |
| `mDrawSelectWhenHilited/mDoFingerWhenHilited` | `bool` | 显示/光标策略 |

- **主要公共成员函数索引表**

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `AddLine(const SexyString&,bool)` | ListWidget.cpp:149 | 追加/按字母插入行（联动链同步） | ★★★ |
| `SetLine/GetLineCount/GetLineIdx` | ListWidget.cpp:214/220/225 | 行访问 | ★★★ |
| `SetLineColor/SetColor` | ListWidget.cpp:245/234 | 行颜色 | ★★ |
| `RemoveLine/RemoveAll` | ListWidget.cpp:264/287 | 删除行/清空 | ★★★ |
| `Sort(bool)` | ListWidget.cpp:71 | 按排序键稳定排序（联动链） | ★★★ |
| `GetOptimalWidth/Height` | ListWidget.cpp:309/319 | 自适应尺寸 | ★★★ |
| `Draw(Graphics*)` | ListWidget.cpp:336 | 绘制可见行+选中/高亮条+边框 | ★★★ |
| `ScrollPosition(int,double)` | ListWidget.cpp:406 | 滚动回调 | ★★ |
| `SetHilite(int,bool)` | ListWidget.cpp:415 | 设高亮并可选回调 | ★★ |
| `MouseMove` | ListWidget.cpp:424 | 由 y 计算高亮行 | ★★★ |
| `MouseDown(x,y,btn,count)` | ListWidget.cpp:453 | 触发 `ListClicked` | ★★ |
| `MouseLeave` | ListWidget.cpp:459 | 清空高亮 | ★★ |
| `SetSelect(int)` | ListWidget.cpp:476 | 设选中行（联动链） | ★★★ |
| `MouseWheel(int)` | ListWidget.cpp:492 | 滚轮按 5 行步进滚动 | ★★ |

**选中/滚动**：`mHiliteIdx` 由鼠标 y 反算（`MouseMove`，424-451）；点击只发 `ListClicked(mId, mHiliteIdx, count)` 而不改选中，选中由外部 `SetSelect` 显式设置（476-489）。滚动完全委托 `ScrollbarWidget`：`Resize` 设置 `mPageSize`（133-147），`AddLine` 设置 `mMaxValue`（207-208），滚轮直接改 `mScrollbar->SetValue`（492-510）。

---

### 3.10 ScrollbarWidget（ScrollbarWidget.h / .cpp）
- **继承关系**：`class ScrollbarWidget : public Widget, public ButtonListener`（ScrollbarWidget.h:17）
- **职责**：滚动条——上下按钮、滑轨、可拖拽滑块、翻页自动重复，向 `ScrollListener` 报告位置。
- **关键成员变量表**

| 变量 | 类型 | 说明 |
|---|---|---|
| `mUpButton/mDownButton` | `ScrollbuttonWidget*` | 上下（或左右）按钮子控件 |
| `mId` | `int` | id |
| `mValue/mMaxValue/mPageSize` | `double` | 当前值/最大值/每页大小 |
| `mHorizontal` | `bool` | 水平/垂直 |
| `mPressedOnThumb` | `bool` | 是否按住滑块 |
| `mMouseDownThumbPos/mMouseDownX/Y` | `int` | 按下时的滑块位/鼠标偏移 |
| `mUpdateMode/mUpdateAcc/mButtonAcc` | `int` | 翻页模式/重复计数 |
| `mInvisIfNoScroll` | `bool` | 无滚动时隐藏 |
| `mScrollListener` | `ScrollListener*` | 回调 |

- **主要公共成员函数索引表**

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `SetMaxValue/SetPageSize/SetValue` | ScrollbarWidget.cpp:77/84/91 | 设置并 Clamp+回调 | ★★★ |
| `SetHorizontal(bool)` | ScrollbarWidget.cpp:52 | 水平/垂直切换 | ★★ |
| `ResizeScrollbar(...)` | ScrollbarWidget.cpp:60 | 布局并摆放按钮 | ★★ |
| `AtBottom/GoToBottom` | ScrollbarWidget.cpp:99/104 | 底部判定/滚到底 | ★★★ |
| `GetTrackSize/GetThumbSize/GetThumbPosition` | ScrollbarWidget.cpp:129/137/146 | 轨道/滑块几何计算 | ★★★ |
| `ClampValue()` | ScrollbarWidget.cpp:193 | 夹取并同步禁用/隐藏状态 | ★★★ |
| `SetThumbPosition(int)` | ScrollbarWidget.cpp:218 | 由滑块像素反推值 | ★★ |
| `ButtonPress/ButtonDownTick` | ScrollbarWidget.cpp:223/237 | 按钮单步/长按重复 | ★★ |
| `Update()` | ScrollbarWidget.cpp:257 | 翻页自动重复 | ★★ |
| `ThumbCompare(int,int)` | ScrollbarWidget.cpp:290 | 判定点击在滑块上/前/后 | ★★★ |
| `MouseDown/MouseDrag/MouseUp` | ScrollbarWidget.cpp:308/348/339 | 翻页/拖滑块 | ★★★ |
| `Draw(Graphics*)` | ScrollbarWidget.cpp:152 | 画轨道/滑块/翻页高亮 | ★★ |

**与 ScrollbuttonWidget 配合**：构造时创建两个 `ScrollbuttonWidget` 作为子控件并 `AddWidget`（`ScrollbarWidget.cpp:15-32`）；按钮 id 0=上/左、1=下/右，`ButtonPress` 单步 ±1、`ButtonDownTick` 每 25 帧重复（223-255）。滑块拖动通过 `MouseDown` 记录 `mMouseDownThumbPos` 与鼠标偏移，`MouseDrag` 调 `SetThumbPosition` 反算值（348-358）。

---

### 3.11 ScrollbuttonWidget（ScrollbuttonWidget.h / .cpp）
- **继承关系**：`class ScrollbuttonWidget : public ButtonWidget`（ScrollbuttonWidget.h:11）
- **职责**：滚动条两端的小箭头按钮，程序化绘制三角箭头。
- **关键成员变量表**

| 变量 | 类型 | 说明 |
|---|---|---|
| `mHorizontal` | `bool` | 水平布局 |
| `mType` | `int` | 新式方向：1 上 / 2 下 / 3 左 / 4 右（覆盖 mHorizontal 与 mId） |

- **主要公共成员函数索引表**

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `ScrollbuttonWidget(int,ButtonListener*,int)` | ScrollbuttonWidget.cpp:7 | 构造 | ★★ |
| `Draw(Graphics*)` | ScrollbuttonWidget.cpp:17 | 画按钮面+四方向三角箭头 | ★★ |

---

### 3.12 Slider（Slider.h / .cpp）
- **继承关系**：`class Slider : public Widget`（Slider.h:11）
- **职责**：0–1 连续滑杆——点轨跳值、拖滑块连续回调 `SliderListener::SliderVal`。
- **关键成员变量表**

| 变量 | 类型 | 说明 |
|---|---|---|
| `mListener` | `SliderListener*` | 回调 |
| `mVal` | `double` | 当前值 [0,1] |
| `mId` | `int` | id |
| `mTrackImage/mThumbImage` | `Image*` | 轨道三连图/滑块图 |
| `mDragging` | `bool` | 是否拖拽 |
| `mRelX/mRelY` | `int` | 按下点相对滑块左上偏移 |
| `mHorizontal` | `bool` | 水平/垂直 |

- **主要公共成员函数索引表**

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `SetValue(double)` | Slider.cpp:22 | 夹取到 [0,1] 并标脏 | ★★★ |
| `HasTransparencies()` | Slider.cpp:33 | 恒返回 true | ★★ |
| `Draw(Graphics*)` | Slider.cpp:38 | 三段轨道图平铺+滑块定位 | ★★ |
| `MouseDown(x,y,count)` | Slider.cpp:79 | 点滑块开始拖/点轨跳值 | ★★★ |
| `MouseMove` | Slider.cpp:117 | 悬停切换拖拽光标 | ★★ |
| `MouseDrag` | Slider.cpp:139 | 拖拽中换算值并回调 | ★★★ |
| `MouseUp` | Slider.cpp:163 | 结束拖拽并最终回调 | ★★ |
| `MouseLeave` | Slider.cpp:170 | 非拖拽时还原光标 | ★★ |

**拖动**：`MouseDown` 判定点击是否落在滑块内；是则 `mDragging=true` 并记 `mRelX/Y`（79-114），否则按点击位置比例 `SetValue`。`MouseDrag` 用 `(x-mRelX)/(宽-滑块宽)` 反算值，变化时逐次回调 `SliderVal`（139-161）。

---

### 3.13 TextWidget（TextWidget.h / .cpp）
- **继承关系**：`class TextWidget : public Widget, public ScrollListener`（TextWidget.h:16）
- **职责**：可滚动的多行文本查看器——逻辑行/物理行换行映射、彩色文本、区域选择复制。
- **关键成员变量表**

| 变量 | 类型 | 说明 |
|---|---|---|
| `mFont` | `Font*` | 字体 |
| `mScrollbar` | `ScrollbarWidget*` | 滚动条 |
| `mLogicalLines/mPhysicalLines` | `SexyStringVector` | 逻辑行 / 换行后的物理行 |
| `mLineMap` | `IntVector` | 物理行→逻辑行索引映射 |
| `mPosition/mPageSize` | `double` | 滚动位置/每页行数 |
| `mStickToBottom` | `bool` | 自动吸底 |
| `mHiliteArea[2][2]` | `int` | 选区两端 (行内偏移, 物理行号) |
| `mMaxLines` | `int` | 逻辑行上限（默认 2048） |

- **主要公共成员函数索引表**

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `GetLines/SetLines/Clear` | TextWidget.cpp:19/24/29 | 行集合管理 | ★★★ |
| `AddLine(const SexyString&)` | TextWidget.cpp:224 | 追加行并触发换行/滚动 | ★★★ |
| `AddToPhysicalLines(...)` | TextWidget.cpp:179 | 按宽度换行并维护 `mLineMap` | ★★ |
| `GetColorStringWidth(...)` | TextWidget.cpp:106 | 含色码 `0x100` 的串宽 | ★★ |
| `Draw(Graphics*)` | TextWidget.cpp:310 | 绘制可见物理行+选区高亮 | ★★★ |
| `ScrollPosition(int,double)` | TextWidget.cpp:335 | 滚动回调 | ★★ |
| `GetTextIndexAt(int,int,int*)` | TextWidget.cpp:341 | 坐标→(行内偏移,物理行) | ★★★ |
| `MouseDown/MouseDrag` | TextWidget.cpp:364/374 | 设选区两端 | ★★★ |
| `GetSelection()` | TextWidget.cpp:382 | 提取选中文本（去色码） | ★★★ |
| `SelectionReversed/GetSelectedIndices` | TextWidget.cpp:285/292 | 选区方向判定/逐行索引 | ★★ |

---

### 3.14 HyperlinkWidget（HyperlinkWidget.h / .cpp）
- **继承关系**：`class HyperlinkWidget : public ButtonWidget`（HyperlinkWidget.h:9）
- **职责**：带下划线的文本超链接（复用按钮按下/抬起回调）。
- **关键成员变量表**

| 变量 | 类型 | 说明 |
|---|---|---|
| `mColor/mOverColor` | `Color` | 常态/悬停文字色 |
| `mUnderlineSize/mUnderlineOffset` | `int` | 下划线粗细/偏移 |

- **主要公共成员函数索引表**

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `Draw(Graphics*)` | HyperlinkWidget.cpp:19 | 居中文字+下划线 | ★★ |
| `MouseEnter/MouseLeave` | HyperlinkWidget.cpp:39/46 | 悬停变色（标全脏） | ★★ |

---

### 3.15 CursorWidget（CursorWidget.h / .cpp）
- **继承关系**：`class CursorWidget : public Widget`（CursorWidget.h:12）
- **职责**：软件鼠标指针贴图控件（`mMouseVisible=false`，不参与命中）。
- **关键成员变量表**

| 变量 | 类型 | 说明 |
|---|---|---|
| `mImage` | `Image*` | 指针图 |

- **主要公共成员函数索引表**

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `Draw(Graphics*)` | CursorWidget.cpp:12 | 绘制指针图 | ★★ |
| `SetImage(Image*)` | CursorWidget.cpp:18 | 设图并调整尺寸 | ★★ |
| `GetHotspot()` | CursorWidget.cpp:25 | 返回图中心热点 | ★★ |

---

### 3.16 Listener 接口（一行说明）

| 接口 | 文件 | 关键方法（签名） | 说明 |
|---|---|---|---|
| `ButtonListener` | ButtonListener.h:7 | `ButtonPress(int)`/`ButtonPress(int,int)`/`ButtonDepress(int)`/`ButtonDownTick(int)`/`ButtonMouseEnter/Move/Leave` | 按钮按下/抬起/长按/悬停回调，全部空实现 |
| `CheckboxListener` | CheckboxListener.h:7 | `CheckboxChecked(int,bool)` | 勾选状态变化 |
| `DialogListener` | DialogListener.h:7 | `DialogButtonPress(int,int)`/`DialogButtonDepress(int,int)` | 对话框按钮按下/确定（dialogId, buttonId） |
| `EditListener` | EditListener.h:10 | `EditWidgetText(int,const SexyString&)`/`AllowKey`/`AllowChar`/`AllowText` | 编辑提交 + 输入合法性过滤 |
| `ListListener` | ListListener.h:7 | `ListClicked(int,int,int)`/`ListClosed(int)`/`ListHiliteChanged(int,int,int)` | 列表点击/关闭/高亮变化 |
| `ScrollListener` | ScrollListener.h:7 | `ScrollPosition(int,double)` | 滚动位置变化（滚动条/列表/文本共用） |
| `SliderListener` | SliderListener.h:7 | `SliderVal(int,double)` | 滑杆值变化 |

## 4. 字体系统（Font / SysFont / ImageFont）

> 说明：本工程实际文件行数 `ImageFont.cpp` 为 **1772 行**（非 1521）。任务中提到的 `ReadFontFile`、`AddCharacter`、`mLayer/mLayerCount` 在本版反编译源码中并不存在，实际对应的是 `FontData::Load/LoadLegacy/HandleCommand`、`FontLayer::GetCharData`（按需自动插入字符）与 `mFontLayerList/mFontLayerMap`，下文按真实符号名给出。

### 4.1 Font —— 字体基类（Font.h / Font.cpp）

- **继承关系**：`class Font`（无基类，纯接口/数据基类，`Font.h:13`）
- **职责**：定义所有字体的统一量度（ascent/height/行距）与绘制/测量虚接口，供 SysFont、ImageFont 及 Graphics 多态调用。

**关键成员变量**（`Font.h:16-19`）：

| 变量 | 类型 | 说明 |
|---|---|---|
| mAscent | int | 基线到顶部的 ascent 高度（像素） |
| mAscentPadding | int | 平均大写字母上方的额外留白 |
| mHeight | int | 单行总高度 |
| mLineSpacingOffset | int | 与 mHeight 相加得到行间距 |

**主要公共成员函数索引**：

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| Font() / Font(const Font&) / ~Font() | Font.cpp:6 / 14 / 22 | 初始化四度量字段；拷贝构造逐字段复制 | ★★★ |
| virtual int GetAscent() | Font.cpp:26 | 返回 mAscent | ★★★ |
| virtual int GetAscentPadding() | Font.cpp:31 | 返回 mAscentPadding | ★★★ |
| virtual int GetDescent() | Font.cpp:36 | 返回 mHeight - mAscent | ★★★ |
| virtual int GetHeight() | Font.cpp:41 | 返回 mHeight | ★★★ |
| virtual int GetLineSpacingOffset() | Font.cpp:46 | 返回 mLineSpacingOffset | ★★★ |
| virtual int GetLineSpacing() | Font.cpp:51 | 返回 mHeight + mLineSpacingOffset | ★★★ |
| virtual int StringWidth(const SexyString&) | Font.cpp:56 | **基类空实现，返回 0**（必须由子类覆盖） | ★ |
| virtual int CharWidth(SexyChar) | Font.cpp:61 | 用单字符串调用 StringWidth | ★★ |
| virtual int CharWidthKern(SexyChar, SexyChar) | Font.cpp:67 | 默认退化为 CharWidth（忽略前驱字符） | ★★ |
| virtual void DrawString(Graphics*, int, int, const SexyString&, const Color&, const Rect&) | Font.cpp:72 | **基类空实现**（无绘制动作） | ★ |
| virtual Font* Duplicate() = NULL | Font.h:38 | 纯虚克隆接口，子类必须实现 | ★★★ |

- **与 Image 的关系**：Font 本身不持有图像；绘制统一走 `Graphics*`（`DrawString` 签名 `Font.h:36`）。Font.cpp 仅 `#include "Image.h"` 但未直接使用，真正的位图/图集绘制由 ImageFont（贴图）与 SysFont（GDI 文本）各自实现。
- 基类的 `StringWidth`、`DrawString` 是**空壳**（`Font.cpp:56-74`），`GetDescent/GetLineSpacing` 由派生字段推导（`Font.cpp:36/51`）；只有 `Duplicate` 是真正的纯虚。

### 4.2 SysFont —— 系统 TrueType 字体（SysFont.h / SysFont.cpp）

- **继承关系**：`class SysFont : public Font`（`SysFont.h:12`）
- **职责**：封装 Windows GDI 系统字体（CreateFont），用 `TextOut`/`GetTextExtent` 直接走 GDI 渲染与测量，不维护字符位图缓存（运行时每次绘制都直接 TextOut）。

**关键成员变量**（`SysFont.h:15-18`）：

| 变量 | 类型 | 说明 |
|---|---|---|
| mHFont | HFONT | GDI 字体句柄 |
| mApp | SexyAppBase* | 所属应用（取 HWND/DC 用） |
| mDrawShadow | bool | 是否先画黑色投影 |
| mSimulateBold | bool | 是否以 +1/+2 偏移重绘模拟粗体（Win9x 兼容） |

**主要公共成员函数索引**：

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| SysFont(face, pointSize, bold, italics, underline) | SysFont.cpp:13 | 便捷构造，转调 Init(useDevCaps=false) | ★★★ |
| SysFont(app, face, pointSize, script, bold, italics, underline) | SysFont.cpp:18 | 完整构造，转调 Init(useDevCaps=true) | ★★★ |
| void Init(...) | SysFont.cpp:23 | **核心**：GetDC→`-MulDiv(pt, LOGPIXELSY/96, 72)` 求高度→`CreateFontA`（`ANTIALIASED_QUALITY`）→`GetTextMetrics` 填 mHeight/mAscent | ★★ |
| SysFont(const SysFont&) | SysFont.cpp:48 | `GetObject`+`CreateFontIndirect` 复制字体句柄 | ★★★ |
| ~SysFont() | SysFont.cpp:62 | `DeleteObject(mHFont)` | ★★★ |
| ImageFont* CreateImageFont() | SysFont.cpp:67 | 把 256 个 ASCII 字符渲染进 DIB→转 MemoryImage(alpha 取红色通道)→构造 ImageFont 并回填 CharData；**唯一把 SysFont 烘焙成图集字体的入口** | ★★ |
| virtual int StringWidth(...) | SysFont.cpp:161 | 宽字符路径 `GetTextExtentPoint32W`，否则 `DrawTextEx(DT_CALCRECT|DT_NOPREFIX)` | ★★ |
| virtual void DrawString(...) | SysFont.cpp:188 | 见下 | ★★ |
| virtual Font* Duplicate() | SysFont.cpp:318 | `new SysFont(*this)` | ★★★ |

**绘制流程（DrawString, SysFont.cpp:188）**：
1. `dynamic_cast<DDImage*>(g->mDestImage)` 判断目标是否 DirectDraw 表面。
2. **DDImage 路径**（192-231）：`GetDC`→`SelectObject` 字体→`SetBkMode(TRANSPARENT)`→`IntersectClipRect` 裁剪→若 `mDrawShadow` 先 `SetTextColor(黑)` 并 `TextOut(+1,+1)`（`mSimulateBold` 时再 +2）→`SetTextColor(theColor)` 主 `TextOut`（模拟粗体时再 +1）→`ReleaseDC`。y 坐标统一 `theY - mAscent + 1`。
3. **非 DDImage 路径**（232-315）：用白/黑两张 DIB 分别画同串文本，逐像素求差反推**抗锯齿 alpha**（`alpha = min(255+黑-白各通道)`，见 291-300），写入 MemoryImage 后 `g->DrawImage`——用于无表面/内存目标时的合成。
4. 关键点：SysFont **不做字符位图缓存**，每次都是 GDI `TextOut` 现画；`CreateImageFont` 才是一次性把字符栅格化缓存为图集。

### 4.3 ImageFont —— 图集/描述文件字体（ImageFont.h / ImageFont.cpp）

- **继承关系**：`class ImageFont : public Font`（`ImageFont.h:137`）
- **职责**：由「.txt 描述文件 + 若干图片图层」组成的多图层、支持字号缩放与 tag 切换的图集字体；按字符序（order）分桶收集绘制命令后批量渲染。

**辅助结构**：

| 结构 | 声明行 | 说明 |
|---|---|---|
| CharData | ImageFont.h:16 | 单字符数据：`mImageRect`(图集矩形)、`mOffset`(偏移)、`mKerningOffsets`(CharIntMap 字距表)、`mWidth`、`mOrder`(绘制序) |
| FontLayer | ImageFont.h:32 | 一个「图层」= 一张图 + 一批字符 + 颜色/字号/标签过滤 |
| FontData : DescParser | ImageFont.h:71 | 描述文件解析器 + 图层容器，引用计数共享 |
| ActiveFontLayer | ImageFont.h:106 | 运行期激活图层：基图层指针 + 可能重采样出的 `mScaledImage` + `mScaledCharImageRects` |
| RenderCommand | ImageFont.h:123 | 单条渲染命令（图、目标/源矩形、模式、颜色、链表 next） |

**ImageFont 关键成员变量**（`ImageFont.h:140-147`）：

| 变量 | 类型 | 说明 |
|---|---|---|
| mFontData | FontData* | 共享描述数据（引用计数，见 ImageFont.cpp:105/110） |
| mPointSize | int | 当前字号（选择/缩放图层用） |
| mTagVector | StringVector | 当前激活 tag 集合（决定图层可见性） |
| mActivateAllLayers | bool | 是否忽略 tag 全开图层 |
| mActiveListValid | bool | 活动图层缓存是否有效 |
| mActiveLayerList | ActiveFontLayerList | 运行期激活图层列表 |
| mScale | double | 整体缩放系数 |
| mForceScaledImagesWhite | bool | 缩放生成的图强制刷白（供着色） |

**FontLayer 关键成员**（`ImageFont.h:35-57`）：`mFontData`、`mLayerName`、`mRequiredTags/mExcludedTags`、`mCharDataMap`（`CharDataMap`=map<SexyChar,CharData>，即任务中的“字符映射表 mCharData”）、`mColorMult/mColorAdd`、`mImage`(SharedImageRef)、`mDrawMode`、`mOffset`、`mSpacing`、`mMinPointSize/mMaxPointSize/mPointSize`、`mAscent/mAscentPadding/mHeight/mDefaultHeight/mLineSpacingOffset`、`mBaseOrder`、`mUseAlphaCorrection`。

**主要公共成员函数索引**：

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| CharData::CharData() | ImageFont.cpp:10 | 零初始化 mWidth/mOrder | ★★★ |
| FontLayer::FontLayer(FontData*) | ImageFont.cpp:19 | 默认值（mColorMult=白、mColorAdd=透明、mDrawMode=-1、mMinPointSize=-1 等） | ★★★ |
| FontLayer::GetCharData(SexyChar) | ImageFont.cpp:69 | **字符注册/查找入口**：`mCharDataMap.find` 未命中则插入默认 CharData（即“AddCharacter”的真实实现） | ★★★ |
| FontData::FontData / ~FontData | ImageFont.cpp:80 / 92 | 初始化 + 释放 mDefineMap | ★★★ |
| FontData::Ref / DeRef | ImageFont.cpp:105 / 110 | 引用计数增/减，减到 0 自杀 | ★★★ |
| FontData::Error | ImageFont.cpp:118 | 弹窗报错并携带当前行号/行内容 | ★★ |
| FontData::DataToLayer | ImageFont.cpp:135 | 按大写名字从 `mFontLayerMap` 取图层 | ★★ |
| FontData::GetColorFromDataElement | ImageFont.cpp:156 | 解析 `{r,g,b,a}`(0-1 因子) 或单整数颜色 | ★★ |
| FontData::HandleCommand | ImageFont.cpp:182 | **描述文件命令分发器**（见下方命令表，跨度 182-968） | ★ |
| FontData::Load(app, descFileName) | ImageFont.cpp:970 | 记 mSourceFile/mFontErrorHeader 后调 `LoadDescriptor`（继承自 DescParser） | ★★ |
| FontData::LoadLegacy(Image*, descFileName) | ImageFont.cpp:989 | 旧版格式：单图层，`fscanf "%d%d"` 读空格宽与 ascent，逐字符 `%1s%d` 读宽；自动补全大小写映射 | ★ |
| ActiveFontLayer 构造/拷贝/析构 | ImageFont.cpp:1058/1064/1081 | 拥有 mScaledImage 时深拷贝 | ★★ |
| ImageFont(app, descFileName) | ImageFont.cpp:1089 | new FontData→Ref→Load→取默认字号→GenerateActiveFontLayers | ★★★ |
| ImageFont(Image*) | ImageFont.cpp:1102 | 无描述文件构造：单图层挂整图，ascent=图高（供 SysFont::CreateImageFont 手工填数据） | ★★ |
| ImageFont(const ImageFont&) | ImageFont.cpp:1121 | 共享 mFontData(Ref) + 拷贝活动图层 | ★★★ |
| ImageFont(Image*, descFileName) | ImageFont.cpp:1137 | 旧版：LoadLegacy 路径 | ★ |
| ~ImageFont() | ImageFont.cpp:1150 | DeRef mFontData | ★★★ |
| void GenerateActiveFontLayers() | ImageFont.cpp:1166 | **核心**：按字号范围 + 必选/排除 tag 过滤图层，必要时重采样缩放图，汇总 mAscent/mHeight/mAscentPadding/mLineSpacingOffset | ★★ |
| int StringWidth(...) | ImageFont.cpp:1345 | 逐字符累加 CharWidthKern | ★★★ |
| int CharWidthKern(char, prev) | ImageFont.cpp:1359 | 先 `GetMappedChar`，取各激活图层 `mWidth + mSpacing + kerning` 的最大值（含缩放） | ★★ |
| int CharWidth(char) | ImageFont.cpp:1425 | 转调 CharWidthKern(char, 0) | ★★★ |
| void DrawStringEx(...) | ImageFont.cpp:1436 | **绘制核心**（见下） | ★★ |
| void DrawString(...) | ImageFont.cpp:1683 | 包一层 DrawStringEx 传裁剪矩形 | ★★ |
| Font* Duplicate() | ImageFont.cpp:1688 | `new ImageFont(*this)` | ★★★ |
| SetPointSize / SetScale | ImageFont.cpp:1693 / 1699 | 改字号/缩放并置 `mActiveListValid=false` | ★★★ |
| GetPointSize / GetDefaultPointSize | ImageFont.cpp:1705 / 1710 | 取当前/描述默认字号 | ★★★ |
| AddTag / RemoveTag / HasTag | ImageFont.cpp:1715 / 1726 / 1739 | tag 增删查，均大写化；增删后失效活动列表 | ★★★ |
| GetDefine | ImageFont.cpp:1745 | 取描述文件 `Define` 定义的字符串值 | ★★ |
| void Prepare() | ImageFont.cpp:1755 | 惰性重建活动图层（mActiveListValid 为假时） | ★★★ |
| SexyChar GetMappedChar(SexyChar) | ImageFont.cpp:1764 | `mFontData->mCharMap` 查表映射字符，未命中返回原字符 | ★★★ |

**描述文件格式（HandleCommand 支持的命令，ImageFont.cpp:182-968）**：

| 命令 | 行号 | 作用 |
|---|---|---|
| Define | 191 | 定义可复用数据（列表/标量，进 mDefineMap） |
| CreateHorzSpanRectList | 240 | 由 x 起点 + 宽度列表生成水平矩形列表 |
| SetDefaultPointSize | 295 | 设默认字号 |
| SetCharMap | 312 | 字符→字符映射（进 mCharMap） |
| CreateLayer | 344 | 新建命名图层（进 mFontLayerList/mFontLayerMap） |
| CreateLayerFrom | 369 | 拷贝已有图层新建 |
| LayerRequireTags / LayerExcludeTags | 393 / 412 | 图层可见性必选/排除 tag |
| LayerPointRange | 431 | 图层字号范围 mMinPointSize/mMaxPointSize |
| LayerSetPointSize | 458 | 图层基准字号 |
| LayerSetHeight | 480 | 图层行高 |
| LayerSetImage | 502 | 指定图层图片（`mApp->GetSharedImage`，首次 Palletize） |
| LayerSetDrawMode | 535 | 0/1 混合模式 |
| LayerSetColorMult / LayerSetColorAdd | 557 / 573 | 颜色乘/加系数 |
| LayerSetAscent / LayerSetAscentPadding / LayerSetLineSpacingOffset | 589 / 611 / 633 | 图层度量 |
| LayerSetOffset | 655 | 图层整体偏移 |
| LayerSetCharWidths | 673 | 逐字符 mWidth |
| LayerSetSpacing | 708 | 图层字间距 |
| LayerSetImageMap | 733 | **逐字符图集矩形**（越界报错）+ 重算 mDefaultHeight |
| LayerSetCharOffsets | 806 | 逐字符 mOffset |
| LayerSetKerningPairs | 845 | 两字符对 → kerning 偏移（进 CharData.mKerningOffsets） |
| LayerSetBaseOrder | 880 | 图层绘制基序 |
| LayerSetCharOrders | 902 | 逐字符 mOrder（绘制先后） |
| LayerSetExInfo | 936 | 扩展信息（空实现） |

**DrawStringEx 图层绘制流程（ImageFont.cpp:1436-1681）**：
1. 临界区保护 + 清空 256 个 order 桶（`gRenderHead/gRenderTail`，静态池 `POOL_SIZE=4096`，1430-1434）。
2. 逐字符：`GetMappedChar` 取映射字符；遍历所有激活图层，计算目标坐标（`layer.mOffset + char.mOffset`，y 以 `mAscent` 为基准，1505-1506）与缩放后的 `mScaledCharImageRects` 源矩形。
3. 颜色：`out = min(in * mColorMult/255 + mColorAdd, 255)` 逐 RGBA（1540-1543）。
4. 每条渲染写入一个 `RenderCommand`，按 `anOrder = mBaseOrder + char.mOrder` 映射到 `[0,255]` 桶（1568），串入桶链表（**实现图层/字符间的 z 序**）。
5. 第二遍按桶序 0→255 顺序 flush：`SetDrawMode(mMode)`→`SetColor`→`g->DrawImage`（1642-1658），期间开启 `SetColorizeImages(true)`（1467）。
6. 注意（反编译细节）：`DrawStringEx` 接收的 `theClipRect` 参数在函数体内**未被使用**（1436 签名后无任何引用），即本版 ImageFont 绘制不做裁剪；另外缩放重采样循环 1291-1297 用到了上一个循环遗留的 `aCharNum`（应为 `anItr->first`），属明显反编译/移植瑕疵。

### 4.4 字体加载路径（ResourceManager / SexyApp 集成，简要）

- 字体资源在 `ResourceManager.h:86-104` 的 `FontRes` 中登记，全局映射 `mFontMap`（`ResourceManager.h:114`）。
- `ParseFontResource`（`ResourceManager.cpp:384`）解析 XML：读 `image`/`tags` 属性；`path` 以 **`!sys:`** 前缀开头则标记为系统字体，读 `size`(必填)/`bold`/`italic`/`shadow`/`underline`（419-436）。
- `DoLoadFont`（`ResourceManager.cpp:850`）三条构造分支：
  1. SysFont：`new SysFont(path, size, bold, italic, underline)` 后补 `mDrawShadow/mSimulateBold`（856-867，Win9x 用 simulateBold）。
  2. 无独立图、`path` 以 **`!ref:`** 开头：`GetFont` 取引用字体并 `Duplicate()`（871-878）。
  3. 其余：`new ImageFont(mApp, path)` 加载 .txt 描述文件；若有 `image` 属性则 `new ImageFont(image, path)` 走 `LoadLegacy`（869-891）。
  4. 对 ImageFont 校验 `mFontData->mInitialized`，并按 `tags` 属性逐个 `AddTag` 后 `Prepare()`（893-913）。
- 对外接口：`LoadFont`（`ResourceManager.cpp:926`，惰性加载）/ `GetFont`（1141，仅取已加载）/ `GetFontThrow`（1192，缺失即 Fail）。
- 简言之：**PvZ 的字体由 XML 资源清单声明 → ResourceManager 按需实例化 SysFont（GDI 现画）或 ImageFont（.txt 描述 + 图集）→ 游戏侧通过 `LoadFont/GetFont` 取 `Font*` 基类指针使用**，ResourceManager 只负责创建与缓存，度量/绘制全部委托给上面三个类。

## 5. 渲染管线（Graphics / Image / DDImage / MemoryImage / DDInterface / D3DInterface 分层说明 + 函数索引）

> 本章覆盖画布、图像、贴图与设备四个层次。核心继承链：`Image`（抽象接口）→ `MemoryImage`（软件参考实现）→ `DDImage`（DirectDraw 表面加速）；设备层由 `DDInterface`（继承 `NativeDisplay`，DirectDraw 封装）与 `D3DInterface`（独立类，被 `DDInterface` 组合持有，Direct3D7 渲染器）构成。

**端到端绘制路径（一条链讲清）**：

1. `WidgetManager::DrawScreen()`（WidgetManager.cpp:398）以 `mImage`（即 `DDInterface::mScreenImage`，包着 `mDrawSurface` 的那张 `DDImage`）为画布，创建 `Graphics aScrG(mImage)`；
2. 控件 `Draw(g)` 调用 `g->DrawImage / FillRect / DrawLine / DrawString …`；
3. `Graphics` 先做平移（`mTransX/mTransY`）、裁剪（`mClipRect`）、缩放换算，再把调用转发到绑定的 `mDestImage`（`Graphics.cpp` 中 `mDestImage->Blt/FillRect/...`）；
4. `mDestImage` 若是 `DDImage`，其 `Blt/StretchBlt/...` 按 `mIs3D` 选择：**3D 且目标为 `mDrawSurface`** 时转发 `mDDInterface->mD3DInterface->Blt/...`（纹理三角形）；否则锁 `mSurface` 做 DirectDraw blit，或回退 `MemoryImage` 软件写 `mBits`；若 `mDestImage` 是纯 `MemoryImage`，直接软件光栅化写 `mBits`；
5. 整帧画完后 `DDInterface::Redraw()`（DDInterface.cpp:884）把 `mDrawSurface` `Blt/Flip` 到主表面完成上屏。

下文 **5.1** 讲画布与图像（Graphics / Image / DDImage / MemoryImage），**5.2** 讲设备层与基础类型（DDInterface / D3DInterface / D3D8Helper / SharedImage / TriVertex / Color 及 Rect 等基础类型）。

## 5.1 画布与图像（Graphics / Image / DDImage / MemoryImage）

> 说明：本工程实际继承链为 **`Image`（抽象基类）→ `MemoryImage : public Image` → `DDImage : public MemoryImage`**，与常见描述"DDImage 直接继承 Image"不同（`DDImage.h:13` 为 `class DDImage : public MemoryImage`）。此外，源码中**不存在**任务描述所提的 `mImageSurface`、`GetPixel/SetPixel`、`Blend/Resize`、`Composite`、`BltRing` 等符号——像素级访问通过 `GetBits()` 返回裸 `ulong*` 数组直接下标完成，缩放即 `StretchBlt` 族。以下均按真实源码为准。

### 5.1.1 Graphics（Graphics.h / Graphics.cpp）

- **继承关系**：`class Graphics : public GraphicsState`（`Graphics.h:56`）；`GraphicsState`（`Graphics.h:29`）承载全部绘制状态字段；另有 RAII 辅助 `GraphicsAutoState`（`Graphics.h:180`，构造 PushState / 析构 PopState）。
- **职责**：SexyAppFramework 的 2D 绘制上下文/画布状态机。所有 `FillRect/DrawLine/DrawImage/DrawString` 等高层绘制 API 都在此层完成**坐标变换 + 裁剪**，然后**全部转发到绑定的目标图像 `mDestImage`**（一个 `MemoryImage` 或 `DDImage`）的虚函数上；Graphics 本身不保存像素。

**关键成员变量表**（状态字段定义在基类 `GraphicsState`，`Graphics.h:32-48`）：

| 变量 | 类型 | 说明 |
|---|---|---|
| `mDestImage` | `Image*` | **画布目标**：所有绘制最终写入它；构造时传入，缺省落到静态空图 `mStaticImage` |
| `mStaticImage` | `static Image` | 无目标时兜底的静态空图像（`Graphics.cpp:13`） |
| `mTransX / mTransY` | `float` | 平移变换（`Translate` 累加） |
| `mScaleX / mScaleY` | `float` | 缩放系数（`SetScale` 设置，缺省 1） |
| `mScaleOrigX / mScaleOrigY` | `float` | 缩放原点（`SetScale` 时并入当前平移） |
| `mClipRect` | `Rect` | 裁剪矩形（绝对坐标，`SetClipRect/ClipRect` 维护） |
| `mColor` | `Color` | 当前填充/文字颜色 |
| `mFont` | `Font*` | 当前字体 |
| `mDrawMode` | `int` | `DRAWMODE_NORMAL`(0) / `DRAWMODE_ADDITIVE`(1) 混色模式 |
| `mColorizeImages` | `bool` | 是否用 mColor 着色图像 |
| `mFastStretch` | `bool` | 是否用快速（近邻）拉伸 |
| `mWriteColoredString` | `bool` | 是否解析 `^RRGGBB^` 颜色标签 |
| `mLinearBlend` | `bool` | 矩阵/三角纹理是否线性混合 |
| `mIs3D` | `bool` | 目标是否为 3D 主绘制表面（由 `DDImage::Check3D` 判定） |
| `mStateStack` | `GraphicsStateList` | 状态栈（`PushState/PopState`） |
| `mPFActiveEdgeList/mPFNumActiveEdges/mPFPoints/mPFNumVertices` | `Edge*/int/const Point*/int` | 多边形扫描线填充（PolyFill）的活跃边表等临时量 |

**主要公共成员函数索引表**（`Graphics.cpp`）：

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `Graphics(Image* theDestImage = NULL)` | Graphics.cpp:45 | 构造：初始化平移/缩放/颜色等，绑定 mDestImage，ClipRect 置满幅 | ★★ |
| `PushState()/PopState()` | :78 / :84 | 状态入栈/出栈（保存/恢复全部状态） | ★★★ |
| `Create()` | :94 | 拷贝构造当前 Graphics（同状态新建） | ★★★ |
| `SetFont/GetFont` | :104 / :99 | 设置/读取字体 | ★★★ |
| `SetColor/GetColor` | :109 / :114 | 设置/读取绘制色 | ★★★ |
| `SetDrawMode/GetDrawMode` | :119 / :124 | 普通/加法混色模式 | ★★★ |
| `SetColorizeImages/GetColorizeImages` | :129 / :134 | 图像着色开关 | ★★★ |
| `SetFastStretch/GetFastStretch` | :139 / :144 | 快速拉伸开关 | ★★★ |
| `SetLinearBlend/GetLinearBlend` | :149 / :154 | 线性混合开关 | ★★★ |
| `FillRect(int…)/(Rect)` | :170 / :179 | 填充矩形（加 mTrans、与 ClipRect 求交后转 mDestImage） | ★★ |
| `ClearRect(int…)/(Rect)` | :159 / :165 | 清空矩形为全透明（转 mDestImage->ClearRect） | ★★ |
| `DrawRect(int…)/(Rect)` | :184 / :215 | 描边矩形（未裁剪走 DrawRect，裁剪时拆成 4 条 FillRect） | ★★ |
| `DrawString(const SexyString&, int, int)` | :643 | 用 mFont 绘制字符串 | ★★ |
| `DrawLine(int…)` | :617 | 裁剪后画直线（`DrawLineClipHelper` :546 做 Cohen–Sutherland 式裁剪） | ★★ |
| `DrawLineAA(int…)` | :630 | 抗锯齿直线 | ★★ |
| `PolyFill(Point*, int, bool convex=false)` | :266 | 任意多边形扫描线填充（活跃边表算法，转 FillScanLines） | ★★★ |
| `PolyFillAA(Point*, int, bool)` | :360 | 抗锯齿多边形（覆盖率缓冲 + FillScanLinesWithCoverage） | ★★★ |
| `DrawImage(4 重载)` | :649 / :667 / :742 / :750 | 画图（原大/带源矩形/拉伸宽高/目标矩形+源矩形）；带缩放时转 StretchBlt | ★★ |
| `DrawImageF(2 重载)` | :757 / :766 | 浮点坐标画图（转 BltF） | ★★ |
| `DrawImageMirror(3 重载)` | :693 / :698 / :728 | 水平镜像画图（转 BltMirror/StretchBltMirror） | ★★ |
| `DrawImageRotated(2 重载)` | :777 / :813 | 整数坐标旋转（转 DrawImageRotatedF） | ★★ |
| `DrawImageRotatedF(2 重载)` | :795 / :818 | 浮点旋转（转 mDestImage->BltRotated） | ★★ |
| `DrawImageMatrix(2 重载)` | :832 / :838 | 仿射矩阵变换画图（转 BltMatrix） | ★★ |
| `DrawImageTransform(2 重载)` | :904 / :914 | 按 `Transform` 画图（Helper :843 把旋转/缩放/镜像分解为对应调用） | ★★ |
| `DrawImageTransformF(2 重载)` | :909 / :919 | Transform 的浮点版 | ★★ |
| `DrawTriangleTex / DrawTrianglesTex` | :924 / :930 | 纹理三角形（转 BltTrianglesTex，供 3D） | ★ |
| `DrawImageCel(4 重载)` | :1037 / :1042 / :1047 / :1064 | 从动画条按行列格画单帧 | ★★★ |
| `DrawImageAnim` | :1059 | 按时间取动画帧并画 | ★★★ |
| `ClearClipRect()` | :935 | 裁剪重置为整幅目标 | ★★★ |
| `SetClipRect(2 重载)` | :940 / :947 | 设置绝对裁剪（与目标满幅求交） | ★★★ |
| `ClipRect(2 重载)` | :952 / :957 | 与现有裁剪求交（收缩裁剪） | ★★★ |
| `Translate(int,int)` | :962 | 累加平移 | ★★★ |
| `TranslateF(float,float)` | :968 | 浮点累加平移 | ★★★ |
| `SetScale(float,float,float,float)` | :974 | 设置缩放+原点（仅影响 DrawImage） | ★★★ |
| `StringWidth(const SexyString&)` | :982 | 字符串宽度（转 mFont） | ★★★ |
| `DrawImageBox(2 重载)` | :987 / :992 | 九宫格（3×3 切分）拉伸绘制按钮/面板边框 | ★★★ |
| `WriteString(...)` | :1076 | 底层文字绘制：支持 `^RRGGBB^` / `^oldclr^` 颜色标签、对齐 | ★★★ |
| `WriteWordWrapped(...)` | :1180 | 自动换行文字绘制（含对齐、行距、最大宽度回填） | ★★★ |
| `DrawStringColor(...)` | :1343 | 带颜色标签的 DrawString（转 WriteString） | ★★★ |
| `DrawStringWordWrapped(...)` | :1348 | 自动换行版 DrawString | ★★★ |
| `GetWordWrappedHeight(...)` | :1357 | 预估换行后文本高度（内部用临时 Graphics 实测） | ★★★ |

**核心机制**：
- **画布语义**：Graphics 是"轻量绘图代理"。每个绘制调用先在 `Graphics` 层做 `+ mTransX/mTransY` 平移、与 `mClipRect` 求交、必要时按 `mScaleX/mScaleY` 换算目标矩形，然后调用 `mDestImage` 上同名的 `Image` 虚函数。像素真正写入由 `mDestImage`（`MemoryImage::FillRect/Blt/…` 或 `DDImage::FillRect/Blt/…`）完成。
- **状态机/变换矩阵**：`mTransX/mTransY/mScaleX/mScaleY` 构成简化 2D 仿射变换（只支持平移+缩放，旋转走 `DrawImageRotated/Transform/Matrix` 单独路径）。`Translate/SetScale` 只改状态、不落像素；`PushState/PopState` 用 `std::list<GraphicsState>` 栈整体快照恢复（`CopyStateFrom`，`Graphics.cpp:18`）。
- **裁剪**：`mClipRect` 是绝对坐标系下的交集矩形；`SetClipRect` 重置、`ClipRect` 收缩、`ClearClipRect` 复位为整幅。`DrawRect` 在裁剪命中时退化为四条 `FillRect` 以正确描边。
- **与 DDImage 的绑定**：构造时若传入 `DDImage` 且其为 3D 主表面，则 `mIs3D = DDImage::Check3D(theDestImage)`（`Graphics.cpp:68`）；`PolyFill3D`、`DrawImageTransformHelper` 等据此走 D3D 快路径。

### 5.1.2 Image（Image.h / Image.cpp）

- **继承关系**：`class Image`（`Image.h:49`），抽象基类（多数绘制虚函数为空实现，供子类覆写）。
- **职责**：图像对象的公共抽象：尺寸、动画条（cel）几何、动画信息，以及统一的 `FillRect/Blt/StretchBlt/…` 虚接口。所有"图像"（含 `MemoryImage`、`DDImage`）都以 `Image*` 互操作。

**关键成员变量表**：

| 变量 | 类型 | 说明 |
|---|---|---|
| `mDrawn` | `bool` | 是否已被绘制过（性能/调试标记） |
| `mFilePath` | `std::string` | 来源文件路径 |
| `mWidth / mHeight` | `int` | 整幅像素宽高 |
| `mNumRows / mNumCols` | `int` | 动画条的行/列格数（缺省 1×1） |
| `mAnimInfo` | `AnimInfo*` | 动画信息（可空）；`AnimInfo` 含 `mAnimType/mFrameDelay/mNumCels/mPerFrameDelay/mFrameMap/mTotalAnimTime` |

**主要公共成员函数索引表**（`Image.cpp`）：

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `Image()/Image(const Image&)/~Image()` | Image.cpp:6 / :18 / :31 | 构造/深拷贝动画信息/析构 | ★★★ |
| `GetWidth()` / `GetHeight()` | :36 / :41 | 宽/高 | ★★★ |
| `GetCelHeight()` / `GetCelWidth()` | :46 / :51 | 单帧格高/宽（整幅 ÷ 行列数） | ★★★ |
| `GetCelRect(int theCel)` | :56 | 按线性帧号算源矩形（按行优先） | ★★★ |
| `GetCelRect(int col, int row)` | :66 | 按行列算源矩形 | ★★★ |
| `GetAnimCel(int theTime)` | :179 | 由动画信息按时间取当前帧号 | ★★★ |
| `GetAnimCelRect(int theTime)` | :187 | 按时间取帧矩形 | ★★★ |
| `CopyAttributes(Image* from)` | :199 | 复制行列数+动画信息 | ★★★ |
| `GetGraphics()` | :209 | `new Graphics(this)`——以本图为画布创建绘制上下文 | ★★★ |
| `FillRect/ClearRect/DrawRect` | :216 / :220 / :224 | 矩形填充/清空/描边（基类 `DrawRect` 用 4 条 FillRect 实现，其余空） | ★★★ |
| `DrawLine / DrawLineAA` | :232 / :236 | 画线（基类空实现） | — |
| `FillScanLines(Span*,int,…)` | :240 | 批量扫描线段填充（基类已实现：逐段转 FillRect） | ★★★ |
| `FillScanLinesWithCoverage(…)` | :249 | 带覆盖率（AA）的扫描线填充（基类空） | — |
| `PolyFill3D(…)` | :253 | 3D 多边形填充（基类返回 false） | — |
| `Blt / BltF / BltRotated / StretchBlt / BltMatrix / BltTrianglesTex` | :258 / :262 / :266 / :270 / :274 / :278 | 位块传输族（基类空实现，子类覆写） | — |
| `BltMirror / StretchBltMirror` | :283 / :287 | 镜像 blit（基类空） | — |

**`AnimInfo` 成员函数**（`Image.cpp`）：`AnimInfo()` :76、`SetPerFrameDelay` :83、`Compute` :91（据 AnimType 生成 `mFrameMap` 与 `mTotalAnimTime`）、`GetPerFrameCel` :141、`GetCel` :155（按 `AnimType_Once/PingPong/Loop` 取帧）。动画类型枚举 `AnimType` 定义于 `Image.h:22-28`。

### 5.1.3 MemoryImage（MemoryImage.h / MemoryImage.cpp）

- **继承关系**：`class MemoryImage : public Image`（`MemoryImage.h:19`）。
- **职责**：纯软件（内存）位图。像素存于 `mBits` 数组（`ulong*`，ARGB8888 布局），实现全部 `Image` 绘制虚函数的**参考实现**；也是 `DDImage` 的基类（DDImage 在其上叠加 DDSurface 加速路径，软件回退即落到本类）。

**关键成员变量表**：

| 变量 | 类型 | 说明 |
|---|---|---|
| `mBits` | `ulong*` | 像素数据（ARGB，每像素 4 字节，末尾追加 `MEMORYCHECK_ID=0x4BEEFADE` 哨兵） |
| `mBitsChangedCount` | `int` | 位图变更计数 |
| `mColorTable` | `ulong*` | 调色板（可选，256 色） |
| `mColorIndices` | `uchar*` | 调色板索引（可选） |
| `mHasTrans / mHasAlpha` | `bool` | 是否含全透明像素 / 半透明像素（`CommitBits` 扫描得到） |
| `mForcedMode` | `bool` | 是否强制指定 trans/alpha 模式（跳过自动分析） |
| `mIsVolatile` | `bool` | 易失图（可被快速路径直接 blit） |
| `mPurgeBits` | `bool` | 允许释放位图标记 |
| `mNativeAlphaData` | `ulong*` | 预乘 alpha 的本地格式缓存（加速 blit） |
| `mRLAlphaData / mRLAdditiveData` | `uchar*` | 行程编码 alpha / additive 缓存 |
| `mD3DData / mD3DFlags` | `void* / DWORD` | D3D 纹理句柄及标志 |
| `mApp` | `SexyAppBase*` | 宿主应用（注册进全局内存图像列表） |

**主要公共成员函数索引表**（`MemoryImage.cpp`）：

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `MemoryImage()/MemoryImage(SexyAppBase*)/拷贝构造` | :23 / :30 / :36 | 构造（拷贝构造深拷贝全部缓冲区） | ★★ |
| `Init()` | :139 | 初始化各字段并注册到 mApp | ★★ |
| `Clear()` | :1362 | 整幅清零（全透明） | ★★★ |
| `SetBits(ulong*,int,int,bool)` | :1193 | 外部位图拷入（`commitBits` 控制是否立即分析） | ★★★ |
| `Create(int,int)` | :1219 | 按尺寸建空图（置 trans+alpha，惰性分配） | ★★★ |
| `GetBits()` | :1234 | 惰性分配/由调色板或 D3D 还原出 ARGB 位图 | ★★★ |
| `SetImageMode(bool,bool)` | :867 | 强制 trans/alpha 模式 | ★★★ |
| `SetVolatile(bool)` | :874 | 设置易失标记 | ★★ |
| `GetNativeAlphaData(NativeDisplay*)` | :879 | 生成预乘 alpha 的本地像素缓存 | ★ |
| `GetRLAlphaData()` / `GetRLAdditiveData(NativeDisplay*)` | :956 / :994 | 生成行程编码 alpha/additive 缓存 | ★ |
| `CommitBits()` | :810 | 扫描 mBits 判定 `mHasTrans/mHasAlpha`（不清缓存） | ★★★ |
| `BitsChanged()` | :164 | 标记位图脏 + 失效各缓存 | ★★★ |
| `FillRect(Rect,Color,int)` | :1295 | 矩形填充（不透明直写 / 半透明做 alpha 混合） | ★★★ |
| `ClearRect(Rect)` | :1347 | 矩形清零 | ★★★ |
| `DrawLine(double×4,Color,int)` | :661 | 直线（Normal :185 / Additive :475 两种算法，Bresenham） | ★★ |
| `DrawLineAA(...)` | :777 | 抗锯齿直线（NormalAA :693 / AdditiveAA :773） | ★★ |
| `NormalBlt(...)` / `AdditiveBlt(...)` | :1414 / :1374 | 源图 blit 到本图（普通/加法，含 alpha 混合与 trans 跳过） | ★★ |
| `Blt(Image*,int,int,Rect,Color,int)` | :1456 | 按 drawMode 分发 Normal/Additive | ★★ |
| `BltF(...)` | :1478 | 浮点坐标 → 转 `BltRotated(…rot=0)` | ★★ |
| `BltRotated(...)` | :1571 | 旋转 blit（`BltRotatedClipHelper` :1487 先算裁剪包围盒，逐像素反投影采样） | ★★ |
| `SlowStretchBlt(...)` / `FastStretchBlt(...)` | :1629 / :1673 | 慢速（双线性）/ 快速（近邻）拉伸 | ★★ |
| `StretchBlt(...)` | :1738 | 拉伸分发（`StretchBltClipHelper` :1533 计算裁剪后的源/目标矩形） | ★★ |
| `BltMatrix(...)` | :1787 | 仿射矩阵 blit（`BltMatrixHelper` :1754 逐像素逆变换采样，可 blend） | ★★ |
| `BltTrianglesTex(...)` | :1866 | 纹理三角形（`BltTrianglesTexHelper` :1801） | ★ |
| `Palletize()` | :1881 | 调色板化（生成 mColorTable/mColorIndices） | ★ |

> 注：本类**没有** `GetPixel/SetPixel`——像素访问统一为 `GetBits()` 返回的 `ulong*` 配合 `mWidth` 直接下标 `bits[y*mWidth+x]`；**没有** `Blend/Resize` 独立方法——alpha 混合在 `FillRect/NormalBlt` 内联完成，缩放即 `SlowStretchBlt/FastStretchBlt`。

### 5.1.4 DDImage（DDImage.h / DDImage.cpp）

- **继承关系**：`class DDImage : public MemoryImage`（`DDImage.h:13`）。
- **职责**：DirectDraw 贴图。在 `MemoryImage` 的软件位图之上，维护一张 `LPDIRECTDRAWSURFACE` 显存/系统表面 `mSurface`，绘制时优先走 DDSurface 硬件 blit，否则回退到 `MemoryImage` 软件路径；负责 ARGB↔显示格式转换与**色键（ColorKey）透明**。

**关键成员变量表**：

| 变量 | 类型 | 说明 |
|---|---|---|
| `mDDInterface` | `DDInterface*` | 所属 DirectDraw 设备接口 |
| `mSurface` | `LPDIRECTDRAWSURFACE` | 实际 DD 表面（可空） |
| `mSurfaceSet` | `bool` | 是否外部直接 SetSurface 而来 |
| `mNoLock` | `bool` | 禁止锁定表面 |
| `mVideoMemory` | `bool` | 是否建在显存 |
| `mFirstPixelTrans` | `bool` | 透明色即"首像素色"（快速色键策略） |
| `mWantDDSurface` | `bool` | 期望生成表面（PurgeBits 时使用） |
| `mDrawToBits` | `bool` | 强制软件绘制到 mBits |
| `mLockCount` | `int` | 表面锁计数 |
| `mLockedSurfaceDesc` | `DDSURFACEDESC` | 锁定时的表面描述 |

**主要公共成员函数索引表**（`DDImage.cpp`）：

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `DDImage(DDInterface*)` / `DDImage()` | :18 / :25 | 构造（绑定设备，`Init()` 注册到 mDDInterface） | ★ |
| `Init()` | :41 | 初始化各标志 | ★ |
| `Check3D(Image*)/(DDImage*)` | :56 / :65 | 判断是否 3D 主绘制表面 | ★ |
| `LockSurface()/UnlockSurface()` | :70 / :92 | 加锁/解锁表面（引用计数） | ★ |
| `SetSurface(LPDIRECTDRAWSURFACE)` | :109 | 外部注入表面并取宽高 | ★ |
| `GenerateDDSurface()` | :128 | **创建表面核心**：把 mBits 转成 16/32 位显示格式写入表面，并设色键 | ★ |
| `DeleteDDSurface()` | :526 | 释放表面 | ★ |
| `ReInit()` | :538 | 重建（表面失效时恢复位图） | ★ |
| `PurgeBits()` | :546 | 尽量把位图换成表面以省内存 | ★ |
| `DeleteAllNonSurfaceData()` | :583 | 删除除表面外的全部缓冲（mBits/调色板/alpha 缓存） | ★ |
| `DeleteNativeData()/DeleteExtraBuffers()` | :604 / :613 | 删除本地/额外缓冲并释放表面 | ★ |
| `SetVideoMemory(bool)` | :622 | 切换显存/系统内存表面 | ★ |
| `RehupFirstPixelTrans()` | :635 | 主表面格式变化后重取首像素重设色键 | ★ |
| `GetSurface()` | :685 | 惰性生成并返回表面 | ★ |
| `PolyFill3D(...)` | :695 | 3D 快路径填充多边形（否则返回 false 回退扫描线） | ★ |
| `FillRect(Rect,Color,int)` | :706 | 矩形填充：3D 走 D3D；有 alpha/trans 或强制时回退 `MemoryImage::FillRect`；否则 `Normal/AdditiveFillRect` 直接写表面 | ★ |
| `NormalFillRect` / `AdditiveFillRect` | :2075 / :2195 | 直写表面的普通/加法填充 | ★ |
| `DrawLine(...)` | :1660 | 直线分发（Normal :734 / Additive :1305 直写表面） | ★ |
| `DrawLineAA(...)` | :1924 | 抗锯齿直线（NormalAA :1706 / AdditiveAA :1920） | ★ |
| `CommitBits()` | :1970 | 无表面时转 `MemoryImage::CommitBits` | ★ |
| `Create(int,int)` | :1979 | 建图（释放旧 mBits，惰性） | ★★ |
| `BitsChanged()` | :1991 | 标记脏 + 释放表面（转 MemoryImage 后再释放 mSurface） | ★ |
| `GetBits()` | :2000 | 锁表面 → 把 16/32 位表面像素读回 ARGB 位图 | ★ |
| `NormalBlt(...)` | :2283 | 核心 blit：源为 DD 且满足条件走硬件 `Surface::Blt`（含 `DDBLT_KEYSRC`）；有 alpha 走 `DDI_AlphaBlt.inc`；易失走 `DDI_NormalBlt_Volatile.inc` | ★ |
| `AdditiveBlt(...)` | :2510 | 加法 blit 直写表面 | ★ |
| `NormalBltMirror(...)` / `AdditiveBltMirror(...)` | :2443 / :2565 | 镜像 blit 直写表面 | ★ |
| `Blt(Image*,int,int,Rect,Color,int)` | :2624 | **blit 总入口**：3D 走 D3D；否则按 drawMode 分发，末了 `DeleteAllNonSurfaceData` | ★ |
| `BltMirror(...)` | :2697 | 镜像 blit 总入口 | ★ |
| `BltF(...)` | :2725 | 浮点坐标（转 BltRotated 或直接软件） | ★ |
| `BltRotated(...)` | :2750 | 旋转 blit（3D 走 D3D，否则转 `MemoryImage::BltRotated`） | ★ |
| `StretchBlt(...)` | :2846 | 拉伸：源为 DD 且快速 → 硬件 `Blt` 拉伸；否则软件 Stretch 到临时 `MemoryImage` 再 blit | ★ |
| `StretchBltMirror(...)` | :2996 | 镜像拉伸 | ★ |
| `BltMatrix(...)` | :3092 | 矩阵 blit（3D 走 D3D，否则转 `MemoryImage::BltMatrix`） | ★ |
| `BltTrianglesTex(...)` | :3124 | 纹理三角形（3D 走 D3D，否则转 `MemoryImage`） | ★ |
| `Palletize()` | :3154 | 调色板化（转 MemoryImage 后视情况） | ★ |
| `FillScanLinesWithCoverage(...)` | :3169 | AA 扫描线填充（转 MemoryImage） | ★ |

**色键 / Create 流程要点**：
- `GenerateDDSurface()`（:128）先 `CommitBits`，`mHasAlpha` 时拒绝（半透明无法用色键）。按 `mVideoMemory` 建表面（:151），`CreateSurface` 后 `LockSurface`，将 32 位 ARGB `mBits` 逐像素转成显示位深（16/32 位，:178/:204）写入表面。透明色选取：`mFirstPixelTrans` 时取首像素（:219），否则扫描"透明像素未占用、实心像素已占用"的颜色（:239-260 区段），随后 `SetColorKey(DDCKEY_SRCBLT, …)`（:345-346、:489-490）。
- `RehupFirstPixelTrans()`（:635）在主表面位深变化后，锁定表面取首像素重新设色键（:664-665、:678-679）。

### 5.1.5 DDImage → Graphics 绘制路径（专项说明）

整条渲染链是 **Graphics（状态/裁剪/变换）→ Image 虚接口 → MemoryImage（软件参考实现）→ DDImage（表面加速，回退软件）**：

1. **绑定**：`Image::GetGraphics()`（`Image.cpp:209`）执行 `new Graphics(this)`，把目标图绑定为 `mDestImage`；引擎每帧对屏幕后备表面 `DDImage` 调用 `GetGraphics()` 得到画布。`Graphics` 构造时用 `DDImage::Check3D` 判断该图是否 3D 主绘制表面并设 `mIs3D`（`Graphics.cpp:68`）。

2. **累积绘制**：对 `Graphics` 的每个绘制调用（如 `FillRect`，`Graphics.cpp:170`）：先平移 `+ mTransX/mTransY`、与 `mClipRect` 求交，然后调用 `mDestImage->FillRect(...)`。若 `mDestImage` 是 `DDImage`，`DDImage::FillRect`（`DDImage.cpp:706`）按 `mIs3D / mHasAlpha / mFirstPixelTrans` 选择 D3D、软件回退或直写表面；若目标是纯 `MemoryImage`，直接写 `mBits`。

3. **贴图 Blt**：`Graphics::DrawImage`（`Graphics.cpp:649`）裁剪后转 `mDestImage->Blt(theImage, …)`。若 `mDestImage` 为 `DDImage`，`DDImage::Blt`（`DDImage.cpp:2624`）：
   - 目标 3D → `mDDInterface->mD3DInterface->Blt(...)`（:2674）；
   - 源/目标都是 `DDImage` 且无 alpha、颜色为白 → **纯硬件 `Surface::Blt`（`DDBLT_WAIT|DDBLT_KEYSRC`）**（:2651-2663），源表面若 `mHasTrans` 则用色键透掉透明像素；
   - 有 alpha 或源为软件图 → `NormalBlt` 内按需 `LockSurface` 后用 `.inc` 内联模板逐像素写（:2283 起）；
   - 源非 DD/无表面 → 直接 `MemoryImage::Blt` 软件路径（:2680）。

4. **上屏**：屏幕后备表面本身是一张与主表面同格式的 `DDImage`，引擎在一个 `Graphics` 上下文里完成整帧绘制后，由 `DDInterface`/`D3DInterface` 把该后备表面 `Blt`/`Flip` 到主表面（本模块之外的设备层逻辑），即"DDImage::Blt 走 DDSurface → 设备层"的最终呈现路径。

---

**可复用性小结**：`Image` 的几何/动画接口与 `Graphics` 的绘制 API、`DrawImageBox`、`WriteWordWrapped` 等**纯逻辑**部分可 ★★★ 独立移植；`MemoryImage` 的 `FillRect/Blt/StretchBlt` 软件像素算法（ARGB 混合、Bresenham、双线性）★★ 可移植但依赖 `SexyAppBase` 等宿主；`DDImage` 的 `GenerateDDSurface`/`NormalBlt` 等 ★ 与 DirectDraw/D3D 设备层强耦合，仅能作为整体框架的一部分复用。

**关键源文件**：`Graphics.h/.cpp`、`Image.h/.cpp`、`MemoryImage.h/.cpp`、`DDImage.h/.cpp`（均在 `D:\dsh-project\LawnProject\src\SexyAppFramework\`）。

## 5.2 设备层与基础类型（DDInterface / D3DInterface / D3D8Helper / SharedImage / TriVertex / Color）

> **重要勘误（与任务前提的差异）**：本工程（PvZ 反编译版 SexyAppFramework）与任务描述所依据的新版框架（CSproj 开源版）结构**不同**，以下全部按当前源码实录：
> - `DDInterface` **不是抽象设备接口**，而是 DirectDraw 的具体实现；它**没有** `InitDriver/InitFromDDraw/InitDevice/CreateImage/Blt/StretchBlt/BltImage` 等纯虚方法（全工程 grep 均无这些符号）。
> - `D3DInterface` **不继承** `DDInterface`，它是独立类，由 `DDInterface` 组合持有（`mD3DInterface`），通过 `InitFromDDInterface` 借用 DD7 对象初始化。
> - `D3D8Helper` 只有 `GetD3D8AdapterInfo`（显卡信息枚举），**不涉及**顶点缓冲/锁定。
> - 行号以当前文件为准：`DDInterface.h` 136 行 / `.cpp` 1432 行、`D3DInterface.h` 154 行 / `.cpp` 2208 行、`D3D8Helper.cpp` 276 行（与任务括号中的旧数字均不一致）。

### 总览：分层与绘制提交路径

- **`NativeDisplay`**（NativeDisplay.h:10）是所有显示设备的共同基类，只保存像素格式/颜色掩码（`mRGBBits`、`mRedMask/GreenMask/BlueMask`、`mRedBits/.../mRedShift/...`），`DDInterface : public NativeDisplay`。
- **`DDInterface`** 是 DirectDraw 侧设备拥有者：创建 DirectDraw 对象、主/后备/绘制表面，处理窗口化/全屏、宽屏呈现矩形、VSync、软件光标，最终用 `Blt`/`Flip` 上屏。
- **`D3DInterface`** 是 Direct3D 7 渲染器（独立类，非继承），由 `DDInterface` 构造时 `new` 出来（DDInterface.cpp:62），`Init` 末尾调 `InitFromDDInterface` 借用 `mDD7` 与 `mDrawSurface` 的 DD7 版本 `mDDSDrawSurface`，把 `mDrawSurface` 当作 D3D 渲染目标直接画纹理三角形。
- **`DDImage : public MemoryImage`**（DDImage.h:13）持有一张 DirectDraw 表面 `mSurface` 与 `mDDInterface` 指针。`DDInterface::mScreenImage`（DDInterface.h:79）就是包着 `mDrawSurface` 的 `DDImage`。
- **绘制提交**：`DDImage::Check3D(this)`（DDImage.cpp:65）判定 `mDDInterface->mIs3D && mSurface==mDDInterface->mDrawSurface`。3D 模式下，所有面向屏幕的 `Blt/FillRect/DrawLine/StretchBlt/...` 经 `Check3D` 短路转发到 `mDDInterface->mD3DInterface->...`（如 DDImage.cpp:2638→2674 转发 `D3DInterface::Blt`）；非 3D 或非屏幕表面则回落到 `MemoryImage` 的软件光栅化路径（写 `mBits`，再由 `CommitBits` 传到 DDraw 表面）。

---

### 5.2.1 DDInterface（DDInterface.h / DDInterface.cpp）

- **继承关系**：`class DDInterface : public NativeDisplay`（DDInterface.h:24）
- **职责**：DirectDraw 设备的完整封装——建 DDraw/表面、全屏/窗口模式、宽屏与呈现矩形、VSync 与软件光标、把 `mDrawSurface` 上屏；并作为 3D 渲染器 `D3DInterface` 的宿主。

**关键成员变量**（行号 = DDInterface.h）：

| 变量 | 类型 | 说明 |
|---|---|---|
| mApp | SexyAppBase* | 所属应用（39） |
| mD3DInterface | D3DInterface* | 拥有的 D3D7 渲染器（40） |
| mD3DTester | D3DTester* | 3D 能力探测（41） |
| mIs3D | bool | 是否启用 3D 渲染（42） |
| mCritSect / mInRedraw | CritSect / bool | 重入锁与重绘标记（44–45） |
| mDD / mDD7 | LPDIRECTDRAW / LPDIRECTDRAW7 | DD 与 DD7 接口（46–47） |
| mPrimarySurface / mSecondarySurface / mDrawSurface | LPDIRECTDRAWSURFACE | 主表面/后备缓冲/离屏绘制表面（48–50） |
| mWidth / mHeight / mAspect | int/int/Ratio | 应用逻辑分辨率与宽高比（51–53） |
| mDesktopWidth/Height, mDesktopAspect | int/int/Ratio | 桌面分辨率与比例（54–56） |
| mIsWidescreen | bool | 是否宽屏补偿（57） |
| mDisplayWidth/Height, mDisplayAspect | int/int/Ratio | 实际显示模式（58–60） |
| mPresentationRect | Rect | 绘制缓冲映射到屏幕的矩形（62） |
| mFullscreenBits / mRefreshRate / mMillisecondsPerFrame | int/DWORD/DWORD | 全屏色深/刷新率/帧间隔（63–65） |
| mRed/Green/BlueAddTable | int* | 加法饱和查找表（68–70） |
| mRed/Green/BlueConvTable[256] | ulong | 8bit→表面掩码转换表（72–74） |
| mInitialized / mHWnd / mIsWindowed | bool/HWND/bool | 初始化状态与窗口（76–78） |
| mScreenImage | DDImage* | 包 `mDrawSurface` 的屏幕图像（79） |
| mDDImageSet | DDImageSet | 已登记 DDImage 集合（80） |
| mVideoOnlyDraw / mInitCount | bool/ulong | 仅视频绘制标记/初始化计数（81–82） |
| mCursorWidth/Height…mNewCursorAreaImage | 各类型 | 软件光标缓冲与状态（84–95） |
| mErrorString | std::string | 最近错误文本（97） |

**主要公共成员函数索引**（行号 = DDInterface.cpp 定义处）：

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `DDInterface(SexyAppBase*)` / `~DDInterface()` | DDInterface.cpp:24/71 | 构造（new D3DInterface、取 DirectDrawCreate 函数指针）/析构 | ★★ |
| `static std::string ResultToString(int)` | :83 | 结果码转文本 | ★★ |
| `bool GotDXError(HRESULT, const char*)` | :106 | HRESULT 失败记录错误 | ★★ |
| `DDImage* GetScreenImage()` | :120 | 取屏幕图像 | ★★ |
| `HRESULT CreateSurface(DDSURFACEDESC2*, LPDIRECTDRAWSURFACE*, void*)` | :125 | 建表面（DD7 优先，回退 DD），校验 16/32 位色深 | ★★ |
| `void ClearSurface(LPDIRECTDRAWSURFACE)` | :186 | 锁定并清零表面 | ★★ |
| `bool Do3DTest(HWND)` | :208 | 用 D3DTester 探测 3D 支持 | ★ |
| `int Init(HWND, bool IsWindowed)` | :234 | **核心**：建 DD、主/绘制表面、宽屏处理、色掩码/转换表、建光标缓冲、末尾 `InitFromDDInterface` | ★★ |
| `void SetVideoOnlyDraw(bool)` | :669 | 切换“仅视频内存”绘制（重建 mScreenImage 指向次级表面） | ★★ |
| `void RemapMouse(int&, int&)` | :701 | 鼠标坐标从呈现矩形反算到逻辑坐标 | ★★ |
| `ulong GetColorRef(ulong)` | :710 | RGB(0xRRGGBB)→表面像素值 | ★★ |
| `void AddDDImage/RemoveDDImage(DDImage*)` | :731/738 | 登记/注销 DDImage | ★★ |
| `void Remove3DData(MemoryImage*)` | :747 | 转发给 D3D 清纹理 | ★ |
| `void Cleanup()` | :752 | 释放全部表面与 DD 对象 | ★★ |
| `bool CopyBitmap(surface, HBITMAP, ...)` | :821 | 用 GDI BitBlt/StretchBlt 把位图拷进表面 | ★★ |
| `bool Redraw(Rect* clipRect=NULL)` | :884 | **核心**：3D 时先 `Flush`，再 VSync/扫描线等待 + `Blt`/`Flip` 上屏、处理光标还原 | ★ |
| `RestoreOldCursorAreaFrom / DrawCursorTo / MoveCursorTo` | :1113/1152/1241 | 软件光标三件套 | ★ |
| `SetCursorImage(Image*)` / `SetCursorPos(int,int)` | :1398/1412 | 设光标图/位置 | ★ |

---

### 5.2.2 D3DInterface（D3DInterface.h / D3DInterface.cpp）

- **继承关系**：`class D3DInterface`（D3DInterface.h:84，**无基类**；被 `DDInterface` 组合持有）
- **职责**：Direct3D 7 渲染器——把 `MemoryImage` 分块转成纹理（`TextureData`），用 D3DTLVERTEX 纹理三角形完成 Blt/Stretch/旋转/变换/图元绘制，`BeginScene/EndScene` 管理场景。

**关键成员变量**（行号 = D3DInterface.h）：

| 变量 | 类型 | 说明 |
|---|---|---|
| mHWnd / mWidth / mHeight | HWND/int/int | 窗口与逻辑尺寸（87–89） |
| mDD | LPDIRECTDRAW7 | 借自 DDInterface 的 DD7（91） |
| mDDSDrawSurface | LPDIRECTDRAWSURFACE7 | `mDrawSurface` 的 DD7 版（渲染目标，92） |
| mZBuffer | LPDIRECTDRAWSURFACE7 | Z 缓冲（预留，实际未附加，93） |
| mD3D / mD3DDevice | LPDIRECT3D7 / LPDIRECT3DDEVICE7 | D3D 与设备（94–95） |
| mD3DViewport | D3DVIEWPORT7 | 视口（96） |
| mSceneBegun / mIsWindowed | bool/bool | 场景进行中/窗口化（98–99） |
| mImageSet | std::set<MemoryImage*> | 已建纹理的图像集（101–102） |
| mTransformStack | std::list<SexyMatrix3> | 全局变换栈（104–105） |
| mErrorString | static std::string | 错误文本（107） |

**主要公共成员函数索引**（行号 = D3DInterface.cpp）：

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `D3DInterface()` / `~D3DInterface()` | :72/106 | 初始化纹理尺寸缺省值 / Cleanup | ★★ |
| `static bool CheckDXError(HRESULT, const char*)` | :51 | 失败时写 mErrorString 与注册表 | ★★ |
| `static void MakeDDPixelFormat(PixelFormat, DDPIXELFORMAT*)` | :113 | 由枚举构造 DDPIXELFORMAT | ★★ |
| `static PixelFormat GetDDPixelFormat(LPDDPIXELFORMAT)` | :156 | 反向：DDPIXELFORMAT→枚举 | ★★ |
| `bool InitD3D()` | :250 | **设备创建**：QI IDirect3D7→CreateDevice、读 caps、枚举纹理格式、建 ZBuffer、Clear | ★★ |
| `bool InitFromDDInterface(DDInterface*)` | :334 | 借 DD7/绘制表面/窗口，调 InitD3D | ★ |
| `bool PreDraw()` | :354 | 惰性 BeginScene + 设置混合/过滤/光照等 RenderState | ★★ |
| `void Flush()` | :2198 | EndScene，结束场景 | ★★ |
| `void Cleanup()` | :1806 | 释放全部 TextureData、设备、D3D | ★★ |
| `bool CreateImageTexture(MemoryImage*)` | :1686 | 为图像建 TextureData（惰性、线程安全） | ★★ |
| `bool RecoverBits(MemoryImage*)` | :1712 | 从纹理回读像素到图像 bits | ★ |
| `void SetCurTexture(MemoryImage*)` | :1754 | 设 0 号纹理（NULL 则清空） | ★★ |
| `void PushTransform(const SexyMatrix3&, bool concat=true)` / `PopTransform()` | :1771/1784 | 全局变换栈压/弹 | ★★ |
| `void RemoveMemoryImage(MemoryImage*)` | :1792 | 删除图像的 TextureData | ★ |
| `void Blt(Image*, float x, float y, const Rect& src, const Color&, int drawMode, bool linearFilter)` | :1879 | 基础 1:1 纹理贴片 | ★★ |
| `void BltMirror(...)` | :1905 | 水平镜像（构造 -x 缩放变换） | ★★ |
| `void BltClipF(...)` | :1918 | 带裁剪矩形、浮点坐标贴图 | ★★ |
| `void StretchBlt(Image*, const Rect& dest, const Rect& src, const Rect* clip, const Color&, int, bool fastStretch, bool mirror)` | :1928 | 缩放（构造 Scale 变换） | ★★ |
| `void BltRotated(...)` | :1948 | 绕中心旋转 | ★★ |
| `void BltTransformed(Image*, const Rect* clip, ..., const SexyMatrix3&, bool linearFilter, float x=0, float y=0, bool center=false)` | :1961 | 通用仿射变换贴图（含全局变换栈叠加） | ★★ |
| `void DrawLine(double x1,y1,x2,y2, const Color&, int)` | :2004 | 画线（LINESTRIP，3 顶点） | ★★ |
| `void FillRect(const Rect&, const Color&, int)` | :2047 | 纯色矩形（TRIANGLESTRIP） | ★★ |
| `void DrawTriangle(const TriVertex& ×3, const Color&, int)` | :2090 | 纯色三角形 | ★★ |
| `void DrawTriangleTex(..., Image* tex, bool blend)` | :2148 | 单纹理三角形（转发 DrawTrianglesTex） | ★★ |
| `void DrawTrianglesTex(const TriVertex [][3], int, ..., Image* tex, float tx, float ty, bool blend)` | :2156 | 纹理三角形批 | ★★ |
| `void DrawTrianglesTexStrip(const TriVertex[], int, ...)` | :2178 | 三角带（按 100 个/批切分） | ★★ |
| `void FillPoly(const Point[], int, const Rect* clip, const Color&, int, int tx, int ty)` | :2112 | 三角形扇填充多边形（可裁剪） | ★★ |
| `void SetupDrawMode(int, const Color&, Image*)`（protected） | :1847 | 配置 NORMAL/加色混合（SRCBLEND/DESTBLEND） | ★★ |
| `void UpdateViewport()`（protected） | :208 | 更新视口 | ★★ |

**内部辅助（file-local static，行号 D3DInterface.cpp）**：
`DisplayError`:29、`EnumZBufferCallback`:237、`CreateTextureSurface`:411（建一张纹理表面）、`CopyImageToTexture8888/4444/565/Palette8`:448/516/591/667、`CopyTexture*ToImage`:496/568/643/689、`CopyImageToTexture`:715、`GetClosestPowerOf2Above`:756、`IsPowerOf2`:767、`GetBestTextureDimensions`:780（计算最优分块尺寸）、`SetLinearFilter`:1159、`DrawPolyClipped`:1386、`DoPolyTextureClip`:1414。

---

### 5.2.3 TextureData / TextureDataPiece / PixelFormat / D3DImageFlags（D3DInterface.h）

- **声明**：`struct TextureDataPiece`（:30）、`enum PixelFormat`（:38）、`struct TextureData`（:49）、`enum D3DImageFlags`（:20）
- **职责**：把一张（可能超大/非 2 的幂）`MemoryImage` 划分成多张显卡纹理片的元数据与操作集合。

**关键成员**（TextureData）：

| 变量 | 类型 | 说明 |
|---|---|---|
| mTextures | std::vector<TextureDataPiece> | 纹理片数组（54） |
| mPalette | LPDIRECTDRAWPALETTE | 8bit 调色板（55） |
| mWidth / mHeight | int | 源图像尺寸（57） |
| mTexVecWidth / mTexVecHeight | int | 分块网格列/行数（58） |
| mTexPieceWidth / mTexPieceHeight | int | 内部标准块尺寸（59） |
| mBitsChangedCount | int | 用于脏检测的 bits 版本（60） |
| mTexMemSize | int | 显存占用统计（61） |
| mMaxTotalU / mMaxTotalV | float | 总 UV 跨度（62） |
| mPixelFormat / mImageFlags | PixelFormat/DWORD | 像素格式与 D3DImageFlags（63–64） |

**主要成员函数**（行号 D3DInterface.cpp）：`CreateTextureDimensions`:905（算内部/右/下/角四类块尺寸）、`CreateTextures`:985（选像素格式、逐块建表面+`CopyImageToTexture`）、`CheckCreateTextures`:1089（脏检测后按需重建）、`GetTexture`:1097 / `GetTextureF`:1128（按像素/浮点坐标取块并算 UV）、`Blt`:1177（遍历源矩形逐块画 TRIANGLESTRIP）、`BltTransformed`:1435（变换+裁剪多边形光栅）、`BltTriangles`:1546（按 TriVertex 纹理三角形）。

**说明**：像素格式优先级 A8R8G8B8 → R5G6B5(无 alpha) → A4R4G4B4 → Palette8（见 :990–1023）；块尺寸由 `GetBestTextureDimensions` 受 `D3DImageFlag_MinimizeNumSubdivisions`/`Use64By64Subdivisions` 与硬件 caps 约束（:780）。

---

### 5.2.4 D3D8Helper（D3D8Helper.h / D3D8Helper.cpp）

- **继承关系**：无类，仅一个自由函数。
- **职责**：因 `d3d.h` 与 `d3d8.h` 头冲突，把 D3D8 需要的类型手工抄进本文件，运行时 `LoadLibrary("d3d8.dll")` + `Direct3DCreate8` 只为了取默认适配器的 GUID/驱动/描述（用于 3D 测试/日志）。

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `bool Sexy::GetD3D8AdapterInfo(GUID&, std::string& driver, std::string& desc)` | D3D8Helper.h:8 / .cpp:256 | 枚举默认 D3D8 适配器信息，用完释放 d3d8.dll | ★★ |

> 注：D3D8Helper.cpp 大部分是 D3D8 类型的手工再声明（D3DFORMAT/D3DDEVTYPE/D3DADAPTER_IDENTIFIER8/D3DPRESENT_PARAMETERS/IDirect3D8 接口，行 9–219），**无**顶点缓冲/锁定功能。

---

### 5.2.5 SharedImage / SharedImageRef（SharedImage.h / SharedImage.cpp）

- **声明**：`class SharedImage`（SharedImage.h:13）、`class SharedImageRef`（:24）、`typedef std::map<std::pair<std::string,std::string>, SharedImage> SharedImageMap`（:22）
- **职责**：以引用计数共享已加载 `DDImage`（资源去重：同名同路径只加载一次），`SharedImageRef` 是 RAII 引用句柄，可退化为“未共享的 `MemoryImage`”。

**SharedImage**：

| 变量 | 类型 | 说明 |
|---|---|---|
| mImage | DDImage* | 共享的图像（16） |
| mRefCount | int | 引用计数（17） |

**SharedImageRef 成员**：`mSharedImage`（SharedImage*）、`mUnsharedImage`（MemoryImage*）、`mOwnsUnshared`（bool）（SharedImage.h:27–29）。

**主要成员函数索引**（行号 SharedImage.cpp）：

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `SharedImage()` | :7 | 置空 mImage/mRefCount | ★ |
| `SharedImageRef()` / 拷贝构造 / `SharedImageRef(SharedImage*)` | :22/13/29 | 构造并 ++mRefCount | ★ |
| `~SharedImageRef()` | :39 | 调 Release | ★ |
| `void Release()` | :44 | --mRefCount，归零置 `gSexyAppBase->mCleanupSharedImages=true`（延迟清理）；mOwnsUnshared 则 delete | ★ |
| `operator=`(Ref/SharedImage*/MemoryImage*) | :57/66/74 | 赋新引用（先 Release 旧） | ★ |
| `operator->` / `operator Image*/MemoryImage*/DDImage*` | :81/87/92/100 | 取底层图像指针（未共享优先） | ★ |

---

### 5.2.6 TriVertex（TriVertex.h）

- **声明**：`class TriVertex`（TriVertex.h:9）
- **职责**：供 D3D 纹理三角形绘制使用的 2D 顶点结构（对应 D3DTLVERTEX 的投影前数据源）。

| 变量 | 类型 | 说明 |
|---|---|---|
| x, y | float | 屏幕坐标（12） |
| u, v | float | 纹理坐标（12） |
| color | DWORD | ARGB；**0 = 用调用处颜色**（13，见 D3DInterface.cpp:1542 宏 `GetColorFromTriVertex`） |

构造函数（:16–19）：无参 / (x,y) / (x,y,u,v) / (x,y,u,v,color)，均把 `color` 初始化为 0（可复用 ★★★）。

---

### 5.2.7 Color（Color.h / Color.cpp）

- **声明**：`struct SexyRGBA { unsigned char b,g,r,a; }`（Color.h:10，`#pragma pack(1)`）、`class Color`（:13）
- **职责**：RGBA 颜色值类型（每个分量 `int`），提供与 0xAARRGGBB 整数、`SexyRGBA`、数组之间的互转，供绘制 API 使用。

**关键成员**：`mRed/mGreen/mBlue/mAlpha`（Color.h:16–19）、`static Color Black/White`（:21–22，定义 Color.cpp:5–6）。

**主要成员函数索引**（行号 Color.cpp）：

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `Color()` | :8 | 黑、alpha=255 | ★★★ |
| `Color(int theColor)` | :16 | 从 0xAARRGGBB 拆（alpha=0 视为 255） | ★★★ |
| `Color(int,int alpha)` / `Color(int r,g,b)` / `Color(int r,g,b,a)` | :26/34/42 | 各种组合 | ★★★ |
| `Color(const SexyRGBA&)` / `Color(const uchar*)` / `Color(const int*)` | :50/58/66 | 从结构/数组 | ★★★ |
| `GetRed/Green/Blue/Alpha()` | :74/79/84/89 | 取值 | ★★★ |
| `operator[](int) const/非const` | :94/113 | 按索引 0..3 取 r/g/b/a | ★★★ |
| `ulong ToInt() const` | :130 | 打包为 0xAARRGGBB | ★★★ |
| `SexyRGBA ToRGBA() const` | :135 | 转 SexyRGBA | ★★★ |
| `operator==/!=` | :146/155 | 分量比较 | ★★★ |

> 与 D3DCOLOR 的转换不在本类，而由调用点 `RGBA_MAKE(r,g,b,a)` 宏完成（见 D3DInterface.cpp:1191、2012 等）；`Color` 只产出 0xAARRGGBB 整数，字节序上即标准 D3DCOLOR。

---

### 5.2.8 基础类型（Rect / Point / Insets / SexyMatrix / SexyVector）

- **Rect**（Rect.h）：模板 `TRect<_T>`（:12，成员 `mX/mY/mWidth/mHeight` :15–18），提供 `Intersects`:36、`Intersection`:44、`Union`:56、`Contains`:65/71、`Offset`:77/83、`Inflate`:89、`ToRECT`:104；`typedef TRect<int> Rect; typedef TRect<double> FRect;`（:111–112）。用途：矩形/裁剪区/源目标矩形。★★★
- **Point**（Point.h）：模板 `TPoint<_T>`（:9，成员 `mX/mY` :12–13），全套四则运算符（:34–53）；`Point = TPoint<int>`、`FPoint = TPoint<double>`（:56–57）。用途：整数/浮点坐标。★★★
- **Insets**（Insets.h）：`class Insets`（:7），四边留白 `mLeft/mTop/mRight/mBottom`（:10–13）+ 三个构造函数（:16–18）。用途：控件/边框内边距。★★★
- **SexyMatrix**（SexyMatrix.h/.cpp）：`SexyMatrix3`（:11，3×3 矩阵 `m[3][3]`，`ZeroMatrix/LoadIdentity` + 矩阵/向量乘法，定义 SexyMatrix.cpp:9 起）；`SexyTransform2D : public SexyMatrix3`（:39，`Translate/RotateRad/RotateDeg/Scale`）；`Transform`（:61，惰性缓存的复合变换，`GetMatrix()` SexyMatrix.cpp:291）。用途：D3D Blt 的仿射变换与全局变换栈。★★★
- **SexyVector**（SexyVector.h）：`SexyVector2`（:11，`x,y`，Dot/±/*/÷/Magnitude/Normalize/Perp）、`SexyVector3`（:51，`x,y,z`，Dot/Cross/Magnitude/Normalize）。用途：变换计算中的 2D/3D 向量。★★★

## 6. ResourceManager

### 6.1 ResourceManager 类（ResourceManager.h/.cpp）

- **继承关系**：`class ResourceManager`（ResourceManager.h:30）——**无基类**，独立类；被 `SexyAppBase` 以指针成员持有（`ResourceManager* mResourceManager`，SexyAppBase.h:324），构造函数保存反向指针 `mApp`。
- **职责**：集中解析 PopCap 资源清单 XML（`ResourceManifest`），把「资源 id → 实际文件路径/加载参数」登记成描述对象（`BaseRes` 子类），并按 id 提供缓存查找 + 惰性实例化（图像/字体/声音）与生命周期清理。

**关键成员变量表**（protected，ResourceManager.h:110-129）

| 变量 | 类型 | 说明 |
|---|---|---|
| `mImageMap` / `mSoundMap` / `mFontMap` | `ResMap` = `std::map<std::string, BaseRes*>` | 三种资源的「id → 资源描述」缓存表（ResourceManager.h:106,112-114） |
| `mResGroupMap` | `ResGroupMap` = `std::map<std::string, ResList, StringLessNoCase>` | 分组 id → 该组资源按声明顺序的链表（:108,127） |
| `mLoadedGroups` | `std::set<std::string, StringLessNoCase>` | 已实例化完成的分组集合（:110） |
| `mXMLParser` | `XMLParser*` | 当前清单解析器（:116） |
| `mError` / `mHasFailed` | `std::string` / `bool` | 错误文本 / 解析失败标志（:117-118） |
| `mApp` | `SexyAppBase*` | 宿主应用反向指针（:119） |
| `mCurResGroup` / `mCurResGroupList` / `mCurResGroupListItr` | `std::string` / `ResList*` / `ResList::iterator` | 当前加载分组及其进度迭代器（:120,128-129） |
| `mDefaultPath` / `mDefaultIdPrefix` | `std::string` | 由 `SetDefaults` 设置的缺省路径前缀 / id 前缀（:121-122） |
| `mAllowMissingProgramResources` / `mAllowAlreadyDefinedResources` / `mHadAlreadyDefinedError` | `bool` | 容错开关（缺省允许缺少程序资源 / 允许重复定义用于重解析）（:123-125） |

内嵌描述结构（ResourceManager.h:41-104）：`BaseRes`（公共字段 `mType/mId/mResGroup/mPath/mXMLAttributes/mFromProgram`）派生 `ImageRes`（`SharedImageRef mImage`、alpha 图、动画 `AnimInfo`、rows/cols、palletize/DD 标志等）、`SoundRes`（`mSoundId/mVolume/mPanning`）、`FontRes`（`Font* mFont`、`Image* mImage`、`mSysFont/mSize/mBold/...`）。

**主要成员函数索引表**

| 签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `ResourceManager(SexyAppBase*)` | .cpp:46 | 初始化 mApp、失败标志、XML 解析器为空 | ★★★ |
| `~ResourceManager()` | .cpp:59 | 对三张 map 调 `DeleteMap` 全部释放 | ★★★ |
| `ParseResourcesFile(const std::string&)` | .cpp:586 | 打开 XML、定位 `ResourceManifest` 后 `DoParseResources` | ★★★ |
| `ReparseResourcesFile(...)` | .cpp:614 | 允许重复定义前提下重解析（热重载） | ★★ |
| `ParseCommonResource(...)` | .cpp:169 | 取 path/id、拼 `mDefaultPath`/`mDefaultIdPrefix`、插入 map 与分组链表 | ★★★ |
| `ParseImageResource / ParseSoundResource / ParseFontResource / ParseSetDefaults` | .cpp:271 / :212 / :384 / :445 | 解析各类资源 XML 属性 | ★★★ |
| `LoadImage(const std::string&)` | .cpp:802 | 查 `mImageMap` → 已缓存直接返回；否则 `DoLoadImage` 惰性加载 | ★★★ |
| `DoLoadImage(ImageRes*)` | .cpp:708 | 经 `gSexyAppBase->GetSharedImage` 取 DDImage、合成 alpha、设 palletize/D3D 标志/动画 | ★★ |
| `LoadAlphaImage / LoadAlphaGridImage` | .cpp:677 / :628 | 把独立 alpha 图逐像素/逐格写入 alpha 通道 | ★★ |
| `GetImage(const std::string&)` | .cpp:1119 | 纯查 `mImageMap` 返回 `SharedImageRef`（不触发加载） | ★★★ |
| `GetImageThrow(...)` | .cpp:1152 | 查不到抛 `ResourceManagerException` | ★★ |
| `LoadFont(const std::string&)` | .cpp:926 | 查 `mFontMap` → 已缓存返回；否则 `DoLoadFont` 惰性加载 | ★★★ |
| `DoLoadFont(FontRes*)` | .cpp:850 | 按 `!sys:`/`!ref:`/图片/字体文件 分派创建 SysFont/ImageFont，应用 tags | ★★ |
| `GetFont / GetFontThrow` | .cpp:1141 / :1192 | 纯查 `mFontMap` / 抛异常版本 | ★★★ / ★★ |
| `GetSound(const std::string&)` | .cpp:1130 | 纯查 `mSoundMap` 返回 `mSoundId`（未加载时 -1） | ★★★ |
| `GetSoundThrow(...)` | .cpp:1172 | 抛异常版本 | ★★ |
| `DoLoadSound(SoundRes*)` | .cpp:823 | `SoundManager::GetFreeSoundId`+`LoadSound`，设置音量/声像 | ★★ |
| `ReplaceImage / ReplaceSound / ReplaceFont` | .cpp:1218 / :1234 / :1249 | 运行时替换已登记资源的实际对象 | ★★ |
| `DeleteImage / DeleteFont` | .cpp:795 / :947 | 包装 `Replace*(name, NULL)` 触发删除 | ★★ |
| `DeleteResources(const std::string&)` | .cpp:99 | 按分组释放三张 map 中匹配项并从 `mLoadedGroups` 移除 | ★★ |
| `DeleteExtraImageBuffers(...)` | .cpp:109 | 释放图像的额外内存缓冲（保留表面） | ★★ |
| `DeleteMap(ResMap&)` | .cpp:75 | 遍历 `DeleteResource()`+`delete`+`clear` | ★★ |
| `StartLoadResources / LoadNextResource / LoadResources` | .cpp:1010 / :954 / :1053 | 设置分组迭代器、逐个实例化、标记分组已加载 | ★★ |
| `ResourceLoadedHook(BaseRes*)` | .cpp:1004 | 空钩子，供子类在资源加载后扩展 | ★★★ |
| `GetNumImages/Sounds/Fonts/Resources` | .cpp:1091 / :1098 / :1105 / :1112 | 统计某组资源数量 | ★★ |
| `GetImageAttributes(...)` | .cpp:1265 | 返回图像资源原始 XML 属性表 | ★★ |
| `IsGroupLoaded / GetErrorText / HadError / Fail` | .cpp:68 / :125 / :132 / :139 | 状态查询与错误记录（带行号/文件名） | ★★ |

> 说明：`LoadSound` 并非公开方法——声音没有像图像/字体那样的独立惰性 `LoadSound` 入口，只能通过 `LoadResources`/`LoadNextResource` 的 `DoLoadSound` 批量实例化；`GetSound` 只回读 `mSoundId`。框架也没有 `DeleteAllImages` 这类接口，整体清理靠 `DeleteResources(分组)` 或析构。

---

### 6.2 资源缓存机制（惰性加载）

三张 map（`mImageMap/mSoundMap/mFontMap`，`std::map<std::string, BaseRes*>`）**只存「描述对象」**，id 在 `ParseCommonResource`（.cpp:199-206）登记时即插入。实际对象的实例化分两级：

1. **纯查询不加载**：`GetImage`（.cpp:1119）/`GetFont`（.cpp:1141）/`GetSound`（.cpp:1130）只 `find()` 后返回 `ImageRes::mImage`、`FontRes::mFont`、`SoundRes::mSoundId`，不会触发任何加载。
2. **惰性加载**：`LoadImage`（.cpp:802）/`LoadFont`（.cpp:926）流程一致：`find` → 已缓存非空直接返回 → `mFromProgram`（程序资源）返回 NULL → 否则调用 `DoLoadImage`/`DoLoadFont` 真正创建并写回 `mImage`/`mFont`。批量路径 `LoadResources(分组)`（.cpp:1053）用 `mCurResGroupListItr` 逐个 `LoadNextResource`（.cpp:954）跳过已加载项，完成后把分组写入 `mLoadedGroups`。

描述对象销毁时：`ImageRes::DeleteResource`（.cpp:18）`mImage.Release()`；`SoundRes::DeleteResource`（.cpp:25）`SoundManager->ReleaseSound`；`FontRes::DeleteResource`（.cpp:35）`delete mFont`+`delete mImage`。

### 6.3 Pak 路径映射（与 PakLib 的衔接）

ResourceManager **本身不直接碰 PakLib**，路径映射分两处完成：

1. **清单内重写**（`ParseCommonResource`，.cpp:179-186）：`path` 以 `!` 开头 → 原样使用（`!program` 标记为程序资源 `mFromProgram=true`）；否则 `mPath = mDefaultPath + path`。`mDefaultPath` 来自 `SetDefaults` 的 `path` 属性（.cpp:445-450，`RemoveTrailingSlash` 后补 `/`）。缺省 id 用 `mDefaultIdPrefix + GetFileName(mPath,true)`（.cpp:192）。
2. **Pak 落盘映射**：图像在 `DoLoadImage`（.cpp:719）经 `gSexyAppBase->GetSharedImage(mPath, mVariant, &isNew)`（SexyAppBase.cpp:7080）→ 内部 `GetImage` 走全局 `gPakInterface`。`PakInterface::AddPakFile`（PakInterface.cpp:36，`main.pak` 于 SexyAppBase.cpp:6123 注册）内存映射 pak、校验魔数 `0xBAC04AC0`/版本 0、把每条文件名（大写）记入 `mPakRecordMap` → `PakRecord{偏移,大小}`。读取时 `PakInterface::FOpen`（PakInterface.cpp:194）先 `FixFileName`（PakInterface.cpp:143：绝对路径转相对、`/`→`\`、折叠 `..`、转大写）再查 `mPakRecordMap`，命中则从内存映射读包内数据，未命中回退 `fopen` 磁盘文件。

### 6.4 字体加载（结合字体章节）

`DoLoadFont`（.cpp:850-922）按 `mPath` 前缀三路分派：
- **SysFont**：`mPath` 以 `!sys:` 开头（`ParseFontResource` .cpp:419-436 已剥前缀并置 `mSysFont`）→ `new SysFont(path, mSize, mBold, mItalic, mUnderline)`，再设 `mDrawShadow`/`mSimulateBold`（.cpp:856-867）。
- **ImageFont（无图片属性）**：`mPath` 以 `!ref:` 开头 → `GetFont(引用名)->Duplicate()`（.cpp:871-878）；否则 `new ImageFont(mApp, mPath)` 从字体文件加载（.cpp:881）。
- **ImageFont（带图片属性）**：`mImagePath` 非空 → `mApp->GetImage(mImagePath)` 取图后 `new ImageFont(anImage, mPath)`（.cpp:883-890）。
随后校验 `ImageFont::mFontData->mInitialized`，失败即删除并 `Fail`；有 `tags` 属性则 `AddTag`+`Prepare`（.cpp:893-913）。

### 6.5 图像加载

`DoLoadImage`（.cpp:708-791）：设置 `ImageLib::gAlphaComposeColor = mAlphaColor` → `gSexyAppBase->GetSharedImage(mPath, mVariant, &isNew)` 取得 `SharedImageRef`（内部是 `DDImage`，可回退 `MemoryImage`）；`isNew` 时才执行 `LoadAlphaImage`（整图 alpha，.cpp:677）或 `LoadAlphaGridImage`（逐格 alpha，.cpp:628，用于 rows×cols 精灵表）；随后 `CommitBits`、按属性设置 `mPurgeBits`/`mDDSurface`+`mWantDDSurface`/`Palletize()`（`nopal` 关闭，.cpp:761）/D3D 标志 `A4R4G4B4/A8R8G8B8/MinimizeSubdivisions`（.cpp:771-778）、`AnimInfo` 与 `mNumRows/mNumCols`（.cpp:780-784），最后 `ResourceLoadedHook`。源数据从文件/pak 读取统一由 `GetSharedImage`→`gPakInterface` 完成。

### 6.6 声音注册

`DoLoadSound`（.cpp:823-846）：`mApp->mSoundManager->GetFreeSoundId()` 申请槽位 → `LoadSound(id, mPath)`（路径同样经 pak 层）→ 按 `mVolume`/`mPanning` 调 `SetBaseVolume`/`SetBasePan` → 记录 `mSoundId`。释放见 6.2。

### 6.7 生命周期

- 单资源删除：`DeleteImage`/`DeleteFont`（.cpp:795/:947）实为 `Replace*(name, NULL)`（.cpp:1218/:1249 内先 `DeleteResource()` 再置空）；声音无独立 `DeleteSound` 公开接口。
- 分组删除：`DeleteResources(group)`（.cpp:99）→ 三张 map 各 `DeleteResources(map, group)`（.cpp:88，`DeleteResource` 但不从 map 摘除描述对象）+ `mLoadedGroups.erase(group)`。
- 图像额外缓冲：`DeleteExtraImageBuffers`（.cpp:109）调 `MemoryImage::DeleteExtraBuffers`。
- 全量清理：`DeleteMap`（.cpp:75）`DeleteResource()+delete+clear`；析构（.cpp:59）对三张 map 各执行一次。

### 6.8 与 SexyApp 的关系

`SexyAppBase` 持有 `mResourceManager`（SexyAppBase.h:324）；`ResourceManager` 构造函数保存 `mApp`（.cpp:48），运行时通过 `mApp->mSoundManager`（.cpp:28/:828）、`gSexyAppBase->GetSharedImage`（.cpp:719）反用宿主服务；全局实例 `gSexyAppBase` 在 `SoundRes::DeleteResource` 中直接引用。资源加载的底层图像复用 SexyAppBase 的 `mSharedImageMap`（按「大写文件名+变体」去重，SexyAppBase.cpp:7082-7104），保证同一路径多资源共享同一份 `DDImage`。

## 7. 可复用性评估

SexyAppFramework 本身就是 PopCap 用于多款游戏（Bejeweled、Zuma、Peggle、PvZ 等）的可复用 2D 引擎，本模块是其「应用与渲染核心」。按「独立抽出难度」分三档评估（★★★ 可独立抽出/依赖少；★★ 需少量依赖适配；★ 与平台/框架强耦合）。

### 7.1 独立可抽出（★★★，纯逻辑 / 数据类，几乎零依赖）

| 类/模块 | 依赖 | 说明 |
|---|---|---|
| `Rect` / `Point` / `Insets` / `Color` / `SexyMatrix` / `SexyVector` / `Ratio` | 仅 C++ 标准库 | 数学/几何/颜色基础类型，跨平台通用，可直接复制使用 |
| `Buffer` / `Flags` / `SmartPtr` / `CritSect` / `MTRand` | 标准库 + 少量 Win 原语 | 缓冲区/标志位/引用计数/互斥/随机数，通用 |
| `AnimInfo`（`Image.h` 内） | 仅 `std::vector` | 动画帧表（Once/PingPong/Loop）纯逻辑，可独立 |
| `Image`（几何与动画接口部分） | `Rect` / `Color` / `Point` | 抽象接口 + cel 几何 + 动画信息，去掉绘制实现即可抽 |
| `Graphics` 的纯逻辑 API | `Image` / `Font` / `Rect` / `Color` | `DrawImageBox` 九宫格、`WriteString/WriteWordWrapped` 文字排版、`PolyFill` 扫描线填充、状态栈，均平台无关 |
| `Font`（量度接口） | 无 | ascent/height/行距派生逻辑 |

### 7.2 需少量依赖适配（★★，含少量 Windows/框架类型，替换依赖即可复用）

| 类/模块 | 依赖 | 说明 |
|---|---|---|
| `MemoryImage` | `Image` + `SexyAppBase`(注册) + `NativeDisplay`(像素格式) | 软件像素算法（ARGB 混合 / Bresenham / 双线性）平台无关，但 `mApp` 注册与 `GetNativeAlphaData` 需剥离 |
| `SysFont` | `Font` + Win32 GDI | 逻辑简单，但 `CreateFont/TextOut` 仅 Windows，跨平台需重写为 FreeType 等 |
| `ImageFont` | `Font` + `DescParser` + `SharedImage`/`ResourceManager` | 描述文件解析与多图层渲染逻辑可抽，但图片加载依赖资源层 |
| `ResourceManager` | `XMLParser` + `SharedImage` + `SoundManager` + `SexyAppBase` | 惰性缓存/分组加载模型通用，替换声音与图像后端即可 |
| `Widget` / `WidgetContainer` | `Graphics` + `Image` + `KeyCodes` | 控件树（父子 / Z 序 / 命中 / 脏区 / 模态）逻辑较平台无关，但绘制接口绑定 `Graphics` |
| `WidgetManager` + 控件族（Button / Checkbox / Edit / List / Scrollbar / Slider / Text…） | `Graphics` + `SexyAppBase` | 控件实现依赖 `Graphics` 画布与 `SexyAppBase`（光标/焦点/资源），需抽象「画布+应用」接口后整体移植 |

### 7.3 强耦合，仅能作为整体引擎复用（★，Windows + 旧版 DirectX）

| 类/模块 | 依赖 | 说明 |
|---|---|---|
| `SexyApp` / `SexyAppBase` | Win32（HWND/消息泵/注册表/线程/`timeGetTime`）+ DirectDraw | 引擎主循环/事件/线程/资源编排，与 Windows 深度绑定，是「骨架」而非可拆零件 |
| `DDImage` | `MemoryImage` + DirectDraw(`LPDIRECTDRAWSURFACE`) + `DDInterface` | 色键/表面锁定/ARGB↔16/32 位转换，绑死 DirectDraw7 |
| `DDInterface` | DirectDraw + `NativeDisplay` | DirectDraw 设备封装（主/后备/绘制表面、Flip/Blt、软件光标） |
| `D3DInterface` / `D3D8Helper` | Direct3D7 / d3d8.dll | 纹理分块、TLVERTEX 三角形、BeginScene/EndScene，Direct3D7 已废弃 API |

### 7.4 抽取 / 迁移建议

1. **纯逻辑层先行**：把 `Rect/Point/Insets/Color/SexyMatrix/SexyVector/AnimInfo/Buffer/Flags` + `Graphics` 的排版/九宫格/扫描线 + `WidgetContainer/Widget` 的树逻辑抽成一个无后端依赖的 2D 基础库。
2. **替换设备层**：`DDImage/DDInterface/D3DInterface` 是历史包袱（DirectDraw7/Direct3D7），可整体替换为现代后端（SDL_Renderer / OpenGL / Direct3D11 / bgfx），保持 `Image→MemoryImage→*Image` 与 `Graphics` 的接口契约不变。
3. **替换平台层**：`SexyAppBase` 的 Win32 部分（消息泵/注册表/线程/`p_fopen`）可用跨平台替代（GLFW/SDL 事件、pthread、标准文件 I/O），保留 `DoMainLoop/UpdateAppStep/Process/UpdateFrames` 的主循环语义与 `WidgetManager` 事件路由。
4. **资源层保留模型**：`ResourceManager` 的「XML 清单 + id 缓存 + 分组惰性加载」是良好实践，仅需替换 `SharedImage`/`SoundManager`/`p_fopen` 后端；`PakInterface`（`src/PakLib`）的「内存映射归档 + 文件名→偏移映射」也可独立复用于任意打包资源系统。

