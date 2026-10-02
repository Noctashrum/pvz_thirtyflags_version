$ErrorActionPreference = "Stop"
. "D:\dsh-project\LawnProject\tools\gbkfunc.ps1"
$f = "D:\dsh-project\LawnProject\src\Lawn\Board.cpp"
$T = [char]9
$eol = "`r`n"

# actual text of the anchor block
$old = @(
  ($T + '// 【三十旗改版】调试键：''.'' 立即开下一旗（含整备结算），'','' 直接跳过当前旗'),
  ($T + 'if (mApp->mGameMode == GameMode::GAMEMODE_THIRTY_FLAGS && mApp->mTodCheatKeys)'),
  ($T + '{'),
  ($T + $T + 'if (theChar == _S(''.''))')
) -join $eol

$new = @(
  ($T + '// 【三十旗改版】测试入口：Release 下 mTodCheatKeys 不可用，故单开一个无门槛键，'),
  ($T + '// 只为「直接打到第 30 旗验僵王」用，避免真打 29 旗才能测终战。'),
  ($T + 'if (mApp->mGameMode == GameMode::GAMEMODE_THIRTY_FLAGS && theChar == _S(''\t''))'),
  ($T + '{'),
  ($T + $T + 'if (mChallenge)'),
  ($T + $T + '{'),
  ($T + $T + $T + 'mChallenge->mSurvivalStage = TF_BOSS_FLAG - 1;'),
  ($T + $T + '}'),
  ($T + $T + 'ThirtyFlagsIntermissionFinish(this);'),
  ($T + $T + 'ThirtyFlagsIntermissionBegin(this);'),
  ($T + $T + 'ThirtyFlagsAdvanceFlag(this);'),
  ($T + $T + 'InitSurvivalStage();'),
  ($T + $T + 'DisplayAdvice(_S("[TF_START_NEXT_FLAG]"), MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_NONE);'),
  ($T + $T + 'return;'),
  ($T + '}'),
  '',
  ($T + '// 【三十旗改版】调试键：''.'' 立即开下一旗（含整备结算），'','' 直接跳过当前旗'),
  ($T + 'if (mApp->mGameMode == GameMode::GAMEMODE_THIRTY_FLAGS && mApp->mTodCheatKeys)'),
  ($T + '{'),
  ($T + $T + 'if (theChar == _S(''.''))')
) -join $eol

$ok = Edit-Gbk $f $old $new
if (-not $ok) { Write-Host "FAIL"; exit 1 }
Write-Host "boss jump key (Tab) added"
