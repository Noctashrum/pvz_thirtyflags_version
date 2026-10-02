# 《植物大战僵尸》源码工程文档集

本目录是对 `D:\dsh-project\LawnProject` 中 **Plants vs. Zombies（植物大战僵尸）C++ 反编译源码工程** 的完整分析文档，覆盖约 **12.3 万行**第一方代码（`src/Lawn` 游戏本体 + `src/SexyAppFramework` 引擎 + `src/TodLib`/`src/PakLib`/`src/ImageLib` 支撑库）。

> 源码为 **GBK 编码**；本文档集为 UTF-8。函数索引表中的「行号」均指源文件中的定义起始行。

## 文档导航

### 快速上手
| 文档 | 内容 |
|---|---|
| [00-项目总览与构建.md](00-项目总览与构建.md) | 项目结构、统计、编译配置、构建运行步骤、源码编码注意事项、第三方组件 |
| [01-架构总览.md](01-架构总览.md) | 六层架构、类继承体系、主循环、场景管理、资源/渲染管线、玩法数据流、Modding 切入点 |

### 游戏本体（src/Lawn）
| 文档 | 覆盖模块 |
|---|---|
| [10-Board核心.md](10-Board核心.md) | 主战场 Board（9136 行）：网格、实体管理、游戏循环、波次推进 |
| [11-LawnApp与通用控件.md](11-LawnApp与通用控件.md) | 程序主类 LawnApp + 光标/消息/提示/泳池特效等小控件 |
| [20-植物系统.md](20-植物系统.md) | Plant 类全函数 + `gPlantDefs` 数据表字段详解 + SeedType 枚举 |
| [21-僵尸系统.md](21-僵尸系统.md) | Zombie 类全函数 + `gZombieDefs` 数据表字段详解 + ZombieType 枚举 |
| [22-种子包投射物网格物品.md](22-种子包投射物网格物品.md) | SeedPacket/SeedBank/Projectile/GridItem/LawnMower/Coin |
| [23-挑战与游戏模式.md](23-挑战与游戏模式.md) | Challenge 模式控制器 + 波次程序化生成算法 + `gChallengeDefs` 表 |
| [24-禅境花园与过场.md](24-禅境花园与过场.md) | ZenGarden/CutScene/AwardScreen/CreditScreen |
| [25-UI界面与对话框.md](25-UI界面与对话框.md) | 主菜单/模式选择/选卡/商店/图鉴/全部对话框 |
| [26-存档音乐与资源系统.md](26-存档音乐与资源系统.md) | SaveGame 二进制格式/DataSync/Music/Resources/PlayerInfo/TypingCheck |

### 引擎与支撑库
| 文档 | 覆盖模块 |
|---|---|
| [30-SexyApp框架-应用与渲染.md](30-SexyApp框架-应用与渲染.md) | SexyAppBase 主循环、控件树系统、Graphics/DDImage/DDInterface 渲染管线、ResourceManager |
| [31-SexyApp框架-声音网络与工具.md](31-SexyApp框架-声音网络与工具.md) | BASS/FMOD 声音后端、HTTP、XML/Properties 解析器、Color/Rect/MTRand/PerfTimer 工具库 |
| [32-TodLib-PakLib-ImageLib.md](32-TodLib-PakLib-ImageLib.md) | Reanimator 骨骼动画、TodParticle 粒子、Definition/DataArray、特效系统、pak 读取、图像解码 |

### 复用资产
| 文档 | 内容 |
|---|---|
| [90-可复用部件总索引.md](90-可复用部件总索引.md) | 全部 ★★★/★★ 级可复用函数、类、模块的跨文档汇总索引 |

## 使用建议

- **了解整体**：先读 `00`、`01` 两篇；
- **改玩法数值**：直接查 `20`/`21`/`23` 的数据表章节；
- **复用代码**：查 `90-可复用部件总索引.md`，按「零依赖工具 / 引擎组件 / 游戏逻辑」分类定位；
- **修改某功能**：按上表定位模块文档，用函数索引表中的行号回到源码。

## 重要发现（阅读前必知）

1. 源码为 **GBK 编码**，且是 **VS2019 (v143) 多字节字符集** 工程；
2. 关卡波次为**程序化生成**（`Challenge::InitZombieWaves`），源码中无静态波次表；
3. 游戏资源（`main.pak`、`properties\`）不在本仓库，需自备后放于 exe 同目录才能运行。
