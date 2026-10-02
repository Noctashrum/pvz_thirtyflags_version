#include "TFDialog.h"
#include "Board.h"
#include "LawnApp.h"
#include "Resources.h"
#include "ThirtyFlags.h"
#include "ToolTipWidget.h"
#include "../SexyAppFramework/WidgetManager.h"

using namespace Sexy;

// 《三十旗》整备阶段：肉鸽强化三选一弹窗
//
// 弹窗内容为中文。工程内置字模除拉丁字形外也含中文字形（其他界面显示中文走的同样是
// Graphics::DrawString 这条路），因此这里直接复用原版 ToolTipWidget 承载名称与说明文字。
// 注意 ToolTipWidget 不是 Widget，不能挂进控件树，需要在 Draw 里手动绘制。

static const int TF_CARD_WIDTH = 168;
static const int TF_CARD_GAP = 16;
static const int TF_CARD_Y = 232;
static const int TF_CARD_HEIGHT = 34;

ThirtyFlagsDialog::ThirtyFlagsDialog(Board* theBoard)
	: LawnDialog(theBoard->mApp, Dialogs::DIALOG_THIRTY_FLAGS, true,
		_S("[TF_INTERMISSION]"), _S("[TF_PICK_UPGRADE]"), _S(""), Dialog::BUTTONS_NONE)
{
	mBoard = theBoard;
	mDrawStandardBack = true;
	mVerticalCenterText = false;

	Color aBtnColor(42, 42, 90);

	for (int i = 0; i < 3; i++)
	{
		NewLawnButton* aButton = MakeNewButton(ThirtyFlagsDialog_Choice1 + i, this, _S(""),
			Sexy::FONT_DWARVENTODCRAFT15, Sexy::IMAGE_SEEDCHOOSER_BUTTON,
			Sexy::IMAGE_SEEDCHOOSER_BUTTON, Sexy::IMAGE_SEEDCHOOSER_BUTTON);
		aButton->mColors[GameButton::COLOR_LABEL] = aBtnColor;
		aButton->mColors[GameButton::COLOR_LABEL_HILITE] = aBtnColor;
		aButton->mTextDownOffsetY = 1;
		mChoiceButton[i] = aButton;

		ToolTipWidget* aToolTip = new ToolTipWidget();
		aToolTip->mCenter = false;
		aToolTip->mVisible = true;
		aToolTip->mMinLeft = 0;
		aToolTip->mMaxBottom = 600;
		mChoiceToolTip[i] = aToolTip;
	}

	CalcSize(190, 250);
	RefreshLabels();
	LayoutCards();
}

ThirtyFlagsDialog::~ThirtyFlagsDialog()
{
	for (int i = 0; i < 3; i++)
	{
		delete mChoiceButton[i];
		delete mChoiceToolTip[i];
	}
}

void ThirtyFlagsDialog::LayoutCards()
{
	int aTotalWidth = TF_CARD_WIDTH * 3 + TF_CARD_GAP * 2;
	int aStartX = (mWidth - aTotalWidth) / 2;

	for (int i = 0; i < 3; i++)
	{
		int aX = aStartX + i * (TF_CARD_WIDTH + TF_CARD_GAP);
		mChoiceButton[i]->Resize(aX, TF_CARD_Y, TF_CARD_WIDTH, TF_CARD_HEIGHT);

		// 说明面板：卡片下方，水平居中于卡片
		mChoiceToolTip[i]->CalculateSize();
		mChoiceToolTip[i]->mX = aX + (TF_CARD_WIDTH - mChoiceToolTip[i]->mWidth) / 2;
		mChoiceToolTip[i]->mY = TF_CARD_Y + TF_CARD_HEIGHT + 12;
	}
}

void ThirtyFlagsDialog::RefreshLabels()
{
	for (int i = 0; i < 3; i++)
	{
		ThirtyFlagsUpgrade aUpgrade = gThirtyFlags.mPendingUpgradeChoices[i];
		const ThirtyFlagsUpgradeDef& aDef = ThirtyFlags::GetUpgradeDef(aUpgrade);

		int aStacks = gThirtyFlags.GetStacks(aUpgrade);
		SexyString aSkillName = aDef.mName;

		// 按钮显示中文名 + 本次选择后将达到的层数
		SexyString aLabel;
		if (aStacks > 0)
		{
			aLabel = StrFormat(_S("%s  x%d"), aSkillName.c_str(), aStacks + 1);
		}
		else
		{
			aLabel = aSkillName;
		}

		mChoiceButton[i]->SetLabel(aLabel);
		mChoiceToolTip[i]->SetTitle(aSkillName);
		mChoiceToolTip[i]->SetLabel(aDef.mDescription);
		mChoiceToolTip[i]->SetWarningText(_S(""));
	}
}

void ThirtyFlagsDialog::AddedToManager(WidgetManager* theWidgetManager)
{
	LawnDialog::AddedToManager(theWidgetManager);

	for (int i = 0; i < 3; i++)
	{
		AddWidget(mChoiceButton[i]);
	}
}

void ThirtyFlagsDialog::RemovedFromManager(WidgetManager* theWidgetManager)
{
	for (int i = 0; i < 3; i++)
	{
		RemoveWidget(mChoiceButton[i]);
	}

	LawnDialog::RemovedFromManager(theWidgetManager);
}

void ThirtyFlagsDialog::Update()
{
	LawnDialog::Update();
	LayoutCards();
}

void ThirtyFlagsDialog::Draw(Graphics* g)
{
	LawnDialog::Draw(g);

	// 说明面板（ToolTipWidget 非 Widget，手动绘制；控件在其后绘制，故不会被遮挡）
	for (int i = 0; i < 3; i++)
	{
		mChoiceToolTip[i]->Draw(g);
	}
}

void ThirtyFlagsDialog::ButtonDepress(int theId)
{
	int aIndex = theId - ThirtyFlagsDialog_Choice1;
	if (aIndex < 0 || aIndex > 2)
		return;

	if (gThirtyFlags.mUpgradeChosen)
		return;

	ThirtyFlagsUpgrade aUpgrade = gThirtyFlags.mPendingUpgradeChoices[aIndex];
	gThirtyFlags.GrantUpgrade(aUpgrade);
	gThirtyFlags.mUpgradeChosen = true;
	gThirtyFlags.mIntermission = false;   // 【三十旗】选卡完成，整备期结束

	mApp->KillDialog(Dialogs::DIALOG_THIRTY_FLAGS);
}
