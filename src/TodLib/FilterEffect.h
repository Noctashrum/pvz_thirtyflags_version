#ifndef __FILTEREFFECT_H__
#define __FILTEREFFECT_H__

#include <map>

namespace Sexy
{
	class Image;
	class MemoryImage;
}

enum FilterEffect
{
	FILTER_EFFECT_NONE = -1,
	FILTER_EFFECT_WASHED_OUT,
	FILTER_EFFECT_LESS_WASHED_OUT,
	FILTER_EFFECT_WHITE,
	NUM_FILTER_EFFECTS
};

typedef std::map<Sexy::Image*, Sexy::Image*> ImageFilterMap;
extern ImageFilterMap gFilterMap[FilterEffect::NUM_FILTER_EFFECTS];

void						FilterEffectInitForApp();
void						FilterEffectDisposeForApp();
void						FilterEffectDoLumSat(Sexy::MemoryImage* theImage, float theLum, float theSat);
void						FilterEffectDoWashedOut(Sexy::MemoryImage* theImage);
void						FilterEffectDoLessWashedOut(Sexy::MemoryImage* theImage);
void						FilterEffectDoWhite(Sexy::MemoryImage* theImage);
Sexy::MemoryImage*			FilterEffectCreateImage(Sexy::Image* theImage, FilterEffect theFilterEffect);
Sexy::Image*				FilterEffectGetImage(Sexy::Image* theImage, FilterEffect theFilterEffect);

#endif
