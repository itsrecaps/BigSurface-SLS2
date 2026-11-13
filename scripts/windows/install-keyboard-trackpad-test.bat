@echo off
net session >nul 2>&1
if errorlevel 1 (
  echo [X] Run this as admin: right click ^> Run as administrator
  pause & exit /b 1
)
if not exist C:\temp\BigSurface-SLS2.kext.zip (
  echo [X] Keyboard driver zip missing, tell Claude
  pause & exit /b 1
)
if not exist C:\temp\SurfaceTHC\SurfaceTHC.kext\Contents\MacOS\SurfaceTHC (
  echo [X] Trackpad probe missing, tell Claude
  pause & exit /b 1
)
if exist C:\temp\bs rmdir /s /q C:\temp\bs
powershell -NoProfile -Command "Expand-Archive -Force C:\temp\BigSurface-SLS2.kext.zip C:\temp\bs"
if not exist C:\temp\bs\BigSurface.kext\Contents\PlugIns\VoodooSerial.kext\Contents\MacOS\VoodooSerial (
  echo [X] Unzip failed, tell Claude
  pause & exit /b 1
)
if exist S:\ (
  echo [X] S: is in use, tell Claude
  pause & exit /b 1
)
mountvol S: /S
copy /y S:\EFI\OC\config.plist S:\EFI\OC\config-before-kbd.plist >nul
if exist S:\EFI\OC\BigSurface-old.kext rmdir /s /q S:\EFI\OC\BigSurface-old.kext
move S:\EFI\OC\Kexts\BigSurface.kext S:\EFI\OC\BigSurface-old.kext >nul
robocopy C:\temp\bs\BigSurface.kext S:\EFI\OC\Kexts\BigSurface.kext /E /NFL /NDL /NJH /NJS
robocopy C:\temp\SurfaceTHC\SurfaceTHC.kext S:\EFI\OC\Kexts\SurfaceTHC.kext /E /NFL /NDL /NJH /NJS
copy /y C:\temp\config-accel-thc-bs.plist S:\EFI\OC\config.plist >nul
dir /s S:\EFI\OC\Kexts\BigSurface.kext S:\EFI\OC\Kexts\SurfaceTHC.kext > C:\temp\oclogs\kbd-check.txt
mountvol S: /D
echo.
echo DONE. Tell Claude so it can check before you reboot
pause
