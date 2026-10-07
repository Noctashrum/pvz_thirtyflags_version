# -*- coding: utf-8 -*-
# 铺开剩下三个特效方向（沿用同一套设计：只用两张现成贴图 + 全加色 + 成组）
#   ① 元素弹拖尾  ② 精英威压（脚下光环 + 慢脉冲）  ③ 全局氛围（换旗/精英登场闪屏）

P = 'src/Lawn/ThirtyFlags.cpp'


def rep(old_t, new_t, tag):
    d = open(P, 'rb').read()
    for eol in ('\r\n', '\n'):
        old = old_t.replace('\n', eol).encode('gbk')
        if d.count(old) == 1:
            open(P, 'wb').write(d.replace(old, to := new_t.replace('\n', eol).encode('gbk')))
            print("  ok", tag, "eol", repr(eol))
            return
    raise AssertionError((tag, 'anchor miss'))


# ---------- 公共：元素配色 + "任意精英"判定 ----------
rep('static void TfFxSpawn(int theKind, float theX, float theY, float theSize, float theGrow,\n'
    '                      int theLife, int theAlpha, int theR, int theG, int theB)\n',
    '// 元素配色：与火炬树桩转换火球时的加色染色保持一致，玩家一眼能对上\n'
    'static void TfFxGetElementColor(int theElement, int& theR, int& theG, int& theB)\n'
    '{\n'
    '    theR = 255; theG = 220; theB = 140;\n'
    '    if (theElement == TF_ELEM_ICE)            { theR = 40;  theG = 90;  theB = 255; }\n'
    '    else if (theElement == TF_ELEM_DEEPFREEZE) { theR = 0;   theG = 40;  theB = 220; }\n'
    '    else if (theElement == TF_ELEM_POISON)     { theR = 40;  theG = 220; theB = 40;  }\n'
    '    else if (theElement == TF_ELEM_CHARM)      { theR = 255; theG = 60;  theB = 180; }\n'
    '    else if (theElement == TF_ELEM_BUTTER)     { theR = 255; theG = 210; theB = 30;  }\n'
    '    else if (theElement == TF_ELEM_GIANT)      { theR = 255; theG = 120; theB = 0;   }\n'
    '}\n'
    '\n'
    '// 是否带任意精英词条（用于精英威压表现）\n'
    'static bool TfFxIsElite(Zombie* theZombie)\n'
    '{\n'
    '    for (int aKind = 0; aKind < TF_ELITE_KIND_COUNT; aKind++)\n'
    '    {\n'
    '        if (TFHasElite(theZombie, aKind))\n'
    '            return true;\n'
    '    }\n'
    '    return false;\n'
    '}\n'
    '\n'
    'static void TfFxSpawn(int theKind, float theX, float theY, float theSize, float theGrow,\n'
    '                      int theLife, int theAlpha, int theR, int theG, int theB)\n',
    'fx-helpers')

# ---------- ① 元素弹拖尾（每帧扫投射物，限流 1/3 帧）----------
rep('void ThirtyFlagsBoardUpdate(Board* theBoard)\n'
    '{\n'
    '    ThirtyFlagsTickMarks();   // ThirtyFlags v5: break-mark timer\n'
    '    ThirtyFlagsTickCorrupt(); // 【三十旗·平衡】腐化计时器\n'
    '    if (!ThirtyFlagsMode() || !theBoard)\n'
    '        return;\n',
    'void ThirtyFlagsBoardUpdate(Board* theBoard)\n'
    '{\n'
    '    ThirtyFlagsTickMarks();   // ThirtyFlags v5: break-mark timer\n'
    '    ThirtyFlagsTickCorrupt(); // 【三十旗·平衡】腐化计时器\n'
    '    if (!ThirtyFlagsMode() || !theBoard)\n'
    '        return;\n'
    '\n'
    '    // 【表现层】元素弹拖尾：带元素的直射弹每 3 帧留一枚加色光点（按元素配色）。\n'
    '    // 用 mMainCounter 限流，避免每帧每弹都生成；同图同模式，最终会被合批。\n'
    '    if ((theBoard->mMainCounter % 3) == 0)\n'
    '    {\n'
    '        Projectile* aProj = nullptr;\n'
    '        while (theBoard->IterateProjectiles(aProj))\n'
    '        {\n'
    '            if (aProj->mDead || aProj->mElement == 0)\n'
    '                continue;\n'
    '\n'
    '            int aR, aG, aB;\n'
    '            TfFxGetElementColor(aProj->mElement, aR, aG, aB);\n'
    '            TfFxSpawn(TF_FX_TRAIL, aProj->mPosX + 12.0f, aProj->mPosY + 10.0f,\n'
    '                14.0f, -0.35f, 10, 110, aR, aG, aB);\n'
    '        }\n'
    '    }\n',
    'fx-trail')

# ---------- ② 精英威压：脚下光环 + 慢脉冲 ----------
rep('    if (TFHasElite(theZombie, ELITE_REGEN) &&\n',
    '    // 【表现层】精英威压：脚下常驻一圈冷色光环，每隔一会儿向外扩一圈脉冲\n'
    '    if ((theZombie->mZombieAge % 30) == 0 && TfFxIsElite(theZombie))\n'
    '    {\n'
    '        TfFxSpawn(TF_FX_GLOW, theZombie->mPosX + 20.0f, theZombie->mPosY + 12.0f,\n'
    '            46.0f, 2.2f, 22, 70, 190, 120, 255);\n'
    '    }\n'
    '    if ((theZombie->mZombieAge % 7) == 0 && TfFxIsElite(theZombie))\n'
    '    {\n'
    '        TfFxSpawn(TF_FX_GLOW, theZombie->mPosX + 20.0f, theZombie->mPosY + 14.0f,\n'
    '            30.0f, 0.0f, 8, 45, 150, 90, 255);\n'
    '    }\n'
    '\n'
    '    if (TFHasElite(theZombie, ELITE_REGEN) &&\n',
    'fx-elite')

# ---------- ③ 全局氛围：换旗闪屏 ----------
rep('void ThirtyFlagsFlagChanged(Board* theBoard)\n{\n',
    'void ThirtyFlagsFlagChanged(Board* theBoard)\n{\n'
    '    // 【表现层】换旗瞬间全屏闪一下（加色白，低 alpha，短促）\n'
    '    if (theBoard != nullptr && ThirtyFlagsMode())\n'
    '    {\n'
    '        TfFxSpawn(TF_FX_FLASH, BOARD_WIDTH * 0.5f, BOARD_HEIGHT * 0.5f,\n'
    '            BOARD_WIDTH * 1.4f, 0.0f, 16, 60, 255, 240, 210);\n'
    '    }\n',
    'fx-flag')

print("fx roll-out patch done")
