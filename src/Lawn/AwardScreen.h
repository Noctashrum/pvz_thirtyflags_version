#ifndef __AWARDSCREEN_H__
#define __AWARDSCREEN_H__

#include "../SexyAppFramework/Widget.h"

class LawnApp;
class GameButton;

enum AwardType
{
	AWARD_FORLEVEL,
	AWARD_CREDITS_ZOMBIENOTE,
	AWARD_HELP_ZOMBIENOTE,
	AWARD_ACHIEVEMENTONLY,
	AWARD_PRECREDITS_ZOMBIENOTE,
};

class AwardScreen : public Sexy::Widget
{
private:
	enum
	{
		AwardScreen_Start = 100,
		AwardScreen_Menu = 101
	};

public:
	GameButton*			mStartButton;		//+0x88
	GameButton*			mMenuButton;		//+0x8C
	LawnApp*			mApp;				//+0x90
	int					mFadeInCounter;		//+0x94
	AwardType			mAwardType;			//+0x98

public:
	AwardScreen(LawnApp* theApp, AwardType theAwardType);
	virtual ~AwardScreen();

	bool				IsPaperNote();
	virtual void		Resize(int theX, int theY, int theWidth, int theHeight) { Sexy::Widget::Resize(theX, theY, theWidth, theHeight); }
	static void			DrawBottom(Sexy::Graphics* g, const SexyString& theTitle, const SexyString& theAward, const SexyString& theMessage);
	void				DrawAwardSeed(Sexy::Graphics* g);
	virtual void		Draw(Sexy::Graphics* g);
	virtual void		Update();
	virtual void		AddedToManager(Sexy::WidgetManager* theWidgetManager) { Sexy::Widget::AddedToManager(theWidgetManager); }
	virtual void		RemovedFromManager(Sexy::WidgetManager* theWidgetManager) { Sexy::Widget::RemovedFromManager(theWidgetManager); }
	virtual void		KeyChar(SexyChar theChar);
	void				StartButtonPressed();
	virtual void		MouseDown(int x, int y, int theClickCount);
	virtual void		MouseUp(int x, int y, int theClickCount);
};

#endif
