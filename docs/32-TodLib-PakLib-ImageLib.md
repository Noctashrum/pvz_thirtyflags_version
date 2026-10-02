# TodLib / PakLib / ImageLib 支撑库模块文档

> 所属: `src/TodLib`, `src/PakLib`, `src/ImageLib`
> 分析日期：本文档集由 DSH 智能体基于工作区源码完整分析生成。
> 源码编码：**GBK（代码页 936）** —— 所有 `.h/.cpp` 均为 GBK，检索与编辑须注意；本文档为 UTF-8。
> 行号说明：任务描述中的行数为约数，本文档一律采用**实测真实行号**（GBK 解码后 `ReadAllLines` 计数）。

---

## 1. 模块职责总览

| 库 | 目录 | 职责 | 自有文件 | 代码量（实测） |
|---|---|---|---|---|
| **TodLib** | `src/TodLib` | PopCap 通用游戏库：骨骼动画、粒子、特效、资源定义、工具函数、容器、软件光栅化 | 42 个 `.h/.cpp` | 约 12,739 行 |
| **PakLib** | `src/PakLib` | `.pak` 资源包读取：内存映射 + 目录索引 + 仿 `fopen/fread` 虚拟文件接口 | 2 个 | 约 693 行 |
| **ImageLib** | `src/ImageLib` | 图像加载/保存/alpha 合成：TGA/JPG/PNG/GIF/JPEG2000 解码 | 2 个自有文件 | 约 1,703 行 |

三者关系：`TodLib` 是核心游戏库；`PakLib` 提供对打包资源的透明读取（`ImageLib`、`TodStringFile`、`Definition` 的读路径都经它）；`ImageLib` 是独立编解码库，供 `TodLib`/`SexyAppFramework`/`Lawn` 使用。

```
Lawn (游戏逻辑)
   └─ SexyAppFramework (渲染/资源/控件框架)
        ├─ TodLib ────────────── 通用游戏库（动画/粒子/特效/定义/工具）
        │    ├─ PakLib  ◄─────── 资源包虚拟文件接口（p_fopen/p_fread/...）
        │    └─ ImageLib ◄────── 图像解码（GetImage）
        └─ ImageLib（独立，也直接被 Sexy 资源管理器调用）
```

三个库均为**高价值可复用独立组件**：TodLib 的数学/容器/定义/光栅化部分几乎零依赖；动画/粒子/特效层依赖 `SexyAppFramework` 的 `Image/Graphics/MemoryImage`；PakLib 仅依赖 Win32 API；ImageLib 依赖第三方解码器（libjpeg/libpng/zlib/j2k-codec DLL）。

---

## 2. Reanimator 骨骼动画系统

**文件**：`Reanimator.h`(437 行) / `Reanimator.cpp`(1419 行) / `ReanimAtlas.h`(53 行) / `ReanimAtlas.cpp`(259 行)

### 2.1 职责总览

骨骼/补间（tween）动画系统，分「定义层（只读、全局共享）」与「实例层（运行时状态）」两层：
- **定义层** `ReanimatorDefinition → ReanimatorTrack[] → ReanimatorTransform[]`：从 XML 描述文件加载关键帧，全局共享（`gReanimatorDefArray`）。
- **实例层** `Reanimation` + 每轨一份 `ReanimatorTrackInstance`：持有播放进度、混合（blend）、震动、附件（attachment）、颜色覆盖等状态。

职责：① 经 Definition/DefMap 框架解析 `.reanim`(XML) 动画定义；② 按时间推进动画并做帧间线性插值 + 轨道混合；③ 把 trans/skew/scale/frame/alpha/image/font/text 转成 `SexyMatrix3` 仿射变换渲染；④ 多轨渲染分组、附件挂接、粒子锚定、附属动画（`attacher__` 轨道）；⑤ 可选图集（ReanimAtlas）合并小图优化绘制。

### 2.2 核心数据结构

**ReanimatorTransform（每帧变换，Reanimator.h:294-311）**

| 字段 | 类型 | 含义 |
|---|---|---|
| mTransX / mTransY | float | 平移量 |
| mSkewX / mSkewY | float | 倾斜角（**角度制**，非弧度） |
| mScaleX / mScaleY | float | 缩放比例 |
| mFrame | float | 图像帧序号（**-1 = 空白帧/不绘制**） |
| mAlpha | float | 透明度 0~1 |
| mImage | Image* | 贴图（图集模式下被改写为「图集编号+1」的编码指针） |
| mFont | Font* | 字体（文本轨道） |
| mText | const char* | 文本 / `attacher__` 附属轨道描述 |

数值字段初值 `DEFAULT_FIELD_PLACEHOLDER = -10000.0f`（Reanimator.h:29），加载时由 `ReanimationFillInMissingData` 用前帧补齐，实现稀疏关键帧。

**ReanimatorTrack（轨道，Reanimator.h:38-47）**

| 字段 | 类型 | 含义 |
|---|---|---|
| mName | const char* | 轨道名（如 `_ground`、`fullscreen`、`attacher__…`） |
| mTransforms | ReanimatorTransform* | 每帧变换数组 |
| mTransformCount | int | 帧数 |

> 注意：本反编译源码中**没有 `TrackFlags` 字段，也没有独立「时长」字段**。时长隐含为 `mTransformCount / mFPS`（fps 属于 `ReanimatorDefinition`）。任务书所称 TrackFlags 在其它版本引擎中，本工程不存在。

**Reanimation 关键成员（Reanimator.h:354-379）**（节选）

| 字段 | 用途 |
|---|---|
| mReanimationType | 动画类型枚举（决定定义） |
| mAnimTime | 归一化播放进度 0~1 |
| mAnimRate | 播放速率（帧/秒，可负=倒放） |
| mDefinition | 指向共享定义 |
| mLoopType | 循环/播放模式 |
| mDead | 停止/死亡标志 |
| mFrameStart / mFrameCount / mFrameBasePose | 当前动作起止帧 / 基准姿态帧 |
| mOverlayMatrix | 覆盖矩阵（m02/m12=位置，m00/m11=缩放） |
| mColorOverride / mExtraAdditiveColor / mExtraOverlayColor | 颜色覆盖 / 加色 / 叠色 |
| mTrackInstances | 每轨运行时状态数组 |
| mLoopCount | 已循环次数 |
| mIsAttachment | 是否作为附件 |
| mRenderOrder / mFilterEffect | 渲染序 / 滤镜 |

**ReanimatorTrackInstance**（Reanimator.h:323-343）：`mBlendCounter/mBlendTime/mBlendTransform`（混合）、`mShakeOverride/mShakeX/mShakeY`（震动）、`mAttachmentID`（附件）、`mImageOverride`、`mRenderGroup`（`RENDER_GROUP_HIDDEN=-1`/`NORMAL=0`）、`mTrackColor`，及四个忽略标志。

**ReanimatorFrameTime**（Reanimator.h:286-292）：`mFraction`（帧间比例）、`mAnimFrameBeforeInt/mAnimFrameAfterInt`（前后整数帧）。

### 2.3 动画加载（来源：XML，走 Definition 框架）

**来源是 XML 文本**（文件名 `.reanim`，如 `reanim\Zombie.reanim`，本身可被打进 pak、经 PakInterface 透明读取），**走 Definition/DataArray 框架**：`DefinitionLoadXML` + `gReanimatorDefMap/gReanimatorTrackDefMap/gReanimatorTransformDefMap` + `DefField` 表（Reanimator.cpp:19-47）。

入口函数链：
1. `ReanimatorLoadDefinitions`（Reanimator.cpp:1103）— 启动时注册参数表、new 出 `gReanimatorDefArray`，对 `DefinitionIsCompiled` 条目预加载。
2. `ReanimatorEnsureDefinitionLoaded`（Reanimator.cpp:1064）— 惰性加载：`mTracks == nullptr` 才真正加载。
3. `ReanimationLoadDefinition`（Reanimator.cpp:140）— 核心解析：`DefinitionLoadXML`（142）后逐轨 `ReanimationFillInMissingData` 补齐稀疏帧（145-179）。
4. `ReanimationCreateAtlas`（286）/ `ReanimationPreload`（305）— 按需建图集 / 预加载（含 `TodSandImageIfNeeded`）。
5. 实例化：`AllocReanimation`（1053）→ `ReanimationInitializeType`（277）→ `ReanimationInitialize`（318）。
6. 释放：`ReanimationFreeDefinition`（184）、`ReanimatorFreeDefinitions`（1122）。

### 2.4 Update 播放流程（入口 `Reanimation::Update`，Reanimator.cpp:345）

1. **推进时间**（351-352）：`mAnimTime += SECONDS_PER_UPDATE(0.01) * mAnimRate / mFrameCount`。
2. **循环/结束处理**（354-421）：按 `mLoopType` 与正/倒放分派——`REANIM_LOOP*` 回绕；`PLAY_ONCE*` 到 1.0 置 `mDead`；`*_AND_HOLD` 停在端点不死；`mLoopCount` 累加。
3. **轨道级更新**（423-444）：递减 `mBlendCounter`；`mShakeOverride` 时随机抖动；`attacher__` 轨道调 `UpdateAttacherTrack`（1332）；有 `mAttachmentID` 刷新附件矩阵。

帧定位与插值子链：
- `GetFrameTime`（778）— 由 `mAnimTime` 算前后整数帧 + 比例。
- `GetTransformAtTime`（493）— 前后两帧 trans/skew/scale/alpha 做 `FloatLerp`，image/font/text/frame 取前帧。
- `GetCurrentTransform`（478）— 自然补间变换，若 `mBlendCounter>0` 再 `BlendTransform`。
- `BlendTransform`（448，全局函数）— 逐字段 lerp；skew 有 ±180° 修正。
- `StartBlend`（958）— 记录当前变换为混合源。
- `PlayReanim`（1214）— 播放动作统一入口：StartBlend → 设速率/循环 → `SetFramesForLayer`（938）。

### 2.5 渲染入口

- `Reanimation::Draw`（829）→ `DrawRenderGroup(g, RENDER_GROUP_NORMAL)`（807）。
- `DrawTrack`（572）单轨核心：`GetCurrentTransform` 取变换；算颜色/alpha；分支 **图集**（AddTriangle 批提交）、**普通贴图**（`ReanimBltMatrix` 537）、**文本**（`TodDrawStringMatrix`）、**全屏**（`fullscreen` FillRect）。
- 矩阵构建 `MatrixFromTransform`（519）；`ReanimBltMatrix` 软件路径无倾斜/正缩放/白色时走快速 `DrawImage`，否则 `TodBltMatrix`。

### 2.6 主要函数索引表（Reanimator.cpp）

| 函数签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `void ReanimationFillInMissingData(float&, float&)` / `(void*&, void*&)` | Reanimator.cpp:123 / 131 | 稀疏关键帧补齐 | ★★★ |
| `bool ReanimationLoadDefinition(const std::string&, ReanimatorDefinition*)` | Reanimator.cpp:140 | XML→定义解析 + 补齐 | ★★ |
| `void ReanimationFreeDefinition(ReanimatorDefinition*)` | Reanimator.cpp:184 | 释放图集与定义 | ★★ |
| `void Reanimation::ReanimationInitialize(float,float,ReanimatorDefinition*)` | Reanimator.cpp:318 | 实例初始化、建图集、分配轨道实例 | ★★ |
| `void Reanimation::Update()` | Reanimator.cpp:345 | 播放推进/循环/混合/震动/附件 | ★★ |
| `void BlendTransform(ReanimatorTransform*, const&, const&, float)` | Reanimator.cpp:448 | 两变换逐字段 lerp | ★★★ |
| `void Reanimation::GetCurrentTransform(int, ReanimatorTransform*)` | Reanimator.cpp:478 | 自然补间+混合后的当前变换 | ★★ |
| `void Reanimation::GetTransformAtTime(int, ReanimatorTransform*, ReanimatorFrameTime*)` | Reanimator.cpp:493 | 帧间线性插值 | ★★ |
| `static void Reanimation::MatrixFromTransform(const&, SexyMatrix3&)` | Reanimator.cpp:519 | 变换→3×3 矩阵（skew 转弧度） | ★★★ |
| `void Reanimation::ReanimBltMatrix(...)` | Reanimator.cpp:537 | 软/硬件矩阵绘制贴图 | ★ |
| `bool Reanimation::DrawTrack(Graphics*, int, int, TodTriangleGroup*)` | Reanimator.cpp:572 | 单轨渲染（图集/图/文/全屏） | ★ |
| `Image* Reanimation::GetCurrentTrackImage(const char*)` | Reanimator.cpp:729 | 取轨道当前贴图（解图集编码） | ★★ |
| `void Reanimation::GetTrackMatrix(int, SexyTransform2D&)` | Reanimator.cpp:746 | 取轨道当前世界矩阵 | ★★ |
| `void Reanimation::GetFrameTime(ReanimatorFrameTime*)` | Reanimator.cpp:778 | 播放进度→帧时间 | ★★ |
| `void Reanimation::DrawRenderGroup(Graphics*, int)` | Reanimator.cpp:807 | 按渲染组绘制全部轨道 | ★ |
| `void Reanimation::Draw(Graphics*)` | Reanimator.cpp:829 | 渲染入口 | ★ |
| `int Reanimation::FindTrackIndex(const char*)` | Reanimator.cpp:835 | 按名找轨道 | ★★ |
| `void Reanimation::AttachToAnotherReanimation(Reanimation*, const char*)` | Reanimator.cpp:851 | 把另一动画挂到本动画轨道 | ★ |
| `void Reanimation::GetTrackBasePoseMatrix(int, SexyTransform2D&)` | Reanimator.cpp:869 | 基准姿态矩阵（附件锚定） | ★★ |
| `AttachEffect* Reanimation::AttachParticleToTrack(...)` | Reanimator.cpp:885 | 粒子挂到轨道 | ★ |
| `void Reanimation::GetAttachmentOverlayMatrix(int, SexyTransform2D&)` | Reanimator.cpp:896 | 附件相对覆盖矩阵 | ★★ |
| `void Reanimation::GetFramesForLayer(const char*, int&, int&)` | Reanimator.cpp:912 | 由轨道非空帧求动作起止帧 | ★★ |
| `void Reanimation::SetFramesForLayer(const char*)` | Reanimator.cpp:938 | 播放指定轨道层 | ★★ |
| `bool Reanimation::TrackExists(const char*)` | Reanimator.cpp:949 | 轨道存在性 | ★★ |
| `void Reanimation::StartBlend(int)` | Reanimator.cpp:958 | 记录当前变换为混合源 | ★★ |
| `void Reanimation::ReanimationDie()` | Reanimator.cpp:978 | 停止并销毁附件 | ★★ |
| `void Reanimation::SetShakeOverride(const char*, float)` | Reanimator.cpp:991 | 设置轨道震动幅度 | ★★ |
| `void Reanimation::SetPosition(float,float)` | Reanimator.cpp:996 | 设置位置 | ★★ |
| `void Reanimation::OverrideScale(float,float)` | Reanimator.cpp:1002 | 设置缩放 | ★★ |
| `Reanimation* ReanimationHolder::AllocReanimation(float,float,int,ReanimationType)` | Reanimator.cpp:1053 | 从 holder 分配并初始化 | ★★ |
| `void ReanimatorEnsureDefinitionLoaded(ReanimationType, bool)` | Reanimator.cpp:1064 | 按需加载定义 | ★★ |
| `void ReanimatorLoadDefinitions(ReanimationParams*, int)` | Reanimator.cpp:1103 | 注册参数表 + 预加载 | ★★ |
| `void ReanimatorFreeDefinitions()` | Reanimator.cpp:1122 | 释放全部定义 | ★★ |
| `float Reanimation::GetTrackVelocity(const char*)` | Reanimator.cpp:1135 | 轨道瞬时横向速度 | ★★ |
| `bool Reanimation::IsTrackShowing(const char*)` | Reanimator.cpp:1148 | 轨道下一帧是否可见 | ★★ |
| `void Reanimation::ShowOnlyTrack(const char*)` | Reanimator.cpp:1159 | 只显示某轨 | ★★ |
| `void Reanimation::AssignRenderGroupToTrack(const char*, int)` | Reanimator.cpp:1169 | 设某轨渲染组 | ★★ |
| `void Reanimation::AssignRenderGroupToPrefix(const char*, int)` | Reanimator.cpp:1180 | 设前缀匹配轨渲染组 | ★★ |
| `void Reanimation::PropogateColorToAttachments()` | Reanimator.cpp:1192 | 颜色覆盖传播到附件 | ★ |
| `bool Reanimation::ShouldTriggerTimedEvent(float)` | Reanimator.cpp:1201 | 判断是否跨过时间阈值 | ★★★ |
| `void Reanimation::PlayReanim(const char*, ReanimLoopType, int, float)` | Reanimator.cpp:1214 | 播放动作统一入口 | ★★ |
| `static void Reanimation::ParseAttacherTrack(const&, AttacherInfo&)` | Reanimator.cpp:1227 | 解析 `attacher__` 轨道 | ★ |
| `void Reanimation::AttacherSynchWalkSpeed(int, Reanimation*, AttacherInfo&)` | Reanimator.cpp:1279 | 走路速度同步 | ★ |
| `void Reanimation::UpdateAttacherTrack(int)` | Reanimator.cpp:1332 | 维护附属动画 | ★ |
| `bool Reanimation::IsAnimPlaying(const char*)` | Reanimator.cpp:1394 | 是否正在播放某层 | ★★ |
| `Reanimation* Reanimation::FindSubReanim(ReanimationType)` | Reanimator.cpp:1402 | 递归查找附属动画 | ★ |

### 2.7 ReanimAtlas（图集，ReanimAtlas.cpp）

把定义中所有 ≤254px 小贴图打包进一张大 `MemoryImage`（`MAX_REANIM_IMAGES=64`），并将各 Transform 的 `mImage` 改写为「图集编号+1」编码指针（ReanimAtlas.cpp:246），渲染时 `GetEncodedReanimAtlas`（33）反解后经 `TodTriangleGroup` 批绘制。打包：按高度降序 → 2 的幂宽度（≤2048）→ 贪心「贴右/贴下」→ 高度取 2 的幂 → `FixPixelsOnAlphaEdgeForBlending` 修透明边缘（258）。

| 函数签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `ReanimAtlasImage::ReanimAtlasImage()` / `ReanimAtlas::ReanimAtlas()` | ReanimAtlas.cpp:11 / 17 | 构造 | ★★ |
| `void ReanimAtlas::ReanimAtlasDispose()` | ReanimAtlas.cpp:23 | 释放 MemoryImage | ★★ |
| `ReanimAtlasImage* ReanimAtlas::GetEncodedReanimAtlas(Image*)` | ReanimAtlas.cpp:33 | 编码指针→图集条目 | ★★ |
| `MemoryImage* ReanimAtlasMakeBlankMemoryImage(int,int)` | ReanimAtlas.cpp:44 | 建空白带 alpha 图 | ★★ |
| `static bool sSortByNonIncreasingHeight(const&, const&)` | ReanimAtlas.cpp:60 | 高度降序排序 | ★★★ |
| `static int GetClosestPowerOf2Above(int)` | ReanimAtlas.cpp:70 | 向上取 2 的幂 | ★★★ |
| `int ReanimAtlas::PickAtlasWidth()` | ReanimAtlas.cpp:80 | 算图集宽 | ★★ |
| `bool ReanimAtlas::ImageFits(int, const Rect&, int)` | ReanimAtlas.cpp:97 | 矩形冲突检测（外扩 1px） | ★★ |
| `bool ReanimAtlas::ImageFindPlaceOnSide(...)` | ReanimAtlas.cpp:112 | 贴已放图右/下侧 | ★★ |
| `bool ReanimAtlas::ImageFindPlace(...)` | ReanimAtlas.cpp:148 | 先右后下找位置 | ★★ |
| `bool ReanimAtlas::PlaceAtlasImage(...)` | ReanimAtlas.cpp:155 | 放置单图 | ★★ |
| `void ReanimAtlas::ArrangeImages(int&, int&)` | ReanimAtlas.cpp:172 | 排序+布局 | ★★ |
| `void ReanimAtlas::AddImage(Image*)` | ReanimAtlas.cpp:193 | 加入待打包图片 | ★★ |
| `int ReanimAtlas::FindImage(Image*)` | ReanimAtlas.cpp:206 | 查原图索引 | ★★ |
| `void ReanimAtlas::ReanimAtlasCreate(ReanimatorDefinition*)` | ReanimAtlas.cpp:216 | 收集→布局→绘制→改写定义指针 | ★ |

---

## 3. TodParticle 粒子系统

**文件**：`TodParticle.h`(495 行) / `TodParticle.cpp`(1333 行)

### 3.1 职责总览

基于「XML 定义 + 参数轨道（FloatParameterTrack）采样」的 2D 粒子系统，三级层次 **粒子系统(TodParticleSystem) → 发射器(TodParticleEmitter) → 粒子(TodParticle)**。用 Definition/DataArray 把 XML 的 `<Emitter>/<Field>` 反序列化为运行时定义，运行时用每粒子随机插值 + 时间轨道求值驱动运动学，支持 11 种粒子场、粒子级/发射器级交叉混合（CrossFade）、软/硬两条渲染路径。由 `EffectSystem::mParticleHolder` 持有驱动，是 100+ 种 `ParticleEffect` 的统一载体。

### 3.2 核心类/结构

**TodParticle（TodParticle.h:395-413）**：`mParticleEmitter`（所属发射器）、`mParticleDuration/mParticleAge`（寿命/年龄，帧）、`mParticleTimeValue/mParticleLastTimeValue`（归一化生命时间）、`mAnimationTimeValue`（动画循环时间）、`mVelocity/mPosition`、`mImageFrame`、`mSpinPosition/mSpinVelocity`（旋转角/碰撞角速度）、`mCrossFadeParticleID/mCrossFadeDuration`（交叉混合）、`mParticleInterp[16]`（每粒子 16 条轨道随机插值）、`mParticleFieldInterp[4][2]`（场插值）。

**TodParticleDefinition**（TodParticle.h:155-160）：仅容器 `mEmitterDefs` + `mEmitterDefCount`。

**TodParticleSystem**（TodParticle.h:464-493）：`mEffectType`、`mParticleDef`、`mParticleHolder`、`mEmitterList`（`TodList<ParticleEmitterID>`）、`mDead/mIsAttachment/mRenderOrder/mDontUpdate`。

**TodEmitterDefinition（发射器定义，TodParticle.h:94-148）**——发射频率/数量/速度/角度关键字段：

| 字段 | 用途 |
|---|---|
| mSpawnRate | 发射速率（每帧累加×0.01 取整产卵） |
| mSpawnMinActive / mSpawnMaxActive | 最少/最多存活粒子数（-1 不启用） |
| mSpawnMaxLaunched | 一生最多发射总数 |
| mParticleDuration | 粒子寿命（帧，默认 100） |
| mLaunchSpeed / mLaunchAngle | 发射速度 / 角度（度） |
| mSystemDuration | 系统寿命 |
| mCrossFadeDuration | 交叉混合时长 |
| mEmitterType | 形状：CIRCLE/BOX/BOX_PATH/CIRCLE_PATH/CIRCLE_EVEN_SPACING |
| mEmitterRadius / mEmitterOffsetX/Y / mEmitterBoxX/Y / mEmitterSkewX/Y / mEmitterPath | 发射几何 |
| mImage / mImageCol / mImageRow / mImageFrames / mAnimated / mAnimationRate | 贴图与动画 |
| mParticleFlags | 位标志（12 个） |
| mName / mOnDuration | 发射器名 / 粒子结束后续接的发射器名 |
| mSystem*/mParticle* 颜色轨道、mParticleSpinAngle/Speed/Scale/Stretch、mCollisionReflect/Spin、mClip* | 外观/碰撞/裁剪轨道 |
| mParticleFields / mParticleFieldCount、mSystemFields / mSystemFieldCount | 粒子场/系统场（各 ≤4） |

**TodParticleEmitter（TodParticle.h:416-438）**：`mEmitterDef`、`mParticleSystem`、`mParticleList`、`mSpawnAccum`、`mSystemCenter`、`mParticlesSpawned`、`mSystemAge/mSystemDuration`、`mSystemTimeValue/mSystemLastTimeValue`、`mDead`、`mColorOverride/mExtraAdditiveDrawOverride/mScaleOverride/mImageOverride/mFrameOverride`、`mCrossFadeEmitterID/mEmitterCrossFadeCountDown`、`mTrackInterp[10]`、`mSystemFieldInterp[4][2]`。

**ParticleField（TodParticle.h:71-77）**：`{ ParticleFieldType mFieldType; FloatParameterTrack mX; FloatParameterTrack mY; }`。`ParticleFieldType`（38-53）11 种场：`FIELD_FRICTION`（摩擦）、`FIELD_ACCELERATION`（加速度）、`FIELD_ATTRACTOR`（吸引）、`FIELD_MAX_VELOCITY`（限速）、`FIELD_VELOCITY`（匀速）、`FIELD_POSITION`（定位）、`FIELD_SYSTEM_POSITION`、`FIELD_GROUND_CONSTRAINT`（地面限制+弹跳）、`FIELD_SHAKE`（震动）、`FIELD_CIRCLE`（圆周）、`FIELD_AWAY`（径向斥离）。运行时 `UpdateParticleField`（cpp:504）按场类型 switch 施加。

### 3.3 发射器系统结构（加载/创建）

- **定义加载走 Definition/DataArray**：`TodParticleLoadDefinitions`（223）→ `TodParticleLoadADef`（163）→ `DefinitionLoadXML(文件名, &gParticleDefMap, def)`。三张 DefMap（`gParticleDefMap/gEmitterDefMap/gParticleFieldDefMap`，cpp:57/111/117）+ 三个构造函数（120-160）。`Field/SystemField` 为 `DT_ARRAY`（91-92）。
- **从定义创建系统**：`AllocParticleSystem`（1329）→ `AllocParticleSystemFromDef`（1309）→ `TodParticleInitializeFromDef`（276）。
- **创建发射器**：遍历定义，跳过带 `CrossFadeDuration` 的混合专用发射器，`DataArrayAlloc` → `TodEmitterInitialize`（302）→ `AddTail`。
- **发射粒子**：`TodParticleEmitter::Update → UpdateSpawning`（699）→ `SpawnParticle`（353）：分配粒子、初始化插值随机数、按形状算角/位置/速度、`AddHead` 进 `mParticleList` 并立即 Update 一次。

### 3.4 粒子更新管线

- 顶层 `TodParticleSystem::Update`（751）→ 遍历发射器 `Update`，全部死亡则置 `mDead`。
- `TodParticleEmitter::Update`（816）：`mSystemAge++`；到寿命按 `PARTICLE_SYSTEM_LOOPS` 循环/死亡；更新交叉混合倒计时；算 `mSystemTimeValue`；逐场 `UpdateSystemField`；遍历粒子 `UpdateParticle`（失败 `DeleteParticle`）；`UpdateSpawning`。
- `UpdateParticle`（658）：寿命判定（死亡/循环/交叉混合续接）→ 算 `mParticleTimeValue` → **逐场 `UpdateParticleField`（位置/速度/旋转物理）** → `mPosition += mVelocity` → 更新旋转角 → 更新动画时间 → `mParticleAge++`。
- `UpdateParticleField`（504）：按场类型施加物理（摩擦/加速度/吸引/限速/匀速/定位/震动/地面限制弹跳/圆周/斥离）。

### 3.5 渲染管线

`TodParticleSystem::Draw`（1092）→ `TodParticleEmitter::Draw`（1099）：先按 `PARTICLE_SOFTWARE_ONLY/HARDWARE_ONLY` 过滤；建栈上 `TodTriangleGroup`，逐粒子 `DrawParticle`（1061）→ `GetRenderParams`（880，含交叉混合插值）→ `RenderParticle`（981，取图/算帧/裁剪/矩阵变换/`AddTriangle`）；最后 `group.DrawGroup(g)` 一次性提交。**三角形提交**经 `TodTriangleGroup→Image::BltTrianglesTex→TodDrawTriangle_*` 软件光栅化（见 §8），粒子系统本身不直接调 `TodDrawTriangle`。

### 3.6 主要函数索引表（TodParticle.cpp）

| 函数签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `void* ParticleFieldConstructor(void*)` | TodParticle.cpp:120 | 粒子场对象构造 | ★★★ |
| `void* TodEmitterDefinitionConstructor(void*)` | TodParticle.cpp:134 | 发射器定义构造 | ★★★ |
| `void* TodParticleDefinitionConstructor(void*)` | TodParticle.cpp:152 | 系统定义构造 | ★★★ |
| `bool TodParticleLoadADef(TodParticleDefinition*, const char*)` | TodParticle.cpp:163 | 从 XML 加载单定义并补默认值 | ★★ |
| `void TodParticleLoadDefinitions(ParticleParams*, int)` | TodParticle.cpp:223 | 批量加载全部定义 | ★ |
| `void TodParticleFreeDefinitions()` | TodParticle.cpp:247 | 释放全部定义 | ★★ |
| `TodParticleSystem::TodParticleSystem()` / `~TodParticleSystem()` | TodParticle.cpp:258 / 269 | 构造/析构 | ★★ |
| `TodParticleSystem::TodParticleInitializeFromDef(...)` | TodParticle.cpp:276 | 按定义创建全部发射器 | ★★ |
| `TodParticleEmitter::TodEmitterInitialize(...)` | TodParticle.cpp:302 | 发射器初始化 | ★★ |
| `TodParticleSystem::ParticleSystemDie()` | TodParticle.cpp:340 | 系统死亡释放 | ★★ |
| `TodParticle* TodParticleEmitter::SpawnParticle(int,int)` | TodParticle.cpp:353 | 发射单粒子 | ★★ |
| `float TodParticleEmitter::ParticleTrackEvaluate(...)` | TodParticle.cpp:498 | 粒子轨道求值 | ★★★ |
| `void TodParticleEmitter::UpdateParticleField(...)` | TodParticle.cpp:504 | 施加单粒子场物理 | ★★ |
| `float TodParticleEmitter::SystemTrackEvaluate(...)` | TodParticle.cpp:605 | 系统轨道求值 | ★★★ |
| `void TodParticleEmitter::UpdateSystemField(...)` | TodParticle.cpp:611 | 施加系统场 | ★★ |
| `bool TodParticleEmitter::CrossFadeParticleToName(...)` | TodParticle.cpp:636 | 交叉混合到命名发射器 | ★★ |
| `bool TodParticleEmitter::UpdateParticle(TodParticle*)` | TodParticle.cpp:658 | 单粒子逐帧更新 | ★★ |
| `void TodParticleEmitter::UpdateSpawning()` | TodParticle.cpp:699 | 按速率产卵 | ★★ |
| `void TodParticleEmitter::DeleteNonCrossFading()` | TodParticle.cpp:729 | 删非混合粒子 | ★★ |
| `void TodParticleEmitter::DeleteAll()` | TodParticle.cpp:740 | 清空粒子 | ★★ |
| `void TodParticleSystem::Update()` | TodParticle.cpp:751 | **系统级更新入口** | ★★ |
| `bool TodParticleEmitter::CrossFadeParticle(...)` | TodParticle.cpp:769 | 单粒子交叉混合 | ★★ |
| `void TodParticleEmitter::DeleteParticle(TodParticle*)` | TodParticle.cpp:801 | 删除单粒子 | ★★ |
| `void TodParticleEmitter::Update()` | TodParticle.cpp:816 | **发射器级更新入口** | ★★ |
| `float CrossFadeLerp(from,to,set1,set2,frac)` | TodParticle.cpp:870 | 交叉混合线性插值 | ★★★ |
| `bool TodParticleEmitter::GetRenderParams(...)` | TodParticle.cpp:880 | 计算渲染参数 | ★★ |
| `void RenderParticle(Graphics*, TodParticle*, const Color&, ParticleRenderParams*, TodTriangleGroup*)` | TodParticle.cpp:981 | 提交单粒子三角形 | ★★ |
| `void TodParticleEmitter::DrawParticle(...)` | TodParticle.cpp:1061 | 单粒子绘制 | ★★ |
| `void TodParticleSystem::Draw(Graphics*)` | TodParticle.cpp:1092 | **系统级绘制入口** | ★★ |
| `void TodParticleEmitter::Draw(Graphics*)` | TodParticle.cpp:1099 | 发射器绘制 | ★★ |
| `void TodParticleSystem::SystemMove(float,float)` / `TodParticleEmitter::SystemMove(...)` | TodParticle.cpp:1113 / 1120 | 整体移动 | ★★ |
| `void TodParticleSystem::OverrideColor(...)` | TodParticle.cpp:1141 | 覆写颜色 | ★★ |
| `void TodParticleSystem::OverrideExtraAdditiveDraw(...)` | TodParticle.cpp:1152 | 覆写叠加绘制 | ★★ |
| `void TodParticleSystem::OverrideImage(...)` | TodParticle.cpp:1163 | 覆写贴图 | ★★ |
| `void TodParticleSystem::OverrideFrame(...)` | TodParticle.cpp:1173 | 覆写帧 | ★★ |
| `void TodParticleSystem::OverrideScale(...)` | TodParticle.cpp:1184 | 覆写缩放 | ★★ |
| `TodParticleEmitter* TodParticleSystem::FindEmitterByName(...)` | TodParticle.cpp:1194 | 查运行中发射器 | ★★ |
| `TodEmitterDefinition* TodParticleSystem::FindEmitterDefByName(...)` | TodParticle.cpp:1206 | 查发射器定义 | ★★★ |
| `void TodParticleEmitter::CrossFadeEmitter(...)` | TodParticle.cpp:1218 | 发射器级交叉混合 | ★★ |
| `void TodParticleSystem::CrossFade(const char*)` | TodParticle.cpp:1244 | 系统级触发交叉混合 | ★★ |
| `~TodParticleHolder()` / `InitializeHolder()` / `DisposeHolder()` | TodParticle.cpp:1279 / 1285 / 1295 | 持有器生命周期 | ★★ |
| `bool TodParticleHolder::IsOverLoaded()` | TodParticle.cpp:1304 | 过载检测（>900） | ★★ |
| `TodParticleSystem* TodParticleHolder::AllocParticleSystemFromDef(...)` | TodParticle.cpp:1309 | 按定义分配系统 | ★★ |
| `TodParticleSystem* TodParticleHolder::AllocParticleSystem(...)` | TodParticle.cpp:1329 | 按特效枚举分配（游戏主入口） | ★ |

---

## 4. Definition / DataArray 资源定义系统

**文件**：`Definition.h`(208 行) / `Definition.cpp`(1225 行) / `DataArray.h`(168 行，纯头文件模板)

> 本节对 mod 开发最有价值。两套设施**相互独立**：`DataArray<T>` 是句柄对象池（运行时实体容器）；`Definition` 是「结构图反射」的 XML 定义加载系统。

### 4.1 DataArray 数据数组（句柄池）

`DataArray<T>`（DataArray.h:18）是纯头文件模板类，**不是** `std::vector`，**也不是** SexyAppFramework 的二进制 `DataArray`。用途：管理海量短命实体（僵尸/植物/投射物/粒子/动画实例等），O(1) 按句柄取对象 + 代际校验 + 空闲槽复用。

**句柄编码**：`mID = (高 16 位代际 key << 16) | (低 16 位槽下标)`。外部只存句柄，`DataArrayTryToGet` 校验槽内 `mID == 请求 id`，代际不符返回 null，杜绝悬空句柄（ABA 问题）。

| 成员 | 说明 |
|---|---|
| mBlock | 槽数组 `DataArrayItem{ T mItem; uint mID }` |
| mMaxUsedCount | 已用槽水位线 |
| mMaxSize / mSize | 容量 / 存活数 |
| mFreeListHead | 空闲链表头 |
| mNextKey | 下一个代际 key |
| mName | 调试名（key 播种：`name[1]`,`name[2]` 哈希 + 0xD000） |

| 方法 | 行号 | 功能 |
|---|---|---|
| `DataArray()` / `~DataArray()` | 38 / 49 | 构造清零 / 析构释放 |
| `DataArrayInitialize(maxSize, name)` | 54 | 分配槽块、播种 key |
| `DataArrayDispose()` | 63 | 释放全部对象与内存 |
| `DataArrayFree(T*)` | 78 | 析构并回收单槽 |
| `DataArrayFreeAll()` | 89 | 释放所有存活对象 |
| `DataArrayGetID(T*)` | 99 | 取对象句柄 |
| `IterateNext(T*&)` | 106 | 遍历在用槽 |
| `DataArrayAlloc()` | 129 | 分配槽 + placement-new + 发句柄 |
| `DataArrayTryToGet(id)` | 152 | 安全按句柄取对象（代际校验） |
| `DataArrayGet(id)` | 161 | 断言版按句柄取对象 |

### 4.2 Definition 资源定义系统（结构图反射）

用一张静态描述表（`DefMap`/`DefField`）声明「定义数据类」每个成员的名字/偏移/类型，从 **XML 文本**逐字段读取赋值，并可编译成 zlib 压缩的二进制 `.compiled` 缓存。

**核心结构**（Definition.h）：

| 结构 | 关键成员 | 说明 |
|---|---|---|
| `DefFieldType` | DT_INVALID/INT/FLOAT/STRING/ENUM/VECTOR2/ARRAY/TRACK_FLOAT/FLAGS/IMAGE/FONT | 字段存储类型（16-29） |
| `DefSymbol` | mSymbolValue / mSymbolName | 枚举/标志「数值↔名字」；`mSymbolName==nullptr` 结束 |
| `DefField` | mFieldName / mFieldOffset / mFieldType / mExtraData | 成员：名字、`offsetof` 偏移、类型、附加（指针→子 DefMap；枚举/标志→DefSymbol[]） |
| `DefMap` | mMapFields / mDefSize / mConstructorFunc | 定义结构图：字段数组、结构大小、构造函数 |
| `DefinitionArrayDef` | mArrayData / mArrayCount | 「指针数组+数量」组合，DT_ARRAY 指向它 |
| `CompressedDefinitionHeader` | mCookie / mUncompressedSize | 压缩缓存头 |
| `DefLoadResPath` | mPrefix / mDirectory | 贴图前缀→目录映射 |
| `FloatParameterTrackNode` | mTime/mLowValue/mHighValue/mCurveType/mDistribution | 轨道节点 |
| `FloatParameterTrack` | mNodes / mCountNodes | 浮点轨道 |

### 4.3 描述文件格式（真实格式：XML）

资源定义是 **XML 文本**（`particles\MelonImpact.xml` 等）。无 `<DefName>` 包裹根节点，顶层直接是各字段元素（同名字段可重复，构成数组）。

- **标量**：`<MaxPoints>20</MaxPoints>`（int 用 `%d`、float 用 `%f`）。
- **字符串**：`<Name>xxx</Name>`。
- **枚举**：值为 `DefSymbol` 表中的名字。
- **矢量**：`<某字段>x y</某字段>`。
- **图片/字体**：值为资源标签（`IMAGE_…`、`FONT_…`）。
- **标志位 DT_FLAGS**：子元素直接以标志名作标签出现，值 `0/1`，例 `<Loops>1</Loops>`。
- **数组 DT_ARRAY**：同名字段重复出现，每个元素递归 `DefinitionLoadMap`。
- **浮点轨道 DT_TRACK_FLOAT**：值为空格分隔的节点串，是全系统唯一用 `[ ]` 的地方。

**FloatTrack 节点语法**：`[ 低值 [ 分布曲线名 高值 ] ] [, 时间(厘秒) ] [ 过渡曲线名 ]`。例：`[0 2], 0 Linear 1 3, 50 EaseIn`。`,时间` 单位厘秒（×0.01 转秒），缺省时按节点序号均匀插值；曲线名支持 `Linear/EaseIn/EaseOut/EaseInOut/EaseInOutWeak/FastInOut/FastInOutWeak/Bounce/BounceFastMiddle/BounceSlowMiddle/SinWave/EaseSinWave`。

**举例**（依据 `gTrailDefFields`，Trail.cpp:15-26）：
```xml
<?xml version="1.0" encoding="utf-8"?>
<Image>IMAGE_TRAIL_FIREPEA</Image>
<MaxPoints>20</MaxPoints>
<MinPointDistance>3.0</MinPointDistance>
<Loops>1</Loops>
<WidthOverLength>[0 2], 0 Linear 1 3, 50 EaseIn</WidthOverLength>
<WidthOverTime>0, 0 Linear 5, 100</WidthOverTime>
```

### 4.4 字段解析流程（入口链，标注 Definition.cpp 行号）

```
DefinitionLoadXML(name, defMap, def)                79
  └─ DefinitionCompileAndLoad(...)                  1099
       ├─ #ifdef _DEBUG：已编译且最新→DefinitionReadCompiledFile 274；否则 DefinitionCompileFile 1083
       └─ #else：直接 DefinitionReadCompiledFile 274，失败 exit
DefinitionCompileFile(xml, compiled, defMap, def)   1083
  ├─ XMLParser::OpenFile
  ├─ DefinitionLoadMap(parser, defMap, def)         843
  │     ├─ 构造（mConstructorFunc）或 DefinitionFillWithDefaults  845-848
  │     └─ 循环 DefinitionReadField(parser,...)     773
  │           ├─ NextElement 取 START 标签
  │           ├─ DT_FLAGS 子元素 → DefinitionReadFlagField  717
  │           └─ 按名字匹配字段后按类型分发 switch   799-831:
  │                DT_INT→437  DT_FLOAT→450  DT_STRING→463  DT_ENUM→482
  │                DT_VECTOR2→495  DT_ARRAY→508(递归)  DT_TRACK_FLOAT→631
  │                DT_IMAGE→745  DT_FONT→759（标量都经 DefinitionReadXMLString 392）
  └─ DefinitionWriteCompiledFile(...)                1045
       └─ DefinitionCompressCompressedBuffer         1026（zlib compress + 头）
```

**二进制缓存读取**：`DefinitionReadCompiledFile`(274) → `DefinitionUncompressCompiledBuffer`(247，校验 `mCookie=0xDEADFED4` + uncompress) → `DefinitionCalcHash`(238，结构图 CRC 校验) → `DefMapReadFromCache`(166，修复指针/深数据)。

### 4.5 字段声明/注册机制（DefMap 反射，无 DefinitionProperties）

> 勘误：本工程**不存在** `DefinitionProperties` 类。真实机制是 `DefField + DefMap` 结构图反射。声明一个可被 XML 加载的「定义数据类」需三样东西，以 `TrailDefinition` 为例：

```cpp
// 1. DefSymbol[] 符号表（枚举/标志用）
DefSymbol gTrailFlagSymbols[] = {
    { TrailFlags::TRAIL_FLAG_LOOPS, "Loops" },
    { -1, nullptr }                          // 结束哨兵
};
// 2. DefField[] 字段表（offsetof 描述布局）
DefField gTrailDefFields[] = {
    { "Image",            offsetof(TrailDefinition, mImage),          DT_IMAGE,       nullptr },
    { "MaxPoints",        offsetof(TrailDefinition, mMaxPoints),      DT_INT,         nullptr },
    { "MinPointDistance", offsetof(TrailDefinition, mMinPointDistance),DT_FLOAT,      nullptr },
    { "TrailFlags",       offsetof(TrailDefinition, mTrailFlags),     DT_FLAGS,       gTrailFlagSymbols },
    { "WidthOverLength",  offsetof(TrailDefinition, mWidthOverLength),DT_TRACK_FLOAT, nullptr },
    { "", 0x0, DT_INVALID, nullptr }         // 结束哨兵
};
// 3. DefMap 结构图 + 构造函数
DefMap gTrailDefMap = { gTrailDefFields, sizeof(TrailDefinition), TrailDefinitionConstructor };
```
之后 `DefinitionLoadXML("trails\\xxx.xml", &gTrailDefMap, &def)` 即可。同类示例：粒子 `TodParticle.cpp:51-117`、动画 `Reanimator.cpp:19-47`。

### 4.6 主要函数索引表（Definition.cpp）

| 函数签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `void* DefinitionAlloc(size_t)` | Definition.cpp:23 | 分配并清零 | ★★★ |
| `bool DefinitionLoadImage(Image**, const SexyString&)` | Definition.cpp:32 | 按标签加载贴图 | ★ |
| `bool DefinitionLoadFont(Font**, const SexyString&)` | Definition.cpp:72 | 按标签加载字体 | ★ |
| `bool DefinitionLoadXML(const string&, DefMap*, void*)` | Definition.cpp:79 | 加载入口 | ★★ |
| `bool DefReadFromCacheArray(void*&, DefinitionArrayDef*, DefMap*)` | Definition.cpp:85 | 缓存读数组深数据 | ★★ |
| `bool DefReadFromCacheFloatTrack(void*&, FloatParameterTrack*)` | Definition.cpp:108 | 缓存读轨道 | ★★★ |
| `bool DefReadFromCacheString(void*&, char**)` | Definition.cpp:123 | 缓存读字符串 | ★★★ |
| `bool DefReadFromCacheImage(void*&, Image**)` | Definition.cpp:140 | 缓存读图片标签 | ★★ |
| `bool DefReadFromCacheFont(void*&, Font**)` | Definition.cpp:153 | 缓存读字体标签 | ★★ |
| `bool DefMapReadFromCache(void*&, DefMap*, void*)` | Definition.cpp:166 | 缓存读整结构 | ★★ |
| `uint DefinitionCalcHashSymbolMap(int, DefSymbol*)` | Definition.cpp:199 | 符号表 CRC | ★★★ |
| `uint DefinitionCalcHashDefMap(int, DefMap*, TodList<DefMap*>&)` | Definition.cpp:211 | 结构图 CRC（递归） | ★★★ |
| `uint DefinitionCalcHash(DefMap*)` | Definition.cpp:238 | 结构图哈希入口 | ★★★ |
| `void* DefinitionUncompressCompiledBuffer(void*, size_t, size_t&, const string&)` | Definition.cpp:247 | zlib 解压 + 头校验 | ★★★ |
| `bool DefinitionReadCompiledFile(const string&, DefMap*, void*)` | Definition.cpp:274 | 读 `.compiled` 缓存 | ★★ |
| `string DefinitionGetCompiledFilePathFromXMLFilePath(const string&)` | Definition.cpp:335 | 生成缓存路径 | ★★★ |
| `bool IsFileInPakFile(const string&)` | Definition.cpp:340 | 判断是否在 pak 内 | ★★ |
| `bool DefinitionIsCompiled(const string&)` | Definition.cpp:351 | 缓存是否最新可用 | ★★ |
| `void DefinitionFillWithDefaults(DefMap*, void*)` | Definition.cpp:372 | 清零并填默认 | ★★ |
| `void DefinitionXmlError(XMLParser*, const SexyChar*, ...)` | Definition.cpp:380 | XML 报错 | ★★ |
| `bool DefinitionReadXMLString(XMLParser*, SexyString&)` | Definition.cpp:392 | 读元素文本值 | ★★ |
| `bool DefSymbolValueFromString(DefSymbol*, const char*, int*)` | Definition.cpp:423 | 名字→数值 | ★★★ |
| `bool DefinitionReadIntField(XMLParser*, int*)` | Definition.cpp:437 | 读 int | ★★ |
| `bool DefinitionReadFloatField(XMLParser*, float*)` | Definition.cpp:450 | 读 float | ★★ |
| `bool DefinitionReadStringField(XMLParser*, char**)` | Definition.cpp:463 | 读字符串并分配 | ★★ |
| `bool DefinitionReadEnumField(XMLParser*, int*, DefSymbol*)` | Definition.cpp:482 | 读枚举 | ★★ |
| `bool DefinitionReadVector2Field(XMLParser*, SexyVector2*)` | Definition.cpp:495 | 读 "x y" | ★★ |
| `bool DefinitionReadArrayField(XMLParser*, DefinitionArrayDef*, DefField*)` | Definition.cpp:508 | 读数组元素（扩容+递归） | ★★ |
| `TodCurves DefParseTrackCurve(const char*&)` | Definition.cpp:537 | 解析曲线名 | ★★★ |
| `bool DefParseTrackTime(const char*&, FloatParameterTrackNode*)` | Definition.cpp:555 | 解析 `,厘秒` | ★★★ |
| `float sFindEvenlySpacedTime(FloatParameterTrack*, int)` | Definition.cpp:576 | 缺省时间均匀插值 | ★★★ |
| `bool DefParseTrackRangeNode(const char*&, FloatParameterTrackNode*)` | Definition.cpp:601 | 解析 `[low high]` | ★★★ |
| `bool DefinitionReadFloatTrackField(XMLParser*, FloatParameterTrack*)` | Definition.cpp:631 | 读整条轨道 | ★★ |
| `bool DefinitionReadFlagField(XMLParser*, const SexyString&, uint*, DefSymbol*)` | Definition.cpp:717 | 读标志位子元素 | ★★ |
| `bool DefinitionReadImageField(XMLParser*, Image**)` | Definition.cpp:745 | 读图片字段 | ★ |
| `bool DefinitionReadFontField(XMLParser*, Font**)` | Definition.cpp:759 | 读字体字段 | ★ |
| `bool DefinitionReadField(XMLParser*, DefMap*, void*, bool*)` | Definition.cpp:773 | 字段分发核心 | ★★ |
| `bool DefinitionLoadMap(XMLParser*, DefMap*, void*)` | Definition.cpp:843 | 构造 + 循环读字段 | ★★ |
| `size_t DefinitionGetArraySize(DefinitionArrayDef*, DefMap*)` | Definition.cpp:857 | 数组序列化大小 | ★★ |
| `size_t DefGetSizeImage(Image**)` | Definition.cpp:867 | 图片深尺寸 | ★ |
| `bool TodFindFontPath(Font*, string*)` | Definition.cpp:878 | 字体文件→标签 | ★ |
| `size_t DefGetSizeFont(Font**)` | Definition.cpp:899 | 字体深尺寸 | ★ |
| `size_t DefinitionGetDeepSize(DefMap*, void*)` | Definition.cpp:910 | 深数据总长 | ★★ |
| `void DefWriteToCacheArray(void*&, DefinitionArrayDef*, DefMap*)` | Definition.cpp:939 | 写数组深数据 | ★★ |
| `void DefWriteToCacheString(void*&, const char*)` | Definition.cpp:949 | 写字符串 | ★★★ |
| `void DefWriteToCacheImage(void*&, Image**)` | Definition.cpp:959 | 写图片标签 | ★★ |
| `void DefWriteToCacheFont(void*&, Font**)` | Definition.cpp:975 | 写字体标签 | ★★ |
| `void DefWriteToCacheFloatTrack(void*&, FloatParameterTrack*)` | Definition.cpp:991 | 写轨道节点 | ★★★ |
| `void DefMapWriteToCache(void*&, DefMap*, void*)` | Definition.cpp:1000 | 写整结构深数据 | ★★ |
| `void* DefinitionCompressCompressedBuffer(unsigned char*, size_t, size_t*)` | Definition.cpp:1026 | zlib 压缩 + 写头 | ★★★ |
| `bool DefinitionWriteCompiledFile(const string&, DefMap*, void*)` | Definition.cpp:1045 | 写 `.compiled` 缓存 | ★★ |
| `bool DefinitionCompileFile(const string&, const string&, DefMap*, void*)` | Definition.cpp:1083 | XML→解析→写缓存 | ★★ |
| `bool DefinitionCompileAndLoad(const string&, DefMap*, void*)` | Definition.cpp:1099 | 编译/读缓存统一入口 | ★★ |
| `float FloatTrackEvaluate(FloatParameterTrack&, float, float)` | Definition.cpp:1133 | 轨道求值 | ★★★ |
| `void FloatTrackSetDefault(FloatParameterTrack&, float)` | Definition.cpp:1160 | 设默认单节点 | ★★★ |
| `bool FloatTrackIsSet(const FloatParameterTrack&)` | Definition.cpp:1175 | 是否已赋值 | ★★★ |
| `bool FloatTrackIsConstantZero(FloatParameterTrack&)` | Definition.cpp:1180 | 是否恒零 | ★★★ |
| `float FloatTrackEvaluateFromLastTime(FloatParameterTrack&, float, float)` | Definition.cpp:1187 | 负时间返回 0 的求值 | ★★★ |
| `void DefinitionFreeArrayField(DefinitionArrayDef*, DefMap*)` | Definition.cpp:1193 | 释放数组深数据 | ★★ |
| `void DefinitionFreeMap(DefMap*, void*)` | Definition.cpp:1202 | 释放整结构深数据 | ★★ |

### 4.7 面向 mod 开发者的最小示例

```cpp
// 定义数据类
struct MyModDefinition { int mLevel; float mSpeed; const char* mName; Image* mIcon; MySubDef* mSubs; int mSubCount; };

// 字段表
DefField gMyModFields[] = {
    { "Level", offsetof(MyModDefinition, mLevel),  DT_INT,   nullptr },
    { "Speed", offsetof(MyModDefinition, mSpeed),  DT_FLOAT, nullptr },
    { "Name",  offsetof(MyModDefinition, mName),   DT_STRING,nullptr },
    { "Icon",  offsetof(MyModDefinition, mIcon),   DT_IMAGE, nullptr },
    { "Sub",   offsetof(MyModDefinition, mSubs),   DT_ARRAY, &gSubDefMap },
    { "", 0, DT_INVALID, nullptr }
};
void* MyModDefinitionConstructor(void* p){ if(p){ memset(p,0,sizeof(MyModDefinition)); ((MyModDefinition*)p)->mSpeed=1.0f; } return p; }
DefMap gMyModDefMap = { gMyModFields, sizeof(MyModDefinition), MyModDefinitionConstructor };

// 加载 / 释放
MyModDefinition def;
DefinitionLoadXML("data\\MyModDef.xml", &gMyModDefMap, &def);
// ... 使用 ...
DefinitionFreeMap(&gMyModDefMap, &def);
```

```xml
<?xml version="1.0" encoding="utf-8"?>
<Level>3</Level><Speed>2.5</Speed><Name>MyMod</Name><Icon>IMAGE_MYMOD</Icon>
<Sub><Kind>A</Kind></Sub><Sub><Kind>B</Kind></Sub>
```

```cpp
// 运行时对象池
DataArray<MyEnemy> enemies;
enemies.DataArrayInitialize(64, "EnemyPool");
MyEnemy* e = enemies.DataArrayAlloc();
unsigned int id = enemies.DataArrayGetID(e);     // 稳定句柄
MyEnemy* e2 = enemies.DataArrayTryToGet(id);     // 代际校验取回
enemies.DataArrayFree(e);
MyEnemy* it = nullptr; while (enemies.IterateNext(it)) { /*...*/ }
enemies.DataArrayDispose();
```

> mod 要点：`char*`/`Image*`/数组是指针型深数据，加载后由系统分配，**必须成对 `DefinitionFreeMap`** 防泄漏；`DT_ARRAY` 依赖「指针成员 + 紧随其后的 int 计数成员」的相邻布局。

---

## 5. Attachment / EffectSystem / FilterEffect / Trail / TodFoley 特效系统

### 5.1 Attachment 附件效果系统（Attachment.h:100 / Attachment.cpp:884）

**职责**：挂点容器，本身不产生画面，持有至多 16 个 `AttachEffect` 子项（粒子/拖尾/骨骼动画/嵌套 Attachment），把位置/矩阵/颜色/缩放/销毁指令转发给 EffectSystem 中对应效果对象，实现「效果跟随宿主」。与 EffectSystem/Reanimator：Attachment 存在 `EffectSystem::mAttachmentHolder` 的 `DataArray<Attachment>` 中；被挂效果置 `mIsAttachment=true` 使 `EffectSystem::Update` 跳过（由父 Attachment 代更新）；`AttachReanim` 写 Reanim 的 `mOverlayMatrix`。

| 关键成员 | 说明 |
|---|---|
| `EffectType`（h:25-32） | EFFECT_PARTICLE/TRAIL/REANIM/ATTACHMENT/OTHER |
| `MAX_EFFECTS_PER_ATTACHMENT=16`（h:12） | 上限 |
| `AttachEffect::mEffectID/mEffectType/mOffset/mDontDrawIfParentHidden/mDontPropogateColor` | 目标效果下标/类型/偏移/绘制开关/颜色传播开关 |
| `Attachment::mEffectArray[16]/mNumEffects/mDead` | 子效果数组/数量/死亡标记 |
| `AttachmentHolder::mAttachments` | `DataArray<Attachment>` 池（容量 1024） |

| 主要函数 | 文件:行号 | 功能 |
|---|---|---|
| `Attachment::Update()` | Attachment.cpp:21 | 更新子效果，空后置 mDead |
| `Attachment::SetPosition(const SexyVector2&)` | Attachment.cpp:99 | 位置转发 |
| `Attachment::OverrideColor(const Color&)` | Attachment.cpp:154 | 颜色覆盖转发 |
| `Attachment::PropogateColor(...)` | Attachment.cpp:197 | 颜色传播 |
| `Attachment::OverrideScale(float)` | Attachment.cpp:251 | 缩放转发 |
| `Attachment::CrossFade(const char*)` | Attachment.cpp:294 | 粒子 CrossFade |
| `Attachment::SetMatrix(const SexyTransform2D&)` | Attachment.cpp:313 | 矩阵转发 |
| `Attachment::Draw(Graphics*, bool)` | Attachment.cpp:368 | 递归绘制 |
| `Attachment::Detach()` | Attachment.cpp:431 | 解挂 |
| `Attachment::AttachmentDie()` | Attachment.cpp:497 | 销毁全部子效果 |
| `AttachmentUpdateAndMove(AttachmentID&, float, float)` | Attachment.cpp:605 | 全局 Update+SetPosition |
| `AttachmentUpdateAndSetMatrix(AttachmentID&, SexyTransform2D&)` | Attachment.cpp:586 | 全局 Update+SetMatrix |
| `AttachmentReanimTypeDie(AttachmentID&, ReanimationType)` | Attachment.cpp:650 | 按类型定向销毁 |
| `AttachmentDetachCrossFadeParticleType(...)` | Attachment.cpp:673 | 按粒子定义定向销毁 |
| `AttachmentDraw/Die/Detach/CrossFade` | Attachment.cpp:734/748/763/720 | 全局薄封装 |
| `FindReanimAttachment(AttachmentID&)` | Attachment.cpp:778 | 查找 Reanim |
| `FindFirstAttachment(AttachmentID&)` | Attachment.cpp:804 | 取第一个效果 |
| `CreateEffectAttachment(...)` | Attachment.cpp:817 | 核心分配 |
| `AttachReanim/AttachParticle/AttachTrail` | Attachment.cpp:842/854/868 | 挂接三种效果 |
| `IsFullOfAttachments(AttachmentID&)` | Attachment.cpp:879 | 是否达上限 |

### 5.2 EffectSystem 效果系统（EffectSystem.h:56 / EffectSystem.cpp:550）

**职责**：统一持有/初始化/销毁四类效果池（粒子/拖尾/骨骼动画/Attachment），每帧 Update、延迟删除队列（mDead 回收），三角形批渲染（`TodTriangleGroup`）与 Sutherland–Hodgman 风格四边裁剪。

| 关键成员 | 说明 |
|---|---|
| `MAX_TRIANGLES=256`（h:7） | 单组批渲染上限 |
| `TodTriVertex{x,y,u,v,color}` | 三角顶点 |
| `TodTriangleGroup::mImage/mVertArray[256][3]/mTriangleCount/mDrawMode` | 批渲染贴图/顶点/计数/模式 |
| `EffectSystem::mParticleHolder/mTrailHolder/mReanimationHolder/mAttachmentHolder` | 四类效果池 |
| `gEffectSystem` | 全局唯一实例 |

| 主要函数 | 文件:行号 | 功能 |
|---|---|---|
| `EffectSystemInitialize()` | EffectSystem.cpp:29 | 创建四池并 Initialize |
| `EffectSystemDispose()` | EffectSystem.cpp:47 | 逐个 Dispose+delete |
| `EffectSystemFreeAll()` | EffectSystem.cpp:78 | 清空全部效果 |
| `ProcessDeleteQueue()` | EffectSystem.cpp:92 | 回收 mDead 对象（延迟删除） |
| `Update()` | EffectSystem.cpp:116 | 更新四类效果（跳过 mIsAttachment） |
| `Tod_*Clip / Tod_*eClip（静态裁剪族）` | EffectSystem.cpp:142-318 | 单边插值裁剪 / 多边形裁剪 |
| `Tod_clipShape(...)` | EffectSystem.cpp:358 | 四边依次裁剪 |
| `TodTriangleGroup::DrawGroup(Graphics*)` | EffectSystem.cpp:391 | 提交整组（3D→D3D，否则软件 BltTrianglesTex） |
| `TodTriangleGroup::AddTriangle(...)` | EffectSystem.cpp:416 | 矩阵变换生成两三角，满组/换图冲刷 |

### 5.3 FilterEffect 滤镜效果（FilterEffect.h:33 / FilterEffect.cpp:181）

**职责**：把源图逐像素做色彩变换（HSL 亮度/饱和度、白色叠加）生成缓存副本，供发光/变白/褪色状态复用。滤镜类型：`WASHED_OUT`（亮 1.8×、饱和 0.2×）、`LESS_WASHED_OUT`（亮 1.2×、饱和 0.3×）、`WHITE`（逐像素 `|= 0x00FFFFFF`）。

| 主要函数 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `RGB_to_HSL(...)` | FilterEffect.cpp:10 | RGB→HSL | ★★★ |
| `HSL_to_RGB(...)` | FilterEffect.cpp:40 | HSL→RGB | ★★★ |
| `FilterEffectInitForApp()` | FilterEffect.cpp:72 | 初始化（空） | ★ |
| `FilterEffectDisposeForApp()` | FilterEffect.cpp:77 | 释放缓存 | ★★ |
| `FilterEffectDoLumSat(MemoryImage*, float, float)` | FilterEffect.cpp:95 | 调整亮度/饱和度 | ★★★ |
| `FilterEffectDoWashedOut / DoLessWashedOut / DoWhite` | FilterEffect.cpp:119/124/130 | 三种滤镜 | ★★ |
| `FilterEffectCreateImage(Image*, FilterEffect)` | FilterEffect.cpp:139 | 复制并施加滤镜 | ★★★ |
| `FilterEffectGetImage(Image*, FilterEffect)` | FilterEffect.cpp:169 | 带缓存取滤镜图 | ★★ |

### 5.4 Trail 拖尾效果（Trail.h:129 / Trail.cpp:343）

**职责**：沿运动路径记录采样点，以点列法线向两侧扩展成三角带，宽度/透明度随「长度(UV)/时间」两条 FloatParameterTrack 衰减，贴图横向平铺。

| 关键成员 | 说明 |
|---|---|
| `MAX_TRAIL_TRIANGLES=38 / MAX_TRAIL_POINTS=20` | 三角/采样点上限 |
| `TrailTracks`（h:17-24） | 四轨道：宽度/透明度 × 长度/时间 |
| `TrailFlags::TRAIL_FLAG_LOOPS` | 循环 |
| `TrailDefinition::mImage/mMaxPoints/mMinPointDistance/mTrailFlags/mTrailDuration/mWidthOver*/mAlphaOver*` | 定义 |
| `TrailPoint::aPos` | 采样点位置（相对 mTrailCenter） |
| `Trail::mTrailPoints[20]/mDead/mRenderOrder/mTrailAge/mTrailDuration/mTrailCenter/mIsAttachment/mColorOverride` | 运行时 |

| 主要函数 | 文件:行号 | 功能 |
|---|---|---|
| `TrailDefinitionConstructor(void*)` | Trail.cpp:43 | 定义构造器 |
| `TrailLoadADef(TrailDefinition*, const char*)` | Trail.cpp:61 | 从 XML 载入单定义 |
| `TrailLoadDefinitions(TrailParams*, int)` | Trail.cpp:77 | 批量载入 |
| `TrailFreeDefinitions()` | Trail.cpp:101 | 释放 |
| `Trail::AddPoint(float, float)` | Trail.cpp:129 | 追加采样点 |
| `Trail::Update()` | Trail.cpp:157 | 递增年龄/循环/死亡 |
| `Trail::GetNormalAtPoint(int, SexyVector2&)` | Trail.cpp:174 | 计算点法线 | ★★★ |
| `Trail::Draw(Graphics*)` | Trail.cpp:204 | 扩三角带绘制 |
| `TrailHolder::AllocTrail(int, TrailType)` | Trail.cpp:325 | 按类型分配 |
| `TrailHolder::AllocTrailFromDef(int, TrailDefinition*)` | Trail.cpp:331 | 按定义分配 |

### 5.5 TodFoley 音效混合器（TodFoley.h:97 / TodFoley.cpp:307）

**职责**：在 SexyAppFramework 的 `SoundManager/DSoundInstance` 之上管理最多 110 类、每类 8 实例的音效：循环/去重叠加/暂停静默/音乐音量/随机变式/随机音高，暂停时备份/还原播放进度。

| 关键成员 | 说明 |
|---|---|
| `MAX_FOLEY_TYPES=110 / MAX_FOLEY_INSTANCES=8` | 上限 |
| `FoleyFlags`：LOOP/ONE_AT_A_TIME/MUTE_ON_PAUSE/USES_MUSIC_VOLUME/DONT_REPEAT | 标志 |
| `FoleyParams::mFoleyType/mPitchRange/mSfxID[10]/mFoleyFlags` | 参数表 |
| `TodDSoundInstance` | 继承 DSoundInstance，扩展播放位置读写 |
| `FoleyInstance::mInstance/mRefCount/mPaused/mStartTime/mPauseOffset` | 实例 |
| `FoleyTypeData::mFoleyInstances[8]/mLastVariationPlayed` | 类数据 |
| `TodFoley::mFoleyTypeData[110]` | 全部音效状态 |

| 主要函数 | 文件:行号 | 功能 |
|---|---|---|
| `TodDSoundInstance::GetSoundPosition/SetSoundPosition` | TodFoley.cpp:26/33 | 播放游标读写 |
| `TodFoleyInitialize(FoleyParams*, int)` / `TodFoleyDispose()` | TodFoley.cpp:38/45 | 挂接/卸载参数表 |
| `SoundSystemReleaseFinishedInstances(TodFoley*)` | TodFoley.cpp:52 | 释放已播完实例 |
| `SoundSystemHasFoleyPlayedTooRecently(...)` | TodFoley.cpp:76 | 防连发 |
| `LookupFoley(FoleyType)` | TodFoley.cpp:88 | 类型→参数 |
| `SoundSystemFindInstance / GetFreeInstanceIndex` | TodFoley.cpp:97/112 | 找实例/空闲槽 |
| `TodFoley::PlayFoleyPitch(FoleyType, float)` | TodFoley.cpp:128 | 核心播放 |
| `TodFoley::PlayFoley(FoleyType)` | TodFoley.cpp:182 | 随机音高播放 |
| `TodFoley::StopFoley(FoleyType)` | TodFoley.cpp:192 | 停止 |
| `TodFoley::GamePause(bool)` | TodFoley.cpp:210 | 暂停/恢复 |
| `TodFoley::CancelPausedFoley()` | TodFoley.cpp:254 | 取消暂停实例 |
| `TodFoley::ApplyMusicVolume / RehookupSoundWithMusicVolume` | TodFoley.cpp:274/283 | 音量换算/重设 |
| `TodFoley::IsFoleyPlaying(FoleyType)` | TodFoley.cpp:303 | 查询是否在播 |

---

## 6. TodCommon 工具函数全集

**文件**：`TodCommon.h`(185 行) / `TodCommon.cpp`(1230 行)。共 **80 个函数**（含 9 个内联、4 个 `TodResourceManager` 成员），与 `TodCommon.h` 声明逐一核对。

### 6.1 加权/平滑随机（9 个）

| 函数签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `int TodPickFromArray(const int*, int)` | TodCommon.cpp:40 | 等概率随机取一项 | ★★★ |
| `int TodPickFromWeightedArray(TodWeightedArray*, int)` | TodCommon.cpp:46 | 按权重随机 | ★★★ |
| `TodWeightedArray* TodPickArrayItemFromWeightedArray(TodWeightedArray*, int)` | TodCommon.cpp:52 | 权重累计-递减法取元素 | ★★★ |
| `TodWeightedGridArray* TodPickFromWeightedGridArray(TodWeightedGridArray*, int)` | TodCommon.cpp:80 | 二维网格加权随机 | ★★★ |
| `float TodCalcSmoothWeight(float, float, float)` | TodCommon.cpp:108 | 平滑去重权重 | ★★★ |
| `void TodUpdateSmoothArrayPick(TodSmoothArray*, int, int)` | TodCommon.cpp:159 | 更新最近被选记录 | ★★★ |
| `int TodPickFromSmoothArray(TodSmoothArray*, int)` | TodCommon.cpp:125 | 平滑权重随机 | ★★★ |
| `int RandRangeInt(int, int)` | TodCommon.cpp:346 | 闭区间随机整数 | ★★★ |
| `float RandRangeFloat(float, float)` | TodCommon.cpp:353 | 区间随机浮点 | ★★★ |

### 6.2 插值曲线（19 个）

| 函数签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `float TodCurveQuad(float)` | TodCommon.cpp:174 | 二次缓入 t² | ★★★ |
| `float TodCurveInvQuad(float)` | TodCommon.cpp:179 | 二次缓出 | ★★★ |
| `float TodCurveS(float)` | TodCommon.cpp:185 | smoothstep | ★★★ |
| `float TodCurveInvQuadS(float)` | TodCommon.cpp:191 | 分段缓入缓出 | ★★★ |
| `float TodCurveBounce(float)` | TodCommon.cpp:203 | 三角弹跳 | ★★★ |
| `float TodCurveQuadS(float)` | TodCommon.cpp:208 | 分段 Quad | ★★★ |
| `float TodCurveCubic(float)` | TodCommon.cpp:217 | 三次缓入 | ★★★ |
| `float TodCurveInvCubic(float)` | TodCommon.cpp:222 | 三次缓出 | ★★★ |
| `float TodCurveCubicS(float)` | TodCommon.cpp:227 | 分段 Cubic | ★★★ |
| `float TodCurvePoly(float, float)` | TodCommon.cpp:236 | 幂曲线 | ★★★ |
| `float TodCurveInvPoly(float, float)` | TodCommon.cpp:241 | 反向幂曲线 | ★★★ |
| `float TodCurvePolyS(float, float)` | TodCommon.cpp:246 | 分段幂曲线 | ★★★ |
| `float TodCurveCircle(float)` | TodCommon.cpp:255 | 圆形缓出 | ★★★ |
| `float TodCurveInvCircle(float)` | TodCommon.cpp:264 | 圆形缓入 | ★★★ |
| `float TodCurveEvaluate(float, float, float, TodCurves)` | TodCommon.cpp:274 | 曲线插值（未钳制） | ★★★ |
| `float TodCurveEvaluateClamped(float, float, float, TodCurves)` | TodCommon.cpp:298 | 带钳制曲线插值 | ★★★ |
| `float TodAnimateCurveFloatTime(float, float, float, float, float, TodCurves)` | TodCommon.cpp:325 | 时间年龄归一化插值 | ★★★ |
| `float TodAnimateCurveFloat(int, int, int, float, float, TodCurves)` | TodCommon.cpp:332 | 整型时间版本 | ★★★ |
| `int TodAnimateCurve(int, int, int, int, int, TodCurves)` | TodCommon.cpp:341 | 整型位置版本 | ★★★ |

### 6.3 矩阵与几何（8 个）

| 函数签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `void TodScaleTransformMatrix(SexyMatrix3&, float, float, float, float)` | TodCommon.cpp:575 | 缩放+平移矩阵 | ★★★ |
| `void TodScaleRotateTransformMatrix(SexyMatrix3&, float, float, float, float, float)` | TodCommon.cpp:589 | 缩放+旋转+平移矩阵 | ★★★ |
| `void SexyMatrix3ExtractScale(const SexyMatrix3&, float&, float&)` | TodCommon.cpp:602 | 反解缩放系数 | ★★★ |
| `void SexyMatrix3Translation(SexyMatrix3&, float, float)` | TodCommon.cpp:569 | 累加平移 | ★★★ |
| `void SexyMatrix3Transpose(const SexyMatrix3&, SexyMatrix3&)` | TodCommon.cpp:854 | 转置 | ★★★ |
| `void SexyMatrix3Inverse(const SexyMatrix3&, SexyMatrix3&)` | TodCommon.cpp:877 | 求逆 | ★★★ |
| `void SexyMatrix3Multiply(SexyMatrix3&, const SexyMatrix3&, const SexyMatrix3&)` | TodCommon.cpp:903 | 乘法 | ★★★ |
| `bool TodIsPointInPolygon(const SexyVector2*, int, const SexyVector2&)` | TodCommon.cpp:1186 | 凸多边形点包含 | ★★★ |

### 6.4 数值内联工具（7 个，TodCommon.h 内联）

| 函数签名 | 文件:行号 | 功能 |
|---|---|---|
| `inline char ClampByte(char, char, char)` | TodCommon.h:162 | 字节钳制 |
| `inline int ClampInt(int, int, int)` | TodCommon.h:163 | 整型钳制 |
| `inline float ClampFloat(float, float, float)` | TodCommon.h:164 | 浮点钳制 |
| `inline float Distance2D(float, float, float, float)` | TodCommon.h:165 | 两点距离 |
| `inline float FloatLerp(float, float, float)` | TodCommon.h:166 | 线性插值 |
| `inline int FloatRoundToInt(float)` | TodCommon.h:167 | 四舍五入转整 |
| `inline bool FloatApproxEqual(float, float)` | TodCommon.h:168 | 浮点近似相等 |

（以上 7 个内联 + 位操作 2 个内联均为 ★★★ 通用）

### 6.5 位操作与颜色（6 个）

| 函数签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `inline void SetBit(uint&, int, bool=true)` | TodCommon.h:175 | 置位/清位 | ★★★ |
| `inline bool TestBit(uint, int)` | TodCommon.h:176 | 测位 | ★★★ |
| `Sexy::Color GetFlashingColor(int, int)` | TodCommon.cpp:927 | 闪烁灰阶 | ★★ |
| `int ColorComponentMultiply(int, int)` | TodCommon.cpp:948 | 单通道正片叠底 | ★★★ |
| `Sexy::Color ColorsMultiply(const Sexy::Color&, const Sexy::Color&)` | TodCommon.cpp:954 | 颜色正片叠底 | ★★★ |
| `Sexy::Color ColorAdd(const Sexy::Color&, const Sexy::Color&)` | TodCommon.cpp:937 | 颜色线性减淡 | ★★★ |

### 6.6 字符串（4 个）

| 函数签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `SexyString TodReplaceString(const SexyString&, const SexyChar*, const SexyString&)` | TodCommon.cpp:1158 | 替换首个匹配 | ★★ |
| `SexyString TodReplaceNumberString(const SexyString&, const SexyChar*, int)` | TodCommon.cpp:1172 | 占位符替换为数字 | ★★ |
| `int TodSnprintf(char*, int, const char*, ...)` | TodCommon.cpp:1222 | 安全 snprintf | ★★★ |
| `int TodVsnprintf(char*, int, const char*, va_list)` | TodCommon.cpp:1203 | 安全 vsnprintf | ★★★ |

### 6.7 文件/资源（8 个）

| 函数签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `bool TodLoadResources(const std::string&)` | TodCommon.cpp:967 | 转调 ResourceManager | ★ |
| `bool TodLoadNextResource()` | TodCommon.cpp:1027 | 转调 | ★ |
| `void TodAddImageToMap(SharedImageRef*, const std::string&)` | TodCommon.cpp:1011 | 注册图片 | ★ |
| `bool TodFindImagePath(Image*, std::string*)` | TodCommon.cpp:1105 | 反查图片路径 | ★ |
| `bool TodResourceManager::FindImagePath(Image*, std::string*)` | TodCommon.cpp:1110 | 遍历 mImageMap | ★ |
| `void TodResourceManager::AddImageToMap(SharedImageRef*, const std::string&)` | TodCommon.cpp:1017 | 注册 | ★ |
| `bool TodResourceManager::TodLoadNextResource()` | TodCommon.cpp:1033 | 逐个加载资源 | ★ |
| `bool TodResourceManager::TodLoadResources(const std::string&)` | TodCommon.cpp:973 | 整组加载（耗时统计） | ★ |

### 6.8 调试/应用级（5 个）

| 函数签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `SexyString TodGetCurrentLevelName()` | TodCommon.cpp:24 | 返回当前关卡名（函数指针覆盖） | ★ |
| `bool TodHasUsedCheatKeys()` | TodCommon.cpp:30 | 是否用过作弊键 | ★ |
| `bool TodAppCloseRequest()` | TodCommon.cpp:35 | 关闭请求 | ★ |
| `TodAllocator* FindGlobalAllocator(int)` | TodCommon.cpp:1129 | 按尺寸查/建全局内存池 | ★★ |
| `void FreeGlobalAllocators()` | TodCommon.cpp:1147 | 释放全部内存池 | ★★ |

### 6.9 渲染绘制（14 个）

| 函数签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `void TodDrawString(Graphics*, const SexyString&, int, int, Font*, const Color&, DrawStringJustification)` | TodCommon.cpp:360 | 带对齐文本绘制 | ★★ |
| `void TodDrawStringMatrix(Graphics*, const Font*, const SexyMatrix3&, const SexyString&, const Color&)` | TodCommon.cpp:396 | 矩阵逐字绘制 | ★★ |
| `void TodDrawImageScaledF(Graphics*, Image*, float, float, float, float)` | TodCommon.cpp:732 | 缩放绘制整图 | ★★ |
| `void TodDrawImageCenterScaledF(Graphics*, Image*, float, float, float, float)` | TodCommon.cpp:754 | 居中缩放绘制 | ★★ |
| `void TodDrawImageCelF(Graphics*, Image*, float, float, int, int)` | TodCommon.cpp:558 | 绘制动画帧单元 | ★★ |
| `void TodDrawImageCelScaled(Graphics*, Image*, int, int, int, int, float, float)` | TodCommon.cpp:378 | 缩放动画帧（整型） | ★★ |
| `void TodDrawImageCelScaledF(Graphics*, Image*, float, float, int, int, float, float)` | TodCommon.cpp:705 | 缩放动画帧（浮点） | ★★ |
| `void TodDrawImageCelCenterScaledF(Graphics*, Image*, float, float, int, float, float)` | TodCommon.cpp:678 | 居中缩放动画帧 | ★★ |
| `void TodBltMatrix(Graphics*, Image*, const SexyMatrix3&, const Rect&, const Color&, int, const Rect&)` | TodCommon.cpp:642 | 核心矩阵位块传输 | ★★ |
| `void TodMarkImageForSanding(Image*)` | TodCommon.cpp:625 | 打「需打磨」标记 | ★★ |
| `void TodSandImageIfNeeded(Image*)` | TodCommon.cpp:630 | 按标记修边缘像素 | ★★ |
| `void FixPixelsOnAlphaEdgeForBlending(Image*)` | TodCommon.cpp:819 | 修透明边缘（消黑边） | ★★ |
| `unsigned long AverageNearByPixels(MemoryImage*, unsigned long*, int, int)` | TodCommon.cpp:776 | 透明像素邻域平均色 | ★★ |
| `void Tod_SWTri_AddAllDrawTriFuncs()` | Tod_SWTri.cpp:20（声明 TodCommon.h:144） | 注册软件光栅函数 | ★ |

### 6.10 关键数据结构 / 常量

`PI`/`BOARD_WIDTH`(800)/`BOARD_HEIGHT`(600)（h:15-17）、`DEG_TO_RAD`/`RAD_TO_DEG`（h:19-20）、`TodWeightedArray`（h:24-28）、`TodWeightedGridArray`（h:30-35）、`TodSmoothArray`（h:37-44）、`TodCurves` 14 种曲线枚举（h:72-88）、`DrawStringJustification` 6 种对齐（h:121-129）、`TodResourceManager`（h:56-63，继承 `Sexy::ResourceManager`）、外部函数指针 `gAppCloseRequest/gAppHasUsedCheatKeys/gGetCurrentLevelName/gExtractResourcesByName`（h:180-183）。

> 复用自 SexyAppFramework 的类型：`Sexy::Rect`(=TRect<int>，含 Intersects/Intersection/Union/Contains/Offset/Inflate)、`Sexy::Color`(mRed/mGreen/mBlue/mAlpha)、`SexyString`(窄字符串 std::string)、`Sexy::SexyMatrix3`(3×3 行主序)、`Sexy::SexyVector2`。**本文件无 `Buffer`/`Ratio` 类型**。

---

## 7. 容器与调试（TodList / TodStringFile / TodDebug / StackWalk）

### 7.1 TodList 链表容器（TodList.h:170 / TodList.cpp:103）

**职责**：通用模板**双向链表** + `TodAllocator` 定长块内存池。节点不直接 new/delete，从按「元素大小」复用的全局分配器池（`gGlobalAllocators[MAX_GLOBAL_ALLOCATORS=128]`）取还。

- `TodAllocator`（TodList.h:8-25）：`mFreeList/mBlockList/mGrowCount/mTotalItems/mItemSize`；`Grow`（TodList.cpp:21）一次 `TodMalloc(growCount*itemSize+4)` 切成等份串入空闲链表。
- `TodListNode<T>`（h:29-35）：`mValue/mNext/mPrev`（**双向**）。
- `TodList<T>`（h:37-168）：`mHead/mTail/mSize/mpAllocator`。

| 函数签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `TodAllocator::Initialize(int,int)` | TodList.cpp:4 | 初始化池 | ★★ |
| `TodAllocator::Dispose()` | TodList.cpp:15 | 释放 | ★★ |
| `TodAllocator::Grow()` | TodList.cpp:21 | 申请一块切分 | ★★ |
| `TodAllocator::Alloc(int)` | TodList.cpp:64 | 取项 | ★★ |
| `TodAllocator::Calloc(int)` | TodList.cpp:75 | 取项并清零 | ★★ |
| `TodAllocator::Free(void*,int)` | TodList.cpp:82 | 归还 | ★★ |
| `TodAllocator::FreeAll()` | TodList.cpp:91 | 释放全部块 | ★★ |
| `TodAllocator::IsPointerFromAllocator(void*)` | TodList.cpp:41 | 归属断言 | ★★ |
| `TodAllocator::IsPointerOnFreeList(void*)` | TodList.cpp:56 | 空闲链表断言 | ★★ |
| `TodList<T>()` / `~TodList<T>()` | TodList.h:46 / 54 | 构造/析构 | ★★★ |
| `GetHead()` / `GetTail()` | TodList.h:59 / 65 | 取头/尾 | ★★★ |
| `AddHead(const T&)` / `AddTail(const T&)` | TodList.h:71 / 89 | 头插/尾插 | ★★★ |
| `RemoveHead()` | TodList.h:107 | 弹头 | ★★★ |
| `RemoveAt(TodListNode<T>*)` | TodList.h:123 | 解链指定节点 | ★★★ |
| `Find(const T&) const` | TodList.h:140 | 按值线性查找 | ★★★ |
| `RemoveAll()` | TodList.h:148 | 清空 | ★★★ |
| `SetAllocator(TodAllocator*)` | TodList.h:163 | 显式指定分配器 | ★★★ |

### 7.2 TodStringFile 字符串文件（TodStringFile.h:54 / TodStringFile.cpp:475）

**职责**：解析字符串表文本文件（`properties/*.txt`），把 `[键名]` 与文本值装入全局表，提供按键名翻译/查找 + 带 `{FORMAT}` 内联格式标记的富文本绘制（对齐/自动换行/裁剪/格式切换）。

**格式**：纯文本 `[键]` 引导（**字符串键名**，非数字 ID）：
```
[HELLO]
你好
[GAME_OVER]
游戏结束 {KEYWORD}提示文字
```
键名转大写存储；值内 `{NORMAL}`/`{KEYWORD}` 控制格式切换（默认 NORMAL 深蓝、KEYWORD 棕）；读取经 Pak 接口 `p_fopen/p_fread`（支持打包资源）。

| 函数 | 文件:行号 | 功能 |
|---|---|---|
| `TodStringListSetColors` | TodStringFile.cpp:31 | 设置格式表 |
| `TodStringListReadName` | TodStringFile.cpp:38 | 读 `[键名]` |
| `TodStringRemoveReturnChars` | TodStringFile.cpp:75 | 去 `\r` |
| `TodStringListReadValue` | TodStringFile.cpp:87 | 读值 |
| `TodStringListReadItems` | TodStringFile.cpp:98 | 循环解析键值对 |
| `TodStringListReadFile` | TodStringFile.cpp:119 | 读文件并解析（Pak） |
| `TodStringListLoad` | TodStringFile.cpp:150 | 读文件，失败弹框 |
| `TodStringListFind` | TodStringFile.cpp:157 | 按键名查字符串 |
| `TodStringTranslate(SexyString/SexyChar*)` | TodStringFile.cpp:172/183 | `[键]`→翻译 |
| `TodStringListExists` | TodStringFile.cpp:201 | 判断键存在 |
| `TodWriteStringSetFormat` | TodStringFile.cpp:212 | 解析 `{FORMAT}` |
| `CharIsSpaceInFormat` | TodStringFile.cpp:230 | 空格判断 |
| `TodWriteString` | TodStringFile.cpp:236 | 单行富文本绘制 |
| `TodWriteWordWrappedHelper` | TodStringFile.cpp:304 | 单行截断绘制 |
| `TodDrawStringWrappedHelper` | TodStringFile.cpp:316 | 自动换行核心 |
| `TodDrawStringWrapped` | TodStringFile.cpp:464 | 对外入口 |

> 说明：任务书所称「LoadString/按 ID 查字符串」对应 `TodStringListLoad`（加载）+ `TodStringListFind`/`TodStringTranslate`（按键名查找/翻译）。

### 7.3 TodDebug 调试模块（TodDebug.h:98 / TodDebug.cpp:322）

**职责**：日志（写文件/OutputDebugString）、断言与崩溃报告（异常栈 + MiniDump + 截图）、内存分配统计骨架、应用初始化（注册未处理异常过滤器）。注意 `TodTraceMemory/TodHesitationTrace/TodHesitationBracket` 为**空实现**（堆追踪未完整还原）。

| 函数签名 | 文件:行号 | 功能 |
|---|---|---|
| `HesitationBuffer::HesitationBuffer()` | TodDebug.cpp:19 | 256KB 跟踪缓冲 |
| `void TodErrorMessageBox(const char*, const char*)` | TodDebug.cpp:29 | 错误弹框 |
| `DECLSPEC_NORETURN void TodTraceMemory()` | TodDebug.cpp:38 | 内存追踪（空壳） |
| `void* TodMalloc(int)` | TodDebug.cpp:42 | malloc 封装 |
| `void TodFree(void*)` | TodDebug.cpp:53 | free 封装 |
| `void TodAssertFailed(const char*, const char*, int, const char*="", ...)` | TodDebug.cpp:61 | 断言失败处理 |
| `void TodLog(const char*, ...)` | TodDebug.cpp:106 | 追加写日志 |
| `void TodLogString(const char*)` | TodDebug.cpp:117 | 写日志 |
| `void TodTrace(const char*, ...)` | TodDebug.cpp:134 | OutputDebugString | ★★★ |
| `void TodHesitationTrace(...)` | TodDebug.cpp:145 | 卡顿跟踪（空壳） |
| `void TodTraceAndLog(const char*, ...)` | TodDebug.cpp:149 | Trace+日志 | ★★★ |
| `void TodTraceWithoutSpamming(const char*, ...)` | TodDebug.cpp:161 | 限频 Trace |
| `void TodGetExeDirectory(char*)` | TodDebug.cpp:180 | 取 exe 目录 |
| `void TodVsnprintfEnsureNewLine(char*, int, const char*, va_list)` | TodDebug.cpp:193 | 格式化保证换行 |
| `void TodCrashScreenshot(const char*)` | TodDebug.cpp:210 | 崩溃截图存 JPEG |
| `void TodReportError(LPEXCEPTION_POINTERS, const char*)` | TodDebug.cpp:249 | 崩溃总入口（栈回溯+MiniDump+截图） |
| `long __stdcall TodUnhandledExceptionFilter(LPEXCEPTION_POINTERS)` | TodDebug.cpp:290 | 未处理异常过滤器 |
| `void TodAssertInitForApp()` | TodDebug.cpp:309 | 建日志/注册异常过滤器 |

关键全局量：`gHesitation`、`gMemoryAppData`、`gLogFileName/gDebugDataFolder`、`gBetaSubmitFunc`、`TOD_ASSERT(condition,...)` 宏（TodDebug.h:78-96，Debug 版断言+断点，Release 空）。

### 7.4 StackWalk 崩溃调用栈（StackWalk.h:62 / StackWalk.cpp:196）

**职责**：崩溃调用栈捕获与符号化。运行时动态加载 `DBGHELP.DLL`，函数指针调用 `StackWalk/SymGetSymFromAddr/SymGetLineFromAddr` 等，输出「文件(行号): 函数名」，支持写出 MiniDump。

| 函数签名 | 文件:行号 | 功能 |
|---|---|---|
| `bool StackWalkInitialize()` | StackWalk.cpp:17 | 加载 DBGHELP.DLL 并解析函数地址 |
| `static void sStackWalkTraceOrLog(...)` | StackWalk.cpp:40 | 输出一行 |
| `void StackWalkShowMiniStack(StackWalkMiniStack*, bool, bool)` | StackWalk.cpp:68 | 符号化并打印迷你栈 |
| `static void sStackWalkCaptureMiniStackContext(...)` | StackWalk.cpp:123 | 逐帧回溯填充迷你栈 |
| `void StackWalkLogExceptionStack(LPEXCEPTION_POINTERS)` | StackWalk.cpp:158 | 异常栈写日志 |
| `void MiniDump(LPEXCEPTION_POINTERS, const char*)` | StackWalk.cpp:165 | 写 .dmp 转储 |

关键结构：`MINISTACK_MAX_FRAMES=12`、`StackWalkMiniStack{mFrameCount + mFrame[12]}`、DBGHELP 函数指针组。注：`"SymInitialzie"`（StackWalk.cpp:28）为原工程拼写错误。

---

## 8. 软件光栅化 SWTri 简述

**文件**：`Tod_SWTri.h`(100) / `Tod_SWTri.cpp`(93) / `TodDrawTriangle.cpp`(305) / `TodDrawTriangleInc.cpp`(496) / `Tod_SWTri_Loop.cpp`(54) / `Tod_SWTri_GetTexel.cpp`(66) / `Tod_SWTri_TexelARGB.cpp`(39) / `Tod_SWTri_Pixel8888.cpp`(87) / `Tod_SWTri_Pixel888.cpp`(64) / `Tod_SWTri_Pixel888_additive.cpp`(60) / `Tod_SWTri_Pixel565.cpp`(68) / `Tod_SWTri_Pixel565_additive.cpp`(66) / `Tod_SWTri_Pixel555.cpp`(69)

### 8.1 架构（宏实例化 + 内联片段）

这些 `.cpp` 均被 `#include` 内联包含、**不应单独编译**：
- `TodDrawTriangle.cpp`：单份「宏参数化函数模板」，被 `TodDrawTriangleInc.cpp` 反复 `#include` 展开出 92 个具体函数。
- `TodDrawTriangleInc.cpp`：用宏 `TRI0..TRI5` 反复 `#include "TodDrawTriangle.cpp"`。
- `Tod_SWTri.cpp:17` 内联 `TodDrawTriangleInc.cpp`，其 `Tod_SWTri_AddAllDrawTriFuncs()`（20-93）把 Tod 版本**覆盖**进 `gDrawTriFunc[]`（带纹理槽位，共 64 个非加性函数）。
- 像素/扫描线片段（`Tod_SWTri_Loop/GetTexel/TexelARGB/Pixel*`）被宏 `PIXEL_INCLUDE` 内联。

### 8.2 算法（TodDrawTriangle.cpp:74-285 骨架）

1. 顶点按 Y 冒泡排序（112-114）。
2. 全局漫反射预乘（MOD_ARGB && GLOBAL_ARGB，116-131）。
3. 整数化 Y 向上取整；`y0==y2` 退化返回（135-138）。
4. 长边增量（16.16 定点，142-156）。
5. 长边中点 mid；`mid==v1.x` 退化返回（160-163）。
6. 首行 sub-pixel 校正（167-181）。
7. 扫描线增量 `oneOverWidth = 2^48/(v1.x-mid)`（185-201）。
8. 上/下半三角各自逐扫描线，`Tod_SWTri_Loop.cpp` 逐像素内插并调 `PIXEL_INCLUDE`。

**关键机制**：Gouraud 着色（MOD_ARGB=1 时 ARGB 行间+行内插值）；**无透视校正**（u/v 屏幕空间仿射插值，无 1/w）；BLEND1 = 双线性纹理过滤（非 alpha 混合）；光栅器内无屏幕裁剪，靠退化三角形提前返回 + 纹素越界保护，真正裁剪在上层 `SWDrawShape` 的 Sutherland–Hodgman `clipShape`（SWTri.cpp:413-421）。

### 8.3 命名约定与宏映射

| 宏 | 名称段 | 含义 |
|---|---|---|
| `TRI0` 0/1/2/3 | `8888`/`0888`/`0565`/`0555` | 像素格式 |
| 固定 TEXTURED | `TEX1` | 恒带纹理 |
| `TRI2` 0/1 | `TALPHA0/1` | 纹理含 alpha |
| `TRI3` 0/1 | `MOD0/1` | 顶点色调制（Gouraud） |
| `TRI4` 0/1 | `GLOB0/1` | 全局漫反射 |
| `TRI5` 0/1 | `BLEND0/1` | 双线性纹理过滤 |
| `NAME_ADDITIVE` | `_ADDITIVE` | 加性混合 |

### 8.4 入口函数

| 入口 | 文件:行号 | 说明 |
|---|---|---|
| `SWHelper::SWDrawTriangle(...)` | SWTri.cpp:637 | 框架分发表入口（7bit 下标查 `gDrawTriFunc[]`） |
| `SWTri_AddDrawTriFunc(...)` | SWTri.cpp:490 | 写函数指针到表 |
| `Tod_SWTri_AddAllDrawTriFuncs()` | Tod_SWTri.cpp:20 | 用 Tod 版覆盖带纹理槽位（64 个） |
| `TodDrawTriangle_<FMT>_TEX1_...`（宏生成） | TodDrawTriangle.cpp:74 | 各具体光栅函数 |
| `gTodTriangleDrawAdditive` | Tod_SWTri.cpp:7 | 加性开关（置位时改调 `_ADDITIVE` 版） |
| `FixedFloor(int)` | Tod_SWTri.cpp:9 | 双线性定点向下取整 |

> 不存在 `Tod_SWTri_DrawTriangle`；总入口是框架 `SWHelper::SWDrawTriangle`。ADDITIVE 变体在头文件有 extern 声明（Tod_SWTri.h:70-97），但不直接注册，由非加性函数在 `gTodTriangleDrawAdditive` 为真时内部转调（仅 0888/0565 两种格式）。

### 8.5 Pixel 变体清单

| 文件 | 格式 | 混合方式 |
|---|---|---|
| `Tod_SWTri_Pixel8888.cpp` | 0x8888（32bit ARGB） | 真 alpha 混合（src over dst） |
| `Tod_SWTri_Pixel888.cpp` | 0x0888（32bit XRGB） | alpha 混合（alpha 恒 0xFF） |
| `Tod_SWTri_Pixel888_additive.cpp` | 0x0888 加性 | 分量相加 clamp |
| `Tod_SWTri_Pixel565.cpp` | 0x0565（16bit RGB565） | alpha 混合 |
| `Tod_SWTri_Pixel565_additive.cpp` | 0x0565 加性 | 相加 clamp |
| `Tod_SWTri_Pixel555.cpp` | 0x0555（16bit RGB555） | alpha 混合（无加性） |
| `Tod_SWTri_GetTexel.cpp` | — | 最近点/双线性采样 + alpha |
| `Tod_SWTri_TexelARGB.cpp` | — | 漫反射调制（预乘） |
| `Tod_SWTri_Loop.cpp` | — | 扫描线内插循环 |

像素片段（Pixel*/GetTexel/TexelARGB/Loop）为纯像素运算、宏驱动 → ★★★ 通用；核心光栅算法为通用扫描线三角形光栅器 → ★★★；`gTodTriangleDrawAdditive` 全局开关与注册覆盖属游戏定制 → ★★。

---

## 9. PakInterface 资源包接口

**文件**：`PakInterface.h`(246 行) / `PakInterface.cpp`(447 行)

### 9.1 职责总览

`.pak` 资源包读取库：把若干 `.pak`（如 `main.pak`）只读内存映射（`CreateFileMapping + MapViewOfFile`），解析包内目录表，向上层提供**仿 C 标准库 `fopen/fread/fseek/fgets` 的虚拟文件接口**（`FOpen/FRead/FSeek/...`）。命中 pak 记录时从映射内存读取并逐字节异或解密；未命中回退真实磁盘文件。另提供 `FindFirstFile/FindNextFile/FindClose` 枚举文件时优先遍历 pak 内索引。头文件里 `static` 的 `p_fopen/p_fread/...` 包装通过命名内存映射（`GetPakPtr`）跨模块共享 `PakInterface` 指针。

### 9.2 pak 文件格式（解析见 `AddPakFile`，PakInterface.cpp:36）

**文件头 + 目录表 + 数据区**（目录在前、数据在后），全文件逐字节 `XOR 0xF7` 混淆（**无压缩**）。

文件头（8 字节）：

| 字段 | 大小 | 值 | 位置 |
|---|---|---|---|
| 魔数 Magic | 4 字节 | `0xBAC04AC0`（解密后） | .cpp:78-80 |
| 版本 Version | 4 字节 | 必须为 0 | .cpp:86-92 |

目录项（.cpp:96-123）：

| 字段 | 大小 | 说明 |
|---|---|---|
| flags | 1 字节 | 位 `0x80`（FILEFLAGS_END）表目录结束 |
| nameWidth | 1 字节 | 文件名长度 |
| name | nameWidth 字节 | 文件名/路径 |
| srcSize | 4 字节 | 文件大小 |
| fileTime | 8 字节 | FILETIME 时间戳 |

目录结束位置即数据区起始偏移，各记录 `mStartPos += 该偏移` 换算成包内绝对偏移。

### 9.3 关键类/结构成员

- **`PakRecord`**（h:19）：`mCollection`（所属集合）、`mFileName`、`mFileTime`、`mStartPos`（包内偏移）、`mSize`。
- **`PakCollection`**（h:34）：`mFileHandle`、`mMappingHandle`、`mDataPtr`（映射基址）。
- **`PFILE`**（h:44）：`mRecord`（命中记录，NULL=真实文件）、`mPos`、`mFP`（回退 FILE*）。
- **`PFindData`**（h:51）：`mWHandle`、`mLastFind`、`mFindCriteria`。
- **`PakInterfaceBase`**（h:58）：抽象接口（纯虚 `FOpen/FClose/FSeek/FTell/FRead/FGetC/UnGetC/FGetS/FEof` + `FindFirstFile/FindNextFile/FindClose`）。
- **`PakInterface`**（h:78）：`mPakCollectionList`（`std::list<PakCollection>`）、`mPakRecordMap`（`std::map<std::string,PakRecord>`，大写文件名索引）。
- 全局：`gPakInterface`（.cpp:14）、`gPakFileMapping`/`gPakInterfaceP`（h:109-110）。

### 9.4 全部接口函数清单

> 勘误：任务书提到的 `OpenPakFile/ClosePakFile/GetFileSize/ReadBufferFromPak/GetFileList/PakFind` **在本库中不存在**。实际 API 是 `AddPakFile` + `F*` 系列虚拟文件接口。

**PakInterface 类方法（.h 声明 → .cpp 定义）**：

| 函数签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `PakInterface::PakInterface()` | .h:88 / .cpp:26 | 构造，写入跨模块共享指针 | ★★ |
| `PakInterface::~PakInterface()` | .h:89 / .cpp:32 | 析构 | ★★★ |
| `bool AddPakFile(const std::string&)` | .h:91 / .cpp:36 | 映射 pak、校验魔数/版本、解析目录索引 | ★★★ |
| `PFILE* FOpen(const char*, const char*)` | .h:92 / .cpp:194 | 只读打开（命中 pak 或回退 fopen） | ★★ |
| `int FClose(PFILE*)` | .h:93 / .cpp:223 | 关闭并释放 | ★★★ |
| `int FSeek(PFILE*, long, int)` | .h:94 / .cpp:232 | 定位（clamp 到 [0,mSize]） | ★★★ |
| `int FTell(PFILE*)` | .h:95 / .cpp:252 | 取位置 | ★★★ |
| `size_t FRead(void*, int, int, PFILE*)` | .h:96 / .cpp:261 | 读取并逐字节 XOR 0xF7 解密 | ★★★ |
| `int FGetC(PFILE*)` | .h:97 / .cpp:280 | 读一字符（解密、跳过 `\r`） | ★★ |
| `int UnGetC(int, PFILE*)` | .h:98 / .cpp:297 | 回退一字符 | ★★★ |
| `char* FGetS(char*, int, PFILE*)` | .h:99 / .cpp:309 | 读一行 | ★★ |
| `int FEof(PFILE*)` | .h:100 / .cpp:335 | 文件尾 | ★★★ |
| `bool PFindNext(PFindData*, LPWIN32_FIND_DATA)` | .h:85 / .cpp:343 | 索引 map 内通配符查找 | ★★ |
| `HANDLE FindFirstFile(LPCTSTR, LPWIN32_FIND_DATA)` | .h:102 / .cpp:401 | 优先 pak 索引，回退系统 API | ★★ |
| `BOOL FindNextFile(HANDLE, LPWIN32_FIND_DATA)` | .h:103 / .cpp:421 | 继续查找 | ★★ |
| `BOOL FindClose(HANDLE)` | .h:104 / .cpp:437 | 结束查找 | ★★ |

**PakInterfaceBase 纯虚接口（h:61-75）**：`FOpen(char*)`、`FClose`、`FSeek`、`FTell`、`FRead`、`FGetC`、`UnGetC`、`FGetS(char*)`、`FEof`、`FindFirstFile`、`FindNextFile`、`FindClose`；非纯虚 `FOpen(wchar_t*)`（h:62）、`FGetS(wchar_t*)`（h:70）。

**头文件内 static 全局入口包装（经 `GetPakPtr()` 路由，跨模块共享）**：`GetPakPtr()`（h:112）、`p_fopen(char*/wchar_t*)`（h:124/138）、`p_fclose`（h:152）、`p_fseek/p_ftell`（h:161/168）、`p_fread/p_fwrite`（h:175/182）、`p_fgetc/p_ungetc`（h:189/196）、`p_fgets(char*/wchar_t*)`（h:203/210）、`p_feof`（h:217）、`p_FindFirstFile/p_FindNextFile/p_FindClose`（h:224/231/238）。

**辅助函数（.cpp）**：`static std::string StringToUpper(const std::string&)`（.cpp:16）、`static void FixFileName(const char*, char*)`（.cpp:143，路径规范化：绝对→相对、统一反斜杠、折叠 `..`、转大写）。

### 9.5 读取流程（入口函数链）

1. **打开并映射** `AddPakFile`（.cpp:36）：`CreateFile→GetFileSize→CreateFileMapping(PAGE_READONLY)→MapViewOfFile`（38-58）→ 记录句柄与 `mDataPtr`（60-65）→ 注册 pak 自身记录（67-72）→ `FOpen("rb")`（74）→ 校验魔数（78-80）/版本（86-92）。
2. **解析目录表**（96-123）：循环读 flags/nameWidth/name/srcSize/fileTime，插入 `mPakRecordMap`（键=大写文件名），累加 `aPos`；`anOffset = FTell` 得数据区偏移，各记录 `mStartPos += anOffset`（125-135）。
3. **按文件名查找** `FOpen`（.cpp:194）：`FixFileName` 规范化+大写 → `mPakRecordMap.find`；命中建 `PFILE`（mRecord 指向记录、mPos=0、mFP=NULL）；未命中/非只读回退 `fopen`。
4. **读取** `FRead`（.cpp:261）：命中时源指针 `mCollection->mDataPtr + mStartPos + mPos`，逐字节 `*dest++ = *src++ ^ 0xF7` 解密；前移 mPos；否则 `fread`。

---

## 10. ImageLib 图像库

**文件**：`ImageLib.h`(42 行) / `ImageLib.cpp`(1661 行)。第三方子目录 `jpeg`(libjpeg 6b)、`png`(libpng 1.0.5)、`zlib`(zlib 1.1.3)、`jpeg2000`(jasper)、`j2k-codec`(商业 DLL) 不逐行通读，仅说明依赖关系。

### 10.1 职责总览

把磁盘（或 `.pak` 打包文件）里的位图统一解码为内存中的 **32 位 ARGB 像素缓冲**（`Image`），并提供若干格式写盘。**加载**：TGA/JPEG/PNG/GIF/JPEG2000（`.j2k`/`.jp2`）。**保存**：TGA/BMP/PNG/JPEG（**BMP 只写不读**）。内置「独立 alpha 图」合成机制（同名 `_xxx`/`xxx_` 存在时取低 8 位作 alpha）。

> 重要勘误：任务提示的 `LoadImage`/`Scale`/`CreateAChromakeyImage`/翻转/旋转/格式转换等函数**本库均不存在**（grep 确认 `Scale/Chroma/Rotate/Flip/Resize` 0 命中）。本库职责就是「加载 + 保存 + alpha 合成」，无缩放/抠像/旋转能力。

### 10.2 关键数据结构

**Image 类**（ImageLib.h:9-23）：`mWidth`/`mHeight`/`mBits`(unsigned long*，长 w*h)。构造清零、析构 `delete mBits`；`GetWidth/GetHeight/GetBits`。**无 mHasAlpha 标志**，alpha 恒作像素高 8 位（不透明处填 0xFF）。

**像素格式**：每像素 `unsigned long` = `0xAARRGGBB`（小端字节序 B,G,R,A）。

**全局配置**（ImageLib.cpp:1551-1553）：`gAlphaComposeColor`(=0xFFFFFF，独立 alpha 图无主图时填充 RGB；特判 0xFFFFFF=直接用 alpha 图 RGB)、`gAutoLoadAlpha`(=true)、`gIgnoreJPEG2000Alpha`(=true)。

### 10.3 全部导出函数清单（ImageLib.h 声明，实现均在 ImageLib.cpp）

| 函数签名 | 文件:行号 | 功能 | 可复用性 |
|---|---|---|---|
| `Image::Image()` | ImageLib.cpp:20 | 构造 | ★★★ |
| `virtual Image::~Image()` | ImageLib.cpp:27 | 析构 | ★★★ |
| `int Image::GetWidth()` | ImageLib.cpp:32 | 取宽 | ★★★ |
| `int Image::GetHeight()` | ImageLib.cpp:37 | 取高 | ★★★ |
| `unsigned long* Image::GetBits()` | ImageLib.cpp:42 | 取像素缓冲 | ★★★ |
| `bool WriteJPEGImage(const std::string&, Image*)` | ImageLib.cpp:796 | 存 JPEG（RGB 质量 80，丢 alpha） | ★★ |
| `bool WritePNGImage(const std::string&, Image*)` | ImageLib.cpp:865 | 存 PNG（RGBA 8bit） | ★★ |
| `bool WriteTGAImage(const std::string&, Image*)` | ImageLib.cpp:947 | 存 32 位 TGA（纯 stdio） | ★★★ |
| `bool WriteBMPImage(const std::string&, Image*)` | ImageLib.cpp:996 | 存 32 位 BMP（纯 stdio） | ★★★ |
| `Image* GetImage(const std::string&, bool lookForAlphaImage=true)` | ImageLib.cpp:1555 | **统一加载入口**，按扩展名分派 + alpha 合成 | ★★ |
| `void InitJPEG2000()` | ImageLib.cpp:1392 | `LoadLibrary("j2k-codec.dll")` | ★★ |
| `void CloseJPEG2000()` | ImageLib.cpp:1397 | `FreeLibrary` | ★★ |
| `void SetJ2KCodecKey(const std::string&)` | ImageLib.cpp:1406 | 设 j2k-codec 注册码 | ★★ |

导出全局变量：`gAlphaComposeColor`、`gAutoLoadAlpha`、`gIgnoreJPEG2000Alpha`。

### 10.4 内部函数（非导出，加载路径实现）

| 函数 | 文件:行号 | 作用 |
|---|---|---|
| `static void png_pak_read_data(...)` | ImageLib.cpp:50 | libpng 从 PFILE 读的回调 |
| `Image* GetPNGImage(...)` | ImageLib.cpp:66 | PNG 解码 |
| `Image* GetTGAImage(...)` | ImageLib.cpp:152 | 32 位 TGA 解码 |
| `int ReadBlobBlock(PFILE*, char*)` | ImageLib.cpp:214 | 读 GIF 数据子块 |
| `Image* GetGIFImage(...)` | ImageLib.cpp:222 | GIF 解码（自带 LZW） |
| `my_error_exit`（jpeg） | ImageLib.cpp:781 | libjpeg setjmp/longjmp 错误出口 |
| `jpeg_pak_src(...)`（+init/fill/skip/term） | ImageLib.cpp:1047-1127 | libjpeg 自定义 PFILE 源管理器 |
| `Image* GetJPEGImage(...)` | ImageLib.cpp:1130 | JPEG 解码 |
| `Image* GetJPEG2000Image(...)`（jasper 版，`#if 0` 死代码） | ImageLib.cpp:1211 | 未编译 |
| `Pak_seek/Pak_read/Pak_close` | ImageLib.cpp:1411-1423 | 适配 j2k-codec 的 PFILE 回调 |
| `Image* GetJPEG2000Image(...)`（j2k-codec DLL 版，实际生效） | ImageLib.cpp:1425 | 经 DLL 解码 `.j2k`/`.jp2` |

### 10.5 加载流程（入口 `GetImage`，ImageLib.cpp:1555）

1. `gAutoLoadAlpha==false` 强制 `lookForAlphaImage=false`。
2. 拆扩展名。
3. 按扩展名分派（匹配或无扩展名时尝试，失败继续下一种）：`.tga→GetTGAImage`、`.jpg→GetJPEGImage`、`.png→GetPNGImage`、`.gif→GetGIFImage`、`.j2k/.jp2→GetJPEG2000Image`。
4. 独立 alpha 图查找（递归找 `_名`/`名_`）。
5. alpha 合成：主图+alpha 图同尺寸时取 alpha 图低 8 位写入主图高 8 位；仅 alpha 图时按 `gAlphaComposeColor` 填 RGB。

各 `Get*Image` 统一把像素写成 `0xAARRGGBB` 并 `new unsigned long[w*h]` 包成 `Image`。

### 10.6 与第三方子目录的依赖关系

| 子目录 | 库/版本 | 关系与用途 |
|---|---|---|
| `jpeg` | libjpeg 6b | JPEG 编解码。解码经自定义 `jpeg_pak_src` 从 PFILE 读；编码 `WriteJPEGImage` 用 `jpeg_stdio_dest`（RGB、质量 80、丢 alpha） |
| `png` | libpng 1.0.5 | PNG 编解码。解码 `png_set_read_fn`+回调 `png_pak_read_data` 从 PFILE 读；编码 `png_init_io`（RGBA 8bit） |
| `zlib` | zlib 1.1.3 | **被 libpng 间接依赖**，ImageLib.cpp 不直接调用（PNG 的 DEFLATE 由 libpng 调 zlib）；GIF 用自带 LZW |
| `jpeg2000` | jasper | **当前构建未使用（死代码）**，头 include 已注释，jasper 版 `GetJPEG2000Image` 被 `#if 0` 排除 |
| `j2k-codec` | 商业 DLL | JPEG2000 实际解码器。`InitJPEG2000` 动态 `LoadLibrary`+`GetProcAddress`（10 个 `__stdcall` 函数），`GetJPEG2000Image` 用 `J2K_Callbacks`（Pak_seek/Pak_read/Pak_close）让 DLL 从 PFILE 读 |

**调用链**：上层 → `GetImage` → 各 `Get*Image` → 第三方库（jpeg/libpng/j2k-codec DLL；GIF 内部 LZW）。所有**读**路径经 PakLib 的 `PFILE/p_fopen/p_fread/p_fseek/p_fclose`（对 pak 透明）；所有**写**路径用标准 `fopen/fwrite` 直接写盘。

---

## 11. 可复用性评估

三个库均为**高价值独立组件**。按「能否剥离游戏上下文单独使用」分级：

### 11.1 直接可复用（★★★，几乎零游戏耦合）

- **TodCommon 数学部分**：插值曲线（19 个）、矩阵/几何（8 个）、加权/平滑随机（9 个）、数值内联（9 个）、颜色运算、字符串 snprintf——纯逻辑，可直接拷走。
- **Definition 系统核心**：`DefMap/DefField` 反射 + XML 解析 + FloatTrack + zlib 编译缓存，是通用「配置表序列化框架」；只依赖 `XMLParser`（Sexy 框架）与 zlib。
- **DataArray<T> 句柄池**：代际校验对象池，纯头文件模板，仅依赖 `TOD_ASSERT`。
- **TodList/TodAllocator**：双向链表 + 定长内存池，仅依赖 `TodMalloc/TodFree`。
- **SWTri 软件光栅化**：92 个扫描线三角形函数 + 像素混合片段，宏驱动纯像素运算，移植价值高。
- **FilterEffect 的 RGB/HSL 转换**、**BlendTransform/FloatLerp** 等纯算法。
- **PakLib**：仅依赖 Win32（CreateFileMapping/MapViewOfFile），`F*` 虚拟文件接口可直接复用。
- **ImageLib 的 Image 类 + TGA/BMP 读写**：纯 stdio 实现。

### 11.2 需少量适配（★★，依赖 SexyAppFramework 或第三方库）

- **Reanimator**：依赖 `Image/Graphics/MemoryImage/XMLParser/Definition`，但动画格式、插值、混合逻辑本身是通用的骨骼动画框架。
- **TodParticle**：依赖 `Definition/FloatTrack/Image/Graphics/DataArray/TodList`，粒子物理/轨道求值逻辑通用。
- **EffectSystem/Attachment/Trail**：依赖效果池与 `TodTriangleGroup`，但「延迟删除 + 批渲染 + 多边形裁剪」模式通用。
- **ImageLib 的 JPG/PNG/JPEG2000**：依赖 libjpeg/libpng/zlib/j2k-codec DLL。
- **TodFoley**：依赖 Sexy 的 SoundManager/DSoundInstance。
- **DefinitionLoadImage/LoadFont**、**TodResourceManager**：依赖 Sexy 资源管理器。

### 11.3 游戏耦合（★）

- `TodGetCurrentLevelName/TodHasUsedCheatKeys/TodAppCloseRequest`（函数指针注入的游戏回调）、`TodLoadResources` 系列、`gTodTriangleDrawAdditive` 全局开关、`GetFlashingColor`（UI 约定）、`FilterEffect` 的 PvZ 滤镜枚举、`gExtractResourcesByName` 等。

### 11.4 给移植/mod 者的建议

1. **做 mod 最有用的是 Definition + DataArray**：学会「DefMap 反射 + offsetof 字段表」就能自定义 XML 数据文件并让引擎加载；DataArray 提供安全的实体句柄。
2. **纯算法层**（TodCommon 数学/矩阵/曲线、SWTri 像素、PakLib、TGA/BMP）可整模块剥离，作为独立静态库。
3. **动画/粒子层**若要复用，需连同 `SexyAppFramework` 的 `Image/Graphics/MemoryImage`（及 `TodTriangleGroup` 批渲染路径）一起移植。
4. 所有资源读路径统一经 PakLib，写路径走 stdio，这是理解「资源在包内还是包外」的关键边界。

---

### 附录：覆盖文件与函数统计

- **覆盖自有源文件**：46 个 `.h/.cpp`（TodLib 42 + PakLib 2 + ImageLib 2），约 **15,135 行**（GBK 实测）。
- **第三方编解码树**（jpeg/png/zlib/jpeg2000/j2k-codec）：仅在依赖关系层面说明，未逐行通读。
- **函数/方法索引**：约 **580 个**，其中含 TodCommon 全局函数 80 个、PakInterface 接口（含包装）约 45 个、ImageLib 导出+内部函数 28 个、SWTri 光栅函数 92 个、Definition 解析函数 55 个、Reanimator 63 个、TodParticle 48 个。
