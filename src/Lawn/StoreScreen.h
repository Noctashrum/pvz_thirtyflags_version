#ifndef __STORESCREEN_H__
#define __STORESCREEN_H__

#include "Coin.h"
#include "PlayerInfo.h"
#include "../TodLib/DataArray.h"
#include "../SexyAppFramework/Dialog.h"

#define MAX_PAGE_SPOTS 8

class NewLawnButton;

enum StoreItem
{
	STORE_ITEM_PLANT_GATLINGPEA,
	STORE_ITEM_PLANT_TWINSUNFLOWER,
	STORE_ITEM_PLANT_GLOOMSHROOM,
	STORE_ITEM_PLANT_CATTAIL,
	STORE_ITEM_PLANT_WINTERMELON,
	STORE_ITEM_PLANT_GOLD_MAGNET,
	STORE_ITEM_PLANT_SPIKEROCK,
	STORE_ITEM_PLANT_COBCANNON,
	STORE_ITEM_PLANT_IMITATER,
	STORE_ITEM_BONUS_LAWN_MOWER,
	STORE_ITEM_POTTED_MARIGOLD_1,
	STORE_ITEM_POTTED_MARIGOLD_2,
	STORE_ITEM_POTTED_MARIGOLD_3,
	STORE_ITEM_GOLD_WATERINGCAN,
	STORE_ITEM_FERTILIZER,
	STORE_ITEM_BUG_SPRAY,
	STORE_ITEM_PHONOGRAPH,
	STORE_ITEM_GARDENING_GLOVE,
	STORE_ITEM_MUSHROOM_GARDEN,
	STORE_ITEM_WHEEL_BARROW,
	STORE_ITEM_STINKY_THE_SNAIL,
	STORE_ITEM_PACKET_UPGRADE,
	STORE_ITEM_POOL_CLEANER,
	STORE_ITEM_ROOF_CLEANER,
	STORE_ITEM_RAKE,
	STORE_ITEM_AQUARIUM_GARDEN,
	STORE_ITEM_CHOCOLATE,
	STORE_ITEM_TREE_OF_WISDOM,
	STORE_ITEM_TREE_FOOD,
	STORE_ITEM_FIRSTAID,
	STORE_ITEM_PVZ,
	STORE_ITEM_INVALID = -1
};

enum StorePages
{
	STORE_PAGE_SLOT_UPGRADES,
	STORE_PAGE_PLANT_UPGRADES,
	STORE_PAGE_ZEN1,
	STORE_PAGE_ZEN2,
	NUM_STORE_PAGES
};

class StoreScreen : public Sexy::Dialog
{
private:
	enum
	{
		StoreScreen_Back = 100,
		StoreScreen_Prev = 101,
		StoreScreen_Next = 102
	};

public:
	LawnApp*					mApp;							//+0x150
	NewLawnButton*				mBackButton;					//+0x154
	NewLawnButton*				mPrevButton;					//+0x158
	NewLawnButton*				mNextButton;					//+0x15C
	Sexy::Widget*				mOverlayWidget;					//+0x160
	int							mStoreTime;						//+0x164
	SexyString					mBubbleText;					//+0x168
	int							mBubbleCountDown;				//+0x184
	bool						mBubbleClickToContinue;			//+0x188
	int							mAmbientSpeechCountDown;		//+0x18C
	int							mPreviousAmbientSpeechIndex;	//+0x190
	StorePages					mPage;							//+0x194
	StoreItem					mMouseOverItem;					//+0x198
	int							mHatchTimer;					//+0x19C
	bool						mHatchOpen;						//+0x1A0
	int							mShakeX;						//+0x1A4
	int							mShakeY;						//+0x1A8
	int							mStartDialog;					//+0x1AC
	bool						mEasyBuyingCheat;				//+0x1B0
	bool						mWaitForDialog;					//+0x1B1
	PottedPlant					mPottedPlantSpecs;				//+0x1B8
	DataArray<Coin>				mCoins;							//+0x210
	bool						mDrawnOnce;						//+0x22C
	bool						mGoToTreeNow;					//+0x22D
	bool						mPurchasedFullVersion;			//+0x22E
	bool						mTrialLockedWhenStoreOpened;	//+0x22F

public:
	StoreScreen(LawnApp* theApp);
	virtual ~StoreScreen();

	StoreItem					GetStoreItemType(int theSpotIndex);
	bool						IsFullVersionOnly(StoreItem theStoreItem);
	static bool					IsPottedPlant(StoreItem theStoreItem);
	bool						IsComingSoon(StoreItem theStoreItem);
	bool						IsItemSoldOut(StoreItem theStoreItem);
	bool						IsItemUnavailable(StoreItem theStoreItem);
	static void					GetStorePosition(int theSpotIndex, int& thePosX, int& thePosY);
	void						DrawItemIcon(Sexy::Graphics* g, int theItemPosition, StoreItem theItemType, bool theIsForHighlight);
	void						DrawItem(Sexy::Graphics* g, int theItemPosition, StoreItem theItemType);
	virtual void				Draw(Sexy::Graphics* g);
	virtual void				DrawOverlay(Sexy::Graphics* g);
	void						SetBubbleText(int theCrazyDaveMessage, int theTime, bool theClickToContinue);
	void						UpdateMouse();
	void						StorePreload();
	bool						CanInteractWithButtons();
	virtual void				Update();
	virtual void				AddedToManager(Sexy::WidgetManager* theWidgetManager);
	virtual void				RemovedFromManager(Sexy::WidgetManager* theWidgetManager);
	virtual void				ButtonPress(int theId);
	bool						IsPageShown(StorePages thePage);
	virtual void				ButtonDepress(int theId);
	virtual void				KeyChar(SexyChar theChar);
	static int					GetItemCost(StoreItem theStoreItem);
	bool						CanAffordItem(StoreItem theStoreItem);
	void						PurchaseItem(StoreItem theStoreItem);
	void						AdvanceCrazyDaveDialog();
	virtual void				MouseDown(int x, int y, int theClickCount);
	void						EnableButtons(bool theEnable);
	void						SetupForIntro(int theDialogIndex);
};

class StoreScreenOverlay : public Sexy::Widget
{
public:
	StoreScreen*				mParent;

public:
	StoreScreenOverlay(StoreScreen* theParent);
	virtual void				Draw(Sexy::Graphics* g);
};


#endif
