$ErrorActionPreference = "Stop"

Set-Location (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location ..

# Build Nuummite-voice as onedir, always bundling Qt .ui files even if a .spec file gets regenerated.
# This avoids the common issue where re-running PyInstaller from the script overwrites manual edits in the .spec file.

$uiSrc = "Nuummite\ui\*.ui"
$uiDst = "Nuummite\ui"

pyinstaller -y `
  -D `
  -n "Nuummite-voice" `
  --add-data "$uiSrc;$uiDst" `
  -i "Nuummite\technical-support.ico" `
  "python\main.py"

Write-Host "Built: dist\Nuummite-voice\Nuummite-voice.exe"
