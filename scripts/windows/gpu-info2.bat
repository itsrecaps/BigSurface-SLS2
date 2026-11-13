@echo off
powershell -NoProfile -Command "Get-ChildItem 'HKLM:\SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}' -ErrorAction SilentlyContinue | ForEach-Object { $p = Get-ItemProperty $_.PSPath -ErrorAction SilentlyContinue; if ($p.DriverDesc) { '=== ' + $_.PSChildName + ' ' + $p.DriverDesc; $p.PSObject.Properties | Where-Object { $_.Name -match 'Memory|Segment|DVMT|Stolen|Aperture|qw' } | ForEach-Object { $_.Name + ' = ' + ($_.Value -join ',') } } } | Out-File C:\temp\gpu2.txt"
echo DONE. Tell Claude
pause
