@echo off
net session >nul 2>&1
if errorlevel 1 (
  echo [X] Run this as admin: right click ^> Run as administrator
  pause & exit /b 1
)
if not exist F:\EFI\OC\Kexts\NootedGreen.kext\Contents\MacOS\NootedGreen (
  echo [X] Plug in the new stick so F: shows up
  pause & exit /b 1
)
if exist S:\ (
  echo [X] S: is in use, tell Claude
  pause & exit /b 1
)
mountvol S: /S
robocopy F:\EFI\OC\Kexts\NootedGreen.kext S:\EFI\OC\Kexts\NootedGreen.kext /E /NFL /NDL /NJH /NJS
if not exist S:\EFI\OC\Kexts\NootedGreen.kext\Contents\MacOS\NootedGreen (
  echo [X] Copy failed, tell Claude
  mountvol S: /D
  pause & exit /b 1
)
dir /s S:\EFI\OC\Kexts\NootedGreen.kext > C:\temp\oclogs\nootedgreen-check.txt
mountvol S: /D
echo.
echo DONE. NootedGreen is back on the SSD. Tell Claude
pause
