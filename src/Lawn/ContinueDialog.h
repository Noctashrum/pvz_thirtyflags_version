#ifndef __CONTINUEDIALOG_H__
#define __CONTINUEDIALOG_H__

#include "LawnDialog.h"

class ContinueDialog : public LawnDialog
{
public:
	enum
	{
		ContinueDialog_Continue,
		ContinueDialog_NewGame
	};

public:
	Sexy::DialogButton*		mContinueButton;	//+0x16C
	Sexy::DialogButton*		mNewGameButton;		//+0x170

public:
	ContinueDialog(LawnApp* theApp);
	virtual ~ContinueDialog();

	virtual int				GetPreferredHeight(int theWidth);
	virtual void			Resize(int theX, int theY, int theWidth, int theHeight);
	virtual void			AddedToManager(Sexy::WidgetManager* theWidgetManager);
	virtual void			RemovedFromManager(Sexy::WidgetManager* theWidgetManager);
	virtual void			ButtonDepress(int theId);
	void					RestartLoopingSounds();
};

#endif