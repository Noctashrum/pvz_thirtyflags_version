#ifndef __REANIMATORCACHE_H__
#define __REANIMATORCACHE_H__

#include "SeedPacket.h"
#include "Zombie.h"
#include "LawnMower.h"
#include "../TodLib/TodList.h"

class LawnApp;
namespace Sexy
{
	class Graphics;
	class MemoryImage;
};

enum DrawVariation
{
	VARIATION_NORMAL,
	VARIATION_IMITATER,
	VARIATION_MARIGOLD_WHITE,
	VARIATION_MARIGOLD_MAGENTA,
	VARIATION_MARIGOLD_ORANGE,
	VARIATION_MARIGOLD_PINK,
	VARIATION_MARIGOLD_LIGHT_BLUE,
	VARIATION_MARIGOLD_RED,
	VARIATION_MARIGOLD_BLUE,
	VARIATION_MARIGOLD_VIOLET,
	VARIATION_MARIGOLD_LAVENDER,
	VARIATION_MARIGOLD_YELLOW,
	VARIATION_MARIGOLD_LIGHT_GREEN,
	VARIATION_ZEN_GARDEN,
	VARIATION_ZEN_GARDEN_WATER,
	VARIATION_SPROUT_NO_FLOWER,
	VARIATION_IMITATER_LESS,
	VARIATION_AQUARIUM
};

class ReanimCacheImageVariation
{
public:
	SeedType				mSeedType;
	DrawVariation			mDrawVariation;
	Sexy::MemoryImage*		mImage;
};
typedef TodList<ReanimCacheImageVariation> ImageVariationList;

class Reanimation;
class ReanimatorCache
{
public:
	Sexy::MemoryImage*		mPlantImages[SeedType::NUM_SEED_TYPES];
	ImageVariationList		mImageVariationList;
	Sexy::MemoryImage*		mLawnMowers[LawnMowerType::NUM_MOWER_TYPES];
	Sexy::MemoryImage*		mZombieImages[ZombieType::NUM_CACHED_ZOMBIE_TYPES];
	LawnApp*				mApp;

public:
	void					ReanimatorCacheInitialize();
	void					ReanimatorCacheDispose();
	void					DrawCachedPlant(Sexy::Graphics* g, float thePosX, float thePosY, SeedType theSeedType, DrawVariation theDrawVariation);
	void					DrawCachedMower(Sexy::Graphics* g, float thePosX, float thePosY, LawnMowerType theMowerType);
	void					DrawCachedZombie(Sexy::Graphics* g, float thePosX, float thePosY, ZombieType theZombieType);
	Sexy::MemoryImage*		MakeBlankMemoryImage(int theWidth, int theHeight);
	Sexy::MemoryImage*		MakeCachedPlantFrame(SeedType theSeedType, DrawVariation theDrawVariation);
	Sexy::MemoryImage*		MakeCachedMowerFrame(LawnMowerType theMowerType);
	Sexy::MemoryImage*		MakeCachedZombieFrame(ZombieType theZombieType);
	void					GetPlantImageSize(SeedType theSeedType, int& theOffsetX, int& theOffsetY, int& theWidth, int& theHeight);
	void					DrawReanimatorFrame(Sexy::Graphics* g, float thePosX, float thePosY, ReanimationType theReanimationType, const char* theTrackName, DrawVariation theDrawVariation);
	void					UpdateReanimationForVariation(Reanimation* theReanim, DrawVariation theDrawVariation);
};

#endif
