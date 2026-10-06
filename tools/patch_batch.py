# -*- coding: utf-8 -*-
# 精灵批次合并：把「连续、同纹理、同混合模式」的四边形攒进一个三角列表，一次提交。
# 原实现每个精灵 = 1 次 SetTexture + 1 次 DrawPrimitive，一帧上千次绘制调用。
# 顺序、顶点位置/颜色/UV 完全不变 → 像素结果一致。

p = 'src/SexyAppFramework/D3DInterface.cpp'
d = open(p, 'rb').read()


def rep(old_text, new_text, tag, count=1):
    global d
    for eol in ('\r\n', '\n'):
        old = old_text.replace('\n', eol).encode('gbk')
        if d.count(old) == count:
            d = d.replace(old, new_text.replace('\n', eol).encode('gbk'))
            print("  ok", tag, "eol", repr(eol))
            return
    raise AssertionError((tag, 'anchor miss', d.count(old_text.replace('\n', '\r\n').encode('gbk'))))


# ---------- 1) 批次状态 + flush 函数（插在 SetLinearFilter 之后、TextureData::Blt 之前） ----------
anchor = ('///////////////////////////////////////////////////////////////////////////////\n'
          '///////////////////////////////////////////////////////////////////////////////\n'
          'void TextureData::Blt(LPDIRECT3DDEVICE7 theDevice, float theX, float theY, const Rect& theSrcRect, const Color& theColor)\n')
batch = ('''///////////////////////////////////////////////////////////////////////////////
// 【性能】精灵批次：把连续、同纹理、同混合模式的四边形攒进一个三角列表，一次提交。
//
// 原实现（下方 TextureData::Blt 的内层循环）对**每个四边形**发一次
//   SetTexture + DrawPrimitive(4 顶点, 即时模式)
// 一帧上千个精灵就是上千次绘制调用 —— 实测「空场绘制 0.3ms、31 个僵尸涨到 4~5ms」，
// 随精灵数线性增长；这就是原版渲染路径的主要瓶颈（DX8 时代即时模式 + 无批次）。
//
// 这里只做「合并提交」，每个四边形的位置/颜色/UV、以及四边形之间的先后顺序**一律不变**，
// 因此画出来的像素与原实现逐像素一致。渲染顺序被任何其它绘制/状态打断时立即 flush，
// 保证顺序语义不破。
//
// 把 TF_SPRITE_BATCH 设为 0 可一键退回原实现（排查用）。
///////////////////////////////////////////////////////////////////////////////
#define TF_SPRITE_BATCH 1

#if TF_SPRITE_BATCH
enum { TF_SPRITE_BATCH_MAX_QUADS = 256 };
static LPDIRECTDRAWSURFACE7 gSpriteBatchTex = nullptr;
static int                  gSpriteBatchCount = 0;        // 已攒顶点数（每四边形 6 个）
static int                  gSpriteBatchModeKey = -1;     // 当前批次的混合模式标识
static D3DTLVERTEX          gSpriteBatchVerts[TF_SPRITE_BATCH_MAX_QUADS * 6];

static void FlushSpriteBatch(LPDIRECT3DDEVICE7 theDevice)
{
	if (gSpriteBatchCount == 0)
		return;

	D3DInterface::CheckDXError(theDevice->SetTexture(0, gSpriteBatchTex), "SetTexture sprite batch");
	D3DInterface::CheckDXError(theDevice->DrawPrimitive(D3DPT_TRIANGLELIST, gVertexType,
		gSpriteBatchVerts, gSpriteBatchCount, 0), "DrawPrimitive sprite batch");

	gSpriteBatchCount = 0;
	gSpriteBatchTex = nullptr;
	gSpriteBatchModeKey = -1;
}
#else
static void FlushSpriteBatch(LPDIRECT3DDEVICE7) { }
#endif

''')
rep(anchor, batch + anchor, 'batch-helper')

# ---------- 2) TextureData::Blt 内层：改为攒批 ----------
rep('''			D3DInterface::CheckDXError(theDevice->SetTexture(0, aTexture),"SetTexture gTexture");			
			D3DInterface::CheckDXError(theDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, gVertexType, aVertex, 4, 0),"DrawPrimitive (Tri) 1");
''',
'''#if TF_SPRITE_BATCH
			// 【性能】同纹理 + 同混合模式 → 追加进当前批次；否则先 flush 再开新批。
			// 顶点改写为三角列表（每四边形 6 个顶点，两个三角形共用 v2 对角线），
			// 位置/颜色/UV 与原 4 顶点三角带完全等价。
			if (aTexture != gSpriteBatchTex || gSpriteBatchModeKey != gCurSpriteBatchModeKey)
				FlushSpriteBatch(theDevice);

			if (gSpriteBatchCount + 6 > TF_SPRITE_BATCH_MAX_QUADS * 6)
				FlushSpriteBatch(theDevice);

			gSpriteBatchTex = aTexture;
			gSpriteBatchModeKey = gCurSpriteBatchModeKey;

			{
				D3DTLVERTEX* aOut = &gSpriteBatchVerts[gSpriteBatchCount];
				aOut[0] = aVertex[0];
				aOut[1] = aVertex[1];
				aOut[2] = aVertex[2];
				aOut[3] = aVertex[2];
				aOut[4] = aVertex[1];
				aOut[5] = aVertex[3];
				gSpriteBatchCount += 6;
			}
#else
			D3DInterface::CheckDXError(theDevice->SetTexture(0, aTexture),"SetTexture gTexture");			
			D3DInterface::CheckDXError(theDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, gVertexType, aVertex, 4, 0),"DrawPrimitive (Tri) 1");
#endif
''', 'blt-batch')

# ---------- 3) 第二处精灵循环（带裁剪判断的那个）：只要走非裁剪分支就同样攒批 ----------
rep('''			D3DInterface::CheckDXError(theDevice->SetTexture(0, aTexture),"SetTexture gTexture");

			if (!clipped)
				D3DInterface::CheckDXError(theDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, gVertexType, aVertex, 4, 0),"DrawPrimitive (Tri) 3");
			else
			{
''',
'''			// 【性能】未裁剪的四边形同样走批次；部分越界的仍走原 CPU 裁剪路径。
			// 裁剪与否必须与批次隔离：先 flush，保证批次里的顶点不会被错误裁剪。
			if (!clipped)
			{
#if TF_SPRITE_BATCH
				if (aTexture != gSpriteBatchTex || gSpriteBatchModeKey != gCurSpriteBatchModeKey)
					FlushSpriteBatch(theDevice);
				if (gSpriteBatchCount + 6 > TF_SPRITE_BATCH_MAX_QUADS * 6)
					FlushSpriteBatch(theDevice);
				gSpriteBatchTex = aTexture;
				gSpriteBatchModeKey = gCurSpriteBatchModeKey;
				{
					D3DTLVERTEX* aOut = &gSpriteBatchVerts[gSpriteBatchCount];
					aOut[0] = aVertex[0];
					aOut[1] = aVertex[1];
					aOut[2] = aVertex[2];
					aOut[3] = aVertex[2];
					aOut[4] = aVertex[1];
					aOut[5] = aVertex[3];
					gSpriteBatchCount += 6;
				}
#else
				D3DInterface::CheckDXError(theDevice->SetTexture(0, aTexture),"SetTexture gTexture");
				D3DInterface::CheckDXError(theDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, gVertexType, aVertex, 4, 0),"DrawPrimitive (Tri) 3");
#endif
			}
			else
			{
				FlushSpriteBatch(theDevice);      // 【性能】裁剪路径与批次互斥
				D3DInterface::CheckDXError(theDevice->SetTexture(0, aTexture),"SetTexture gTexture");
''', 'blt-clip-loop')

# ---------- 4) SetupDrawMode：模式变化时先 flush（并缓存，去掉重复状态） ----------
rep('''void D3DInterface::SetupDrawMode(int theDrawMode, const Color &theColor, Image *theImage)
{
	if (theDrawMode == Graphics::DRAWMODE_NORMAL)
''',
'''void D3DInterface::SetupDrawMode(int theDrawMode, const Color &theColor, Image *theImage)
{
	// 【性能】混合模式没变就不重复下发（原来每个精灵无条件发两次 SetRenderState）。
	// 同时，模式一旦变化必须先把攒着的批次提交掉 —— 否则它们会用到新的混合状态。
	if (gCurDrawModeSet == theDrawMode)
		return;

	FlushSpriteBatch(mD3DDevice);
	gCurDrawModeSet = theDrawMode;
	gCurSpriteBatchModeKey = theDrawMode;

	if (theDrawMode == Graphics::DRAWMODE_NORMAL)
''', 'setup-drawmode')

# 该函数体末尾也要让模式变化后立刻写入批次键（上面已设），这里只补缓存变量声明
rep('''static void SetLinearFilter(LPDIRECT3DDEVICE7 theDevice, bool linear)
{''',
'''// 【性能】混合模式的当前值（用于去重）与精灵批次的模式标识。
static int gCurDrawModeSet = INT_MIN;
static int gCurSpriteBatchModeKey = INT_MIN;

static void SetLinearFilter(LPDIRECT3DDEVICE7 theDevice, bool linear)
{''', 'drawmode-cache-vars')

# ---------- 5) 其它绘制入口：进入前一律 flush，保证渲染顺序 ----------
flush_targets = [
    'void D3DInterface::BltMirror(Image* theImage, float theX, float theY, const Rect& theSrcRect, const Color& theColor, int theDrawMode, bool linearFilter)\n{\n',
    'void D3DInterface::BltClipF(Image* theImage, float theX, float theY, const Rect& theSrcRect, const Rect *theClipRect, const Color& theColor, int theDrawMode)\n{\n',
    'void D3DInterface::StretchBlt(Image* theImage,  const Rect& theDestRect, const Rect& theSrcRect, const Rect* theClipRect, const Color &theColor, int theDrawMode, bool fastStretch, bool mirror)\n{\n',
    'void D3DInterface::BltRotated(Image* theImage, float theX, float theY, const Rect* theClipRect, const Color& theColor, int theDrawMode, double theRot, float theRotCenterX, float theRotCenterY, const Rect &theSrcRect)\n{\n',
    'void D3DInterface::BltTransformed(Image* theImage, const Rect* theClipRect, const Color& theColor, int theDrawMode, const Rect &theSrcRect, const SexyMatrix3 &theTransform, bool linearFilter, float theX, float theY, bool center)\n{\n',
    'void D3DInterface::DrawLine(double theStartX, double theStartY, double theEndX, double theEndY, const Color& theColor, int theDrawMode)\n{\n',
    'void D3DInterface::FillRect(const Rect& theRect, const Color& theColor, int theDrawMode)\n{\n',
    'void D3DInterface::DrawTriangle(const TriVertex &p1, const TriVertex &p2, const TriVertex &p3, const Color &theColor, int theDrawMode)\n{\n',
    'void D3DInterface::FillPoly(const Point theVertices[], int theNumVertices, const Rect *theClipRect, const Color &theColor, int theDrawMode, int tx, int ty)\n{\n',
    'void D3DInterface::DrawTriangleTex(const TriVertex &p1, const TriVertex &p2, const TriVertex &p3, const Color &theColor, int theDrawMode, Image *theTexture, bool blend)\n{\n',
    'void D3DInterface::DrawTrianglesTex(const TriVertex theVertices[][3], int theNumTriangles, const Color &theColor, int theDrawMode, Image *theTexture, float tx, float ty, bool blend)\n{\n',
    'void D3DInterface::DrawTrianglesTexStrip(const TriVertex theVertices[], int theNumTriangles, const Color &theColor, int theDrawMode, Image *theTexture, float tx, float ty, bool blend)\n{\n',
    'void D3DInterface::SetCurTexture(MemoryImage *theImage)\n{\n',
    'void D3DInterface::PushTransform(const SexyMatrix3 &theTransform, bool concatenate)\n{\n',
    'void D3DInterface::PopTransform()\n{\n',
]
for t in flush_targets:
    name = t.split('(')[0].replace('void D3DInterface::', '')
    rep(t, t + '\tFlushSpriteBatch(mD3DDevice);   // 【性能】换用其它绘制路径前，先提交精灵批次\n',
        'flush-' + name)

# Flush（帧末）与 Cleanup 也要处理
rep('''void D3DInterface::Flush()
{
	if (mSceneBegun)
''',
'''void D3DInterface::Flush()
{
	FlushSpriteBatch(mD3DDevice);   // 【性能】帧末必须提交残留批次

	if (mSceneBegun)
''', 'flush-frame')

rep('''void D3DInterface::Cleanup()
{
''',
'''void D3DInterface::Cleanup()
{
	// 设备状态会被重建，缓存作废
	gCurDrawModeSet = INT_MIN;
	gCurSpriteBatchModeKey = INT_MIN;
	gSpriteBatchCount = 0;
	gSpriteBatchTex = nullptr;

''', 'cleanup-reset')

open(p, 'wb').write(d)
bad = 0
for n, l in enumerate(d.split(b'\n'), 1):
    try:
        l.decode('gbk')
    except Exception:
        bad += 1
        print("BAD", n, repr(l[:80]))
print(p, "gbk bad =", bad)
