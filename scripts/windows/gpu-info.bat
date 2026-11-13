@echo off
echo Grabbing GPU info, takes like 20 sec...
dxdiag /t C:\temp\dxdiag.txt
powershell -NoProfile -Command "Get-CimInstance Win32_VideoController | Format-List Name,AdapterRAM,PNPDeviceID,DriverVersion | Out-File C:\temp\gpu.txt"
timeout /t 20 >nul
echo DONE. Tell Claude
pause
