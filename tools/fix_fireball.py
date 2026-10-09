# -*- coding: utf-8 -*-
# 修复：叠种火炬时"变色火球不生效"。
# 根因：上一轮做"重复骨骼动画去重"时，把元素染色写在了 `if (aAlreadyFire) return;` 之后
#       → 已经是火球的那条路径永远不会刷新元素颜色。
# 修法：去重只跳过"重复创建骨骼动画"，元素染色改为对"新建的 / 已挂载的"都执行。

P = 'src/Lawn/Projectile.cpp'
d = open(P, 'rb').read()

old = (
    '\tbool aAlreadyFire = (mProjectileType == ProjectileType::PROJECTILE_FIREBALL);\r\n'
    '\r\n'
    '\tmProjectileType = ProjectileType::PROJECTILE_FIREBALL;\r\n'
    '\tmHitTorchwoodGridX = theGridX;\r\n'
    '\tmApp->PlayFoley(FoleyType::FOLEY_FIREPEA);\r\n'
    '\r\n'
    '\tif (aAlreadyFire)\r\n'
    '\t\treturn;\r\n'
    '\r\n'
    '\tfloat aOffsetX = -25.0f;\r\n'
    '\tfloat aOffsetY = -25.0f;\r\n'
    '\tReanimation* aFirePeaReanim = mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_FIRE_PEA);\r\n'
    '\tif (mMotionType == ProjectileMotion::MOTION_BACKWARDS)\r\n'
    '\t{\r\n'
    '\t\taFirePeaReanim->OverrideScale(-1.0f, 1.0f);\r\n'
    '\t\taOffsetX += 80.0f;\r\n'
    '\t}\r\n'
    '\r\n'
    '\taFirePeaReanim->SetPosition(mPosX + aOffsetX, mPosY + aOffsetY);\r\n'
    '\taFirePeaReanim->mLoopType = ReanimLoopType::REANIM_LOOP;\r\n'
    '\r\n'
    '\t// 【三十旗】元素火球染色：用加色叠加让火焰带上元素色（蓝/绿/粉/金），一眼区分\r\n'
    '\tif (mElement != 0)\r\n'
    '\t{\r\n'
).decode('gbk') if False else None

# 上面那种拼法容易出错，直接用真实片段
old = (
    '\tbool aAlreadyFire = (mProjectileType == ProjectileType::PROJECTILE_FIREBALL);\r\n'
    '\r\n'
    '\tmProjectileType = ProjectileType::PROJECTILE_FIREBALL;\r\n'
    '\tmHitTorchwoodGridX = theGridX;\r\n'
    '\tmApp->PlayFoley(FoleyType::FOLEY_FIREPEA);\r\n'
    '\r\n'
    '\tif (aAlreadyFire)\r\n'
    '\t\treturn;\r\n'
    '\r\n'
    '\tfloat aOffsetX = -25.0f;\r\n'
    '\tfloat aOffsetY = -25.0f;\r\n'
    '\tReanimation* aFirePeaReanim = mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_FIRE_PEA);\r\n'
    '\tif (mMotionType == ProjectileMotion::MOTION_BACKWARDS)\r\n'
    '\t{\r\n'
    '\t\taFirePeaReanim->OverrideScale(-1.0f, 1.0f);\r\n'
    '\t\taOffsetX += 80.0f;\r\n'
    '\t}\r\n'
    '\r\n'
    '\taFirePeaReanim->SetPosition(mPosX + aOffsetX, mPosY + aOffsetY);\r\n'
    '\taFirePeaReanim->mLoopType = ReanimLoopType::REANIM_LOOP;\r\n'
    '\r\n'
    '\t// 【三十旗】元素火球染色：用加色叠加让火焰带上元素色（蓝/绿/粉/金），一眼区分\r\n'
    '\tif (mElement != 0)\r\n'
    '\t{\r\n'
).encode('gbk')

assert d.count(old) == 1, ("anchor A", d.count(old))

new = (
    '\tbool aAlreadyFire = (mProjectileType == ProjectileType::PROJECTILE_FIREBALL);\r\n'
    '\r\n'
    '\tmProjectileType = ProjectileType::PROJECTILE_FIREBALL;\r\n'
    '\tmHitTorchwoodGridX = theGridX;\r\n'
    '\tmApp->PlayFoley(FoleyType::FOLEY_FIREPEA);\r\n'
    '\r\n'
    '\tfloat aOffsetX = -25.0f;\r\n'
    '\tfloat aOffsetY = -25.0f;\r\n'
    '\r\n'
    '\t// 注意·修复（变色火球曾失效）：去重只允许跳过"重复创建骨骼动画"，\r\n'
    '\t// **不能跳过"元素染色参数的刷新"** —— 否则"先当普通火球、后带元素"\r\n'
    '\t// 的那条路径永远染不上色。这里改成：新建就创建，已存在就取回已挂载的那条，\r\n'
    '\t// 之后无论哪种情况都统一执行元素染色。\r\n'
    '\tReanimation* aFirePeaReanim = NULL;\r\n'
    '\tif (!aAlreadyFire)\r\n'
    '\t{\r\n'
    '\t\taFirePeaReanim = mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_FIRE_PEA);\r\n'
    '\t\tif (aFirePeaReanim == NULL)\r\n'
    '\t\t\treturn;\r\n'
    '\r\n'
    '\t\tif (mMotionType == ProjectileMotion::MOTION_BACKWARDS)\r\n'
    '\t\t{\r\n'
    '\t\t\taFirePeaReanim->OverrideScale(-1.0f, 1.0f);\r\n'
    '\t\t\taOffsetX += 80.0f;\r\n'
    '\t\t}\r\n'
    '\r\n'
    '\t\taFirePeaReanim->SetPosition(mPosX + aOffsetX, mPosY + aOffsetY);\r\n'
    '\t\taFirePeaReanim->mLoopType = ReanimLoopType::REANIM_LOOP;\r\n'
    '\t}\r\n'
    '\telse\r\n'
    '\t{\r\n'
    '\t\taFirePeaReanim = mApp->ReanimationGet(mAttachmentID);   // 复用已有表现\r\n'
    '\t}\r\n'
    '\r\n'
    '\t// 【三十旗】元素火球染色：用加色叠加让火焰带上元素色（蓝/绿/粉/金），一眼区分\r\n'
    '\tif (aFirePeaReanim != NULL && mElement != 0)\r\n'
    '\t{\r\n'
).encode('gbk')

d = d.replace(old, new)
open(P, 'wb').write(d)
print("fix A applied")

# 把末尾的 AttachReanim 改成只在新建时调用
old2 = '\tAttachReanim(mAttachmentID, aFirePeaReanim, aOffsetX, aOffsetY);\r\n}\r\n'.encode('gbk')
assert d.count(old2) >= 1, ("anchor B", d.count(old2))
# 只替换 ConvertToFireball 内的那一处（它紧跟在 aFirePeaReanim->mAnimRate 之后）
old3 = '\taFirePeaReanim->mAnimRate = RandRangeFloat(50.0f, 80.0f);\r\n\tAttachReanim(mAttachmentID, aFirePeaReanim, aOffsetX, aOffsetY);\r\n'.encode('gbk')
assert d.count(old3) == 1, ("anchor C", d.count(old3))
new3 = ('\taFirePeaReanim->mAnimRate = RandRangeFloat(50.0f, 80.0f);\r\n'
        '\r\n'
        '\tif (!aAlreadyFire)\r\n'
        '\t\tAttachReanim(mAttachmentID, aFirePeaReanim, aOffsetX, aOffsetY);\r\n').encode('gbk')
d = d.replace(old3, new3)
open(P, 'wb').write(d)
print("fix B applied")
