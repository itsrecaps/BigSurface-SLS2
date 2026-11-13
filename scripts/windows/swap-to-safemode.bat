@echo off
net session >nul 2>&1
if errorlevel 1 (
  echo [X] Run this as admin: right click ^> Run as administrator
  pause & exit /b 1
)
if exist S:\ (
  echo [X] S: is in use, tell Claude
  pause & exit /b 1
)
mountvol S: /S
if not exist S:\EFI\OC\config-safemode-backup.plist (
  echo [X] Safe mode backup missing, tell Claude
  mountvol S: /D
  pause & exit /b 1
)
copy /y S:\EFI\OC\config.plist S:\EFI\OC\config-accel-current.plist >nul
copy /y S:\EFI\OC\config-safemode-backup.plist S:\EFI\OC\config.plist
mountvol S: /D
bcdedit /set {fwbootmgr} bootsequence {c839a303-adc5-11f1-89d7-f95403d486dc}
echo Safe mode config is on. Restarting into OpenCore in 5 sec, pick Tahoe2671
shutdown /r /t 5
