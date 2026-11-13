@echo off
setlocal enabledelayedexpansion
net session >nul 2>&1
if errorlevel 1 (
  echo [X] Run this as admin: right click ^> Run as administrator
  pause & exit /b 1
)
set OUT=C:\temp\oclogs
if not exist %OUT% mkdir %OUT%

echo [1/3] Putting Windows Boot Manager first again...
bcdedit /set {fwbootmgr} displayorder {bootmgr} /addfirst

echo [2/3] Grabbing OpenCore logs off the EFI partition...
if exist S:\ (
  echo [X] S: is in use, tell Claude
  pause & exit /b 1
)
mountvol S: /S
dir /s S:\ > %OUT%\esp-listing.txt 2>&1
copy /y S:\opencore-*.txt %OUT%\ >nul 2>&1
copy /y S:\EFI\OC\config.plist %OUT%\esp-config.plist >nul 2>&1
mountvol S: /D

echo [3/3] Saving boot list...
bcdedit /enum firmware > %OUT%\bcd.txt 2>&1

echo.
echo DONE. Windows will boot normal now. Tell Claude it finished
pause
