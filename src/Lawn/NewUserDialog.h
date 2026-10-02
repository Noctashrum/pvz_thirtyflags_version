#ifndef __NEWUSERDIALOG_H__
#define __NEWUSERDIALOG_H__

#include "LawnDialog.h"
#include "../SexyAppFramework/EditListener.h"
namespace Sexy
{
	class EditWidget;
}

class NewUserDialog : public LawnDialog, public Sexy::EditListener
{
public:
	LawnApp*			mApp;					//+0x170
	Sexy::EditWidget*	mNameEditWidget;		//+0x174

public:
	NewUserDialog(LawnApp* theApp, bool isRename);
	virtual ~NewUserDialog();

	virtual int			GetPreferredHeight(int theWidth);
	virtual void		Resize(int theX, int theY, int theWidth, int theHeight);
	virtual void		AddedToManager(Sexy::WidgetManager* theWidgetManager);
	virtual void		RemovedFromManager(Sexy::WidgetManager* theWidgetManager);
	virtual void		Draw(Sexy::Graphics* g);
	virtual void		EditWidgetText(int theId, const SexyString& theString);
	virtual bool		AllowChar(int theId, SexyChar theChar);
	SexyString			GetName();
	void				SetName(const SexyString& theName);
};

#endif
