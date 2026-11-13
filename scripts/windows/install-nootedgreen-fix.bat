@echo off
net session >nul 2>&1
if errorlevel 1 (
  echo [X] Run this as admin: right click ^> Run as administrator
  pause & exit /b 1
)
set NG=C:\temp\NootedGreen-SLS2\NootedGreen.kext
if not exist %NG%\Contents\MacOS\NootedGreen (
  echo [X] New NootedGreen missing, tell Claude
  pause & exit /b 1
)
if exist S:\ (
  echo [X] S: is in use, tell Claude
  pause & exit /b 1
)
mountvol S: /S
if not exist S:\EFI\OC\config-accel-current.plist (
  echo [X] Graphics config backup missing, tell Claude
  mountvol S: /D
  pause & exit /b 1
)
if exist S:\EFI\OC\NootedGreen-old.kext rmdir /s /q S:\EFI\OC\NootedGreen-old.kext
move S:\EFI\OC\Kexts\NootedGreen.kext S:\EFI\OC\NootedGreen-old.kext >nul
robocopy %NG% S:\EFI\OC\Kexts\NootedGreen.kext /E /NFL /NDL /NJH /NJS
if not exist S:\EFI\OC\Kexts\NootedGreen.kext\Contents\MacOS\NootedGreen (
  echo [X] Copy failed, putting the old one back
  move S:\EFI\OC\NootedGreen-old.kext S:\EFI\OC\Kexts\NootedGreen.kext >nul
  mountvol S: /D
  pause & exit /b 1
)
copy /y S:\EFI\OC\config.plist S:\EFI\OC\config-safemode-current.plist >nul
copy /y S:\EFI\OC\config-accel-current.plist S:\EFI\OC\config.plist >nul
dir /s S:\EFI\OC\Kexts\NootedGreen.kext > C:\temp\oclogs\ng-fix-check.txt
copy /y S:\EFI\OC\config.plist C:\temp\oclogs\esp-config.plist >nul
mountvol S: /D
echo.
echo DONE. Tell Claude before you reboot
pause
