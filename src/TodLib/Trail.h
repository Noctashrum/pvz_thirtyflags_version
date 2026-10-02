#ifndef __TRAIL_H__
#define __TRAIL_H__

#include "DataArray.h"
#include "Definition.h"
#include "../SexyAppFramework/Color.h"
#include "../SexyAppFramework/SexyVector.h"

namespace Sexy
{
	class Graphics;
}

#define MAX_TRAIL_TRIANGLES		38
#define MAX_TRAIL_POINTS		20

enum TrailTracks
{
	TRACK_WIDTH_OVER_LENGTH,
	TRACK_WIDTH_OVER_TIME,
	TRACK_ALPHA_OVER_LENGTH,
	TRACK_ALPHA_OVER_TIME,
	NUM_TRAIL_TRACKS
};

enum TrailFlags
{
	TRAIL_FLAG_LOOPS
};

enum TrailType
{
	TRAIL_NONE = -1,
	TRAIL_ICE,
	NUM_TRAILS
};

class TrailParams
{
public:
	TrailType						mTrailType;
	const char*						mTrailFileName;
};

extern int gTrailParamArraySize;  //[0x6A9F28]
extern TrailParams* gTrailParamArray;  //[0x6A9F2C]

class TrailDefinition
{
public:
	Sexy::Image*					mImage;
	int								mMaxPoints;
	float							mMinPointDistance;
	int								mTrailFlags;
	FloatParameterTrack				mTrailDuration;
	FloatParameterTrack				mWidthOverLength;
	FloatParameterTrack				mWidthOverTime;
	FloatParameterTrack				mAlphaOverLength;
	FloatParameterTrack				mAlphaOverTime;

public:
	TrailDefinition();
	~TrailDefinition();
};
bool								TrailLoadADef(TrailDefinition* theTrailDef, const char* theTrailFileName);
void								TrailLoadDefinitions(TrailParams* theTrailParamArray, int theTrailParamArraySize);
void								TrailFreeDefinitions();

extern int gTrailDefCount;  //[0x6A9F20]
extern TrailDefinition* gTrailDefArray;  //[0x6A9F24]

void* __cdecl						TrailDefinitionConstructor(void* thePointer);       //0x51B7F0

extern DefMap gTrailDefMap;  //0x69D98C

// #################################################################################################### //

class TrailPoint
{
public:
	Sexy::SexyVector2				aPos;

public:
	TrailPoint();
};

class TrailHolder;

class Trail
{
public:
	TrailPoint						mTrailPoints[MAX_TRAIL_POINTS];					//+0x0
	int								mNumTrailPoints;								//+0xA0
	bool							mDead;											//+0xA4
	int								mRenderOrder;									//+0xA8
	int								mTrailAge;										//+0xAC
	int								mTrailDuration;									//+0xB0
	TrailDefinition*				mDefinition;									//+0xB4
	TrailHolder*					mTrailHolder;									//+0xB8
	float							mTrailInterp[TrailTracks::NUM_TRAIL_TRACKS];	//+0xBC
	Sexy::SexyVector2				mTrailCenter;									//+0xCC
	bool							mIsAttachment;									//+0xD4
	Sexy::Color						mColorOverride;									//+0xD8

public:
	Trail();

	void							Update();
	void							Draw(Sexy::Graphics* g);
	void							AddPoint(float x, float y);
	bool							GetNormalAtPoint(int nIndex, Sexy::SexyVector2& theNormal);
};

class TrailHolder
{
public:
	DataArray<Trail>				mTrails;

public:
	TrailHolder();
	~TrailHolder();

	void							InitializeHolder();
	void							DisposeHolder();
	Trail*							AllocTrail(int theRenderOrder, TrailType theTrailType);
	Trail*							AllocTrailFromDef(int theRenderOrder, TrailDefinition* theDefinition);
};

#endif
