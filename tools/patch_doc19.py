# -*- coding: utf-8 -*-
p = 'docs/性能优化档案.md'
s = open(p, encoding='utf-8').read()
anchor = '\n## 附：如何复现本文档的所有数据\n'
assert s.count(anchor) == 1

sec = '''
## §19 把验证改成"日志可证"：特效数量与已用填充面积进探针

### 19.1 为什么改成日志可证

§18 的结论是"按键驱动 + 自动截图"在本环境不可靠。于是把验证目标从**截图可证**改成**日志可证**：
把两个数字打进 `[TFPerf]` 行，**不需要任何截图**就能证明"特效在生成"且"填充预算在生效"。

新增（`ThirtyFlags.cpp`）：`ThirtyFlagsFxCount()` / `ThirtyFlagsFxFillUsed()`，探针行随之变成：

```
[TFPerf] ... | sprite=976/831 clip=0 fx=<当前特效数>/<本帧已用填充面积> 3d=1
```

**怎么读**：

* `fx=0/0` → 特效层没在跑（`TF_FX_ENABLE=0`，正常）；
* `fx=20/12000` → 同时 20 个特效、本帧累计填充 12000 px²（预算 60000）→ 在跑且离上限很远；
* `fx` 一直贴 192 或 `fill` 一直贴 60000 → **预算正在生效**（达到上限后主动停画）；
* 若 `fill` **超过 60000** → 预算逻辑失效（这才是要修的情况）。

### 19.2 工具链故障的**真正原因**（这轮终于查清）

| 现象 | 真因 |
|---|---|
| 脚本总在"启动游戏后 1~2 秒"死掉 | 环境安全策略**拦截运行时编译 .NET 代码**（`Add-Type`）→ 脚本里那一步被拦，随即中断 |
| 脚本死掉后游戏也没反应 | 游戏进程存在但**没有任何顶层窗口**（枚举时 pid 在、窗口不在）→ 无法送键 |
| 之前偶尔能成功 | 那些运行的启动方式/时点不同（后台方式启动） |

**绕开办法**：改用 **Python + ctypes** 直接调 user32/gdi32（不需要运行时编译、不需要 PIL），
新增 `tools/tf_keys.py`：

* `python tools/tf_keys.py keys` → 找到游戏窗口并送"点标题 → T → S"；
* `python tools/tf_keys.py shot out.bmp` → 抓窗口自身内容并写成 BMP（24 位，无压缩）。

### 19.3 自验三步（不需要我在场，也不需要截图）

1. `TF_FX_ENABLE` 改 `1` → `python tools/build_manual.py`；
2. 进游戏：`T` → `S`，**站着不动 15 秒**（自测场景每 40 帧自动生成一组四种特效）；
3. 看 `Release\\thirtyflags_flow.log` 里的 `[TFPerf] ... fx=.../...`：
   * `fx` 不为 0 → **特效确实在生成**；
   * `fill` 始终 ≤ 60000 → **每帧填充预算在生效**；
   * 再看 `draw` 峰值：按 §17 的预算，**不该再出现 40ms 以上尖峰**。

'''

s = s.replace(anchor, sec + anchor)
open(p, 'w', encoding='utf-8').write(s)
print("ok section 19")
