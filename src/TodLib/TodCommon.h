#ifndef __TODCOMMON_H__
#define __TODCOMMON_H__

#include "../SexyAppFramework/Common.h"
#include "../SexyAppFramework/ResourceManager.h"

struct TodAllocator;
namespace Sexy
{
	class Graphics;
	class SexyMatrix3;
	class SexyVector2;
};

constexpr const double      PI              =   3.141592653589793;
constexpr const int			BOARD_WIDTH     =   800;
constexpr const int			BOARD_HEIGHT    =   600;

#define DEG_TO_RAD(deg) ((deg) * 0.017453292f)
#define RAD_TO_DEG(rad) ((rad) * 57.29578f)

// #################################################################################################### //

struct TodWeightedArray
{
    int					mItem;
	int					mWeight;
};

struct TodWeightedGridArray
{
	int					mX;
	int					mY;
	int					mWeight;
};

class TodSmoothArray
{
public:
	int					mItem;
	float				mWeight;
	float				mLastPicked;
	float				mSecondLastPicked;
};

int						TodPickFromArray(const int* theArray, int theCount);
int						TodPickFromWeightedArray(TodWeightedArray* theArray, int theCount);
TodWeightedArray*		TodPickArrayItemFromWeightedArray(TodWeightedArray* theArray, int theCount);
TodWeightedGridArray*	TodPickFromWeightedGridArray(TodWeightedGridArray* theArray, int theCount);
float					TodCalcSmoothWeight(float aWeight, float aLastPicked, float aSecondLastPicked);
void					TodUpdateSmoothArrayPick(TodSmoothArray* theArray, int theCount, int thePickIndex);
int						TodPickFromSmoothArray(TodSmoothArray* theArray, int theCount);

// #################################################################################################### //

class TodResourceManager : public Sexy::ResourceManager
{
public:
	bool				FindImagePath(Sexy::Image* theImage, std::string* thePath);
	void				AddImageToMap(Sexy::SharedImageRef* theImage, const std::string& thePath);
	bool				TodLoadNextResource();
	bool				TodLoadResources(const std::string& theGroup);
};

bool					TodLoadResources(const std::string& theGroup);
bool					TodLoadNextResource();
void					TodAddImageToMap(Sexy::SharedImageRef* theImage, const std::string& thePath);
bool					TodFindImagePath(Sexy::Image* theImage, std::string* thePath);

// #################################################################################################### //

enum TodCurves
{
    CURVE_CONSTANT,             // 常函数曲线
    CURVE_LINEAR,               // 线性曲线
    CURVE_EASE_IN,              // 二次曲线（缓入）
    CURVE_EASE_OUT,             // 二次曲线（缓出）
    CURVE_EASE_IN_OUT,          // 缓入缓出曲线
    CURVE_EASE_IN_OUT_WEAK,     // 缓入缓出曲线（效果减弱）
    CURVE_FAST_IN_OUT,          // 快入快出曲线
    CURVE_FAST_IN_OUT_WEAK,     // 快入快出曲线（效果减弱）
    CURVE_WEAK_FAST_IN_OUT,     // 【废弃】弱快入快出曲线
    CURVE_BOUNCE,               // 弹跳效果曲线
    CURVE_BOUNCE_FAST_MIDDLE,   // 弹跳效果曲线（尖形）
    CURVE_BOUNCE_SLOW_MIDDLE,   // 弹跳效果曲线（罩形）
    CURVE_SIN_WAVE,             // 正弦曲线
    CURVE_EASE_SIN_WAVE         // 缓入缓出的正弦曲线
};

float					TodCurveQuad(float theTime);
float					TodCurveInvQuad(float theTime);
float					TodCurveS(float theTime);
float					TodCurveInvQuadS(float theTime);
float					TodCurveBounce(float theTime);
float					TodCurveQuadS(float theTime);
float					TodCurveCubic(float theTime);
float					TodCurveInvCubic(float theTime);
float					TodCurveCubicS(float theTime);
float					TodCurvePoly(float theTime, float thePoly);
float					TodCurveInvPoly(float theTime, float thePoly);
float					TodCurvePolyS(float theTime, float thePoly);
float					TodCurveCircle(float theTime);
float					TodCurveInvCircle(float theTime);
float					TodCurveEvaluate(float theTime, float thePositionStart, float thePositionEnd, TodCurves theCurve);
float					TodCurveEvaluateClamped(float theTime, float thePositionStart, float thePositionEnd, TodCurves theCurve);
float					TodAnimateCurveFloatTime(float theTimeStart, float theTimeEnd, float theTimeAge, float thePositionStart, float thePositionEnd, TodCurves theCurve);
float					TodAnimateCurveFloat(int theTimeStart, int theTimeEnd, int theTimeAge, float thePositionStart, float thePositionEnd, TodCurves theCurve);
int						TodAnimateCurve(int theTimeStart, int theTimeEnd, int theTimeAge, int thePositionStart, int thePositionEnd, TodCurves theCurve);

void					TodScaleTransformMatrix(Sexy::SexyMatrix3& m, float x, float y, float theScaleX, float theScaleY);
void					TodScaleRotateTransformMatrix(Sexy::SexyMatrix3& m, float x, float y, float rad, float theScaleX, float theScaleY);
void					SexyMatrix3ExtractScale(const Sexy::SexyMatrix3& m, float& theScaleX, float& theScaleY);
void					SexyMatrix3Translation(Sexy::SexyMatrix3& m, float x, float y);
void					SexyMatrix3Transpose(const Sexy::SexyMatrix3& m, Sexy::SexyMatrix3& r);  // r = m ^ T
void					SexyMatrix3Inverse(const Sexy::SexyMatrix3& m, Sexy::SexyMatrix3& r);  // r = m ^ -1
void					SexyMatrix3Multiply(Sexy::SexyMatrix3& m, const Sexy::SexyMatrix3& l, const Sexy::SexyMatrix3& r);  // m = l × r
bool					TodIsPointInPolygon(const Sexy::SexyVector2* thePolygonPoint, int theNumberPolygonPoints, const Sexy::SexyVector2& theCheckPoint);

// #################################################################################################### //

enum DrawStringJustification
{
    DS_ALIGN_LEFT,
    DS_ALIGN_RIGHT,
    DS_ALIGN_CENTER,
    DS_ALIGN_LEFT_VERTICAL_MIDDLE,
    DS_ALIGN_RIGHT_VERTICAL_MIDDLE,
    DS_ALIGN_CENTER_VERTICAL_MIDDLE
};

void					TodDrawString(Sexy::Graphics* g, const SexyString& theText, int thePosX, int thePosY, Sexy::Font* theFont, const Sexy::Color& theColor, DrawStringJustification theJustification);
void					TodDrawStringMatrix(Sexy::Graphics* g, const Sexy::Font* theFont, const Sexy::SexyMatrix3& theMatrix, const SexyString& theString, const Sexy::Color& theColor);
void					TodDrawImageScaledF(Sexy::Graphics* g, Sexy::Image* theImage, float thePosX, float thePosY, float theScaleX, float theScaleY);
void					TodDrawImageCenterScaledF(Sexy::Graphics* g, Sexy::Image* theImage, float thePosX, float thePosY, float theScaleX, float theScaleY);
void					TodDrawImageCelF(Sexy::Graphics* g, Sexy::Image* theImageStrip, float thePosX, float thePosY, int theCelCol, int theCelRow);
void					TodDrawImageCelScaled(Sexy::Graphics* g, Sexy::Image* theImageStrip, int thePosX, int thePosY, int theCelCol, int theCelRow, float theScaleX, float theScaleY);
void					TodDrawImageCelScaledF(Sexy::Graphics* g, Sexy::Image* theImageStrip, float thePosX, float thePosY, int theCelCol, int theCelRow, float theScaleX, float theScaleY);
void					TodDrawImageCelCenterScaledF(Sexy::Graphics* g, Sexy::Image* theImageStrip, float thePosX, float thePosY, int theCelCol, float theScaleX, float theScaleY);
void					TodBltMatrix(Sexy::Graphics* g, Sexy::Image* theImage, const Sexy::SexyMatrix3& theTransform, const Sexy::Rect& theClipRect, const Sexy::Color& theColor, int theDrawMode, const Sexy::Rect& theSrcRect);
void					TodMarkImageForSanding(Sexy::Image* theImage);
void					TodSandImageIfNeeded(Sexy::Image* theImage);
void					FixPixelsOnAlphaEdgeForBlending(Sexy::Image* theImage);
unsigned long			AverageNearByPixels(Sexy::MemoryImage* theImage, unsigned long* thePixel, int x, int y);
void					Tod_SWTri_AddAllDrawTriFuncs();

SexyString				TodReplaceString(const SexyString& theText, const SexyChar* theStringToFind, const SexyString& theStringToSubstitute);
SexyString				TodReplaceNumberString(const SexyString& theText, const SexyChar* theStringToFind, int theNumber);
int						TodSnprintf(char* theBuffer, int theSize, const char* theFormat, ...);
int						TodVsnprintf(char* theBuffer, int theSize, const char* theFormat, va_list theArgList);

TodAllocator*			FindGlobalAllocator(int theSize);
void                    FreeGlobalAllocators();

SexyString				TodGetCurrentLevelName();
bool					TodHasUsedCheatKeys();
bool					TodAppCloseRequest();

// #################################################################################################### //

int						RandRangeInt(int theMin, int theMax);
float					RandRangeFloat(float theMin, float theMax);
inline char				ClampByte(char theNum, char theMin, char theMax)			{ return theNum <= theMin ? theMin : theNum >= theMax ? theMax : theNum; }
inline int				ClampInt(int theNum, int theMin, int theMax)				{ return theNum <= theMin ? theMin : theNum >= theMax ? theMax : theNum; }
inline float			ClampFloat(float theNum, float theMin, float theMax)		{ return theNum <= theMin ? theMin : theNum >= theMax ? theMax : theNum; }
inline float			Distance2D(float x1, float y1, float x2, float y2)			{ return sqrtf((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1)); }
inline float			FloatLerp(float theStart, float theEnd, float theFactor)	{ return theStart + theFactor * (theEnd - theStart); }
inline int				FloatRoundToInt(float theFloatValue)						{ return theFloatValue > 0.0f ? (theFloatValue + 0.5f) : (theFloatValue - 0.5f); }
inline bool				FloatApproxEqual(float theFloatVal1, float theFloatVal2)	{ return fabsf(theFloatVal1 - theFloatVal2) < FLT_EPSILON; }

Sexy::Color				GetFlashingColor(int theCounter, int theFlashTime);
int						ColorComponentMultiply(int theColor1, int theColor2);
Sexy::Color				ColorsMultiply(const Sexy::Color& theColor1, const Sexy::Color& theColor2);
Sexy::Color				ColorAdd(const Sexy::Color& theColor1, const Sexy::Color& theColor2);

inline void				SetBit(uint& theNum, int theIdx, bool theValue = true)		{ if (theValue) theNum |= 1 << theIdx; else theNum &= ~(1 << theIdx); }
inline bool				TestBit(uint theNum, int theIdx)							{ return theNum & (1 << theIdx); }

// #################################################################################################### //

extern bool				(*gAppCloseRequest)();				//[0x69E6A0]
extern bool				(*gAppHasUsedCheatKeys)();			//[0x69E6A4]
extern SexyString		(*gGetCurrentLevelName)();
extern bool				(*gExtractResourcesByName)(Sexy::ResourceManager* theResourceManager, const char* theName);

#endif
