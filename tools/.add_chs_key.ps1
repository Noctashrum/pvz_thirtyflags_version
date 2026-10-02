$ErrorActionPreference = "Stop"
. "D:\dsh-project\LawnProject\tools\gbkfunc.ps1"

$file = "D:\dsh-project\LawnProject\src\Lawn\SeedChooserScreen.cpp"

$old = @'
void SeedChooserScreen::KeyChar(SexyChar theChar)
{
	if (mChooseState == CHOOSE_VIEW_LAWN && (theChar == ' ' || theChar == '\r' || theChar == '\u001B'))
		CancelLawnView();
	else if (mApp->mTodCheatKeys && theChar == '\u001B')
		PickRandomSeeds();
	else mBoard->KeyChar(theChar);
}
'@

$new = @'
void SeedChooserScreen::KeyChar(SexyChar theChar)
{
	if (mChooseState == CHOOSE_VIEW_LAWN && (theChar == ' ' || theChar == '\r' || theChar == '\u001B'))
		CancelLawnView();
	else if (mApp->mTodCheatKeys && theChar == '\u001B')
		PickRandomSeeds();
	// 《三十旗》测试入口：Release 下 mTodCheatKeys 不可用，故不加门槛。
	// 仅在本模式下生效：随机选满卡槽并直接开战（PickRandomSeeds 内部会 CloseSeedChooser）。
	else if ((theChar == 'S' || theChar == 's') && mApp->mGameMode == GAMEMODE_THIRTY_FLAGS)
		PickRandomSeeds();
	else mBoard->KeyChar(theChar);
}
'@

Edit-Gbk $file $old $new
