# -*- coding: utf-8 -*-
# 图集接线第二部分：CreateTextures 走图集 / GetTexture* 返回页内 UV / ReleaseTextures 不误放共享页


def rep(path, old_t, new_t, tag):
    d = open(path, 'rb').read()
    for eol in ('\r\n', '\n'):
        old = old_t.replace('\n', eol).encode('gbk')
        if d.count(old) == 1:
            open(path, 'wb').write(d.replace(old, new_t.replace('\n', eol).encode('gbk')))
            print("  ok", tag, "eol", repr(eol))
            return
    raise AssertionError((tag, 'anchor miss'))


P = 'src/SexyAppFramework/D3DInterface.cpp'

# ---- 1) CreateTextures 主循环：优先入图集 ----
rep(P,
    '\ti=0;\n'
    '\tfor(y=0; y<aHeight; y+=mTexPieceHeight)\n'
    '\t{\n'
    '\t\tfor(x=0; x<aWidth; x+=mTexPieceWidth, i++)\n'
    '\t\t{\n'
    '\t\t\tTextureDataPiece &aPiece = mTextures[i];\n'
    '\t\t\tif (createTextures)\n'
    '\t\t\t{\n'
    '\t\t\t\taPiece.mTexture = CreateTextureSurface(theDevice, theDraw, aPiece.mWidth, aPiece.mHeight, aFormat);\n'
    '\t\t\t\tif (aPiece.mTexture==NULL) // create texture failure\n'
    '\t\t\t\t{\n'
    '\t\t\t\t\tmPixelFormat = PixelFormat_Unknown;\n'
    '\t\t\t\t\treturn;\n'
    '\t\t\t\t}\n'
    '\n'
    '\t\t\t\tif (mPalette!=NULL)\n'
    '\t\t\t\t\taPiece.mTexture->SetPalette(mPalette);\n'
    '\t\t\t\t\t\n'
    '\t\t\t\tmTexMemSize += aPiece.mWidth*aPiece.mHeight*aFormatSize;\n'
    '\t\t\t}\n'
    '\n'
    '\t\t\tCopyImageToTexture(aPiece.mTexture,theImage,x,y,aPiece.mWidth,aPiece.mHeight,aFormat);\n'
    '\t\t}\n'
    '\t}\n',
    '\t// 【性能】优先把整张图放进共享图集：所有 piece 都成功才用图集（失败整图回退），\n'
    '\t// 保证不会出现"一半在图集、一半在独立纹理"导致 UV 语义不一致。\n'
    '\tbool aUseAtlas = false;\n'
    '#if TF_TEXTURE_ATLAS\n'
    '\taUseAtlas = (aFormat != PixelFormat_Palette8);\n'
    '#endif\n'
    '\n'
    '\ti=0;\n'
    '\tfor(y=0; y<aHeight; y+=mTexPieceHeight)\n'
    '\t{\n'
    '\t\tfor(x=0; x<aWidth; x+=mTexPieceWidth, i++)\n'
    '\t\t{\n'
    '\t\t\tTextureDataPiece &aPiece = mTextures[i];\n'
    '\t\t\tif (createTextures)\n'
    '\t\t\t{\n'
    '#if TF_TEXTURE_ATLAS\n'
    '\t\t\t\tif (aUseAtlas)\n'
    '\t\t\t\t{\n'
    '\t\t\t\t\tint aAtlasX = 0, aAtlasY = 0;\n'
    '\t\t\t\t\tint aPage = TfAtlasAlloc(theDevice, theDraw, (int)aFormat, aPiece.mWidth, aPiece.mHeight, aAtlasX, aAtlasY);\n'
    '\t\t\t\t\tif (aPage >= 0)\n'
    '\t\t\t\t\t{\n'
    '\t\t\t\t\t\taPiece.mTexture = gTfAtlasPages[aPage].mSurface;\n'
    '\t\t\t\t\t\taPiece.mAtlasPage = aPage;\n'
    '\t\t\t\t\t\taPiece.mAtlasX = aAtlasX;\n'
    '\t\t\t\t\t\taPiece.mAtlasY = aAtlasY;\n'
    '\t\t\t\t\t\tmTexMemSize += aPiece.mWidth*aPiece.mHeight*aFormatSize;\n'
    '\t\t\t\t\t}\n'
    '\t\t\t\t\telse\n'
    '\t\t\t\t\t{\n'
    '\t\t\t\t\t\taUseAtlas = false;      // 这一张图整体回退（已分配的空间视为浪费，量很小）\n'
    '\t\t\t\t\t}\n'
    '\t\t\t\t}\n'
    '#endif\n'
    '\t\t\t\tif (aPiece.mAtlasPage < 0)\n'
    '\t\t\t\t{\n'
    '\t\t\t\t\taPiece.mTexture = CreateTextureSurface(theDevice, theDraw, aPiece.mWidth, aPiece.mHeight, aFormat);\n'
    '\t\t\t\t\tif (aPiece.mTexture==NULL) // create texture failure\n'
    '\t\t\t\t\t{\n'
    '\t\t\t\t\t\tmPixelFormat = PixelFormat_Unknown;\n'
    '\t\t\t\t\t\treturn;\n'
    '\t\t\t\t\t}\n'
    '\n'
    '\t\t\t\t\tif (mPalette!=NULL)\n'
    '\t\t\t\t\t\taPiece.mTexture->SetPalette(mPalette);\n'
    '\n'
    '\t\t\t\t\tmTexMemSize += aPiece.mWidth*aPiece.mHeight*aFormatSize;\n'
    '\t\t\t\t}\n'
    '\t\t\t}\n'
    '\n'
    '\t\t\tif (aPiece.mAtlasPage >= 0)\n'
    '\t\t\t\tCopyImageToTexture(aPiece.mTexture, theImage, x, y, aPiece.mWidth, aPiece.mHeight, aFormat, aPiece.mAtlasX, aPiece.mAtlasY);\n'
    '\t\t\telse\n'
    '\t\t\t\tCopyImageToTexture(aPiece.mTexture,theImage,x,y,aPiece.mWidth,aPiece.mHeight,aFormat);\n'
    '\t\t}\n'
    '\t}\n'
    '\n'
    '#if TF_TEXTURE_ATLAS\n'
    '\t// 入图集后，GetTexture 直接返回"页内绝对 UV"，故缩放取 1（单 piece 图原本就是 1）\n'
    '\tif (aUseAtlas)\n'
    '\t{\n'
    '\t\tfor (int aPieceIndex = 0; aPieceIndex < (int)mTextures.size(); aPieceIndex++)\n'
    '\t\t{\n'
    '\t\t\tif (mTextures[aPieceIndex].mAtlasPage < 0)\n'
    '\t\t\t{\n'
    '\t\t\t\taUseAtlas = false;\n'
    '\t\t\t\tbreak;\n'
    '\t\t\t}\n'
    '\t\t}\n'
    '\t}\n'
    '\tif (aUseAtlas)\n'
    '\t{\n'
    '\t\tmMaxTotalU = 1.0f;\n'
    '\t\tmMaxTotalV = 1.0f;\n'
    '\t}\n'
    '#endif\n',
    'create-atlas')

# ---- 2) GetTexture：图集返回页内绝对 UV ----
rep(P,
    '\tu1 = (float)left/aPiece.mWidth;\n'
    '\tv1 = (float)top/aPiece.mHeight;\n'
    '\tu2 = (float)right/aPiece.mWidth;\n'
    '\tv2 = (float)bottom/aPiece.mHeight;\n'
    '\n'
    '\treturn aPiece.mTexture;\n'
    '}\n',
    '\tif (aPiece.mAtlasPage >= 0)\n'
    '\t{\n'
    '\t\t// 【性能】图集：UV 以整页为分母，并加上本 piece 在页内的偏移\n'
    '\t\tu1 = (float)(aPiece.mAtlasX + left) / TF_ATLAS_PAGE_SIZE;\n'
    '\t\tv1 = (float)(aPiece.mAtlasY + top) / TF_ATLAS_PAGE_SIZE;\n'
    '\t\tu2 = (float)(aPiece.mAtlasX + right) / TF_ATLAS_PAGE_SIZE;\n'
    '\t\tv2 = (float)(aPiece.mAtlasY + bottom) / TF_ATLAS_PAGE_SIZE;\n'
    '\t\treturn aPiece.mTexture;\n'
    '\t}\n'
    '\n'
    '\tu1 = (float)left/aPiece.mWidth;\n'
    '\tv1 = (float)top/aPiece.mHeight;\n'
    '\tu2 = (float)right/aPiece.mWidth;\n'
    '\tv2 = (float)bottom/aPiece.mHeight;\n'
    '\n'
    '\treturn aPiece.mTexture;\n'
    '}\n',
    'gettex')

# ---- 3) ReleaseTextures：不释放共享图集页 ----
rep(P,
    '\tfor(int i=0; i<(int)mTextures.size(); i++)\n'
    '\t{\n'
    '\t\tLPDIRECTDRAWSURFACE7 aSurface = mTextures[i].mTexture;\n'
    '\t\tif (aSurface!=NULL)\n'
    '\t\t\taSurface->Release();\n'
    '\t}\n',
    '\tfor(int i=0; i<(int)mTextures.size(); i++)\n'
    '\t{\n'
    '\t\t// 【性能】入图集的 piece 共用图集页表面，**绝不能**在这里 Release\n'
    '\t\t// （否则会释放掉其它精灵共用的整页纹理）。\n'
    '\t\tif (mTextures[i].mAtlasPage >= 0)\n'
    '\t\t\tcontinue;\n'
    '\n'
    '\t\tLPDIRECTDRAWSURFACE7 aSurface = mTextures[i].mTexture;\n'
    '\t\tif (aSurface!=NULL)\n'
    '\t\t\taSurface->Release();\n'
    '\t}\n',
    'release-guard')

print("part 2 done")
