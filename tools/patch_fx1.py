# -*- coding: utf-8 -*-
# 【表现层】特效层：加色四边形池 + 按类型分组绘制（每类只 1 张现成贴图 → 天然合批）
# 设计依据（§12 结论）：特效之间无遮盖语义 → 允许成组；同一遍只有一种混合模式 → 不打断批次。

P = 'src/Lawn/ThirtyFlags.cpp'


def rep(old_t, new_t, tag):
    d = open(P, 'rb').read()
    for eol in ('\r\n', '\n'):
        old = old_t.replace('\n', eol).encode('gbk')
        if d.count(old) == 1:
            open(P, 'wb').write(d.replace(old, new_t.replace('\n', eol).encode('gbk')))
            print("  ok", tag, "eol", repr(eol))
            return
    raise AssertionError((tag, 'anchor miss'))


FX = r'''// ----------------------------------------------------------------------------------------------------
// 【表现层】特效层（加色四边形池）
//
// 为什么要单独成层（而不是"每个特效一个骨骼动画"）：
//   §12 的结论是"合批的红利要靠同纹理 + 同混合模式 + 顺序无关"。而特效恰好三者都满足：
//     * 只用两张**现成**贴图：IMAGE_SPOTLIGHT（软光晕）/ IMAGE_WHITEPIXEL（实心四边形）—— 零新资源；
//     * 全层只用加色混合（DRAWMODE_ADDITIVE），中间不换模式；
//     * 特效之间是加色叠加，**无遮盖语义** → 绘制顺序不影响画面 → 允许按类型成组。
//   于是 "按类型分组绘制" 就等于 "每类所有特效合并成极少数次绘制调用"。
//
// 池满时覆盖寿命最短的一个（而不是丢弃），保证"新事件一定看得见"。
// ----------------------------------------------------------------------------------------------------
enum { TF_FX_MAX = 384 };
enum { TF_FX_GLOW = 0, TF_FX_FLASH = 1, TF_FX_TRAIL = 2, TF_FX_KIND_COUNT = 3 };

struct TfFxQuad
{
    float   mX, mY;          // 中心
    float   mSize;           // 边长
    float   mGrow;           // 每帧膨胀
    int     mLife, mLifeMax;
    int     mKind;
    int     mAlpha;
    int     mR, mG, mB;
};

static TfFxQuad gTfFx[TF_FX_MAX];
static int      gTfFxCount = 0;

static void TfFxSpawn(int theKind, float theX, float theY, float theSize, float theGrow,
                      int theLife, int theAlpha, int theR, int theG, int theB)
{
    if (!ThirtyFlagsMode())
        return;

    int aSlot;
    if (gTfFxCount < TF_FX_MAX)
        aSlot = gTfFxCount++;
    else
    {
        aSlot = 0;
        for (int i = 1; i < TF_FX_MAX; i++)
        {
            if (gTfFx[i].mLife < gTfFx[aSlot].mLife)
                aSlot = i;
        }
    }

    TfFxQuad& aQuad = gTfFx[aSlot];
    aQuad.mX = theX;
    aQuad.mY = theY;
    aQuad.mSize = theSize;
    aQuad.mGrow = theGrow;
    aQuad.mLife = theLife;
    aQuad.mLifeMax = theLife;
    aQuad.mKind = theKind;
    aQuad.mAlpha = theAlpha;
    aQuad.mR = theR;
    aQuad.mG = theG;
    aQuad.mB = theB;
}

static void ThirtyFlagsFxUpdate()
{
    for (int i = 0; i < gTfFxCount; i++)
    {
        gTfFx[i].mLife--;
        gTfFx[i].mSize += gTfFx[i].mGrow;
    }

    for (int i = 0; i < gTfFxCount; i++)
    {
        if (gTfFx[i].mLife <= 0 && i < gTfFxCount - 1)
        {
            gTfFx[i] = gTfFx[gTfFxCount - 1];
            gTfFxCount--;
            i--;
        }
    }
    if (gTfFxCount > 0 && gTfFx[gTfFxCount - 1].mLife <= 0)
        gTfFxCount--;
}

// 按类型分组画：同一类用同一张贴图 + 同一混合模式 → 相邻特效同纹理，直接吃到合批。
static void ThirtyFlagsFxDraw(Sexy::Graphics* g)
{
    if (!ThirtyFlagsMode() || gTfFxCount == 0)
        return;

    for (int aKind = 0; aKind < TF_FX_KIND_COUNT; aKind++)
    {
        Sexy::Image* aImage = (aKind == TF_FX_FLASH) ? Sexy::IMAGE_WHITEPIXEL : Sexy::IMAGE_SPOTLIGHT;
        if (aImage == NULL)
            continue;

        g->SetDrawMode(Sexy::Graphics::DRAWMODE_ADDITIVE);
        g->SetColorizeImages(true);

        for (int i = 0; i < gTfFxCount; i++)
        {
            TfFxQuad& aQuad = gTfFx[i];
            if (aQuad.mKind != aKind)
                continue;

            float aFade = (float)aQuad.mLife / (float)(aQuad.mLifeMax > 0 ? aQuad.mLifeMax : 1);
            if (aFade < 0.0f)
                aFade = 0.0f;
            if (aFade > 1.0f)
                aFade = 1.0f;

            int anAlpha = (int)(aQuad.mAlpha * aFade);
            if (anAlpha <= 0)
                continue;

            int aSize = (int)aQuad.mSize;
            if (aSize < 2)
                continue;

            g->SetColor(Sexy::Color(aQuad.mR, aQuad.mG, aQuad.mB, anAlpha));
            g->DrawImage(aImage, (int)(aQuad.mX - aSize * 0.5f), (int)(aQuad.mY - aSize * 0.5f), aSize, aSize);
        }

        g->SetDrawMode(Sexy::Graphics::DRAWMODE_NORMAL);
        g->SetColorizeImages(false);
    }
}

'''
rep('void ThirtyFlagsUpdateVisuals()\n', FX + 'void ThirtyFlagsUpdateVisuals()\n', 'fx-core')

# 每帧推进
rep('void ThirtyFlagsUpdateVisuals()\n{\n    if (!ThirtyFlagsMode())\n        return;\n',
    'void ThirtyFlagsUpdateVisuals()\n{\n    if (!ThirtyFlagsMode())\n        return;\n'
    '\n'
    '    // 【表现层】特效池推进\n'
    '    ThirtyFlagsFxUpdate();\n',
    'fx-update')

# 绘制（放在 DrawVisuals 的 token 检查之后、结束之前最省事：直接接在 mode/g 检查后面）
rep('void ThirtyFlagsDrawVisuals(Sexy::Graphics* g)\n{\n    if (!ThirtyFlagsMode() || !g)\n        return;\n',
    'void ThirtyFlagsDrawVisuals(Sexy::Graphics* g)\n{\n    // 【表现层】特效层（加色，独立成组）\n'
    '    ThirtyFlagsFxDraw(g);\n'
    '\n'
    '    if (!ThirtyFlagsMode() || !g)\n        return;\n',
    'fx-draw')

# ---- 触发点 1：僵尸受伤 → 小光晕（限流，避免刷屏）----
rep('int ThirtyFlagsZombieTakeDamage(Zombie* theZombie, int theDamage, unsigned int theDamageFlags)\n'
    '{\n'
    '    if (!ThirtyFlagsMode() || !theZombie)\n'
    '        return theDamage;\n'
    '\n'
    '    if (theZombie->mZombieType == ZOMBIE_BOSS)\n'
    '        return theDamage;\n',
    'int ThirtyFlagsZombieTakeDamage(Zombie* theZombie, int theDamage, unsigned int theDamageFlags)\n'
    '{\n'
    '    if (!ThirtyFlagsMode() || !theZombie)\n'
    '        return theDamage;\n'
    '\n'
    '    if (theZombie->mZombieType == ZOMBIE_BOSS)\n'
    '        return theDamage;\n'
    '\n'
    '    // 【表现层】命中光晕：只对"打得动"的伤害放，且约 1/3 概率，避免特效池被刷屏\n'
    '    if (theDamage > 0 && (RandRangeInt(0, 2) == 0))\n'
    '    {\n'
    '        TfFxSpawn(TF_FX_GLOW, theZombie->mPosX + 20.0f, theZombie->mPosY - 30.0f,\n'
    '            34.0f, 0.35f, 12, 90, 255, 240, 170);\n'
    '    }\n',
    'fx-hit')

# ---- 触发点 2：僵尸死亡 → 冲击波光环 ----
rep('void ThirtyFlagsZombieKilled(Zombie* theZombie)\n'
    '{\n'
    '    if (!ThirtyFlagsMode() || !theZombie || !theZombie->mBoard)\n'
    '        return;\n'
    '\n'
    '    if (theZombie->mDead)\n'
    '        return;\n',
    'void ThirtyFlagsZombieKilled(Zombie* theZombie)\n'
    '{\n'
    '    if (!ThirtyFlagsMode() || !theZombie || !theZombie->mBoard)\n'
    '        return;\n'
    '\n'
    '    if (theZombie->mDead)\n'
    '        return;\n'
    '\n'
    '    // 【表现层】击杀冲击波：先来一圈大而慢的白光，再叠一层暖色余晖\n'
    '    TfFxSpawn(TF_FX_GLOW, theZombie->mPosX + 20.0f, theZombie->mPosY - 35.0f,\n'
    '        40.0f, 3.2f, 16, 120, 255, 235, 200);\n'
    '    TfFxSpawn(TF_FX_GLOW, theZombie->mPosX + 20.0f, theZombie->mPosY - 35.0f,\n'
    '        20.0f, 1.6f, 24, 70, 255, 170, 90);\n',
    'fx-kill')

print("fx patch done")
