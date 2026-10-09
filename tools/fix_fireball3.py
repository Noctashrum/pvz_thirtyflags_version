# -*- coding: utf-8 -*-
# 正确修复"变色火球不生效"，同时**保留**去重（后期上百火球时去重是性能关键）。
#
# 原实现的两个诉求：
#   (a) 叠种火炬时同一颗弹只建一条火焰表现（否则 N 层叠种 = N 条骨骼动画，越打越卡）；
#   (b) 元素染色必须总能生效（先普通火球、后带元素的路径也要染上）。
# 之前把 (b) 写在了 (a) 的提前返回之后 → (b) 失效。
#
# 现在：表现只建一条（保留去重），染色改为对"新建的 / 已挂载的"统一执行 ——
# 用 GameObject::FindReanimAttachment(mAttachmentID) 取回已挂载的那条表现再刷新染色。

P = 'src/Lawn/Projectile.cpp'
d = open(P, 'rb').read()

start = d.find(b'void Projectile::ConvertToFireball(int theGridX)')
assert start > 0, "func start"
end = d.find(b'\r\n}\r\n', start)
assert end > start, "func end"

new_body = (
    'void Projectile::ConvertToFireball(int theGridX)\r\n'
    '{\r\n'
    '\tif (mHitTorchwoodGridX == theGridX)\r\n'
    '\t\treturn;\r\n'
    '\r\n'
    '\t// 【性能】同格多个树桩（叠种火炬）会反复调用本函数，把伤害/元素逐层叠上去；\r\n'
    '\t// 但**火焰表现只需要一条** —— 原版每次调用都新建一条 REANIM_FIRE_PEA，\r\n'
    '\t// 后期"上百火球 × 多层叠种"会养出成倍的骨骼动画（就是越打越卡的元凶之一）。\r\n'
    '\t// 所以：状态照旧叠加，**表现只在第一次创建**。\r\n'
    '\tbool aAlreadyFire = (mProjectileType == ProjectileType::PROJECTILE_FIREBALL);\r\n'
    '\r\n'
    '\tmProjectileType = ProjectileType::PROJECTILE_FIREBALL;\r\n'
    '\tmHitTorchwoodGridX = theGridX;\r\n'
    '\tmApp->PlayFoley(FoleyType::FOLEY_FIREPEA);\r\n'
    '\r\n'
    '\tif (!aAlreadyFire)\r\n'
    '\t{\r\n'
    '\t\tfloat aOffsetX = -25.0f;\r\n'
    '\t\tfloat aOffsetY = -25.0f;\r\n'
    '\t\tReanimation* aNewReanim = mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_FIRE_PEA);\r\n'
    '\t\tif (aNewReanim != NULL)\r\n'
    '\t\t{\r\n'
    '\t\t\tif (mMotionType == ProjectileMotion::MOTION_BACKWARDS)\r\n'
    '\t\t\t{\r\n'
    '\t\t\t\taNewReanim->OverrideScale(-1.0f, 1.0f);\r\n'
    '\t\t\t\taOffsetX += 80.0f;\r\n'
    '\t\t\t}\r\n'
    '\r\n'
    '\t\t\taNewReanim->SetPosition(mPosX + aOffsetX, mPosY + aOffsetY);\r\n'
    '\t\t\taNewReanim->mLoopType = ReanimLoopType::REANIM_LOOP;\r\n'
    '\t\t\taNewReanim->mAnimRate = RandRangeFloat(50.0f, 80.0f);\r\n'
    '\t\t\tAttachReanim(mAttachmentID, aNewReanim, aOffsetX, aOffsetY);\r\n'
    '\t\t}\r\n'
    '\t}\r\n'
    '\r\n'
    '\t// 【三十旗】元素火球染色：用加色叠加让火焰带上元素色（蓝/绿/粉/金），一眼区分。\r\n'
    '\t// 这一句**必须在去重之外**：无论表现是新建的还是早就挂着的，都要按当前元素刷新，\r\n'
    '\t// 否则"先成为普通火球、之后才带元素"的路径就永远染不上色（曾出现过的回归）。\r\n'
    '\tif (mElement != 0)\r\n'
    '\t{\r\n'
    '\t\tReanimation* aFireReanim = FindReanimAttachment(mAttachmentID);\r\n'
    '\t\tif (aFireReanim != NULL)\r\n'
    '\t\t{\r\n'
    '\t\t\taFireReanim->mEnableExtraAdditiveDraw = true;\r\n'
    '\t\t\tif (mElement == TF_ELEM_ICE)            aFireReanim->mExtraAdditiveColor = Sexy::Color(40, 90, 255);\r\n'
    '\t\t\telse if (mElement == TF_ELEM_DEEPFREEZE) aFireReanim->mExtraAdditiveColor = Sexy::Color(0, 40, 220);\r\n'
    '\t\t\telse if (mElement == TF_ELEM_POISON)     aFireReanim->mExtraAdditiveColor = Sexy::Color(40, 220, 40);\r\n'
    '\t\t\telse if (mElement == TF_ELEM_CHARM)      aFireReanim->mExtraAdditiveColor = Sexy::Color(255, 60, 180);\r\n'
    '\t\t\telse if (mElement == TF_ELEM_BUTTER)     aFireReanim->mExtraAdditiveColor = Sexy::Color(255, 210, 30);\r\n'
    '\t\t\telse if (mElement == TF_ELEM_GIANT)      aFireReanim->mExtraAdditiveColor = Sexy::Color(255, 120, 0);\r\n'
    '\t\t}\r\n'
    '\t}\r\n'
    '}\r\n'
).encode('gbk')

d = d[:start] + new_body + d[end + 5:]
open(P, 'wb').write(d)
print("ConvertToFireball: 去重保留 + 染色统一刷新")
