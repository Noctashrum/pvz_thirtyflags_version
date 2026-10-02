#ifndef __POOLEFFECT_H__
#define __POOLEFFECT_H__

class LawnApp;
namespace Sexy
{
	class MemoryImage;
	class Graphics;
};

class PoolEffect
{
public:
	unsigned char*			mCausticGrayscaleImage;
	Sexy::MemoryImage*		mCausticImage;
	LawnApp*				mApp;
	int						mPoolCounter;

public:
	void					PoolEffectInitialize();
	void					PoolEffectDispose();
	void					PoolEffectDraw(Sexy::Graphics* g, bool theIsNight);
	void					UpdateWaterEffect(Sexy::Graphics* g);
	unsigned char			BilinearLookupFixedPoint(unsigned int u, unsigned int v);
	unsigned char			BilinearLookup(float u, float v);
	void					PoolEffectUpdate();
};

#endif
