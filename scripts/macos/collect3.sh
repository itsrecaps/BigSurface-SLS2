#!/bin/zsh
# Round 3: keyboard / battery after the GPIO + SSH fix
OUT=/Volumes/OPENCORE/sls2-test3
mkdir -p $OUT
kextstat 2>/dev/null | grep -iE "voodoo|bigsurface|surface" > $OUT/kexts.txt
ioreg -lw0 -p IOACPIPlane | grep -E "\+-o (SSH|GPI0)" > $OUT/acpi-nodes.txt
ioreg -lw0 -r -n SSH > $OUT/ssh.txt 2>&1
ioreg -lw0 -r -n GPI0 > $OUT/gpio.txt 2>&1
ioreg -lw0 -r -c SurfaceSerialHubDriver > $OUT/hub.txt 2>&1
ioreg -lw0 -c AppleACPIPlatformExpert | grep -i "ACPI Tables" > $OUT/acpi-tables.txt
pmset -g batt > $OUT/battery.txt 2>&1
/usr/bin/log show --last boot --style compact --predicate 'process == "kernel" AND (eventMessage CONTAINS[c] "surface" OR eventMessage CONTAINS[c] "voodoo" OR eventMessage CONTAINS[c] "gpio" OR eventMessage CONTAINS[c] "uart" OR eventMessage CONTAINS[c] "SSH" OR eventMessage CONTAINS[c] "ACPI")' > $OUT/log-kernel.txt 2>&1
echo "saved to $OUT"; ls -la $OUT
