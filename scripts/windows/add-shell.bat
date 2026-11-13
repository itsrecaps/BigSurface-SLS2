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
if not exist S:\EFI\OC\Tools mkdir S:\EFI\OC\Tools
copy /y C:\temp\OpenShell.efi S:\EFI\OC\Tools\OpenShell.efi >nul
copy /y C:\temp\config-shell.plist S:\EFI\OC\config.plist >nul
if not exist S:\EFI\OC\Tools\OpenShell.efi (
  echo [X] Copy failed, tell Claude
  mountvol S: /D
  pause & exit /b 1
)
mountvol S: /D
bcdedit /set {fwbootmgr} bootsequence {c839a303-adc5-11f1-89d7-f95403d486dc}
echo Shell added. Restarting into OpenCore in 5 sec, pick OpenShell
shutdown /r /t 5
