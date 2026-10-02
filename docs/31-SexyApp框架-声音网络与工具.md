# SexyAppFramework：声音/网络/工具库 模块文档

> 所属: `src/SexyAppFramework`
> 说明: PopCap 自研 C++ 游戏框架（SexyApp Framework）中的「声音 / 网络 / 工具库」子模块，供《植物大战僵尸》主程序与其它 SexyApp 系游戏复用。全部源文件为 **GBK（代码页 936）** 编码。
> 可复用性标注：★★★ = 零/低依赖纯工具，可直接抽出；★★ = 少量 Windows/框架依赖，稍加适配可用；★ = 深度耦合框架/平台，仅作参考。

---

## 1. 模块职责

本模块位于 `src/SexyAppFramework`，是框架的「非图形」基础设施层，按职责分为 5 组：

| 分组 | 文件 | 职责 |
|---|---|---|
| 声音系统 | `SoundManager.h` / `SoundInstance.h` / `DSoundManager.*` / `DSoundInstance.*` / `MusicInterface.*` / `Bass*` / `FMod*` | 音效(SFX)播放与音乐(Music)播放的抽象接口 + 两套后端（BASS 解码音乐、DirectSound 播放音效，FMOD 作可选解码器），运行时动态加载 DLL |
| 网络 | `HTTPTransfer.*` / `WinInetHTTPTransfer.*` | 异步 HTTP GET/POST 传输：`HTTPTransfer` 基于原始 WinSock，`WinInetHTTPTransfer` 基于 WinInet API（还支持写文件、multipart POST、HTTPS） |
| 解析器 | `XMLParser.*` / `PropertiesParser.*` / `DescParser.*` / `EncodingParser.*` | 文本解析栈：`EncodingParser` 底层字符/编码读取 → `XMLParser` 流式 XML → `PropertiesParser` 配置属性表 → `DescParser` 脚本化「描述符」语言 |
| 基础工具库 | `Common.*` / `Color.*` / `Rect.h` / `Point.h` / `Insets.*` / `Ratio.*` / `SexyVector.h` / `SexyMatrix.*` / `Buffer.*` / `KeyCodes.*` / `MTRand.*` / `PerfTimer.*` / `CritSect.*` / `SmartPtr.h` / `memmgr.h` / `ModVal.*` / `NativeDisplay.*` / `Debug.*` | 通用工具函数集、颜色/几何/矩阵、位缓冲区、键码、随机数、计时、临界区、智能指针、内存追踪、运行时改常量、调试 |
| 附加 | `FlashWidget.*` / `SEHCatcher.*` | Flash(Shockwave) 播放支持；结构化异常(SEH)捕获与崩溃上报 |

关键约定（来自 `Common.h`）：
- `SexyString` = `std::string` 或 `std::wstring`（由 `_USE_WIDE_STRING` 宏切换），`_S("x")` 生成对应字面量。
- 跨字符集转换宏：`SexyStringToStringFast` / `StringToWString` 等。
- 基本类型别名：`uchar/ushort/uint/ulong/int64`；数组长度宏 `LENGTH(a)`；大端转主机序宏 `LONG_BIGE_TO_NATIVE` 等。

---

## 2. 声音系统（BASS/FMOD 双后端）

### 2.1 架构总览

```
        SoundManager(抽象)                    MusicInterface(抽象)
        ├─ DSoundManager  (DirectSound)      ├─ BassMusicInterface (BASS)
        └─ FModSoundManager(FMOD)            └─ FModMusicInterface (FMOD)
        SoundInstance(抽象)                   BassLoader/BASS_INSTANCE (动态加载 bass.dll)
        ├─ DSoundInstance                     FModLoader/FMOD_INSTANCE (动态加载 fmod.dll)
        └─ FModSoundInstance
```

- **SFX 播放主线**：`DSoundManager` 用 DirectSound 建 `SOUND_FLAGS`(CTRLPAN|CTRLVOLUME|STATIC|LOCSOFTWARE|GLOBALFOCUS|CTRLFREQUENCY) 静态缓冲；`LoadSound` 优先加载缓存 `.wav`，否则依次尝试 `.wav / .mp3(FMOD) / .ogg(OggVorbis 或 FMOD) / .au` 并把解码结果回写为带 `dep `(源文件时间戳) + `xor `(异或加密) 的缓存 wav。
- **Music 播放主线**：`BassMusicInterface` 用 BASS 加载 MOD/流；`FModMusicInterface` 用 FMOD 加载样本/模块。
- **动态 DLL 加载**：`BassLoader` 与 `FModLoader` 把整库 API 声明为函数指针结构，`LoadBassDLL()/LoadFModDLL()` 失败即退出；避免静态链接第三方音频库。

### 2.2 SoundManager（抽象基类） — `SoundManager.h`

职责：音效管理器的统一接口（工厂基类），定义 256 个源声音 / 32 个播放通道的上限。

| 常量 | 值 | 说明 |
|---|---|---|
| `MAX_SOURCE_SOUNDS` | 256 | 可加载的源音效上限 |
| `MAX_CHANNELS` | 32 | 同时播放通道上限 |

主要函数索引：

| 签名 | 文件:行 | 功能 | 可复用 |
|---|---|---|---|
| `virtual bool Initialized()` | SoundManager.h:21 | 是否初始化成功 | — |
| `virtual bool LoadSound(unsigned int theSfxID, const std::string& theFilename)` | SoundManager.h:23 | 按 ID 加载音效 | — |
| `virtual int LoadSound(const std::string& theFilename)` | SoundManager.h:24 | 按文件名加载，返回分配的 ID | — |
| `virtual void ReleaseSound(unsigned int)` | SoundManager.h:25 | 释放单个源音效 | — |
| `virtual void SetVolume(double)` | SoundManager.h:27 | 设置全局音量 | — |
| `virtual bool SetBaseVolume(unsigned int, double)` | SoundManager.h:28 | 设置某音效基础音量 | — |
| `virtual bool SetBasePan(unsigned int, int)` | SoundManager.h:29 | 设置某音效基础声像 | — |
| `virtual SoundInstance* GetSoundInstance(unsigned int)` | SoundManager.h:31 | 取得可播放实例 | — |
| `virtual void ReleaseSounds()/ReleaseChannels()` | SoundManager.h:33/34 | 释放全部源/通道 | — |
| `virtual double GetMasterVolume()/SetMasterVolume(double)` | SoundManager.h:36/37 | 系统主音量 | — |
| `virtual void Flush()` | SoundManager.h:39 | 刷新 | — |
| `virtual void SetCooperativeWindow(HWND,bool)` | SoundManager.h:40 | 设置协作窗口 | — |
| `virtual void StopAllSounds()` | SoundManager.h:41 | 停止全部播放 | — |
| `virtual int GetFreeSoundId()/GetNumSounds()` | SoundManager.h:42/43 | 空闲 ID / 已加载数 | — |

### 2.3 SoundInstance（抽象基类） — `SoundInstance.h`

职责：单个音效播放实例接口。`SetPan` 注释指出 pan 单位是「-百分之一 dB 到 +百分之一 dB = 左到右」。

| 签名 | 文件:行 | 功能 |
|---|---|---|
| `virtual void Release()` | :15 | 停止并标记释放 |
| `virtual void SetBaseVolume(double)/SetBasePan(int)` | :17/18 | 基础音量/声像 |
| `virtual void AdjustPitch(double)` | :20 | 调音高（半音步数） |
| `virtual void SetVolume(double)/SetPan(int)` | :22/23 | 实例音量/声像 |
| `virtual bool Play(bool looping, bool autoRelease)` | :25 | 播放（循环/自动释放） |
| `virtual void Stop()/bool IsPlaying()/bool IsReleased()/double GetVolume()` | :26-29 | 停止/状态查询 |

### 2.4 DSoundManager — `DSoundManager.h/.cpp`（28KB，DirectSound 后端）

职责：核心音效管理器。基于 DirectSound 的静态缓冲 + 复制缓冲播放模型。

关键成员表：

| 成员 | 类型 | 说明 |
|---|---|---|
| `mSourceSounds[256]` | `LPDIRECTSOUNDBUFFER` | 源音效缓冲（静态） |
| `mSourceFileNames[256]` | `std::string` | 源文件名 |
| `mPrimaryBuffer` | `LPDIRECTSOUNDBUFFER` | 主缓冲（44.1kHz/16bit/立体声） |
| `mSourceDataSizes[256]` | `ulong` | 数据字节数 |
| `mBaseVolumes[256]/mBasePans[256]` | `double[]/int[]` | 每音效基础音量/声像 |
| `mPlayingSounds[32]` | `DSoundInstance*` | 播放通道槽位 |
| `mMasterVolume` | `double` | 主音量 |
| `mLastReleaseTick` | `DWORD` | 上次自动回收 tick |
| `mDirectSound` | `LPDIRECTSOUND` | DirectSound 对象（public） |
| `mHaveFMod` | `bool` | 是否启用 FMOD 解码（public） |

主要函数索引（DSoundManager.cpp）：

| 签名 | 文件:行 | 功能 | 可复用 |
|---|---|---|---|
| `DSoundManager(HWND, bool haveFMod)` | :21 | 初始化 DirectSound + 可选 FMOD，建主缓冲 44.1k/16bit/2ch | ★ |
| `~DSoundManager()` | :105 | 释放通道/源/FMOD/DirectSound | ★ |
| `int FindFreeChannel()` | :125 | 找空闲通道（每秒自动 `ReleaseFreeChannels`） | ★ |
| `bool Initialized()` | :150 | `mDirectSound != NULL` | ★ |
| `int VolumeToDB(double)` | :162 | 音量 0..1 → DSB 分贝值 `log10(1+v*9)` 曲线 | ★★ |
| `void SetVolume(double)` | :171 | 设置全部源音量 | ★ |
| `bool LoadWAVSound(unsigned int, const string&)` | :180 | 解析 RIFF/WAVE（fmt/data/dep/xor chunk），异或解密后填充缓冲 | ★ |
| `bool LoadFModSound(unsigned int, const string&)` | :325 | 用 FMOD 解码 mp3/ogg 到样本 | ★ |
| `bool LoadOGGSound(unsigned int, const string&)` | :467 / :542 | 用 libvorbis 解码 ogg 到缓冲 | ★ |
| `bool LoadAUSound(unsigned int, const string&)` | :549 | 加载 Sun .au 格式 | ★ |
| `bool LoadSound(unsigned int, const string&)` | :708 | **主入口**：缓存 wav → wav → mp3/ogg/au 回退链 | ★ |
| `int LoadSound(const string&)` | :769 | 按文件名查重/分配 ID 加载 | ★ |
| `void ReleaseSound(unsigned int)` | :790 | 释放单个源 | ★ |
| `int GetFreeSoundId()/GetNumSounds()` | :800/811 | 查询 | ★ |
| `bool SetBaseVolume/SetBasePan` | :823/832 | 存基础音量/声像 | ★ |
| `bool GetTheFileTime(const string&, FILETIME*)` | :841 | 取文件时间（用于缓存校验） | ★★ |
| `bool WriteWAV(unsigned int, const string&, const string&)` | :853 | 把源缓冲回写为带 `dep`/`xor` chunk 的缓存 wav | ★ |
| `SoundInstance* GetSoundInstance(unsigned int)` | :943 | 取可播放实例，套用基础 pan/vol | ★ |
| `void ReleaseSounds()/ReleaseChannels()/ReleaseFreeChannels()` | :970/980/990 | 释放源/通道/已释放通道 | ★ |
| `void StopAllSounds()` | :1000 | 停止全部（保留 autoRelease 标记） | ★ |
| `double GetMasterVolume()` | :1012 | 用 Win32 mixer 读 WaveOut 主音量 | ★ |
| `void SetMasterVolume(double)` | :1049 | 用 Win32 mixer 写 WaveOut 主音量 | ★ |
| `void Flush()/SetCooperativeWindow()` | :1085/1089 | 空实现 / 设协作级别 | ★ |

关键实现细节：
- 构造（:21-103）：若 `haveFMod`，`LoadFModDLL()` 后 `FSOUND_SetHWND/SetBufferSize(200)/Init(44100,64,GLOBALFOCUS)`；主缓冲设 44.1kHz/16bit/立体声，`SetCooperativeLevel(DSSCL_PRIORITY)`，失败则降级 `DSSCL_NORMAL`。
- `LoadSound`（:708）：先试 `GetAppDataFolder()+cached\xxx.wav` 缓存（校验 `dep` 时间戳），再试原始 `.wav`，再 FMOD `.mp3/.ogg`，再 OggVorbis `.ogg`，最后 `.au`；每次成功解码后 `WriteWAV` 生成缓存。
- `VolumeToDB`（:162）：`(log10(1+v*9)-1.0)*2333`，低于 -2000 时取 -10000（静音）。

### 2.5 DSoundInstance — `DSoundInstance.h/.cpp`

职责：DirectSound 音效实例。播放时用 `DuplicateSoundBuffer` 复制源缓冲。

关键成员表：`mSoundManagerP`（管理器）、`mSourceSoundBuffer`（源）、`mSoundBuffer`（复制缓冲）、`mAutoRelease/mHasPlayed/mReleased`、`mBasePan/mBaseVolume`、`mPan/mVolume`、`mDefaultFrequency`。

主要函数索引（DSoundInstance.cpp）：

| 签名 | 文件:行 | 功能 | 可复用 |
|---|---|---|---|
| `DSoundInstance(DSoundManager*, LPDIRECTSOUNDBUFFER)` | :6 | 复制源缓冲，取默认频率 | ★ |
| `~DSoundInstance()` | :47 | 释放复制缓冲 | ★ |
| `void RehupVolume()/RehupPan()` | :53/59 | 合成 `base*volume*masterVolume` 与 `basePan+pan` 应用到缓冲 | ★ |
| `void Release()` | :65 | 停止并标记 released | ★ |
| `void SetVolume(double)/SetPan(int)` | :71/77 | 设置实例音量/声像 | ★ |
| `void SetBaseVolume/SetBasePan` | :83/89 | 设置基础音量/声像 | ★ |
| `bool Play(bool looping, bool autoRelease)` | :95 | 停止后按循环标记播放 | ★ |
| `void Stop()` | :123 | 停止并回卷到 0 | ★ |
| `void AdjustPitch(double)` | :134 | 频率乘 `2^(steps/12)`（半音），钳制 DSBFREQUENCY_MIN/MAX | ★★ |
| `bool IsPlaying()` | :149 | 查询 DSBSTATUS_PLAYING | ★ |
| `bool IsReleased()` | :165 | 自动释放判定（autoRelease 且已停） | ★ |
| `double GetVolume()` | :173 | 返回 mVolume | ★ |

### 2.6 MusicInterface — `MusicInterface.h/.cpp`

职责：音乐播放抽象基类，**所有方法默认空实现**（cpp 里只有空函数体），供子类覆盖。

主要函数索引（MusicInterface.h）：

| 签名 | 文件:行 | 功能 |
|---|---|---|
| `LoadMusic(int, const string&)` | :17 | 加载歌曲 |
| `PlayMusic(int, int offset=0, bool noLoop=false)` | :18 | 播放 |
| `StopMusic/PauseMusic/ResumeMusic/StopAllMusic` | :19-22 | 控制 |
| `UnloadMusic/UnloadAllMusic/PauseAllMusic/ResumeAllMusic` | :24-27 | 卸载/批量控制 |
| `FadeIn(int,int=-1,double=0.002,bool=false)` | :29 | 淡入 |
| `FadeOut(int,bool=true,double=0.004)` / `FadeOutAll` | :30/31 | 淡出 |
| `SetSongVolume/SetSongMaxVolume/IsPlaying` | :32-34 | 音量/状态 |
| `SetVolume/SetMusicAmplify/Update` | :36-38 | 主音量/放大/每帧更新 |

### 2.7 BassMusicInterface + BassLoader — `BassMusicInterface.*` / `BassLoader.*`

职责：BASS 后端的音乐播放器。`BassMusicInfo` 保存单个歌曲状态，`BASS_INSTANCE` 运行时动态加载 `bass.dll`。

**BassMusicInfo**（BassMusicInterface.h:12-26）成员：`mHMusic(HMUSIC)/mHStream(HSTREAM)/mVolume/mVolumeAdd/mVolumeCap/mStopOnFade`；`GetHandle()` 返回 `mHMusic?mHMusic:mHStream`。

**BassMusicInterface** 成员：`mMusicMap(BassMusicMap=std::map<int,BassMusicInfo>)/mMaxMusicVolume/mMusicLoadFlags`。

主要函数索引：

| 签名 | 文件:行 | 功能 | 可复用 |
|---|---|---|---|
| `BassMusicInterface(HWND)` | BassMusicInterface.cpp:21 | 调 BASS_Init | ★ |
| `~BassMusicInterface()` | :78 | 卸载全部 + BASS_Free | ★ |
| `bool LoadMusic(int, const string&)` | :86 | 按扩展名 `.mo3` 走 BASS_MusicLoad，否则 BASS_StreamCreateFile | ★ |
| `void PlayMusic(int,int,bool)` | :131 | 播放 + 设置位置/循环 | ★ |
| `void StopMusic/StopAllMusic/UnloadMusic/UnloadAllMusic` | :160/171/183/200 | 停止/卸载 | ★ |
| `PauseMusic/PauseAllMusic/ResumeMusic/ResumeAllMusic` | :214/224/245/234 | 暂停/恢复 | ★ |
| `FadeIn/FadeOut/FadeOutAll` | :255/290/306 | 通过 mVolumeAdd/mVolumeCap 逐帧淡入淡出 | ★ |
| `SetVolume(double)` | :320 | 全局音量 + BASS_SetVolume | ★ |
| `SetSongVolume/SetSongMaxVolume/IsPlaying` | :333/345/358 | 歌曲音量/状态 | ★ |
| `SetMusicAmplify(int,double)` | :370 | MOD 放大 | ★ |
| `void Update()` | :380 | 逐帧推进淡入淡出（每 15ms 步进） | ★ |
| `int GetMusicOrder(int)` | :415 | 取 MOD order | ★ |

**BASS_INSTANCE**（BassLoader.h:15-92）：把 `bass.dll` 全部 API 声明为 `DWORD(WINAPI *...)` 函数指针（GetVersion/Init/Free/Channel*/MusicLoad/Stream*/Sample*/PluginLoad 等 70+ 项）；成员 `mModule(HMODULE)/mVersion2`。

**BassLoader.cpp** 函数索引：

| 签名 | 文件:行 | 功能 | 可复用 |
|---|---|---|---|
| `BASS_INSTANCE::BASS_INSTANCE(const char* dllName)` | :24 | `LoadLibrary` + 逐个 `GetProcAddress` 解析函数指针 | ★★ |
| `~BASS_INSTANCE()` | :123 | FreeLibrary | ★★ |
| `BASS_MusicSetAmplify/BASS_MusicPlay/BASS_MusicPlayEx/BASS_ChannelResume/BASS_StreamPlay` | :130/137/143/155/160 | 兼容 BASS 1.x/2.x 版本差异的封装 | ★★ |
| `Sexy::LoadBassDLL()` | :169 | 创建 `gBass`，失败退出 | ★ |
| `Sexy::FreeBassDLL()` | :185 | 释放 `gBass` | ★ |
| `BASS_INSTANCE* BASS_CreateInstance(char*)` / `BASS_FreeInstance` | BassLoader.h:97/98 | 独立实例创建/释放 | ★★ |

> 全局 `extern BASS_INSTANCE *gBass;`（BassLoader.h:95）。

### 2.8 FModMusicInterface + FModLoader + FModSoundManager/Instance

**FModMusicInfo**（FModMusicInterface.h:12-25）：`mHSample(FSOUND_SAMPLE*)/mHMusic(FMUSIC_MODULE*)/mVolume/mVolumeAdd/mVolumeCap/mStopOnFade/mRepeats`。

**FModMusicInterface** 成员：`mMusicMap/mMasterVolume/mMaxMusicVolume/mMaxSampleVolume`。

主要函数索引（FModMusicInterface.cpp）：

| 签名 | 文件:行 | 功能 |
|---|---|---|
| `FModMusicInterface(HWND)` | :18 | 初始化 FMOD 输出 |
| `~FModMusicInterface()` | :27 | 清理 |
| `FSOUND_SAMPLE* LoadFMODSample(const string&)` | :32 | 加载样本（优先缓存 wav） |
| `LoadSample(int, const string&, bool)` | :105 | 加载循环样本 |
| `LoadSample(int, intro, repeat, bool)` | :120 | 加载「前奏+循环段」两段样本 |
| `LoadMusic(int, const string&)` | :189 | FMUSIC_LoadSong 加载模块音乐 |
| `PlayMusic/PauseMusic/ResumeMusic/StopMusic/StopAllMusic` | :203/228/246/264/283 | 播放控制 |
| `FadeIn/FadeOut/FadeOutAll` | :304/338/350 | 淡入淡出 |
| `SetVolume/SetSongVolume/IsPlaying/Update` | :364/386/406/419 | 音量/状态/更新 |

**FMOD_INSTANCE**（FModLoader.h:15-230）：同 `BASS_INSTANCE` 思路，把 `fmod.dll` API 声明为函数指针；**活动部分仅前 55 行**（SetBufferSize/SetHWND/Init/Close/Sample*/PlaySound/Stream*/FMUSIC*），56-228 行为注释掉的完整 API 清单（CD/DSP/Reverb/3D/Record 等）。`extern FMOD_INSTANCE *gFMod;` + `LoadFModDLL()/FreeFModDLL()`（FModLoader.h:232-235）。FModLoader.cpp 中 `LoadFModDLL()` 解析这些入口，失败退出。

**FModSoundManager**（FModSoundManager.h）：成员 `mMasterVolume`、`mSourceStreams[256]`（`FSOUND_STREAM*`）。函数索引（FModSoundManager.cpp）：构造 :6、`Initialized():31`、`LoadSound(uint,..):36`、`LoadSound(string):64`、`SetVolume:80`、`GetSoundInstance:84`、`ReleaseSounds:91`、`ReleaseChannels:95`、`GetMasterVolume:99`、`SetMasterVolume:104`、`Flush:108`、`SetCooperativeWindow:112`。基于 `FSOUND_Stream_OpenFile` 的流式后端。

**FModSoundInstance**（FModSoundInstance.h）：成员 `mChannelNum(int)`、`mStream(FSOUND_STREAM*)`。函数索引（FModSoundInstance.cpp）：构造 :6、`Release:17`、`SetVolume:21`、`SetPan:25`、`Play:29`、`Stop:38`、`IsPlaying:42`、`IsReleased:47`。基于 FMOD channel 的实例。

---

## 3. 网络（HTTPTransfer / WinInetHTTPTransfer）

### 3.1 HTTPTransfer — `HTTPTransfer.h/.cpp`（原始 WinSock）

职责：基于 `winsock.h` 的非阻塞 TCP 实现异步 HTTP GET/POST。成员含 URL 拆分字段（proto/host/port/path）、内容缓冲、线程/状态标记。

结果码枚举（HTTPTransfer.h:12-24）：`RESULT_DONE/NOT_STARTED/NOT_COMPLETED/NOT_FOUND/HTTP_ERROR/ABORTED/SOCKET_ERROR/INVALID_ADDR/CONNECT_FAIL/DISCONNECTED`。

关键成员表：

| 成员 | 类型 | 说明 |
|---|---|---|
| `mTransferId` | `int` | 全局自增传输 ID |
| `mSocket` | `int` | socket 描述符 |
| `mSendStr/mSpecifiedBaseURL/mSpecifiedRelURL/mURL` | `std::string` | 发送串 / 基准URL / 相对URL / 完整URL |
| `mProto/mHost/mPath` | `std::string` | 协议/主机/路径 |
| `mPort` | `int` | 端口 |
| `mContentLength/mContent` | `int/std::string` | 内容长度/内容 |
| `mTransferPending/mThreadRunning/mExiting/mAborted/mResult` | `bool..int` | 状态标记 |

主要函数索引（HTTPTransfer.cpp）：

| 签名 | 文件:行 | 功能 | 可复用 |
|---|---|---|---|
| `HTTPTransfer()` / `~HTTPTransfer()` | :10/19 | 初始化；析构 Abort 后等线程退出 | ★ |
| `static std::string GetAbsURL(base, rel)` | :28 | 相对 URL 拼接（处理 `http://`、`/`、相对路径） | ★★★ |
| `void Fail(int)` | :64 | 设结果码 + 置 exiting | ★ |
| `bool SocketWait(bool checkRead, bool checkWrite)` | :70 | 非阻塞 `select`（100ms 超时），检测读/写/异常 | ★★ |
| `void GetThreadProc()` | :122 | 线程体：WSAStartup→socket→解析域名→connect→发请求→收响应解析头/内容 | ★ |
| `static void GetThreadProcStub(void*)` | :334 | 线程入口包装 | ★ |
| `void PrepareTransfer(const string&)` | :339 | 拆分 URL 为 proto/host/port/path | ★★ |
| `void StartTransfer()` | :382 | 启动线程 | ★ |
| `GetHelper/PostHelper` | :394/408 | GET/POST 内部构造请求 | ★ |
| `Get/Post(url)` 与 `Get/Post(base,rel)` | :424-446 | 公开接口 | ★ |
| `SendRequestString(host, sendString)` | :454 | 发原始请求串 | ★ |
| `Abort()/Reset()/WaitFor()/GetResultCode()/GetContent()` | :463/469/552/561/567 | 中断/重置/等待/取结果 | ★ |

### 3.2 WinInetHTTPTransfer — `WinInetHTTPTransfer.h/.cpp`（WinInet API）

职责：功能更强的 HTTP 客户端，基于 `wininet`。支持写输出文件、multipart POST、HTTPS、用户名密码、自定义 User-Agent/Content-Type。

结果码枚举（WinInetHTTPTransfer.h:11-25）：比 `HTTPTransfer` 多 `RESULT_HTTP_REDIRECT` 与 `RESULT_INTERNAL_ERROR`。

关键成员表（WinInetHTTPTransfer.h:28-51，含逆向偏移注释）：

| 成员 | 说明 |
|---|---|
| `mSpecifiedBaseURL/mSpecifiedRelURL/mURL/mProto` | URL 字段 |
| `mUserName/mUserPass` | 认证 |
| `mHost/mPort/mPath/mAction` | 主机/端口/路径/动作(GET|POST) |
| `mUserAgent/mPostContentType/mPostData` | 请求头/正文 |
| `mFP(FILE*)/mUsingFile` | 输出文件 |
| `mContent/mContentLength/mCurContentLength` | 内容缓冲 |
| `mTransferPending/mThreadRunning/mExiting/mAborted/mResult` | 状态 |
| `mFileCritSection(RTL_CRITICAL_SECTION)` | 文件写临界区 |

主要函数索引（WinInetHTTPTransfer.cpp）：

| 签名 | 文件:行 | 功能 | 可复用 |
|---|---|---|---|
| `WinInetHTTPTransfer()` / `~` | :8/19 | 初始化/清理 | ★ |
| `static GetAbsURL(base,rel)` | :30 | URL 拼接 | ★★★ |
| `Get/Post/PostMultiPart(url)` 与 `(base,rel)` 重载 | :67-105 | 公开接口 | ★ |
| `SetOutputFile(wstring/string)` | :113/124 | 设输出文件（结果写文件而非内存） | ★ |
| `Reset()/Abort()/WaitFor()/GetContent()` | :136/164/171/180 | 控制/取结果 | ★ |
| `TransferThreadProcStub(void*)` | :206 | 线程入口 | ★ |
| `GetHelper/PostHelper` | :217/229 | 构造请求（POST 设 multipart boundary） | ★ |
| `PrepareTransfer(const string&)` | :248 | 解析 URL（含 user:pass@host:port 处理） | ★★ |
| `StartTransfer()` | :338 | `CreateThread` 启动 | ★ |
| `TransferThreadProc()` | :362 | 主流程：InternetOpen→InternetConnect→HttpOpenRequest→HttpSendRequest→HttpQueryInfo→InternetReadFile | ★ |
| `Fail(EResult)` / `GetResultCode()` | :540/546 | 结果 | ★ |

关键实现细节：`TransferThreadProc`（:362-537）用状态机 `switch(aCurIndex)` 分三步建立 `hInternet[3]`（Open/Connect/OpenRequest），第 4 步循环 `InternetReadFile` 读 1024 字节块；若 `mUsingFile` 则 `fwrite` 到文件（加 `AutoCrit` 保护），否则存入 `mContent`。**注意**：内存模式下 `mContent = aBuf`（:503）只保留最后一帧，疑似旧代码遗留的缺陷，实际应使用文件输出模式。HTTPS 通过 `INTERNET_FLAG_SECURE`（:412）。

---

## 4. 解析器（XML / Properties / Desc / Encoding）

### 4.1 EncodingParser — `EncodingParser.h/.cpp`（底层编码感知字符读取）

职责：把字节流按编码解码为 `wchar_t` 流，是 XML/Desc 解析器的公共基类。支持 ASCII / UTF-8 / UTF-16(含 BOM 自动检测) / UTF-16LE / UTF-16BE。

关键成员：`mFile(PFILE*)`、`mBufferedText(WcharBuffer)`（`std::vector<wchar_t>` 回退缓冲）、`mGetCharFunc`（函数指针成员，动态选择解码器）、`mForcedEncodingType/mFirstChar/mByteSwap`。

枚举：
- `EncodingType { ASCII, UTF_8, UTF_16, UTF_16_LE, UTF_16_BE }`（:33-40）
- `GetCharReturnType { SUCCESSFUL, INVALID_CHARACTER, END_OF_FILE, FAILURE }`（:42-48）

主要函数索引（EncodingParser.cpp）：

| 签名 | 文件:行 | 功能 | 可复用 |
|---|---|---|---|
| `EncodingParser()` | :9 | 默认 `GetUTF8Char` | ★★ |
| `void SetEncodingType(EncodingType)` | :28 | 强制指定编码 | ★★ |
| `bool GetAsciiChar(wchar_t*, bool*)` | :42 | ASCII 单字节读取 | ★★ |
| `bool GetUTF8Char(wchar_t*, bool*)` | :54 | UTF-8 多字节解码（含非法序列检测） | ★★★ |
| `bool GetUTF16Char(wchar_t*, bool*)` | :146 | UTF-16（自动 BOM/字节序） | ★★ |
| `GetUTF16LEChar/GetUTF16BEChar` | :191/221 | 小端/大端 UTF-16 | ★★ |
| `bool OpenFile(const string&)` | :251 | 打开文件 + **BOM 自动检测**（FF FE/FE FF→UTF16，EF BB BF→UTF8） | ★★ |
| `bool CloseFile()/EndOfFile()` | :305/319 | 关闭/EOF | ★★ |
| `SetStringSource(wstring/string)` | :332/343 | 从内存字符串解析 | ★★ |
| `GetCharReturnType GetChar(wchar_t*)` | :350 | 取下一个字符（先回退缓冲再读文件） | ★★ |
| `PutChar/PutString` | :381/389 | 回退字符/字符串（用于预读回放） | ★★ |

**用法模式**：继承 `EncodingParser` → `OpenFile`（自动识别 BOM）或 `SetStringSource` → 循环 `GetChar(&c)` 处理字符。

### 4.2 XMLParser — `XMLParser.h/.cpp`（流式 XML）

职责：拉模式（pull）XML 解析器，每次 `NextElement` 返回一个元素节点。基于 `EncodingParser`。

**XMLParam**（:13-18）：`mKey/mValue` 字符串对。
**XMLElement**（:25-48）：类型枚举 `TYPE_NONE/START/END/ELEMENT/INSTRUCTION/COMMENT`；字段 `mType/mSection/mValue/mValueEncoded/mInstruction`；属性表 `mAttributes/mAttributesEncoded`（`XMLParamMap=std::map<SexyString,SexyString>`）+ `mAttributeIteratorList`（保序迭代器）。

`XMLParser` 成员：`mFileName/mErrorText/mLineNum/mHasFailed/mAllowComments/mSection`。

主要函数索引（XMLParser.cpp）：

| 签名 | 文件:行 | 功能 | 可复用 |
|---|---|---|---|
| `XMLParser()` / `~` | :7/14 | 初始化 | ★★ |
| `void Fail(const SexyString&)` | :18 | 记录错误 | ★★ |
| `void Init()` | :24 | 重置状态 | ★★ |
| `bool AddAttribute(XMLElement*, key, value)` | :32 | 加属性（map 去重） | ★★ |
| `bool AddAttributeEncoded(...)` | :46 | 加编码态属性 | ★★ |
| `bool OpenFile(const string&)` | :60 | 打开文件 | ★★ |
| `SetStringSource(const wstring&)` | :74 | 内存字符串源 | ★★ |
| `bool NextElement(XMLElement*)` | :81 | **核心**：状态机逐字符解析，产出 START/END/ELEMENT/INSTRUCTION/COMMENT 节点 | ★★ |
| `bool HasFailed()` | :435 | 失败标记 | ★★ |
| `GetErrorText/GetCurrentLineNum/GetFileName` | :440/445/450 | 错误信息 | ★★ |
| `inline void AllowComments(bool)` | XMLParser.h:78 | 是否产出 COMMENT 节点 | ★★ |

**用法模式**（拉取循环）：

```cpp
XMLParser aParser;
if (!aParser.OpenFile("data.xml")) { /* 失败 */ }
XMLElement anElement;
while (aParser.NextElement(&anElement)) {
    if (anElement.mType == XMLElement::TYPE_START) {
        // 开始标签：anElement.mValue 为标签名，anElement.mAttributes 为属性
    } else if (anElement.mType == XMLElement::TYPE_ELEMENT) {
        // 文本内容：anElement.mValue
    } else if (anElement.mType == XMLElement::TYPE_END) {
        // 结束标签
    }
}
```

### 4.3 PropertiesParser — `PropertiesParser.h/.cpp`（属性表）

职责：把 XML 配置文件解析进 `SexyAppBase` 的属性存储。依赖 `XMLParser`。

成员：`mApp(SexyAppBase*)/mXMLParser(XMLParser*)/mError/mHasFailed`。

主要函数索引（PropertiesParser.cpp）：

| 签名 | 文件:行 | 功能 | 可复用 |
|---|---|---|---|
| `PropertiesParser(SexyAppBase*)` | :7 | 创建 XMLParser | ★ |
| `Fail/ParseSingleElement/ParseStringArray` | :14/33/59 | 错误/单值/字符串数组 | ★ |
| `bool ParseProperties()` | :99 | 解析 `<String>/<StringArray>/<Boolean>/<Integer>/<Double>` 标签 | ★ |
| `bool DoParseProperties()` | :207 | 顶层循环，要求 `<Properties>` 根标签 | ★ |
| `ParsePropertiesBuffer(const Buffer&)` | :247 | 从二进制 Buffer 解析 | ★ |
| `ParsePropertiesFile(const string&)` | :255 | 从文件解析 | ★ |
| `GetErrorText()` | :262 | 错误文本 | ★ |

**用法模式**（配置文件结构）：

```xml
<Properties>
  <String id="Title">Plants vs. Zombies</String>
  <Integer id="Width">800</Integer>
  <Double id="Scale">1.5</Double>
  <Boolean id="Fullscreen">0</Boolean>   <!-- 1/YES/ON/TRUE 或 0/NO/OFF/FALSE -->
  <StringArray id="Levels">
    <String>level1</String><String>level2</String>
  </StringArray>
</Properties>
```

解析后经 `mApp->SetString/SetBoolean/SetInteger/SetDouble` 与 `mApp->mStringVectorProperties` 写入应用属性表。

### 4.4 DescParser — `DescParser.h/.cpp`（描述符脚本语言）

职责：一种类 LISP 的「描述符」脚本解析器，用于关卡/实体定义。把文本解析为 `DataElement` 树（单值/列表），`HandleCommand` 由使用者实现以驱动业务逻辑（纯虚）。

数据模型：
- `DataElement`（:10-20）：抽象基类，`mIsList` + `Duplicate()` 虚函数。
- `SingleDataElement`（:22-34）：`mString(std::wstring)` + `mValue(DataElement*)`。
- `ListDataElement`（:38-51）：`mElementVector(ElementVector)` 列表。
- 类型别名：`ElementVector/DataElementMap/WStringVector/IntVector/DoubleVector`（:36-56）。

成员：`mCmdSep`（命令分隔符，`CMDSEP_SEMICOLON=1`/`CMDSEP_NO_INDENT=2`）、`mError/mCurrentLineNum/mCurrentLine/mDefineMap`。

主要函数索引（DescParser.cpp）：

| 签名 | 文件:行 | 功能 | 可复用 |
|---|---|---|---|
| `DataElement/SingleDataElement/ListDataElement` 构造/析构/Duplicate | :8-82 | 数据树 + 深拷贝 | ★★ |
| `DescParser()` / `~` | :84/89 | 初始化 | ★★ |
| `virtual bool Error(const string&)` | :93 | 记录错误（返回 false） | ★★ |
| `virtual DataElement* Dereference(const wstring&)` | :101 | 按名查找 `mDefineMap`（`#define` 引用） | ★★ |
| `IsImmediate/Unquote` | :112/118 | 判断字面量 / 去引号 | ★★ |
| `GetValues(ListDataElement*, ListDataElement*)` | :162 | 求值列表（展开 define） | ★★ |
| `DataElementToString` | :214 | 树转回文本 | ★★ |
| `DataToString/DataToKeyAndValue` | :244/270 | 取值/键值 | ★★ |
| `DataToInt/DataToDouble/DataToBoolean` | :298/312/327 | 类型转换 | ★★ |
| `DataToStringVector/DataToList/DataToIntVector/DataToDoubleVector` | :351/399/418/438 | 向量转换 | ★★ |
| `ParseToList(const wstring&, ListDataElement*, bool, int*)` | :458 | 递归解析括号列表 | ★★ |
| `ParseDescriptorLine(const wstring&)` | :618 | 解析单行描述符 | ★★ |
| `virtual bool HandleCommand(const ListDataElement&) = 0` | DescParser.h:95 | **纯虚**，使用者实现 | ★★ |
| `LoadDescriptor(const string&)` | :639 | 读文件逐行解析 | ★★ |

**用法模式**：继承 `DescParser`，实现 `HandleCommand`；脚本形如 `command (param1 param2 (nested))`，支持 `#define NAME value` 宏与 `;` 命令分隔。

---

## 5. 基础工具库

### 5.1 Common — `Common.h/.cpp`（全局工具函数集，最佳可复用资产）

职责：`Sexy::` 命名空间下的数学/字符串/文件/内存/格式化工具函数。**这是框架复用价值最高的部分**。

**5.1.1 随机数（委托给 MTRand，见 5.10）**

| 签名 | 文件:行 | 功能 | 可复用 |
|---|---|---|---|
| `int Rand()` | Common.cpp:21 | 全局 MTRand 取随机数 | ★★ |
| `int Rand(int range)` | :26 | [0,range) 整数 | ★★ |
| `float Rand(float range)` | :31 | [0,range) 浮点 | ★★ |
| `void SRand(ulong theSeed)` | :36 | 播种 | ★★ |

**5.1.2 系统/目录**

| 签名 | 文件:行 | 功能 | 可复用 |
|---|---|---|---|
| `bool CheckFor98Mill()` | :41 | 检测 Win9x/Me（缓存结果） | ★ |
| `bool CheckForVista()` | :67 | 检测 Vista+（`dwMajorVersion>=6`） | ★ |
| `std::string GetAppDataFolder()` / `SetAppDataFolder(string)` | :93/98 | 读写 AppData 目录（Vista 才写） | ★ |
| `std::string GetCurDir()` | :520 | 当前目录 | ★ |
| `std::string GetFullPath(const string&)` | :526 | 相对→绝对路径 | ★★ |
| `std::string GetPathFrom(rel, dir)` | :531 | 基于 dir 解析相对路径 | ★★ |
| `bool AllowAllAccess(const string&)` | :629 | 放宽文件 ACL（ACL API） | ★ |
| `time_t GetFileDate(const string&)` | :853 | 文件修改时间 | ★★ |

**5.1.3 文件系统**

| 签名 | 文件:行 | 功能 | 可复用 |
|---|---|---|---|
| `bool Deltree(const string&)` | :716 | 递归删除目录树 | ★★ |
| `bool FileExists(const string&)` | :763 | 文件存在性 | ★★★ |
| `void MkDir(const string&)` | :775 | 递归建目录 | ★★★ |
| `std::string GetFileName(path, bool noExtension=false)` | :796 | 取文件名（可去扩展名） | ★★★ |
| `std::string GetFileDir(path, bool withSlash=false)` | :813 | 取目录部分 | ★★★ |
| `std::string RemoveTrailingSlash(dir)` | :828 | 去尾斜杠 | ★★★ |
| `std::string AddTrailingSlash(dir, bool backSlash=false)` | :838 | 加尾斜杠 | ★★★ |

**5.1.4 字符串转换/格式化**

| 签名 | 文件:行 | 功能 | 可复用 |
|---|---|---|---|
| `std::string URLEncode(const string&)` | :114 | URL 编码（空格→+，?&%+ 等→%XX） | ★★★ |
| `StringToUpper/Lower(string/wstring)` | :146/156/166/176 | 大小写转换 | ★★★ |
| `std::wstring StringToWString(const string&)` | :186 | ANSI→宽字符（MultiByteToWideChar） | ★★ |
| `std::string WStringToString(const wstring&)` | :195 | 宽→ANSI | ★★ |
| `StringToSexyString/WStringToSexyString/SexyStringToString/SexyStringToWString` | :217/226/235/244 | SexyString 双向转换 | ★★ |
| `Trim(string/wstring)` | :253/266 | 去首尾空白 | ★★★ |
| `bool StringToInt(string/wstring, int*)` | :279/331 | 严格整型转换（含 0x 十六进制/负数/错误码） | ★★★ |
| `bool StringToDouble(string/wstring, double*)` | :383/440 | 严格浮点转换（科学计数法） | ★★★ |
| `SexyString CommaSeperate(int)` | :498 | 数字千分位逗号 | ★★★ |
| `std::string vformat(char*, va_list)` / `wstring vformat(wchar_t*, va_list)` | :876/942 | vsprintf 安全格式化 | ★★ |
| `std::string StrFormat(char* fmt...)` / `wstring StrFormat(wchar_t* fmt...)` | :932/999 | printf 风格格式化 | ★★★ |
| `std::string Evaluate(string, const DefinesMap&)` | :1009 | 宏替换（`%NAME%` → 值） | ★★★ |
| `std::string XMLDecodeString(string/wstring)` | :1040/1083 | XML 实体解码（lt/amp/gt/quot/apos/nbsp/cr） | ★★★ |
| `std::string XMLEncodeString(string/wstring)` | :1127/1188 | XML 实体编码 | ★★★ |
| `Upper/Lower(string/wstring)` | :1249/1256/1263/1270 | 返回副本的大小写转换 | ★★★ |
| `int StrFindNoCase(char*, char*)` | :1279 | 忽略大小写查找子串 | ★★★ |
| `bool StrPrefixNoCase(char*, char*, int max=...)` | :1307 | 忽略大小写前缀判断 | ★★★ |
| `std::wstring UTF8StringToWString(const string&)` | :1326 | UTF-8 → 宽字符 | ★★ |

**5.1.5 内存序列化（移动指针式读写）**

| 签名 | 文件:行 | 功能 | 可复用 |
|---|---|---|---|
| `void SMemR(void*& src, void* dst, size_t)` | :1336 | 读内存并前移指针 | ★★★ |
| `void SMemRStr(void*& src, std::string&)` | :1342 | 读字符串（长度前缀） | ★★★ |
| `void SMemW(void*& dst, const void* src, size_t)` | :1350 | 写内存并前移指针 | ★★★ |
| `void SMemWStr(void*& dst, const string&)` | :1356 | 写字符串 | ★★★ |

**内联工具（Common.h）**：`inlineUpper/inlineLower/inlineLTrim/inlineRTrim/inlineTrim`（:195-247，原地字符串操作）；`StringLessNoCase/WStringLessNoCase`（:249-250，忽略大小写比较器）。

**其它宏/常量**：`SEXY_RAND_MAX=0x7FFFFFFF`（:127）、字节序转换宏（:106-109）、`LENGTH`（:111）。

### 5.2 Color — `Color.h/.cpp`

职责：RGBA 颜色类（int 分量）。`#pragma pack(1)` 的 `SexyRGBA{b,g,r,a}` 内存序结构（BGR 顺序，匹配 Win32 位图）。

关键成员：`mRed/mGreen/mBlue/mAlpha`；静态 `Color::Black/White`。

| 签名 | 文件:行 | 功能 | 可复用 |
|---|---|---|---|
| 7 个构造（int 色值/int RGBA/uchar*/int*/SexyRGBA） | Color.cpp:8-72 | 多源构造 | ★★★ |
| `GetRed/GetGreen/GetBlue/GetAlpha()` | :74-92 | 取分量 | ★★★ |
| `int& operator[](int)/int operator[](int) const` | :94/113 | 下标访问 [0..3] | ★★★ |
| `ulong ToInt() const` | :130 | 转 32 位色值 | ★★★ |
| `SexyRGBA ToRGBA() const` | :135 | 转 BGR 结构 | ★★★ |
| `operator==/operator!=` | :146/155 | 比较 | ★★★ |

### 5.3 Rect — `Rect.h`（模板矩形，纯头文件）

职责：模板类 `TRect<_T>`，含 `mX/mY/mWidth/mHeight`。别名 `Rect=TRect<int>`、`FRect=TRect<double>`。**零依赖纯工具**。

| 签名 | 文件:行 | 功能 | 可复用 |
|---|---|---|---|
| 3 个构造 | :21-34 | 坐标/拷贝/默认 | ★★★ |
| `bool Intersects(const TRect&)` | :36 | 相交判断 | ★★★ |
| `TRect Intersection(const TRect&)` | :44 | 求交集 | ★★★ |
| `TRect Union(const TRect&)` | :56 | 求并集 | ★★★ |
| `bool Contains(x,y)/Contains(TPoint)` | :65/71 | 点包含 | ★★★ |
| `void Offset(x,y)/Offset(TPoint)` | :77/83 | 平移 | ★★★ |
| `TRect Inflate(x,y)` | :89 | 膨胀 | ★★★ |
| `operator==` | :99 | 相等 | ★★★ |
| `RECT ToRECT()` | :104 | 转 Win32 RECT | ★★ |

### 5.4 Point — `Point.h`（模板点，纯头文件）

职责：`TPoint<_T>`，`mX/mY`。别名 `Point=TPoint<int>`、`FPoint=TPoint<double>`。**零依赖纯工具**。

| 签名 | 文件:行 | 功能 | 可复用 |
|---|---|---|---|
| 3 个构造 | :16-32 | 坐标/拷贝/默认 | ★★★ |
| `==/!=` | :34/39 | 比较 | ★★★ |
| `+,-,*,/`（点-点） | :44-47 | 算术 | ★★★ |
| `+=,-=,*=,/=` | :48-51 | 复合赋值 | ★★★ |
| `*,_T` / `/_T`（标量） | :52/53 | 缩放 | ★★★ |

### 5.5 Insets — `Insets.h/.cpp`

职责：四边内边距（`mLeft/mTop/mRight/mBottom`）。

| 签名 | 文件:行 | 功能 | 可复用 |
|---|---|---|---|
| `Insets()` / `Insets(l,t,r,b)` / 拷贝 | Insets.cpp:5/14/22 | 构造 | ★★★ |

### 5.6 Ratio — `Ratio.h/.cpp`

职责：整数分数比（`mNumerator/mDenominator`），用于比例缩放。

| 签名 | 文件:行 | 功能 | 可复用 |
|---|---|---|---|
| `Ratio()` / `Ratio(int,int)` / `Set(int,int)` | Ratio.cpp:7/13/18 | 构造/设置 | ★★★ |
| `==/!=/<` | Ratio.h:20/25/30 | 比较（通分） | ★★★ |
| `int operator*(int)/operator/(int)` | Ratio.h:36/41 | 缩放/反缩放 | ★★★ |
| 全局 `int operator*(int,Ratio)/(int,Ratio)` | Ratio.h:46/51 | 对称运算 | ★★★ |

### 5.7 SexyVector — `SexyVector.h`（2D/3D 向量，纯头文件）

职责：`SexyVector2{x,y}` 与 `SexyVector3{x,y,z}`。**零依赖纯工具**（仅 `<math.h>`）。

`SexyVector2`：`Dot`、`+ - - * /`、`+= -= *= /=`、`== !=`、`Magnitude/MagnitudeSquared/Normalize/Perp`。
`SexyVector3`：`Dot/Cross`、`+ - * /`、`Magnitude/Normalize`。

### 5.8 SexyMatrix — `SexyMatrix.h/.cpp`

职责：3×3 矩阵 + 2D 变换 + 复合变换。依赖 `SexyVector.h`。

- `SexyMatrix3`（:11-34）：union `m[3][3]` / 命名分量 `m00..m22`；`ZeroMatrix/LoadIdentity`、`operator*`（矩阵×向量/矩阵×矩阵）、`operator*=`。
- `SexyTransform2D`（:39-57）：`Translate/RotateRad/RotateDeg/Scale`（注意：`Rotate` 已改为 `RotateRad`，正旋转为屏幕坐标逆时针）。
- `Transform`（:61-86）：惰性矩阵（`mComplex/mHaveRot/mHaveScale` + `mNeedCalcMatrix`），`Translate/RotateRad/RotateDeg/Scale/Reset/GetMatrix`。

函数索引（SexyMatrix.cpp）：`SexyMatrix3():9`、`ZeroMatrix:15`、`LoadIdentity:24`、`operator*(矩阵):32`、`operator*(Vec2):53`、`operator*(Vec3):62`、`operator*=:72`、`SexyTransform2D 构造:79/86/94`、`Translate:108`、`RotateRad:121`、`RotateDeg:139`、`Scale:146`、`Transform():158`、`Reset:165`、`Translate:179`、`RotateRad:201`、`RotateDeg:223`、`Scale:230`、`MakeComplex:253`、`CalcMatrix:264`、`GetMatrix:291`。

### 5.9 Buffer — `Buffer.h/.cpp`（位级缓冲区）

职责：二进制序列化缓冲区，支持**位级**读写与 Web 安全字符串（Base64 风格）转换。

关键成员：`mData(ByteVector=std::vector<uchar>)/mDataBitSize/mReadBitPos/mWriteBitPos`。

| 签名 | 文件:行 | 功能 | 可复用 |
|---|---|---|---|
| `Buffer()` / `~` | :157/164 | 初始化 | ★★★ |
| `std::string ToWebString() const` | :168 | 转 Base64 风格字符串 | ★★★ |
| `std::wstring UTF8ToWideString() const` | :189 | UTF-8 内容转宽字符 | ★★ |
| `void FromWebString(const string&)` | :216 | 从 Web 字符串解码 | ★★★ |
| `SeekFront()/Clear()` | :254/259 | 回卷/清空 | ★★★ |
| `WriteByte/WriteNumBits/GetBitsRequired` | :267/283/298 | 写字节/写 N 位/算位数 | ★★★ |
| `WriteBoolean/WriteShort/WriteLong` | :313/318/324 | 写基本类型 | ★★★ |
| `WriteString/WriteUTF8String/WriteLine` | :332/339/373 | 写字符串 | ★★★ |
| `WriteBuffer/WriteBytes/SetData(x2)` | :378/385/391/397 | 写/设数据 | ★★★ |
| `ReadByte/ReadNumBits/ReadBoolean/ReadShort/ReadLong` | :404/432/458/463/470 | 读 | ★★★ |
| `ReadString/ReadUTF8String/ReadLine` | :480/491/518 | 读字符串 | ★★★ |
| `ReadBytes/ReadBuffer` | :536/542 | 读数据 | ★★★ |
| `GetDataPtr/GetDataLen/GetDataLenBits` | :551/558/563 | 访问 | ★★★ |
| `ulong GetCRC32(ulong theSeed=0) const` | :568 | CRC32 校验 | ★★★ |
| `AtEnd()/PastEnd()` | :575/581 | 位置判断 | ★★★ |

### 5.10 MTRand — `MTRand.h/.cpp`（梅森旋转随机数）

职责：MT19937 梅森旋转（Mersenne Twister）伪随机数发生器，算法版权归 Makoto Matsumoto / Takuji Nishimura。

**算法说明**：
- 状态：`unsigned long mt[624]` + 索引 `mti`（`MTRAND_N=624`）。
- 周期参数：`MTRAND_M=397`、`MATRIX_A=0x9908b0df`、`UPPER_MASK=0x80000000`、`LOWER_MASK=0x7fffffff`（:46-49）。
- 播种 `SRand(seed)`（:100）：`mt[0]=seed`，`mt[i]=1812433253*(mt[i-1]^(mt[i-1]>>30))+i`（Knuth TAOCP 线性同余初始化，seed 为 0 时改用默认 4357）。
- 生成 `NextNoAssert()`（:129）：每 624 次「扭曲」一次——`y=(mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK)`，`mt[kk]=mt[kk+M]^y>>1^mag01[y&1]`；随后**回火(tempering)**：`y^=y>>11; y^=(y<<7)&0x9d2c5680; y^=(y<<15)&0xefc60000; y^=y>>18`；最后 `y&=0x7FFFFFFF`（输出 31 位非负数）。

主要函数索引：

| 签名 | 文件:行 | 功能 | 可复用 |
|---|---|---|---|
| `MTRand(string)` / `MTRand(ulong)` / `MTRand()` | :60/66/71 | 从序列化串/种子/默认(4357) 构造 | ★★★ |
| `static void SetRandAllowed(bool)` | :77 | 断言用：控制随机调用开关（配 `MTAutoDisallowRand` RAII） | ★★★ |
| `void SRand(string)` / `SRand(ulong)` | :90/100 | 恢复状态/播种 | ★★★ |
| `unsigned long Next()/NextNoAssert()` | :123/129 | 取随机数（含/不含断言） | ★★★ |
| `Next(range)/NextNoAssert(range)` | :172/167 | 取模范围随机数 | ★★★ |
| `float Next(range)/NextNoAssert(range)` | :183/178 | 浮点范围随机数 | ★★★ |
| `std::string Serialize()` | :189 | 序列化 624×4 字节状态（用于存档恢复） | ★★★ |

> 配套 `MTAutoDisallowRand`（MTRand.h:35-39）为 RAII 守卫，作用域内禁止调用 `Next()`（用于确保「确定性路径」不引入随机）。

### 5.11 PerfTimer — `PerfTimer.h/.cpp`（高性能计时）

职责：基于 `QueryPerformanceCounter` 的计时器 + CPU 测速 + 命名区间性能统计。

- `PerfTimer`（:11-29）：`mStart/mDuration/mRunning`；`Start/Stop/GetDuration`（毫秒）、`static GetCPUSpeed()`(Hz)/`GetCPUSpeedMHz()`。
- `SexyPerf`（:33-44）：`BeginPerf/EndPerf/IsPerfOn/StartTiming/StopTiming/GetResults`（静态，全局按名称统计调用次数/累计/最长耗时）。
- `SexyAutoPerf`（:48-80）：RAII 计时器，构造 `StartTiming`、析构 `StopTiming`。
- 宏（:94-120）：`SEXY_PERF_BEGIN/END/SEXY_AUTO_PERF` 在 `SEXY_PERF_ENABLED && !RELEASEFINAL` 时展开为真实调用，否则空。

函数索引（PerfTimer.cpp）：`PerfTimer():66`、`CalcDuration:75`、`Start:85`、`Stop:93`、`GetDuration:104`、`GetCPUSpeed:114`、`GetCPUSpeedMHz:128`、`IsPerfOn:235`、`BeginPerf:242`、`EndPerf:257`、`StartTiming:280`、`StopTiming:292`、`GetResults:304`。

### 5.12 CritSect — `CritSect.h/.cpp`（临界区）

职责：`CRITICAL_SECTION` 封装（构造 InitializeCriticalSection、析构 DeleteCriticalSection）。配套 `AutoCrit`（在框架其它头文件，此处仅 `friend class AutoCrit`）。**极简 Windows 依赖**。

函数索引（CritSect.cpp）：`CritSect():10`、`~CritSect():17`。

### 5.13 SmartPtr — `SmartPtr.h`（智能指针，纯头文件）

职责：引用计数智能指针（非侵入式 `RefCount` + 侵入式指针接口）。

- `RefCount`（:13-47）：`mRefCount`；`CreateRef()`（`InterlockedIncrement`）、`Release()`（`InterlockedDecrement` ≤0 时 `delete this`）、`GetRefCount()`。
- `ConstSmartPtr<T>`（:54-102）：只读智能指针，`->/const T*/=/==/!=/</get()`，含 `Comp` 比较器。
- `SmartPtr<T>`（:107-131）：派生可写指针。
- 宏 `SEXY_PTR_FORWARD(X)`（:143-144）：前向声明 + 生成 `X##Ptr` 别名。
- 注意 `#pragma pack(push,8)` 保证 `InterlockedIncrement` 对齐（:5）。

### 5.14 memmgr — `memmgr.h`（内存追踪，纯头文件）

职责：`SEXY_MEMTRACE` 宏开启时，重载全局 `operator new/delete`，记录分配的文件/行号到 `SexyMemAddTrack/SexyMemRemoveTrack`，退出时 `SexyDumpUnfreed()` 打印泄漏到 `mem_leaks.txt`。`#define new DEBUG_NEW`（`new(__FILE__,__LINE__)`）。需在 `.cpp` 文件**所有 include 之后**包含。

### 5.15 ModVal — `ModVal.h/.cpp`（运行时改常量）

职责：用 `M(x)` 宏包裹数值常量，允许运行时通过 `ReparseModValues()` 重读源码并热更新常量（调试利器）。

- 宏：`M(val)` → `ModVal(0, "SEXY_SEXYMODVAL"__FILE__","__COUNTER__","__LINE__", val)`；`M1..M9` 别名。`SEXY_DISABLE_MODVAL || RELEASEFINAL` 时退化为 `(val)`。
- 函数（ModVal.cpp）：`ModVal(int,..int):202`、`ModVal(..double):214`、`ModVal(..float):226`、`ModVal(..char*):231`、`AddModValEnum:244`、`ReparseModValues():430`。
- 内部机制：`FindModValsInMemory` 在进程内存中搜索 `SEXYMODVAL` 标记定位源文件与行，`CreateFileMods` 建 `FileMod` 映射，`ReparseModValues` 用 `fstream` 重读源文件并比对改动。

### 5.16 NativeDisplay — `NativeDisplay.h/.cpp`

职责：记录本机显示像素格式（RGB 位深/掩码/移位）。成员：`mRGBBits/mRedMask/mGreenMask/mBlueMask/mRedBits/mGreenBits/mBlueBits/mRedShift/mGreenShift/mBlueShift`。函数：`NativeDisplay():7`、`~NativeDisplay():26`（在 .cpp 中从屏幕 DC 读取 `GetDeviceCaps`/掩码）。

### 5.17 Debug — `Debug.h/.cpp`

职责：调试输出 + 断言 + 内存泄漏追踪（`SexyAllocMap`）。

- `SexyTraceFmt/SexyTrace/OutputDebug`：格式化调试输出。
- 宏 `DBG_ASSERT/DBG_ASSERTE`：`NDEBUG` 时为空，否则 `assert`（先置 `gInAssert`）。
- `SEXY_TRACE(theStr)`：`SEXY_TRACING_ENABLED` 时才输出。
- Debug.cpp 内 `SexyAllocMap`（:30）配合 memmgr 追踪分配。

### 5.18 KeyCodes — `KeyCodes.h/.cpp`

职责：键码枚举（`KeyCode`，ASCII/扩展键/F1-F24/小键盘）与名称互转。

- 枚举 `KeyCode`（KeyCodes.h:9-102）：`KEYCODE_*`（LBUTTON..SCROLL，ASCII 0x30-0x5A、0xB3-0xE0）。
- `GetKeyCodeFromName(const string&)`（KeyCodes.cpp:104）：名字→键码（单字符直接映射 ASCII，否则查 `aKeyCodeArray` 表）。
- `GetKeyNameFromCode(const KeyCode&)`（:132）：键码→名字。

---

## 6. FlashWidget / SEHCatcher 简述

### 6.1 FlashWidget — `FlashWidget.h/.cpp`（35KB）

职责：Flash(Shockwave) 播放 Widget。基于 COM 加载 `ShockwaveFlashObjects::IShockwaveFlash` 与 `IOleObject/IOleInPlaceObjectWindowless`，把 Flash 渲染到 `DDImage`，并桥接鼠标事件与 Flash 变量。

- `FlashListener`（FlashWidget.h:24-29）：`FlashAnimEnded/FlashCommand` 回调接口。
- 状态枚举：`STATE_IDLE/PLAYING/STOPPED`；质量 `QUALITY_LOW/MEDIUM/HIGH`。
- 关键成员：`mFlashLibHandle/mControlSite/mFlashSink/mFlashInterface/mOleObject/mWindowlessObject/mCOMCount/mImage/mDirtyRect` 等（:49-74）。
- 主要函数（FlashWidget.cpp）：`FlashWidget():909`、`~:979`、`static GetFlashVersion():1009`、`StartAnimation:1085`、`SetQuality:1101`、`Pause/Unpause/Rewind/Back/Forward:1109/1120/1130/1148/1157`、`GotoFrame:1139`、`GetCurrentFrame:1174`、`GetCurrentLabel:1182`、`CallFrame/CallLabel:1190/1196`、`GetVariable/SetVariable:1202/1210`、`Update:1216`、`Draw:1277`、鼠标事件 :1321-1366。
- `FlashSink`（:396 起）实现 `_IShockwaveFlashEvents` 事件接收器（通过连接点 `FindConnectionPoint`）。**重依赖 Flash OCX，仅作参考**。

### 6.2 SEHCatcher — `SEHCatcher.h/.cpp`（37KB）

职责：全局结构化异常(SEH)捕获 + 崩溃报告。用 `SetUnhandledExceptionFilter` 挂 `UnhandledExceptionFilter`，弹出错误对话框，可把崩溃堆栈提交到服务器。

- 成员：全静态——`mApp`、字体/窗口句柄、`mImageHelpLib` 与 `imagehlp.dll` 的符号函数指针（`mSymInitialize/mStackWalk/...`）、`mSubmitReportTransfer(HTTPTransfer)`（:35-64）。
- 主要函数（SEHCatcher.cpp）：`SEHCatcher():86`、`UnhandledExceptionFilter:96`、`LoadImageHelp:109`、`UnloadImageHelp:167`、`GetSymbolsFromMapFile:197`（解析 `.map` 文件符号）、`DoHandleDebugEvent:346`（生成崩溃 dump）、`IntelWalk:472`/`ImageHelpWalk:513`（堆栈回溯）、`GetLogicalAddress:593`、`ShowErrorDialog:1066`、`SubmitReportThread:854`、`GetSysInfo:1243`。
- 依赖：`imagehlp.dll` 动态加载（`LoadImageHelp` 用 `GetProcAddress`）、`HTTPTransfer` 上报、`SexyAppBase::GetGameSEHInfo/NotifyCrashHook`。**深度耦合框架与 Windows，仅作参考**。

---

## 7. 可复用性评估

### 7.1 零依赖/低依赖纯工具（★★★，可直接抽出）

| 资产 | 文件 | 依赖 | 复用要点 |
|---|---|---|---|
| 矩形 `TRect` | `Rect.h` | 仅 `Common.h`(可剥离)、`Point.h` | 相交/交集/并集/包含/膨胀齐全 |
| 点 `TPoint` | `Point.h` | 无实质依赖 | 完整运算符重载 |
| 2D/3D 向量 | `SexyVector.h` | `<math.h>` | Dot/Cross/Magnitude/Normalize/Perp |
| 3×3 矩阵/变换 | `SexyMatrix.h/.cpp` | `SexyVector.h` | 平移/旋转/缩放/复合 |
| 颜色 `Color` | `Color.h/.cpp` | `Common.h` 类型别名 | 多源构造/ToInt/ToRGBA |
| 比例 `Ratio` | `Ratio.h/.cpp` | 无 | 整数比例缩放 |
| 内边距 `Insets` | `Insets.h/.cpp` | 无 | 简单 POD |
| 随机数 `MTRand` | `MTRand.h/.cpp` | `Debug.h`(DBG_ASSERT)、`<windows.h>` | 经典 MT19937，可序列化/恢复状态 |
| 位缓冲区 `Buffer` | `Buffer.h/.cpp` | `Common.h` | 位级读写 + CRC32 + Web 字符串 |
| 计时 `PerfTimer` | `PerfTimer.h/.cpp` | `Common.h`、Windows QPC | 毫秒计时 + 命名统计 + RAII |
| 智能指针 `SmartPtr.h` | 纯头文件 | `InterlockedIncrement`(Windows) | RefCount 可跨平台替换 |
| 字符串/文件工具 | `Common.h/.cpp`(部分) | Windows/STL | `Trim/Upper/Lower/StrFindNoCase/StrPrefixNoCase/StrFormat/Evaluate/XMLEncode/Decode/URLEncode/GetFileName/GetFileDir/AddTrailingSlash/FileExists/MkDir/Deltree/CommaSeperate/SMemR/SMemW` |
| 严格数值转换 | `Common.cpp:279/383` | STL | `StringToInt/StringToDouble` 带错误处理 |
| 键码映射 | `KeyCodes.h/.cpp` | 无 | 键名↔键码双向查表 |
| 临界区 | `CritSect.h/.cpp` | Windows | CRITICAL_SECTION 薄封装 |
| 内存追踪 | `memmgr.h` | 需配套 `Debug.cpp` | 可选宏开关 |

### 7.2 少量依赖，稍加适配可用（★★）

| 资产 | 文件 | 依赖 | 适配点 |
|---|---|---|---|
| 编码解析 | `EncodingParser.h/.cpp` | `Common.h`(SexyString)、`PFILE`(PakLib 的 p_fopen) | 把 `p_fopen` 换成标准 `fopen` 即可跨平台 |
| XML 流式解析 | `XMLParser.h/.cpp` | `EncodingParser`、`SexyString` | 同上，拉模式接口清晰 |
| 描述符解析 | `DescParser.h/.cpp` | `EncodingParser` | 继承实现 `HandleCommand` |
| 网络 URL 拼接 | `HTTPTransfer::GetAbsURL` / `WinInetHTTPTransfer::GetAbsURL` | 无(纯字符串) | 可直接复用 |
| HTTP WinSock 客户端 | `HTTPTransfer.cpp` | `winsock.h`、`SexyAppBase.h`(仅 include) | 线程 + 非阻塞 select 模型 |
| 全局随机封装 | `Common.cpp:21-39` | `MTRand` | `Rand/SRand` 薄封装 |
| BASS/FMOD 动态加载 | `BassLoader.*` / `FModLoader.*` | Windows `LoadLibrary`、bass/fmod 头 | 动态加载 DLL 模板，可借鉴 |
| 属性解析 | `PropertiesParser.*` | `SexyAppBase`、`XMLParser` | 换成自己的属性存储即可 |
| 运行时改常量 | `ModVal.h/.cpp` | STL、Windows | 需编译期 `__FILE__/__COUNTER__` 支持 |

### 7.3 深度耦合，仅作参考（★）

| 资产 | 文件 | 原因 |
|---|---|---|
| DirectSound 音效 | `DSoundManager.*` / `DSoundInstance.*` | 依赖 DirectSound、OggVorbis、FMOD、PakLib，老 API（DSSCL_*） |
| BASS/FMOD 音乐 | `BassMusicInterface.*` / `FModMusicInterface.*` | 依赖第三方音频库与 DLL |
| WinInet 客户端 | `WinInetHTTPTransfer.cpp` | WinInet 已废弃 API + 已知内存模式缺陷 |
| Flash 播放 | `FlashWidget.*` | 依赖 Shockwave Flash OCX |
| 崩溃捕获 | `SEHCatcher.*` | 依赖 imagehlp、SexyAppBase、HTTPTransfer |
| 显示/主音量 | `NativeDisplay.*` / `DSoundManager::Get/SetMasterVolume` | 依赖 Win32 GDI/mixer |

### 7.4 提取建议（按价值排序）

1. **几何/数学包**：`Rect.h` + `Point.h` + `SexyVector.h` + `SexyMatrix.*` + `Ratio.*` + `Insets.*` + `Color.*` —— 无平台依赖，是通用的 2D 数学库。
2. **字符串/格式化包**：从 `Common.h/.cpp` 抽 `Trim/Upper/Lower/StrFormat/vformat/Evaluate/XMLEncode/XMLDecode/URLEncode/StrFindNoCase/StrPrefixNoCase/CommaSeperate/StringToInt/StringToDouble/GetFileName/GetFileDir/AddTrailingSlash/RemoveTrailingSlash/FileExists/MkDir/SMemR/SMemW` 等。
3. **数据/算法包**：`MTRand.*` + `Buffer.*` + `PerfTimer.*` + `SmartPtr.h` + `CritSect.*`。
4. **解析器栈**：`EncodingParser` → `XMLParser`（把 `p_fopen` 换成标准 I/O 即可独立）。

---

## 附：统计

- 覆盖源码文件：**69 个**（含 `.h/.cpp` 及关联的 `SoundBuild.cpp`、`dsoundversion.h` 等）。
- 方法/函数定义（`::` 粗略计数）：**493 个**（含模板内联函数、静态函数）。
- `Common.cpp` 全局工具函数：**55 个**（见 5.1 全部列出，为框架最佳可复用资产）。
