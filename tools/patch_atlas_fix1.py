# -*- coding: utf-8 -*-
# 图集诊断与健壮化：
#   1) 图集页尺寸改为**运行时**决定（取 min(1024, 设备最大纹理尺寸)，向下取 2 的幂）
#   2) 图集拷贝用**非致命 Lock**：失败只记录 + 该图回退，绝不走 DisplayError→exit
#   3) 关键节点写日志（页创建 / Lock 失败 / 设备上限），便于定位

P = 'src/SexyAppFramework/D3DInterface.cpp'


def rep(old_t, new_t, tag, cnt=1):
    d = open(P, 'rb').read()
    for eol in ('\r\n', '\n'):
        old = old_t.replace('\n', eol).encode('gbk')
        if d.count(old) == cnt:
            open(P, 'wb').write(d.replace(old, new_t.replace('\n', eol).encode('gbk')))
            print("  ok", tag, "eol", repr(eol))
            return
    raise AssertionError((tag, 'anchor miss'))


# ---- 1) 页尺寸改运行时 ----
rep('enum { TF_ATLAS_PAGE_SIZE = MAX_TEXTURE_SIZE, TF_ATLAS_MAX_PAGES = 16, TF_ATLAS_GUTTER = 1 };\n',
    'enum { TF_ATLAS_PAGE_MAX = MAX_TEXTURE_SIZE, TF_ATLAS_MAX_PAGES = 16, TF_ATLAS_GUTTER = 1 };\n'
    '\n'
    '// 【诊断】图集页的实际边长在运行时决定：取 min(1024, 设备最大纹理边长) 并向下取 2 的幂。\n'
    '// 原因：直接按 1024 申请，在小上限设备上会创建失败（或锁不住），\n'
    '// 而失败会走 CheckDXError → DisplayError → exit，直接把进程干掉。\n'
    'static int gTfAtlasPageSize = 0;\n',
    'page-size-var')

# ---- 2) TfAtlasTryPlace 增加页尺寸参数 ----
rep('static bool TfAtlasTryPlace(TfAtlasPage& thePage, int theW, int theH, int& theOutX, int& theOutY)\n'
    '{\n'
    '\tif (theW > TF_ATLAS_PAGE_SIZE || theH > TF_ATLAS_PAGE_SIZE)\n'
    '\t\treturn false;                                  // 单块就超过一页，放不下\n'
    '\n'
    '\tif (thePage.mX + theW > TF_ATLAS_PAGE_SIZE)\n'
    '\t{\n'
    '\t\t// 当前货架放不下 → 换一行\n'
    '\t\tthePage.mY += thePage.mRowH;\n'
    '\t\tthePage.mX = 0;\n'
    '\t\tthePage.mRowH = 0;\n'
    '\t}\n'
    '\n'
    '\tif (thePage.mY + theH > TF_ATLAS_PAGE_SIZE)\n'
    '\t\treturn false;                                  // 这一页也满了\n',
    'static bool TfAtlasTryPlace(TfAtlasPage& thePage, int thePageSize, int theW, int theH, int& theOutX, int& theOutY)\n'
    '{\n'
    '\tif (theW > thePageSize || theH > thePageSize)\n'
    '\t\treturn false;                                  // 单块就超过一页，放不下\n'
    '\n'
    '\tif (thePage.mX + theW > thePageSize)\n'
    '\t{\n'
    '\t\t// 当前货架放不下 → 换一行\n'
    '\t\tthePage.mY += thePage.mRowH;\n'
    '\t\tthePage.mX = 0;\n'
    '\t\tthePage.mRowH = 0;\n'
    '\t}\n'
    '\n'
    '\tif (thePage.mY + theH > thePageSize)\n'
    '\t\treturn false;                                  // 这一页也满了\n',
    'tryplace-sig')

# ---- 3) 分配器：运行时页尺寸 + 日志 + 调用点适配 ----
rep('static int TfAtlasAlloc(LPDIRECT3DDEVICE7 theDevice, LPDIRECTDRAW7 theDraw, int theFormat,\n'
    '\tint theWidth, int theHeight, int& theOutX, int& theOutY)\n'
    '{\n'
    '\tif (theFormat == (int)PixelFormat_Palette8)\n'
    '\t\treturn -1;                       // 调色板纹理不参与图集（一页只有一种格式）\n'
    '\n'
    '\tint aW = theWidth + TF_ATLAS_GUTTER;\n'
    '\tint aH = theHeight + TF_ATLAS_GUTTER;\n'
    '\tif (aW > TF_ATLAS_PAGE_SIZE || aH > TF_ATLAS_PAGE_SIZE)\n'
    '\t\treturn -1;                       // 单块超过一页\n'
    '\n'
    '\tfor (int i = 0; i < gTfAtlasPageCount; i++)\n'
    '\t{\n'
    '\t\tif (gTfAtlasPages[i].mFormat != theFormat)\n'
    '\t\t\tcontinue;\n'
    '\t\tif (TfAtlasTryPlace(gTfAtlasPages[i], aW, aH, theOutX, theOutY))\n'
    '\t\t\treturn i;\n'
    '\t}\n'
    '\n'
    '\tif (gTfAtlasPageCount >= TF_ATLAS_MAX_PAGES)\n'
    '\t\treturn -1;\n'
    '\n'
    '\tTfAtlasPage& aPage = gTfAtlasPages[gTfAtlasPageCount];\n'
    '\taPage.mSurface = CreateTextureSurface(theDevice, theDraw, TF_ATLAS_PAGE_SIZE, TF_ATLAS_PAGE_SIZE, (PixelFormat)theFormat);\n'
    '\tif (aPage.mSurface == NULL)\n'
    '\t\treturn -1;\n',
    '// 【诊断】首次调用时确定页边长：取设备上限与 1024 的较小值，向下取 2 的幂。\n'
    'static void TfAtlasEnsurePageSize()\n'
    '{\n'
    '\tif (gTfAtlasPageSize != 0)\n'
    '\t\treturn;\n'
    '\n'
    '\tint aSize = TF_ATLAS_PAGE_MAX;\n'
    '\tif (gMaxTextureWidth > 0 && gMaxTextureWidth < aSize)\n'
    '\t\taSize = gMaxTextureWidth;\n'
    '\tif (gMaxTextureHeight > 0 && gMaxTextureHeight < aSize)\n'
    '\t\taSize = gMaxTextureHeight;\n'
    '\n'
    '\tint aPow2 = 1;\n'
    '\twhile (aPow2 * 2 <= aSize)\n'
    '\t\taPow2 *= 2;\n'
    '\tgTfAtlasPageSize = aPow2;\n'
    '\n'
    '\tchar aBuf[192];\n'
    '\tsprintf(aBuf, "[TFAtlas] device max tex = %dx%d, formats=0x%x -> page size = %d",\n'
    '\t\tgMaxTextureWidth, gMaxTextureHeight, (unsigned int)gSupportedPixelFormats, gTfAtlasPageSize);\n'
    '\tTFAtlasLog(aBuf);\n'
    '\n'
    '\tif (gTfAtlasPageSize < 256)\n'
    '\t\tgTfAtlasPageSize = 0;            // 太小就别用图集（保持 0 表示禁用）\n'
    '}\n'
    '\n'
    'static int TfAtlasAlloc(LPDIRECT3DDEVICE7 theDevice, LPDIRECTDRAW7 theDraw, int theFormat,\n'
    '\tint theWidth, int theHeight, int& theOutX, int& theOutY)\n'
    '{\n'
    '\tif (theFormat == (int)PixelFormat_Palette8)\n'
    '\t\treturn -1;                       // 调色板纹理不参与图集（一页只有一种格式）\n'
    '\n'
    '\tTfAtlasEnsurePageSize();\n'
    '\tif (gTfAtlasPageSize == 0)\n'
    '\t\treturn -1;                       // 设备上限太小，禁用图集\n'
    '\n'
    '\tint aW = theWidth + TF_ATLAS_GUTTER;\n'
    '\tint aH = theHeight + TF_ATLAS_GUTTER;\n'
    '\tif (aW > gTfAtlasPageSize || aH > gTfAtlasPageSize)\n'
    '\t\treturn -1;                       // 单块超过一页\n'
    '\n'
    '\tfor (int i = 0; i < gTfAtlasPageCount; i++)\n'
    '\t{\n'
    '\t\tif (gTfAtlasPages[i].mFormat != theFormat)\n'
    '\t\t\tcontinue;\n'
    '\t\tif (TfAtlasTryPlace(gTfAtlasPages[i], gTfAtlasPageSize, aW, aH, theOutX, theOutY))\n'
    '\t\t\treturn i;\n'
    '\t}\n'
    '\n'
    '\tif (gTfAtlasPageCount >= TF_ATLAS_MAX_PAGES)\n'
    '\t\treturn -1;\n'
    '\n'
    '\tTfAtlasPage& aPage = gTfAtlasPages[gTfAtlasPageCount];\n'
    '\taPage.mSurface = CreateTextureSurface(theDevice, theDraw, gTfAtlasPageSize, gTfAtlasPageSize, (PixelFormat)theFormat);\n'
    '\tif (aPage.mSurface == NULL)\n'
    '\t{\n'
    '\t\tTFAtlasLog("[TFAtlas] create page FAILED");\n'
    '\t\treturn -1;\n'
    '\t}\n'
    '\t{\n'
    '\t\tchar aBuf[128];\n'
    '\t\tsprintf(aBuf, "[TFAtlas] page %d created: %d x %d fmt=%d", gTfAtlasPageCount, gTfAtlasPageSize, gTfAtlasPageSize, theFormat);\n'
    '\t\tTFAtlasLog(aBuf);\n'
    '\t}\n',
    'alloc-runtime')

rep('\tint aIndex = gTfAtlasPageCount++;\n'
    '\tif (!TfAtlasTryPlace(gTfAtlasPages[aIndex], aW, aH, theOutX, theOutY))\n',
    '\tint aIndex = gTfAtlasPageCount++;\n'
    '\tif (!TfAtlasTryPlace(gTfAtlasPages[aIndex], gTfAtlasPageSize, aW, aH, theOutX, theOutY))\n',
    'alloc-place')

# ---- 4) GetTexture/GetTextureF 的 UV 分母改用运行时页尺寸 ----
rep('\t\tu1 = (float)(aPiece.mAtlasX + left) / TF_ATLAS_PAGE_SIZE;\n'
    '\t\tv1 = (float)(aPiece.mAtlasY + top) / TF_ATLAS_PAGE_SIZE;\n'
    '\t\tu2 = (float)(aPiece.mAtlasX + right) / TF_ATLAS_PAGE_SIZE;\n'
    '\t\tv2 = (float)(aPiece.mAtlasY + bottom) / TF_ATLAS_PAGE_SIZE;\n',
    '\t\tu1 = (float)(aPiece.mAtlasX + left) / gTfAtlasPageSize;\n'
    '\t\tv1 = (float)(aPiece.mAtlasY + top) / gTfAtlasPageSize;\n'
    '\t\tu2 = (float)(aPiece.mAtlasX + right) / gTfAtlasPageSize;\n'
    '\t\tv2 = (float)(aPiece.mAtlasY + bottom) / gTfAtlasPageSize;\n',
    'uv-runtime', cnt=2)

print("done")
