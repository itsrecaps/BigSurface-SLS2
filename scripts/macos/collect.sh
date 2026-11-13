#!/bin/zsh
# Saves keyboard / battery / trackpad test results onto the OPENCORE stick for Claude
OUT=/Volumes/OPENCORE/sls2-test
mkdir -p $OUT
ioreg -lw0 -c SurfaceTHCProbe > $OUT/thc.txt 2>&1
ioreg -lw0 -r -c IOPCIDevice -k IOPCIMatch > /dev/null 2>&1
ioreg -lw0 | grep -iE -B2 -A12 "VoodooSerial|BigSurface|SurfaceSerialHub|SurfaceBattery|SurfaceHID|MSHW0084" > $OUT/bigsurface.txt 2>&1
kextstat 2>/dev/null | grep -iE "voodoo|bigsurface|surfacethc|nootedgreen" > $OUT/kexts.txt
pmset -g batt > $OUT/battery.txt 2>&1
log show --last boot --style compact --predicate 'eventMessage CONTAINS[c] "VoodooSerial" OR eventMessage CONTAINS[c] "BigSurface" OR eventMessage CONTAINS[c] "SurfaceTHC" OR eventMessage CONTAINS[c] "SSAM" OR eventMessage CONTAINS[c] "Surface"' > $OUT/log.txt 2>&1
ls /Library/Logs/DiagnosticReports/*panic* > $OUT/panics.txt 2>&1
echo "saved to $OUT"
ls -la $OUT
