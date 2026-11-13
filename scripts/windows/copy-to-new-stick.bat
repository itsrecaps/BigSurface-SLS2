@echo off
if not exist E:\EFI\OC\OpenCore.efi (
  echo [X] Old stick E: not found, plug it in
  pause & exit /b 1
)
if not exist F:\ (
  echo [X] New stick F: not found
  pause & exit /b 1
)
echo Copying EFI...
robocopy E:\EFI F:\EFI /E /NFL /NDL /NJH
echo Copying recovery...
robocopy E:\com.apple.recovery.boot F:\com.apple.recovery.boot /E /NFL /NDL /NJH
echo.
echo DONE. Tell Claude
pause
