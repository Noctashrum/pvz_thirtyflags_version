#ifndef __GAMEOBJECT_H__
#define __GAMEOBJECT_H__

#include "LawnCommon.h"

class Board;
class LawnApp;

enum RenderLayer
{
    RENDER_LAYER_ROW_OFFSET     = 10000,
    RENDER_LAYER_UI_BOTTOM      = 100000,
    RENDER_LAYER_GROUND         = 200000,
    RENDER_LAYER_LAWN           = 300000,
    RENDER_LAYER_GRAVE_STONE    = 301000,
    RENDER_LAYER_PLANT          = 302000,
    RENDER_LAYER_ZOMBIE         = 303000,
    RENDER_LAYER_BOSS           = 304000,
    RENDER_LAYER_PROJECTILE     = 305000,
    RENDER_LAYER_LAWN_MOWER     = 306000,
    RENDER_LAYER_PARTICLE       = 307000,
    RENDER_LAYER_TOP            = 400000,
    RENDER_LAYER_FOG            = 500000,
    RENDER_LAYER_COIN_BANK      = 600000,
    RENDER_LAYER_UI_TOP         = 700000,
    RENDER_LAYER_ABOVE_UI       = 800000,
    RENDER_LAYER_SCREEN_FADE    = 900000
};

enum GraveStoneLayers
{
    GRAVESTONE_LAYER_BRAIN,
    GRAVESTONE_LAYER_SQUIRREL,
    GRAVESTONE_LAYER_DOOR2,
    GRAVESTONE_LAYER_STONES,
    GRAVESTONE_LAYER_DOOR1,
    GRAVESTONE_LAYER_DAISIES,
    GRAVESTONE_LAYER_BOSS_FIREBALL,
    GRAVESTONE_LAYER_BUNGEE,
    GRAVESTONE_LAYER_SQUISHED_PLANT,
    GRAVESTONE_LAYER_RAKE
};

enum FogLayers
{
    FOG_LAYER_FOG,
    FOG_LAYER_PLANTERN_SHINE,
    FOG_LAYER_COIN,
    FOG_LAYER_RAIN
};

enum
{
    GROUND_LAYER_POOL_SPARKLE,
    GROUND_LAYER_CRATER,
    GROUND_LAYER_ICE,
    GROUND_LAYER_SHADOW
};

class GameObject
{
public:
	LawnApp*					mApp;
	Board*						mBoard;
	int							mX;
	int							mY;
	int							mWidth;
	int							mHeight;
	bool						mVisible;
	int							mRow;
	int							mRenderOrder;

public:
	GameObject();

	bool						BeginDraw(Sexy::Graphics* g);
	void						EndDraw(Sexy::Graphics* g);
	void						MakeParentGraphicsFrame(Sexy::Graphics* g);
};

#endif

