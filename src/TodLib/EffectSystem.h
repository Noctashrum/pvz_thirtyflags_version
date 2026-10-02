#ifndef __EFFECTSYSTEM_H__
#define __EFFECTSYSTEM_H__

#include "DataArray.h"
#include "../SexyAppFramework/Graphics.h"

#define MAX_TRIANGLES 256

class TodTriVertex
{
public:
	float						x;
	float						y;
	float						u;
	float						v;
	unsigned long				color;
};

class TodTriangleGroup
{
public:
	Sexy::Image*				mImage;
	Sexy::TriVertex				mVertArray[MAX_TRIANGLES][3];
	int							mTriangleCount;
	int							mDrawMode;

	TodTriangleGroup();
	void						DrawGroup(Sexy::Graphics* g);
	void						AddTriangle(Sexy::Graphics* g, Sexy::Image* theImage, const Sexy::SexyMatrix3& theMatrix, const Sexy::Rect& theClipRect, const Sexy::Color& theColor, int theDrawMode, const Sexy::Rect& theSrcRect);
};

class TodParticleHolder;
class TrailHolder;
class ReanimationHolder;
class AttachmentHolder;
class EffectSystem
{
public:
	TodParticleHolder*			mParticleHolder;
	TrailHolder*				mTrailHolder;
	ReanimationHolder*			mReanimationHolder;
	AttachmentHolder*			mAttachmentHolder;

public:
	EffectSystem();
	~EffectSystem();

	void						EffectSystemInitialize();
	void						EffectSystemDispose();
	void						EffectSystemFreeAll();
	void						ProcessDeleteQueue();
	void						Update();
};
extern EffectSystem* gEffectSystem;  //[0x6A9EB8]

#endif
