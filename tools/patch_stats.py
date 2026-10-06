# -*- coding: utf-8 -*-
# 给探针加「本帧精灵四边形数 / 实际绘制调用次数」统计，供合批 A/B 对照。
# 合批关闭时 calls == quads；开启时 calls 远小于 quads。

def to_bytes(text, eol):
    return text.replace('\n', eol).encode('gbk')


def rep(path, old_text, new_text, tag):
    d = open(path, 'rb').read()
    for eol in ('\r\n', '\n'):
        old = to_bytes(old_text, eol)
        if d.count(old) == 1:
            open(path, 'wb').write(d.replace(old, to_bytes(new_text, eol)))
            print("  ok", tag, "eol", repr(eol))
            return
    raise AssertionError((tag, 'anchor miss', d.count(to_bytes(old_text, '\r\n'))))


P = 'src/SexyAppFramework/D3DInterface.cpp'

# 1) 计数器 + 访问器
rep(P,
    'static void FlushSpriteBatch(LPDIRECT3DDEVICE7 theDevice);\n',
    'static void FlushSpriteBatch(LPDIRECT3DDEVICE7 theDevice);\n'
    '\n'
    '// 【性能】精灵绘制统计：本帧追加的四边形数 / 实际提交的绘制调用次数。\n'
    '// 合批关闭时两者相等（一个精灵一次调用）；开启时调用数远小于四边形数。\n'
    'static unsigned int gSpriteQuadsThisFrame = 0;\n'
    'static unsigned int gSpriteCallsThisFrame = 0;\n'
    '\n'
    'void D3DInterfaceGetSpriteStats(unsigned int* theQuads, unsigned int* theCalls)\n'
    '{\n'
    '\tif (theQuads != NULL) *theQuads = gSpriteQuadsThisFrame;\n'
    '\tif (theCalls != NULL) *theCalls = gSpriteCallsThisFrame;\n'
    '\tgSpriteQuadsThisFrame = 0;\n'
    '\tgSpriteCallsThisFrame = 0;\n'
    '}\n',
    'stats-vars')

# 2) 批次提交时记一次调用
rep(P,
    '\t\tgSpriteBatchVerts, gSpriteBatchCount, 0), "DrawPrimitive sprite batch");\n',
    '\t\tgSpriteBatchVerts, gSpriteBatchCount, 0), "DrawPrimitive sprite batch");\n'
    '\tgSpriteCallsThisFrame++;\n',
    'flush-call')

# 3) Blt 追加四边形：计数（合批分支）
rep(P,
    '\t\t\t\taOut[5] = aVertex[3];\n'
    '\t\t\t\tgSpriteBatchCount += 6;\n',
    '\t\t\t\taOut[5] = aVertex[3];\n'
    '\t\t\t\tgSpriteBatchCount += 6;\n'
    '\t\t\t\tgSpriteQuadsThisFrame++;\n',
    'blt-quads')

# 4) 裁剪循环里的合批分支：计数
rep(P,
    '\t\t\t\t\taOut[5] = aVertex[3];\n'
    '\t\t\t\t\tgSpriteBatchCount += 6;\n',
    '\t\t\t\t\taOut[5] = aVertex[3];\n'
    '\t\t\t\t\tgSpriteBatchCount += 6;\n'
    '\t\t\t\t\tgSpriteQuadsThisFrame++;\n',
    'blt2-quads')

# 5) 关闭合批的分支：quads 与 calls 各计一次
rep(P,
    '\t\t\tD3DInterface::CheckDXError(theDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, gVertexType, aVertex, 4, 0),"DrawPrimitive (Tri) 1");\n'
    '#endif\n',
    '\t\t\tD3DInterface::CheckDXError(theDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, gVertexType, aVertex, 4, 0),"DrawPrimitive (Tri) 1");\n'
    '\t\t\tgSpriteQuadsThisFrame++;\n'
    '\t\t\tgSpriteCallsThisFrame++;\n'
    '#endif\n',
    'nobatch-1')

rep(P,
    '\t\t\t\tD3DInterface::CheckDXError(theDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, gVertexType, aVertex, 4, 0),"DrawPrimitive (Tri) 3");\n'
    '#endif\n',
    '\t\t\t\tD3DInterface::CheckDXError(theDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, gVertexType, aVertex, 4, 0),"DrawPrimitive (Tri) 3");\n'
    '\t\t\t\tgSpriteQuadsThisFrame++;\n'
    '\t\t\t\tgSpriteCallsThisFrame++;\n'
    '#endif\n',
    'nobatch-2')

# 6) 头文件声明
rep('src/SexyAppFramework/D3DInterface.h',
    'class D3DInterface\n',
    '// 【性能】读取并清零「本帧精灵四边形数 / 批次提交次数」（调试探针用）\n'
    'void D3DInterfaceGetSpriteStats(unsigned int* theQuads, unsigned int* theCalls);\n'
    '\n'
    'class D3DInterface\n',
    'h-decl')

# 7) 探针：收集 + 打印
rep('src/Lawn/ThirtyFlags.cpp',
    'static double TFPerfMs(LARGE_INTEGER theFrom, LARGE_INTEGER theTo)\n',
    'static unsigned int gTFPerfSpriteQuads = 0;   // 【性能】本帧精灵四边形数\n'
    'static unsigned int gTFPerfSpriteCalls = 0;   // 【性能】本帧实际绘制调用次数\n'
    '\n'
    'static double TFPerfMs(LARGE_INTEGER theFrom, LARGE_INTEGER theTo)\n',
    'probe-vars')

rep('src/Lawn/ThirtyFlags.cpp',
    '\t\tif (aDrawMs > gTFPerfMaxDraw)\n'
    '\t\t\tgTFPerfMaxDraw = aDrawMs;\n'
    '\t}\n',
    '\t\tif (aDrawMs > gTFPerfMaxDraw)\n'
    '\t\t\tgTFPerfMaxDraw = aDrawMs;\n'
    '\t}\n'
    '\n'
    '\tD3DInterfaceGetSpriteStats(&gTFPerfSpriteQuads, &gTFPerfSpriteCalls);\n',
    'probe-collect')

rep('src/Lawn/ThirtyFlags.cpp',
    '" | z=%u pl=%u pr=%u reanim=%u emit=%u part=%u | 3d=%d",\n',
    '" | z=%u pl=%u pr=%u reanim=%u emit=%u part=%u | sprite=%u/%u 3d=%d",\n',
    'probe-fmt')

rep('src/Lawn/ThirtyFlags.cpp',
    '\t\t\ttheBoard->mApp->Is3DAccelerated() ? 1 : 0);\n'
    '\t\tTFLog(aBuf);\n',
    '\t\t\tgTFPerfSpriteQuads, gTFPerfSpriteCalls,\n'
    '\t\t\ttheBoard->mApp->Is3DAccelerated() ? 1 : 0);\n'
    '\t\tTFLog(aBuf);\n',
    'probe-args')

print("ok")
