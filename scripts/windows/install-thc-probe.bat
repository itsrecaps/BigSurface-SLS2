@echo off
net session >nul 2>&1
if errorlevel 1 (
  echo [X] Run this as admin: right click ^> Run as administrator
  pause & exit /b 1
)
set K=C:\temp\SurfaceTHC\SurfaceTHC.kext
if not exist %K%\Contents\MacOS\SurfaceTHC (
  echo [X] Probe kext missing, tell Claude
  pause & exit /b 1
)
if exist S:\ (
  echo [X] S: is in use, tell Claude
  pause & exit /b 1
)
mountvol S: /S
copy /y S:\EFI\OC\config.plist S:\EFI\OC\config-before-thc.plist >nul
robocopy %K% S:\EFI\OC\Kexts\SurfaceTHC.kext /E /NFL /NDL /NJH /NJS
copy /y C:\temp\config-accel-thc.plist S:\EFI\OC\config.plist >nul
mountvol S: /D
bcdedit /set {fwbootmgr} bootsequence {c839a303-adc5-11f1-89d7-f95403d486dc}
echo Probe installed. Restarting into OpenCore in 5 sec, pick Tahoe2671
shutdown /r /t 5
