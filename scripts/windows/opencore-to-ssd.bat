@echo off
setlocal enabledelayedexpansion
set LOG=C:\temp\opencore-to-ssd-log.txt
echo OpenCore to SSD > "%LOG%"

net session >nul 2>&1
if errorlevel 1 (
  echo [X] Run this as admin: right click the file ^> Run as administrator
  pause & exit /b 1
)

if not exist E:\EFI\OC\OpenCore.efi (
  echo [X] Plug in the OPENCORE stick first ^(E:\EFI\OC not found^)
  pause & exit /b 1
)

echo [1/6] Checking BitLocker...
manage-bde -status C: | findstr /i /c:"Protection On" >nul
if not errorlevel 1 (
  echo     BitLocker is on, pausing it for 2 reboots so it doesnt ask for the recovery key
  manage-bde -protectors -disable C: -RebootCount 2 >> "%LOG%" 2>&1
)

echo [2/6] Mounting the EFI partition as S:...
if exist S:\ (
  echo [X] S: is already used by something, tell Claude
  pause & exit /b 1
)
mountvol S: /S
if errorlevel 1 (
  echo [X] Couldnt mount the EFI partition
  pause & exit /b 1
)

echo [3/6] Checking free space...
for /f %%F in ('powershell -NoProfile -Command "[math]::Floor((Get-PSDrive S).Free/1MB)"') do set FREE=%%F
echo     Free: !FREE! MB >> "%LOG%"
echo     Free: !FREE! MB
if exist S:\EFI\OC (
  echo     Old OpenCore found, backing it up to S:\EFI\OC-old
  if exist S:\EFI\OC-old rmdir /s /q S:\EFI\OC-old
  move S:\EFI\OC S:\EFI\OC-old >> "%LOG%" 2>&1
)
if !FREE! LSS 70 (
  echo [X] Not enough room, need about 70MB. Tell Claude the number above
  mountvol S: /D
  pause & exit /b 1
)

echo [4/6] Copying OpenCore...
robocopy E:\EFI\OC S:\EFI\OC /E /NFL /NDL /NJH /NJS >> "%LOG%" 2>&1
if not exist S:\EFI\OC\OpenCore.efi (
  echo [X] Copy failed, tell Claude
  mountvol S: /D
  pause & exit /b 1
)
if not exist S:\EFI\OC\config.plist (
  echo [X] config.plist missing after copy, tell Claude
  mountvol S: /D
  pause & exit /b 1
)

echo [5/6] Adding OpenCore to the boot menu...
set GUID=
for /f "tokens=2 delims={}" %%G in ('bcdedit /copy {bootmgr} /d "OpenCore"') do set GUID={%%G}
if "!GUID!"=="" (
  echo [X] Couldnt make the boot entry, tell Claude
  mountvol S: /D
  pause & exit /b 1
)
echo     Entry: !GUID! >> "%LOG%"
bcdedit /set !GUID! path \EFI\OC\OpenCore.efi >> "%LOG%" 2>&1
bcdedit /set {fwbootmgr} displayorder !GUID! /addfirst >> "%LOG%" 2>&1

echo [6/6] Cleaning up...
mountvol S: /D

bcdedit /enum firmware >> "%LOG%" 2>&1
echo.
echo DONE. Unplug the stick and restart, OpenCore should show up by itself
echo Log saved to %LOG%
pause
