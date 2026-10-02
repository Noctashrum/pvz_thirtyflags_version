#ifndef __THIRTY_FLAGS_DIALOG_H__
#define __THIRTY_FLAGS_DIALOG_H__

// ====================================================================================================
// 《三十旗》整备阶段：肉鸽强化三选一弹窗
//
// 每面旗结束后弹出，提供 3 张候选强化（同名可重复抽取、效果线性叠加）。
// 复用原版 LawnDialog + NewLawnButton + ToolTipWidget 承载中文文本。
// ====================================================================================================

#include "GameButton.h"
#include "LawnDialog.h"

namespace Sexy
{
	class WidgetManager;
}

class ToolTipWidget;
class Board;

class ThirtyFlagsDialog : public LawnDialog
{
public:
	enum
	{
		ThirtyFlagsDialog_Choice1 = 1000,
		ThirtyFlagsDialog_Choice2,
		ThirtyFlagsDialog_Choice3
	};

public:
	Board*					mBoard;
	NewLawnButton*			mChoiceButton[3];
	ToolTipWidget*			mChoiceToolTip[3];

public:
	ThirtyFlagsDialog(Board* theBoard);
	virtual ~ThirtyFlagsDialog();

	virtual void			Update() override;
	virtual void			Draw(Sexy::Graphics* g) override;
	virtual void			AddedToManager(Sexy::WidgetManager* theWidgetManager) override;
	virtual void			RemovedFromManager(Sexy::WidgetManager* theWidgetManager) override;
	virtual void			ButtonDepress(int theId) override;

	void					RefreshLabels();
	void					LayoutCards();
};

#endif // __THIRTY_FLAGS_DIALOG_H__
