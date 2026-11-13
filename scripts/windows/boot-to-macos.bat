@echo off
net session >nul 2>&1
if errorlevel 1 (
  echo [X] Run this as admin: right click ^> Run as administrator
  pause & exit /b 1
)
bcdedit /set {fwbootmgr} bootsequence {c839a303-adc5-11f1-89d7-f95403d486dc}
if errorlevel 1 (
  echo [X] Couldnt set it, tell Claude
  pause & exit /b 1
)
echo Restarting into OpenCore one time in 5 sec...
shutdown /r /t 5
