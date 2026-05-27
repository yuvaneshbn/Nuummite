$ErrorActionPreference = "Stop"

Set-Location (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location ..

# Build Nuummite-voice as onedir, always bundling Qt .ui files even if a .spec file gets regenerated.
# This avoids the common issue where re-running PyInstaller from the script overwrites manual edits in the .spec file.

$uiSrc = "Nuummite\ui\*.ui"
$uiDst = "Nuummite\ui"
$qssSrc = "Nuummite\ui\*.qss"
$qssDst = "Nuummite\ui"
$svgSrc = "Nuummite\ui\*.svg"
$svgDst = "Nuummite\ui"

pyinstaller -y `
  -D `
  -n "Nuummite-voice" `
  --add-data "$uiSrc;$uiDst" `
  --add-data "$qssSrc;$qssDst" `
  --add-data "$svgSrc;$svgDst" `
  -i "Nuummite\technical-support.ico" `
  "python\main.py"

Write-Host "Built: dist\Nuummite-voice\Nuummite-voice.exe"
