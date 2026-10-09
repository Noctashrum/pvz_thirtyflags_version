# -*- coding: utf-8 -*-
# 撤回"火豌豆骨骼动画去重"：它导致的"变色火球不生效"是功能回归，优先级高于那点性能收益。
# 恢复为原版行为：每次 ConvertToFireball 都重建 REANIM_FIRE_PEA 并施加元素染色。
# （正确的做法应是"复用已有 reanim 并刷新染色"，但那需要附件→reanim 的取用接口，
#   等定位到接口再改；先把功能恢复。）

P = 'src/Lawn/Projectile.cpp'
d = open(P, 'rb').read()

# 找到 ConvertToFireball 的函数体
start = d.find(b'void Projectile::ConvertToFireball(int theGridX)')
assert start > 0, "func start"
end = d.find(b'\r\n}\r\n', start)
assert end > start, "func end"
body_old = d[start:end + 5]

new_body = (
    'void Projectile::ConvertToFireball(int theGridX)\r\n'
    '{\r\n'
    '\tif (mHitTorchwoodGridX == theGridX)\r\n'
    '\t\treturn;\r\n'
    '\r\n'
    '\tmProjectileType = ProjectileType::PROJECTILE_FIREBALL;\r\n'
    '\tmHitTorchwoodGridX = theGridX;\r\n'
    '\tmApp->PlayFoley(FoleyType::FOLEY_FIREPEA);\r\n'
    '\r\n'
    '\t// 说明：这里曾做过"叠种火炬时避免重复创建骨骼动画"的去重优化，但那样会让\r\n'
    '\t// "先成为普通火球、之后才带元素"的路径永远刷不上元素色（表现为变色火球失效）。\r\n'
    '\t// 功能正确性优先，已撤回该去重，恢复为每次转换都重建表现。\r\n'
    '\tfloat aOffsetX = -25.0f;\r\n'
    '\tfloat aOffsetY = -25.0f;\r\n'
    '\tReanimation* aFirePeaReanim = mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_FIRE_PEA);\r\n'
    '\tif (aFirePeaReanim == NULL)\r\n'
    '\t\treturn;\r\n'
    '\r\n'
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
    '\t\taFirePeaReanim->mEnableExtraAdditiveDraw = true;\r\n'
    '\t\tif (mElement == TF_ELEM_ICE)            aFirePeaReanim->mExtraAdditiveColor = Sexy::Color(40, 90, 255);\r\n'
    '\t\telse if (mElement == TF_ELEM_DEEPFREEZE) aFirePeaReanim->mExtraAdditiveColor = Sexy::Color(0, 40, 220);\r\n'
    '\t\telse if (mElement == TF_ELEM_POISON)     aFirePeaReanim->mExtraAdditiveColor = Sexy::Color(40, 220, 40);\r\n'
    '\t\telse if (mElement == TF_ELEM_CHARM)      aFirePeaReanim->mExtraAdditiveColor = Sexy::Color(255, 60, 180);\r\n'
    '\t\telse if (mElement == TF_ELEM_BUTTER)     aFirePeaReanim->mExtraAdditiveColor = Sexy::Color(255, 210, 30);\r\n'
    '\t\telse if (mElement == TF_ELEM_GIANT)      aFirePeaReanim->mExtraAdditiveColor = Sexy::Color(255, 120, 0);\r\n'
    '\t}\r\n'
    '\r\n'
    '\taFirePeaReanim->mAnimRate = RandRangeFloat(50.0f, 80.0f);\r\n'
    '\tAttachReanim(mAttachmentID, aFirePeaReanim, aOffsetX, aOffsetY);\r\n'
    '}\r\n'
).encode('gbk')

d = d[:start] + new_body + d[end + 5:]
open(P, 'wb').write(d)
print("ConvertToFireball 已恢复为原版行为（染色必定生效）")
