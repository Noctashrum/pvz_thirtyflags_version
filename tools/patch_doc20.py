# -*- coding: utf-8 -*-
p = 'docs/性能优化档案.md'
s = open(p, encoding='utf-8').read()
anchor = '\n## 附：如何复现本文档的所有数据\n'
assert s.count(anchor) == 1

sec = '''
## §20 功能回归修复：变色火球失效（去重不能顺带跳过"参数刷新"）

### 20.1 回归根因

早期为了治"后期越打越卡"，在 `Projectile::ConvertToFireball` 里做了**表现去重**：
叠种火炬时同一颗弹只建一条火焰骨骼动画。但当时把**元素染色**写在了提前返回之后：

```cpp
bool aAlreadyFire = (mProjectileType == PROJECTILE_FIREBALL);
...
if (aAlreadyFire)
    return;                 // ← 提前返回
// 下面才是 [创建火焰表现] + [元素染色]
```

→ 染色只对"新建表现"生效，"先成为普通火球、之后才带元素色"的路径**永远染不上色**。
玩家看到的就是**变色火球不生效**。

### 20.2 一度想简单回退 —— **错，被用户纠正**

我第一反应是"把去重整体撤掉，恢复原版行为"。**用户立刻指出不能这么做**：
后期随便就上百颗火球，**去重正是治卡顿的关键**，回退等于把性能问题请回来。

这条纠正很关键，记在这里：
> **"功能回归"和"性能优化"冲突时，正确做法是找到两者兼容的实现，而不是二选一。**

### 20.3 正确修法：去重保留 + 染色统一刷新

关键接口：`GameObject::FindReanimAttachment(AttachmentID&)`（在子类里可直接调用）
→ 能取回"已经挂在附件上的那条表现"。

```cpp
if (!aAlreadyFire)          // 表现只建一条（去重保留：后期上百火球靠它）
{
    ...AddReanimation + 定位 + 循环类型 + 动画速率 + AttachReanim...
}

if (mElement != 0)          // 染色在去重之外：新建的、早就挂着的，都按当前元素刷新
{
    Reanimation* aFireReanim = FindReanimAttachment(mAttachmentID);
    if (aFireReanim != NULL)
    {
        aFireReanim->mEnableExtraAdditiveDraw = true;
        aFireReanim->mExtraAdditiveColor = <按元素取色>;
    }
}
```

**两个诉求同时满足**：骨骼动画仍然只建一条（性能）；元素色每次都会被刷新（功能）。

### 20.4 逐项审计：其它改动有没有同类风险

| 改动 | 影响玩法 | 结论 |
|---|---|---|
| **火豌豆表现去重** | 是 | ✗ 曾回归（本节已修）；**去重保留** |
| **投射物碰撞按行分桶** | 是 | 语义保持：行过滤条件一致、BOSS 单独遍历、取"最左侧"与顺序无关。**唯一行为差异**：僵尸在同一帧内换行时，可能晚一帧被新行子弹命中（可忽略） |
| 对象池活槽位图 | 否 | 遍历长度与顺序有随机化测试保证（572 次比对一致） |
| 精灵合批 / 混合状态去重 / 裸土条带合并 | 否（仅画面） | 玩家已确认画面无大问题 |
| 图集（`TF_TEXTURE_ATLAS`）、特效层（`TF_FX_ENABLE`） | 否 | 均已关闭，代码惰性 |
| 框架 unity 重编修复、路径日志、`[TFPerf]` 探针 | 否 | 仅构建链与日志 |

**结论**：真正影响玩法的只有两处 —— 火球表现去重（本轮修复，且保留优化）与投射物按行分桶（审计通过）。

'''
s = s.replace(anchor, sec + anchor)
open(p, 'w', encoding='utf-8').write(s)
print("ok section 20")
