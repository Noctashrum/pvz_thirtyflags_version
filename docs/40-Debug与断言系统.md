# PvzDebug：日志与断言系统

> 本文面向「在本工程上继续做玩法修改」的人。
> 新增模块：`src/TodLib/PvzDebug.h` / `PvzDebug.cpp`（纯 ASCII，与 GBK 源码隔离）。

---

## 1. 为什么要加这套东西

本工程实际发布的是 **Release|Win32 + NDEBUG**。在这个配置下：

| 现象 | 后果 |
|---|---|
| `TOD_ASSERT(...)` 在 `TodDebug.h` 里被 `#else` 分支定义成空 | 全工程几百条不变量检查**全部失效**，包括实体池上限、波次下标、非空图像 |
| `TodAssertInitForApp()` 只在 `_DEBUG` 下被调用 | `gLogFileName` 为空，`TodLog`/`TodTrace` **写不到任何文件** |
| 崩溃证据只有 `Release\crash.txt` | 只有一条 `Access Violation` + 地址，没有上下文 |

所以「改一下就要靠猜、靠重编重跑」不是偶然，是这套构建配置的必然结果。

更糟的是 `DataArray`（`src/TodLib/DataArray.h`，所有僵尸/植物/网格物品的对象池）：

```cpp
T* DataArrayAlloc()
{
    TOD_ASSERT(mSize < mMaxSize, "Data array full: %s", mName);   // ← Release 下是空的
    unsigned int aNext = mMaxUsedCount;
    if (mFreeListHead == mMaxUsedCount)
        mFreeListHead = ++mMaxUsedCount;      // ← 池满时继续自增，越过块尾
    ...
}
```

池满后 `mMaxUsedCount` 会越过 `mMaxSize`，直接写到堆块外面 —— **静默堆损坏**，通常在很久以后以一个完全不相干的访问违例表现出来。
`mGridItems` 的容量只有 **128**，而三十旗的「掘地」突变会在僵尸死亡时持续 `AddACrater()`，这是最容易踩到的一条路径。

---

## 2. 做了什么

### 2.1 PvzDebug 模块（新增）

- **Release 下也生效**。默认开启，`PVZ_DEBUG_ENABLED=0` 可整体编译掉。
- **分级 + 分通道**的日志：只开你正在改的子系统，不用在噪音里捞。
- **环形缓冲区**：保留最近 192 条消息，崩溃时先落盘 —— 能看见出事前发生了什么，而不只是出事那一刻。
- **崩溃处理器**：异常码/地址/读写方向/寄存器 + 游戏状态 + 最近日志 + minidump。
- **断言默认「记录并继续」**：不会因为第一条不变量就把试玩打断，但日志里写得清清楚楚。

### 2.2 `DataArray` 加固

- 分配时多申请一个槽位作 **overflow sink**（溢出兜底槽）。
- 池满时：**报错 + 返回 sink**，不再越界写。`mSize` 不增长、不入空闲链表，账目仍然正确。
- `DataArrayGet()` 加了边界检查，解析不出 id 时返回 sink 而不是野指针。
- 用量达到 **90%** 时提前告警 —— 让你在池真的满掉之前就知道要扩容。

### 2.3 顺手修掉的两个真 bug

| 位置 | 问题 |
|---|---|
| `TodLib/StackWalk.cpp` | `GetProcAddress(..., "SymInitialzie")` 拼错 → `gSymInitialize` 恒为 NULL，任何栈回溯都会跳到 0 地址 |
| `Lawn/Board.cpp: GetTopPlantAt` | 网格下标越界时**静默返回 NULL**。行为保留，但现在会记录越界的坐标 |

### 2.4 `TOD_ASSERT` 重新接线

`TodDebug.h` 的 Release 分支现在走 `PvzAssertReport`；`TodLogString()` 也会镜像一份到 `pvzdebug.log`。
也就是说**原有几百条断言和日志在 Release 下重新活了**，不需要改任何调用点。

---

## 3. 怎么用

### 3.1 日志在哪

`Release\pvzdebug.log`（exe 同目录，追加写，8 MB 后轮转成 `.old`）。
原来的 `thirtyflags.log` 仍在，且所有 `TfLog*` 调用点**同时**写进 `pvzdebug.log`。

### 3.2 配置

改 `Release\pvzdebug.ini`（已放好一份带注释的）：

```ini
level = 3          # 0 关闭 / 1 error / 2 warn / 3 info / 4 verbose
channels = FFFFFFFF   # 通道掩码，见下表
action = continue  # continue | break | abort
```

环境变量 `PVZ_LOG_LEVEL` / `PVZ_LOG_CHANNELS` / `PVZ_ASSERT_ACTION` 优先级高于 ini。

| 通道 | 值 | 通道 | 值 |
|---|---|---|---|
| `PVZ_CH_CORE` | `00000001` | `PVZ_CH_RESOURCE` | `00000200` |
| `PVZ_CH_BOARD` | `00000002` | **`PVZ_CH_MEMORY`** | `00000400` |
| `PVZ_CH_PLANT` | `00000004` | `PVZ_CH_RENDER` | `00000800` |
| `PVZ_CH_ZOMBIE` | `00000008` | `PVZ_CH_SAVEGAME` | `00001000` |
| `PVZ_CH_PROJECTILE` | `00000010` | **`PVZ_CH_TF`** | `00002000` |
| `PVZ_CH_GRIDITEM` | `00000020` | `PVZ_CH_USER` | `00004000` |
| `PVZ_CH_COIN` | `00000040` | `PVZ_CH_ALL` | `FFFFFFFF` |
| `PVZ_CH_CHALLENGE` | `00000080` | | |
| `PVZ_CH_UI` | `00000100` | | |

> 改玩法时建议 `channels = 00000400`（实体池）+ 你正在改的通道。`PVZ_CH_MEMORY` 会明确告诉你池子快满了。

### 3.3 在自己的代码里加日志

```cpp
#include "../TodLib/PvzDebug.h"      // 多数 Lawn 文件已经间接包含（DataArray.h → TodDebug.h）

PVZ_LOG_ERROR  (PVZ_CH_TF, "抽取失败 flag=%d", aFlag);
PVZ_LOG_WARN   (PVZ_CH_ZOMBIE, "僵尸 %p 生命异常 %d", this, mBodyHealth);
PVZ_LOG_INFO   (PVZ_CH_BOARD, "进入第 %d 旗", aFlag);
PVZ_LOG_VERBOSE(PVZ_CH_PLANT, "每帧都打的细节 %d", i);

PVZ_LOG_ONCE(PVZ_CH_TF, PVZ_LOG_WARN, "这条只打一次");
PVZ_LOG_THROTTLED(PVZ_CH_BOARD, PVZ_LOG_WARN, 60, "每帧触发的错误，每 60 次打一条");
```

### 3.4 加断言

```cpp
PVZ_ASSERT(aFlag >= 1 && aFlag <= 30);                    // 不带消息也可以
PVZ_ASSERT(aIdx >= 0, "索引 %d", aIdx);
PVZ_VERIFY(InitSomething(), "初始化必须成功");              // 条件永远会被求值
PVZ_ASSERT_INDEX(theRow, MAX_GRID_SIZE_Y, "row");          // 越界检查（负值也能抓到）
PVZ_ASSERT_RANGE(theCol, 0, 8, "col");
PVZ_ASSERT_PTR(theImage, "IMAGE_PEASHOTTER");
PVZ_FAIL("这个分支不该走到");

PVZ_TRACE_SCOPE(PVZ_CH_BOARD, "InitLevel");                // 进入/退出 + 耗时(ms)
PVZ_TRACE_FN(PVZ_CH_CHALLENGE);                            // 同上，自动带函数名
```

失败时写进日志的是：条件文本、文件、行号、自定义消息、以及累计断言次数。
同一处重复触发会**节流**（首次全量，之后每 100 条摘要），不会把日志刷爆。

---

## 4. 崩溃报告长什么样

```
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!!! UNHANDLED EXCEPTION 0xC0000005 (ACCESS VIOLATION)
!!! fault address: 009E5806   thread: 12345
!!! read at address 00000000
!!! target is near NULL - likely a null pointer or stale object id
!!! regs EAX=... EBX=... ECX=... EDX=... ESI=... EDI=...
!!!      EIP=009E5806 ESP=... EBP=...
---- game state at crash ----
      scene=2 gameMode=72 board=0A1B2C3D
      level=1 wave=3/6 survivalStage=0
      pools: zombies=41/1024 plants=12/1024 projectiles=8/1024 coins=3/1024 griditems=118/128 lawnmowers=6/32
---- end game state ----
---- recent log (oldest first) ----
...
---- end recent log ----
# minidump written: D:\...\pvzdebug.log.dmp
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
```

**`griditems=118/128` 这一行就是关键**：如果它顶到 128，说明 `AddACrater()` 之类已经开始拿 sink 了，
去 `Board.cpp` 把 `mGridItems.DataArrayInitialize(128U, ...)` 调大，或者减少生成。

minidump 用 VS 直接打开即可定位到源码行（需要 `PlantsVsZombies.pdb`）。

---

## 5. 典型排错流程

1. 复现崩溃 → 关掉游戏 → 打开 `Release\pvzdebug.log` 拖到最后。
2. 看 `---- game state at crash ----`：哪个模式、第几旗、第几波、各池用量。
3. 看 `---- recent log ----`：崩溃前几十条消息。
4. 如果是池满：调 `Board.cpp` 顶部的 `DataArrayInitialize` 容量。
5. 如果是不变量失败：日志里有文件+行号+条件，直接跳过去。
6. 都不够：把 `pvzdebug.ini` 的 `level` 调到 4、`channels=FFFFFFFF` 再跑一次。

---

## 6. 已知限制

- **本次改动未经 MSVC 实际编译**（环境不允许执行 MSBuild/cl.exe）。
  `PvzDebug.cpp` 已用 MinGW g++ `-Wall` 做了语法/运行级验证（含宏展开与 ini 解析），
  但 MSVC 侧仍需在 VS 里生成一次确认。工程文件已登记，无需额外配置。
- `DataArray` 多申请的 1 个 sink 槽位不改变 `mMaxSize`，所有边界逻辑与存档布局不受影响。
- 池满后返回 sink 是**保命**行为：游戏不崩，但那个实体实际上没进池子。日志会明确写
  `DataArray FULL: 'xxx' ... this entity is NOT in the pool`，不要忽略它。
- Release 下断言默认「继续」。如果想让第一条断言就断进调试器，设 `action = break`。
- 断言总量超过 20000 条后只计数不落盘，避免日志把磁盘写满。
