$ErrorActionPreference = "Stop"
. "D:\dsh-project\LawnProject\tools\gbkfunc.ps1"

$file = "D:\dsh-project\LawnProject\src\Lawn\SeedChooserScreen.cpp"
$CRLF = "`r`n"

# 先把上次带注释的坏补丁还原成原始函数
$badOld = @(
  "void SeedChooserScreen::KeyChar(SexyChar theChar)",
  "{",
  "`tif (mChooseState == CHOOSE_VIEW_LAWN && (theChar == ' ' || theChar == '\r' || theChar == '\u001B'))",
  "`t`tCancelLawnView();",
  "`telse if (mApp->mTodCheatKeys && theChar == '\u001B')",
  "`t`tPickRandomSeeds();"
) -join $CRLF
# 坏补丁的第二行注释已被吞掉，直接匹配从注释残片到函数尾的整段
$txt = [System.IO.File]::ReadAllText($file, [System.Text.Encoding]::GetEncoding(936))
$startIdx = $txt.IndexOf("void SeedChooserScreen::KeyChar(SexyChar theChar)")
$endIdx = $txt.IndexOf("//0x486F10", $startIdx)
if ($startIdx -lt 0 -or $endIdx -lt 0) { Write-Host "FAIL locate"; exit 1 }
$oldBlock = $txt.Substring($startIdx, $endIdx - $startIdx)
Write-Host "--- old block ---"
Write-Host $oldBlock

$newBlock = @(
  "void SeedChooserScreen::KeyChar(SexyChar theChar)",
  "{",
  "`tif (mChooseState == CHOOSE_VIEW_LAWN && (theChar == ' ' || theChar == '\r' || theChar == '\u001B'))",
  "`t`tCancelLawnView();",
  "`telse if (mApp->mTodCheatKeys && theChar == '\u001B')",
  "`t`tPickRandomSeeds();",
  "`telse if ((theChar == 'S' || theChar == 's') && mApp->mGameMode == GAMEMODE_THIRTY_FLAGS)",
  "`t`tPickRandomSeeds();",
  "`telse mBoard->KeyChar(theChar);",
  "}",
  "",
  ""
) -join $CRLF

[System.IO.File]::WriteAllText($file, $txt.Replace($oldBlock, $newBlock), [System.Text.Encoding]::GetEncoding(936))
Write-Host "--- replaced ---"
