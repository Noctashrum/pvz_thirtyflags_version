$ErrorActionPreference = "Stop"
$CRLF = "`r`n"
$Q = [char]34
$file = "D:\dsh-project\LawnProject\src\Lawn\LawnApp.cpp"
$enc = [System.Text.Encoding]::GetEncoding(936)
$txt = [System.IO.File]::ReadAllText($file, $enc)

# ------------------------------------------------------------------ 1) log implementation
$anchor = "#include " + $Q + "PopDRMComm.h" + $Q
$idx = $txt.IndexOf($anchor)
if ($idx -lt 0) { Write-Host "FAIL: PopDRMComm.h include not found"; exit 1 }
$eol = if ($txt.IndexOf("`r`n") -ge 0) { "`r`n" } else { "`n" }
$anchorFull = $anchor + $eol

$implLines = @(
  '#include <cstdio>',
  '#include <cstdarg>',
  '#include <string>',
  '#include <windows.h>',
  '',
  '// ====================================================================================================',
  '// [ThirtyFlags] Diagnostic log.',
  '// Release builds have no asserts and write no logs, so failures can only be guessed at.',
  '// Key state transitions and unhandled exceptions go to Release\thirtyflags.log so that',
  '// "app exited by itself" / "not responding" can be diagnosed after the fact.',
  '// ====================================================================================================',
  'static const char* gTfPhase = "boot";',
  '',
  'static void TfLogWrite(const char* theFormat, ...)',
  '{',
  '    FILE* aFile = fopen("thirtyflags.log", "a");',
  '    if (!aFile)',
  '        return;',
  '',
  '    SYSTEMTIME aTime;',
  '    GetLocalTime(&aTime);',
  '    fprintf(aFile, "%02d:%02d:%02d.%03d [%s] ", aTime.wHour, aTime.wMinute,',
  '        aTime.wSecond, aTime.wMilliseconds, gTfPhase);',
  '',
  '    va_list aArgs;',
  '    va_start(aArgs, theFormat);',
  '    char aBuf[1024];',
  '    vsnprintf(aBuf, sizeof(aBuf), theFormat, aArgs);',
  '    va_end(aArgs);',
  '',
  '    fprintf(aFile, "%s", aBuf);',
  '    fprintf(aFile, "\n");',
  '    fclose(aFile);',
  '}',
  '',
  'static LONG WINAPI TfUnhandledFilter(struct _EXCEPTION_POINTERS* theInfo)',
  '{',
  '    TfLogWrite("!!! UNHANDLED EXCEPTION code=0x%08X addr=%p",',
  '        theInfo && theInfo->ExceptionRecord ? theInfo->ExceptionRecord->ExceptionCode : 0,',
  '        theInfo && theInfo->ExceptionRecord ? theInfo->ExceptionRecord->ExceptionAddress : 0);',
  '    return EXCEPTION_EXECUTE_HANDLER;',
  '}',
  ''
)
$impl = ($implLines -join $eol)

$txt = $txt.Remove($idx, 0).Insert($idx + $anchorFull.Length, $impl + $eol)
Write-Host "OK 1) log implementation inserted"

# ------------------------------------------------------------------ 2) hooks
$E = @(
  @{ o = 'void LawnApp::Init()';                 n = @('    SetUnhandledExceptionFilter(TfUnhandledFilter);', '    gTfPhase = "init";', '    TfLogWrite("=== app init ===");') },
  @{ o = 'void LawnApp::Start()';                n = @('    gTfPhase = "start";', '    TfLogWrite("app start");') },
  @{ o = 'void LawnApp::LoadingCompleted()';     n = @('    gTfPhase = "loaded";', '    TfLogWrite("loading completed");') },
  @{ o = 'void LawnApp::PreNewGame(GameMode theGameMode, bool theLookForSavedGame)'; n = @('    gTfPhase = "PreNewGame";', '    TfLogWrite("PreNewGame mode=%d", (int)theGameMode);') },
  @{ o = 'void LawnApp::MakeNewBoard()';         n = @('    gTfPhase = "MakeNewBoard";', '    TfLogWrite("MakeNewBoard mode=%d", (int)mGameMode);') },
  @{ o = 'void LawnApp::StartPlaying()';         n = @('    gTfPhase = "play";', '    TfLogWrite("StartPlaying");') },
  @{ o = 'void LawnApp::CloseRequestAsync()';    n = @('    TfLogWrite("!! CloseRequestAsync (WM_CLOSE) -> app will exit");') },
  @{ o = 'void LawnApp::Shutdown()';             n = @('    gTfPhase = "shutdown";', '    TfLogWrite("Shutdown called (loadingThreadCompleted=%d)", (int)mLoadingThreadCompleted);') }
)

$ok = 0
foreach ($e in $E) {
  $old = $e.o + $eol + '{' + $eol
  $c = ([regex]::Matches($txt, [regex]::Escape($old))).Count
  if ($c -ne 1) { Write-Host ("FAIL anchor x{0}: {1}" -f $c, $e.o); continue }
  $rep = $e.o + $eol + '{' + $eol + (($e.n | ForEach-Object { $_ }) -join $eol) + $eol
  $txt = $txt.Replace($old, $rep)
  $ok++
}
Write-Host "OK  $ok/$($E.Count) hooks inserted"

[System.IO.File]::WriteAllText($file, $txt, $enc)
Write-Host "written"
