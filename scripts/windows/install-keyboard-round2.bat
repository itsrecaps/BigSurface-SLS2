@echo off
net session >nul 2>&1
if errorlevel 1 (
  echo [X] Run this as admin: right click ^> Run as administrator
  pause & exit /b 1
)
for %%F in (C:\temp\BigSurface-SLS2.kext.zip C:\temp\SSDT-SSH.aml C:\temp\config-kbd2.plist) do if not exist %%F (
  echo [X] %%F missing, tell Claude
  pause & exit /b 1
)
if exist C:\temp\bs rmdir /s /q C:\temp\bs
powershell -NoProfile -Command "Expand-Archive -Force C:\temp\BigSurface-SLS2.kext.zip C:\temp\bs"
if not exist C:\temp\bs\BigSurface.kext\Contents\PlugIns\VoodooGPIO.kext\Contents\MacOS\VoodooGPIO (
  echo [X] Unzip failed, tell Claude
  pause & exit /b 1
)
if exist S:\ (
  echo [X] S: is in use, tell Claude
  pause & exit /b 1
)
mountvol S: /S
copy /y S:\EFI\OC\config.plist S:\EFI\OC\config-before-kbd2.plist >nul
rmdir /s /q S:\EFI\OC\Kexts\BigSurface.kext
robocopy C:\temp\bs\BigSurface.kext S:\EFI\OC\Kexts\BigSurface.kext /E /NFL /NDL /NJH /NJS
copy /y C:\temp\SSDT-SSH.aml S:\EFI\OC\ACPI\SSDT-SSH.aml >nul
copy /y C:\temp\config-kbd2.plist S:\EFI\OC\config.plist >nul
dir /s S:\EFI\OC\Kexts\BigSurface.kext S:\EFI\OC\ACPI > C:\temp\oclogs\kbd2-check.txt
mountvol S: /D
echo.
echo DONE. Tell Claude so it can check before you reboot
pause
