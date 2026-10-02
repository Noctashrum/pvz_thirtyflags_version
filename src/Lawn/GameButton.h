#ifndef __GAMEBUTTON_H__
#define __GAMEBUTTON_H__

#include "../SexyAppFramework/SexyVector.h"
#include "../SexyAppFramework/DialogButton.h"

class LawnApp;

class GameButton
{
public:
	enum
	{
		BUTTON_LABEL_LEFT = -1,
		BUTTON_LABEL_CENTER = 0,
		BUTTON_LABEL_RIGHT = 1
	};
	enum
	{
		COLOR_LABEL,
		COLOR_LABEL_HILITE,
		COLOR_DARK_OUTLINE,
		COLOR_LIGHT_OUTLINE,
		COLOR_MEDIUM_OUTLINE,
		COLOR_BKG,
		NUM_COLORS
	};

public:
	LawnApp*				mApp;							//+0x0
	Sexy::Widget*			mParentWidget;					//+0x4
	int						mX;								//+0x8
	int						mY;								//+0xC
	int						mWidth;							//+0x10
	int						mHeight;						//+0x14
	bool					mIsOver;						//+0x18
	bool					mIsDown;						//+0x19
	bool					mDisabled;						//+0x1A
	Sexy::Color				mColors[NUM_COLORS];			//+0x1C
	int						mId;							//+0x7C
	SexyString				mLabel;							//+0x80
	int						mLabelJustify;					//+0x9C
	Sexy::Font*				mFont;							//+0xA0
	Sexy::Image*			mButtonImage;					//+0xA4
	Sexy::Image*			mOverImage;						//+0xA8
	Sexy::Image*			mDownImage;						//+0xAC
	Sexy::Image*			mDisabledImage;					//+0xB0
	Sexy::Image*			mOverOverlayImage;				//+0xB4
	Sexy::Rect				mNormalRect;					//+0xB8
	Sexy::Rect				mOverRect;						//+0xC8
	Sexy::Rect				mDownRect;						//+0xD8
	Sexy::Rect				mDisabledRect;					//+0xE8
	bool					mInverted;						//+0xF8
	bool					mBtnNoDraw;						//+0xF9
	bool					mFrameNoDraw;					//+0xFA
	double					mOverAlpha;						//+0x100
	double					mOverAlphaSpeed;				//+0x108
	double					mOverAlphaFadeInSpeed;			//+0x110			
	bool					mDrawStoneButton;				//+0x118
	int						mTextOffsetX;					//+0x11C
	int						mTextOffsetY;					//+0x120
	int						mButtonOffsetX;					//+0x124
	int						mButtonOffsetY;					//+0x128

public:
	GameButton(int theId);
	~GameButton();

	static bool				HaveButtonImage(Sexy::Image* theImage, const Sexy::Rect& theRect);
	void					DrawButtonImage(Sexy::Graphics* g, Sexy::Image* theImage, const Sexy::Rect& theRect, int theX, int theY);
	void					SetFont(Sexy::Font* theFont);
	bool					IsButtonDown();
	void					Draw(Sexy::Graphics* g);
	void					SetDisabled(bool theDisabled);
	bool					IsMouseOver();
	void					Update();
	void					Resize(int theX, int theY, int theWidth, int theHeight);
	void					SetLabel(const SexyString& theLabel);
};

class LawnStoneButton : public Sexy::DialogButton
{
public:
	LawnStoneButton(Sexy::Image* theComponentImage, int theId, Sexy::ButtonListener* theListener) : DialogButton(theComponentImage, theId, theListener) { }

	virtual void			Draw(Sexy::Graphics* g);
	void					SetLabel(const SexyString& theLabel);
};

class NewLawnButton : public Sexy::DialogButton
{
public:
	Sexy::Font*				mHiliteFont;            //+0x138
    int						mTextDownOffsetX;       //+0x13C
    int						mTextDownOffsetY;       //+0x140
	int						mButtonOffsetX;			//+0x144
	int						mButtonOffsetY;			//+0x148
	bool					mUsePolygonShape;		//+0x14C
	Sexy::SexyVector2		mPolygonShape[4];		//+0x150

public:
    NewLawnButton(Sexy::Image* theComponentImage, int theId, Sexy::ButtonListener* theListener);
	virtual ~NewLawnButton();
	
    virtual void			Draw(Sexy::Graphics* g);
	virtual bool			IsPointVisible(int x, int y);
    void					SetLabel(const SexyString& theLabel);
};

LawnStoneButton*			MakeButton(int theId, Sexy::ButtonListener* theListener, const SexyString& theText);
NewLawnButton*				MakeNewButton(int theId, Sexy::ButtonListener* theListener, const SexyString& theText, Sexy::Font* theFont, Sexy::Image* theImageNormal, Sexy::Image* theImageOver, Sexy::Image* theImageDown);
void						DrawStoneButton(Sexy::Graphics* g, int x, int y, int theWidth, int theHeight, bool isDown, bool isHighLighted, const SexyString& theLabel);

#endif
