#ifndef __ALMANACDIALOG_H__
#define __ALMANACDIALOG_H__

#include "LawnDialog.h"

#define NUM_ALMANAC_SEEDS 49
#define NUM_ALMANAC_ZOMBIES 26

class Plant;
class Zombie;
class LawnApp;
class GameButton;
class Reanimation;

enum AlmanacPage
{
	ALMANAC_PAGE_INDEX,
	ALMANAC_PAGE_PLANTS,
	ALMANAC_PAGE_ZOMBIES
};

class AlmanacDialog : public LawnDialog
{
private:
	enum
	{
		ALMANAC_BUTTON_CLOSE = 0,
		ALMANAC_BUTTON_PLANT = 1,
		ALMANAC_BUTTON_ZOMBIE = 2,
		ALMANAC_BUTTON_INDEX = 3
	};

public:
	LawnApp*					mApp;						//+0x16C
	GameButton*					mCloseButton;				//+0x170
	GameButton*					mIndexButton;				//+0x174
	GameButton*					mPlantButton;				//+0x178
	GameButton*					mZombieButton;				//+0x17C
	AlmanacPage					mOpenPage;					//+0x180
	Reanimation*				mReanim[4];					//+0x184
	SeedType					mSelectedSeed;				//+0x194
	ZombieType					mSelectedZombie;			//+0x198
	Plant*						mPlant;						//+0x19C
	Zombie*						mZombie;					//+0x1A0
	Zombie*						mZombiePerfTest[400];		//+0x1A4
	
public:
	AlmanacDialog(LawnApp* theApp);
	virtual ~AlmanacDialog();

	void						ClearPlantsAndZombies();
	virtual void				RemovedFromManager(Sexy::WidgetManager* theWidgetManager);
	void						SetupPlant();
	void						SetupZombie();
	void						SetPage(AlmanacPage thePage);
	virtual void				Update();
	void						DrawIndex(Sexy::Graphics* g);
	void						DrawPlants(Sexy::Graphics* g);
	void						DrawZombies(Sexy::Graphics* g);
	virtual void				Draw(Sexy::Graphics* g);
	void						GetSeedPosition(SeedType theSeedType, int& x, int& y);
	SeedType					SeedHitTest(int x, int y);
	bool						ZombieHasSilhouette(ZombieType theZombieType);
	bool						ZombieIsShown(ZombieType theZombieType);
	bool						ZombieHasDescription(ZombieType theZombieType);
	void						GetZombiePosition(ZombieType theZombieType, int& x, int& y);
	ZombieType					ZombieHitTest(int x, int y);
	virtual void				MouseUp(int x, int y, int theClickCount);
	virtual void				MouseDown(int x, int y, int theClickCount);
	virtual void				KeyChar(SexyChar theChar) {  }

	static ZombieType			GetZombieType(int theIndex);
	void						ShowPlant(SeedType theSeedType);
	void						ShowZombie(ZombieType theZombieType);
};
extern bool gZombieDefeated[NUM_ZOMBIE_TYPES];

void							AlmanacInitForPlayer();
void							AlmanacPlayerDefeatedZombie(ZombieType theZombieType);

#endif
