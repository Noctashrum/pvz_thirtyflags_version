# -*- coding: utf-8 -*-
# 图集接线：把「一图一纹理」改为「优先放共享图集，失败整图回退」。
# 关键安全点：
#   1) TextureDataPiece 增加 mAtlasPage/mAtlasX/mAtlasY；未入图集时 mAtlasPage = -1
#   2) ReleaseTextures 绝不能释放图集页表面（多个 piece 共享它）
#   3) 入图集的图把 mMaxTotalU/V 置 1（GetTexture 直接给页内绝对 UV）
#   4) 调色板纹理（Palette8）不参与图集（页只有一种格式）
#   5) 图集拷贝不启用 padding hack（那会写进邻居的格子）

CRLF = '\r\n'


def rep(path, old_t, new_t, tag, count=1):
    d = open(path, 'rb').read()
    for eol in ('\r\n', '\n'):
        old = old_t.replace('\n', eol).encode('gbk')
        if d.count(old) == count:
            open(path, 'wb').write(d.replace(old, new_t.replace('\n', eol).encode('gbk')))
            print("  ok", tag, "eol", repr(eol))
            return
    raise AssertionError((tag, 'anchor miss', d.count(old_t.replace('\n', '\r\n').encode('gbk'))))


# ---------------- 1) 头文件：piece 增加图集字段 ----------------
rep('src/SexyAppFramework/D3DInterface.h',
    'struct TextureDataPiece\n'
    '{\n'
    '\tLPDIRECTDRAWSURFACE7 mTexture;\n'
    '\tint mWidth,mHeight;\n'
    '};\n',
    'struct TextureDataPiece\n'
    '{\n'
    '\tLPDIRECTDRAWSURFACE7 mTexture;\n'
    '\tint mWidth,mHeight;\n'
    '\t// 【性能】纹理图集：本 piece 在共享图集页里的位置（-1 = 未入图集，用独立纹理）\n'
    '\tint mAtlasPage;\n'
    '\tint mAtlasX, mAtlasY;\n'
    '};\n',
    'h-piece')

# ---------------- 2) CreateTextureDimensions：初始化图集字段 ----------------
rep('src/SexyAppFramework/D3DInterface.cpp',
    '\t\tTextureDataPiece &aPiece = mTextures[i];\n'
    '\t\taPiece.mTexture = NULL;\n'
    '\t\taPiece.mWidth = mTexPieceWidth;\n'
    '\t\taPiece.mHeight = mTexPieceHeight;\n',
    '\t\tTextureDataPiece &aPiece = mTextures[i];\n'
    '\t\taPiece.mTexture = NULL;\n'
    '\t\taPiece.mWidth = mTexPieceWidth;\n'
    '\t\taPiece.mHeight = mTexPieceHeight;\n'
    '\t\taPiece.mAtlasPage = -1;      // 【性能】默认不入图集\n'
    '\t\taPiece.mAtlasX = 0;\n'
    '\t\taPiece.mAtlasY = 0;\n',
    'dims-init')

# ---------------- 3) CopyImageToTexture：支持目标偏移 ----------------
rep('src/SexyAppFramework/D3DInterface.cpp',
    'static void CopyImageToTexture(LPDIRECTDRAWSURFACE7 theTexture, MemoryImage *theImage, int offx, int offy, int texWidth, int texHeight, PixelFormat theFormat)\n'
    '{\n',
    '// 【性能】图集版多两个参数：写入目标子矩形的左上角（非图集时传 0,0，行为与原来完全一致）。\n'
    'static void CopyImageToTexture(LPDIRECTDRAWSURFACE7 theTexture, MemoryImage *theImage, int offx, int offy, int texWidth, int texHeight, PixelFormat theFormat,\n'
    '\tint destX = 0, int destY = 0)\n'
    '{\n',
    'copy-sig')

rep('src/SexyAppFramework/D3DInterface.cpp',
    '\tbool rightPad = aWidth<texWidth;\n'
    '\tbool bottomPad = aHeight<texHeight;\n',
    '\tbool rightPad = aWidth<texWidth;\n'
    '\tbool bottomPad = aHeight<texHeight;\n'
    '\n'
    '\t// 【性能】图集：把目的指针偏移到子矩形；此时不要启用 padding hack\n'
    '\t// （padding 会写到 aWidth/aHeight 之外，在图集里会踩到邻居的格子）。\n'
    '\tvoid* aDestSurface = aDesc.lpSurface;\n'
    '\tif (destX != 0 || destY != 0)\n'
    '\t{\n'
    '\t\tint aBytesPerPixel = 4;\n'
    '\t\tif (theFormat == PixelFormat_Palette8) aBytesPerPixel = 1;\n'
    '\t\telse if (theFormat == PixelFormat_R5G6B5 || theFormat == PixelFormat_A4R4G4B4) aBytesPerPixel = 2;\n'
    '\t\taDestSurface = (void*)((uchar*)aDesc.lpSurface + aDesc.lPitch * destY + destX * aBytesPerPixel);\n'
    '\t\trightPad = false;\n'
    '\t\tbottomPad = false;\n'
    '\t}\n',
    'copy-offset')

rep('src/SexyAppFramework/D3DInterface.cpp',
    '\t\t\tcase PixelFormat_A8R8G8B8:\tCopyImageToTexture8888(aDesc.lpSurface, aDesc.lPitch, theImage, offx, offy, aWidth, aHeight, rightPad); break;\n'
    '\t\t\tcase PixelFormat_A4R4G4B4:\tCopyImageToTexture4444(aDesc.lpSurface, aDesc.lPitch, theImage, offx, offy, aWidth, aHeight, rightPad); break;\n'
    '\t\t\tcase PixelFormat_R5G6B5:\tCopyImageToTexture565(aDesc.lpSurface, aDesc.lPitch, theImage, offx, offy, aWidth, aHeight, rightPad); break;\n'
    '\t\t\tcase PixelFormat_Palette8:\tCopyImageToTexturePalette8(aDesc.lpSurface, aDesc.lPitch, theImage, offx, offy, aWidth, aHeight, rightPad); break;\n',
    '\t\t\tcase PixelFormat_A8R8G8B8:\tCopyImageToTexture8888(aDestSurface, aDesc.lPitch, theImage, offx, offy, aWidth, aHeight, rightPad); break;\n'
    '\t\t\tcase PixelFormat_A4R4G4B4:\tCopyImageToTexture4444(aDestSurface, aDesc.lPitch, theImage, offx, offy, aWidth, aHeight, rightPad); break;\n'
    '\t\t\tcase PixelFormat_R5G6B5:\tCopyImageToTexture565(aDestSurface, aDesc.lPitch, theImage, offx, offy, aWidth, aHeight, rightPad); break;\n'
    '\t\t\tcase PixelFormat_Palette8:\tCopyImageToTexturePalette8(aDestSurface, aDesc.lPitch, theImage, offx, offy, aWidth, aHeight, rightPad); break;\n',
    'copy-dispatch')

rep('src/SexyAppFramework/D3DInterface.cpp',
    '\t\t\tuchar *dstrow = ((uchar*)aDesc.lpSurface)+aDesc.lPitch*aHeight;\n',
    '\t\t\tuchar *dstrow = ((uchar*)aDestSurface)+aDesc.lPitch*aHeight;\n',
    'copy-bottompad')

# ---------------- 4) 图集分配器（放在 CreateTextureSurface 之后） ----------------
rep('src/SexyAppFramework/D3DInterface.cpp',
    'static void CopyImageToTexture8888(void *theDest, DWORD theDestPitch, MemoryImage *theImage, int offx, int offy, int theWidth, int theHeight, bool rightPad)\n',
    '#if TF_TEXTURE_ATLAS\n'
    '// 【性能】从共享图集里分配一块 w×h（含 gutter）。成功返回页号并输出页内左上角坐标；\n'
    '// 失败返回 -1（调用方回退到"一 piece 一纹理"）。\n'
    'static int TfAtlasAlloc(LPDIRECT3DDEVICE7 theDevice, LPDIRECTDRAW7 theDraw, int theFormat,\n'
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
    '\t\treturn -1;\n'
    '\taPage.mFormat = theFormat;\n'
    '\taPage.mX = 0;\n'
    '\taPage.mY = 0;\n'
    '\taPage.mRowH = 0;\n'
    '\n'
    '\tint aIndex = gTfAtlasPageCount++;\n'
    '\tif (!TfAtlasTryPlace(gTfAtlasPages[aIndex], aW, aH, theOutX, theOutY))\n'
    '\t\treturn -1;                       // 刚建的页，理论上不会发生\n'
    '\treturn aIndex;\n'
    '}\n'
    '#endif\n'
    '\n'
    'static void CopyImageToTexture8888(void *theDest, DWORD theDestPitch, MemoryImage *theImage, int offx, int offy, int theWidth, int theHeight, bool rightPad)\n',
    'atlas-alloc')

print("part 1 done")
