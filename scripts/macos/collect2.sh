#!/bin/zsh
# Round 2: why BigSurface (keyboard + battery) didn't load
OUT=/Volumes/OPENCORE/sls2-test2
mkdir -p $OUT
/usr/bin/log show --last boot --style compact --predicate 'process == "kernel" AND (eventMessage CONTAINS[c] "surface" OR eventMessage CONTAINS[c] "voodoo" OR eventMessage CONTAINS[c] "uart" OR eventMessage CONTAINS[c] "MSHW0084" OR eventMessage CONTAINS[c] "SSH")' > $OUT/log-kernel.txt 2>&1
/usr/bin/log show --last boot --style compact --predicate 'eventMessage CONTAINS[c] "BigSurface" OR eventMessage CONTAINS[c] "com.xavier"' > $OUT/log-bigsurface.txt 2>&1
ioreg -lw0 -p IOACPIPlane -n SSH > $OUT/acpi-ssh.txt 2>&1
ioreg -lw0 -r -n UA00 > $OUT/ua00.txt 2>&1
kmutil showloaded --list-only 2>/dev/null | grep -iE "surface|voodoo" > $OUT/loaded.txt
kmutil inspect -V release --kernel-collection /System/Library/KernelCollections/BootKernelExtensions.kc 2>/dev/null | head -0
echo "saved to $OUT"; ls -la $OUT
