#ifndef __MESSAGEWIDGET_H__
#define __MESSAGEWIDGET_H__

#include "LawnCommon.h"

#define MAX_MESSAGE_LENGTH 128
#define MAX_REANIM_LINES 5

class LawnApp;
namespace Sexy
{
	class Font;
	class Graphics;
}
enum ReanimationID;
enum ReanimationType;

enum MessageStyle
{
	MESSAGE_STYLE_OFF,
	MESSAGE_STYLE_TUTORIAL_LEVEL1,
	MESSAGE_STYLE_TUTORIAL_LEVEL1_STAY,
	MESSAGE_STYLE_TUTORIAL_LEVEL2,
	MESSAGE_STYLE_TUTORIAL_LATER,
	MESSAGE_STYLE_TUTORIAL_LATER_STAY,
	MESSAGE_STYLE_HINT_LONG,
	MESSAGE_STYLE_HINT_FAST,
	MESSAGE_STYLE_HINT_STAY,
	MESSAGE_STYLE_HINT_TALL_FAST,
	MESSAGE_STYLE_HINT_TALL_UNLOCKMESSAGE,
	//MESSAGE_STYLE_HINT_TALL_8SECONDS,// ½öÄÚ²â°æ
	MESSAGE_STYLE_HINT_TALL_LONG,
	MESSAGE_STYLE_BIG_MIDDLE,
	MESSAGE_STYLE_BIG_MIDDLE_FAST,
	MESSAGE_STYLE_HOUSE_NAME,
	MESSAGE_STYLE_HUGE_WAVE,
	MESSAGE_STYLE_SLOT_MACHINE,
	MESSAGE_STYLE_ZEN_GARDEN_LONG
};

class MessageWidget
{
public:
	LawnApp*			mApp;									//+0x0
	SexyChar			mLabel[MAX_MESSAGE_LENGTH];				//+0x4
	int					mDisplayTime;							//+0x84
	int					mDuration;								//+0x88
	MessageStyle		mMessageStyle;							//+0x8C
	ReanimationID		mTextReanimID[MAX_MESSAGE_LENGTH];		//+0x90
	ReanimationType		mReanimType;							//+0x290
	int					mSlideOffTime;							//+0x294
	SexyChar			mLabelNext[MAX_MESSAGE_LENGTH];			//+0x298
	MessageStyle		mMessageStyleNext;						//+0x318

public:
	MessageWidget(LawnApp* theApp);
	~MessageWidget();

	void				SetLabel(const SexyString& theNewLabel, MessageStyle theMessageStyle);
	void				Update();
	void				Draw(Sexy::Graphics* g);
	void				ClearReanim();
	void				ClearLabel();
	bool				IsBeingDisplayed();
	Sexy::Font*			GetFont();
	void				DrawReanimatedText(Sexy::Graphics* g, Sexy::Font* theFont, const Sexy::Color& theColor, float thePosY);
	void				LayoutReanimText();
};

#endif