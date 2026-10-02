#ifndef __REANIMATLAS_H__
#define __REANIMATLAS_H__

#include "../SexyAppFramework/Rect.h"

#define MAX_REANIM_IMAGES 64

class ReanimatorDefinition;
namespace Sexy
{
	class Image;
	class MemoryImage;
};

class ReanimAtlasImage
{
public:
	int								mX;
	int								mY;
	int								mWidth;
	int								mHeight;
	Sexy::Image*					mOriginalImage;

public:
	ReanimAtlasImage();
};

class ReanimAtlas
{
public:
	ReanimAtlasImage				mImageArray[MAX_REANIM_IMAGES];		//+0x0
	int								mImageCount;						//+0x500
	Sexy::MemoryImage*				mMemoryImage;						//+0x504

public:
	ReanimAtlas();

	void							ReanimAtlasCreate(ReanimatorDefinition* theReanimDef);
	void							ReanimAtlasDispose();
	void							AddImage(Sexy::Image* theImage);
	int								FindImage(Sexy::Image* theImage);
	bool							ImageFits(int theImageCount, const Sexy::Rect& rectTest, int theMaxWidth);
	bool							ImageFindPlaceOnSide(ReanimAtlasImage* theAtlasImageToPlace, int theImageCount, int theMaxWidth, bool theToRight);
	bool							ImageFindPlace(ReanimAtlasImage* theAtlasImageToPlace, int theImageCount, int theMaxWidth);
	bool							PlaceAtlasImage(ReanimAtlasImage* theAtlasImageToPlace, int theImageCount, int theMaxWidth);
	int								PickAtlasWidth();
	void							ArrangeImages(int& theAtlasWidth, int& theAtlasHeight);
	ReanimAtlasImage*				GetEncodedReanimAtlas(Sexy::Image* theImage);
};

Sexy::MemoryImage*					ReanimAtlasMakeBlankMemoryImage(int theWidth, int theHeight);

#endif
