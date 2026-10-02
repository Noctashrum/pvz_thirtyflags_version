$script:GbkEnc = [System.Text.Encoding]::GetEncoding(936)
function Edit-Gbk([string]$path, [string]$old, [string]$new, [int]$expect = 1) {
  $text = [System.IO.File]::ReadAllText($path, $script:GbkEnc)
  foreach ($cand in @($old, ($old -replace "`r`n","`n"), ($old -replace "`n","`r`n"))) {
    $count = ([regex]::Matches($text, [regex]::Escape($cand))).Count
    if ($count -eq $expect) {
      $newCand = if ($cand -match "`r`n") { $new -replace "`n","`r`n" } else { $new -replace "`r`n","`n" }
      [System.IO.File]::WriteAllText($path, $text.Replace($cand, $newCand), $script:GbkEnc)
      Write-Host "OK   $path  (replaced $count)"
      return $true
    }
  }
  Write-Host "FAIL $path : expected $expect match, not found"
  return $false
}
