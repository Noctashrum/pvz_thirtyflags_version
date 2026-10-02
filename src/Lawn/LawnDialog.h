#ifndef __LAWNDIALOG_H__
#define __LAWNDIALOG_H__

#include "../TodLib/TodCommon.h"
#include "../SexyAppFramework/Dialog.h"

class LawnApp;
class LawnDialog;
class Reanimation;
class LawnStoneButton;
class ReanimationWidget;
namespace Sexy
{
	class Checkbox;
	class DialogButton;
	class CheckboxListener;
}
enum ReanimationType;

enum Dialogs
{
    DIALOG_NEW_GAME,                            // 0：
    DIALOG_OPTIONS,                             // 1：
    DIALOG_NEWOPTIONS,                          // 2：菜单
    DIALOG_ALMANAC,                             // 3：图鉴
    DIALOG_STORE,                               // 4：商店
    DIALOG_PREGAME_NAG,                         // 5：
    DIALOG_LOAD_GAME,                           // 6：
    DIALOG_CONFIRM_UPDATE_CHECK,                // 7：
    DIALOG_CHECKING_UPDATES,                    // 8：
    DIALOG_REGISTER_ERROR,                      // 9：
    DIALOG_COLORDEPTH_EXP,                      // 10：不支持窗口模式
    DIALOG_OPENURL_WAIT,                        // 11：
    DIALOG_OPENURL_FAIL,                        // 12：
    DIALOG_QUIT,                                // 13：退出游戏（主菜单点击退出时）
    DIALOG_HIGH_SCORES,                         // 14：
    DIALOG_NAG,                                 // 15：
    DIALOG_INFO,                                // 16：信息（3D 加速不支持/不推荐）
    DIALOG_GAME_OVER,                           // 17：游戏结束（关卡失败）
    DIALOG_LEVEL_COMPLETE,                      // 18：关卡完成
    DIALOG_PAUSED,                              // 19：暂停游戏
    DIALOG_NO_MORE_MONEY,                       // 20：
    DIALOG_BONUS,                               // 21：
    DIALOG_CONFIRM_BACK_TO_MAIN,                // 22：返回主菜单
    DIALOG_CONFIRM_RESTART,                     // 23：重新开始关卡
    DIALOG_THANKS_FOR_REGISTERING,              // 24：
    DIALOG_NOT_ENOUGH_MONEY,                    // 25：资金不足（购买商品但钱数不够时触发）
    DIALOG_UPGRADED,                            // 26：卡槽升级
    DIALOG_NO_UPGRADE,                          // 27：
    DIALOG_CHOOSER_WARNING,                     // 28：选卡提醒（未携带生产阳光的植物、未携带紫卡原植物等情况下触发）
    DIALOG_USERDIALOG,                          // 29：用户对话
    DIALOG_CREATEUSER,                          // 30：创建新用户
    DIALOG_CONFIRMDELETEUSER,                   // 31：删除用户
    DIALOG_RENAMEUSER,                          // 32：重命名用户
    DIALOG_CREATEUSERERROR,                     // 33：请输入名字（创建新用户对话中不输入用户名时触发）
    DIALOG_RENAMEUSERERROR,                     // 34：请输入名字（重命名用户对话中不输入用户名时触发）
    DIALOG_CHEAT,                               // 35：跳关对话（仅内测版可触发）
    DIALOG_CHEATERROR,                          // 36：跳关指令输入有误
    DIALOG_CONTINUE,                            // 37：继续游戏（读档进入关卡时）
    DIALOG_GETREADY,                            // 38：
    DIALOG_RESTARTCONFIRM,                      // 39：重新开始游戏（继续游戏对话中选择开始新游戏时触发）
    DIALOG_CONFIRMPURCHASE,                     // 40：
    DIALOG_CONFIRMSELL,                         // 41：
    DIALOG_TIMESUP,                             // 42：
    DIALOG_VIRTUALHELP,                         // 43：
    DIALOG_JUMPAHEAD,                           // 44：
    DIALOG_CRAZY_DAVE,                          // 45：
    DIALOG_STORE_PURCHASE,                      // 46：购买商品（戴夫商店）
    DIALOG_ZEN_SELL,                            // 47：出售盆栽植物（禅境花园）
    DIALOG_MESSAGE,                             // 48：通用消息对话，包括：①加载中（切换用户时触发）；②小游戏等模式未解锁；……
    DIALOG_IMITATER,                            // 49：模仿者对话
    DIALOG_PURCHASE_PACKET_SLOT,                // 50：升级卡槽的格子数量
    DIALOG_THIRTY_FLAGS,                        // 51：【三十旗】整备阶段肉鸽强化三选一
    DIALOG_TF_ENTER,                            // 52：【三十旗】冒险 1-1 按 T 的进入确认框
    NUM_DIALOGS
};

class ReanimationWidget : public Sexy::Widget
{
public:
    LawnApp*				mApp;					//+0x88
    Reanimation*			mReanim;				//+0x8C
    LawnDialog*				mLawnDialog;			//+0x90
    float					mPosX;					//+0x94
    float					mPosY;					//+0x98

public:
	ReanimationWidget();
	virtual ~ReanimationWidget();

	void					Dispose();
	virtual void			Draw(Sexy::Graphics* g);
	virtual void			Update();
	void					AddReanimation(float x, float y, ReanimationType theReanimationType);
};

class LawnDialog : public Sexy::Dialog
{
public:
	LawnApp*				mApp;					//+0x150
	int						mButtonDelay;			//+0x154
	ReanimationWidget*		mReanimation;			//+0x158
	bool					mDrawStandardBack;		//+0x15C
	LawnStoneButton*		mLawnYesButton;			//+0x160
	LawnStoneButton*		mLawnNoButton;			//+0x164
	bool					mTallBottom;			//+0x168
	bool					mVerticalCenterText;	//+0x169

public:
	LawnDialog(LawnApp* theApp, int theId, bool isModal, const SexyString& theDialogHeader, const SexyString& theDialogLines, const SexyString& theDialogFooter, int theButtonMode);
	~LawnDialog();

	int						GetLeft();
	int						GetWidth();
	int						GetTop();
	virtual void			SetButtonDelay(int theDelay);
	virtual void			Update();
	virtual void			ButtonPress(int theId);
	virtual void			ButtonDepress(int theId);
	virtual void			CheckboxChecked(int theId, bool theChecked);
	virtual void			KeyDown(Sexy::KeyCode theKey);
	virtual void			AddedToManager(Sexy::WidgetManager* theWidgetManager);
	virtual void			RemovedFromManager(Sexy::WidgetManager* theWidgetManager);
	virtual void			Resize(int theX, int theY, int theWidth, int theHeight);
	virtual void			Draw(Sexy::Graphics* g);
	void					CalcSize(int theExtraX, int theExtraY);
};

class GameOverDialog : public LawnDialog
{
public:
	Sexy::DialogButton*		mMenuButton;

public:
	GameOverDialog(const SexyString& theMessage, bool theShowChallengeName);
	virtual ~GameOverDialog();

	virtual void			ButtonDepress(int theId);
	virtual void			AddedToManager(Sexy::WidgetManager* theWidgetManager);
	virtual void			RemovedFromManager(Sexy::WidgetManager* theWidgetManager);
	virtual void			MouseDrag(int x, int y);
};

#endif
