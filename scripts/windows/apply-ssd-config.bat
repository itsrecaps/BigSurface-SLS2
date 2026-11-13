@echo off
net session >nul 2>&1
if errorlevel 1 (
  echo [X] Run this as admin: right click ^> Run as administrator
  pause & exit /b 1
)
if not exist C:\temp\config-ssd.plist (
  echo [X] C:\temp\config-ssd.plist missing, tell Claude
  pause & exit /b 1
)
if exist S:\ (
  echo [X] S: is in use, tell Claude
  pause & exit /b 1
)
mountvol S: /S
copy /y S:\EFI\OC\config.plist S:\EFI\OC\config-before-test.plist >nul
copy /y C:\temp\config-ssd.plist S:\EFI\OC\config.plist
if errorlevel 1 (
  echo [X] Copy failed, tell Claude
  mountvol S: /D
  pause & exit /b 1
)
mountvol S: /D
bcdedit /set {fwbootmgr} bootsequence {c839a303-adc5-11f1-89d7-f95403d486dc}
echo New config is on the SSD. Restarting into OpenCore in 5 sec, pick Macintosh HD
shutdown /r /t 5
