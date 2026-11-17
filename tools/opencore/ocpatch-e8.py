#!/usr/bin/env python3
# config.plist (argv[1] -> argv[2]): ACPI->Add APIC-BSP0.aml off, APIC-E8.aml on (same place in the list).
# APIC-E8 = APIC-BSP0 (BSP 0x20 first, the 12 P-core threads) + the 8 E-cores 0x00-0x0E as uid 12-19; LP-E cores left out.
import plistlib, sys
c = plistlib.load(open(sys.argv[1], 'rb'))
add = c['ACPI']['Add']
add[:] = [e for e in add if e.get('Path') != 'APIC-E8.aml']
idx = [i for i, e in enumerate(add) if e.get('Path') == 'APIC-BSP0.aml']
assert len(idx) == 1, idx
bsp = add[idx[0]]; bsp['Enabled'] = False
new = dict(bsp); new['Path'] = 'APIC-E8.aml'; new['Enabled'] = True
new['Comment'] = 'MADT: BSP 0x20 first, 12 P-core threads, + 8 E-cores (0x00-0x0E, uid 12-19); LP-E 0x40/0x42 left out (2026-11-17)'
add.insert(idx[0] + 1, new)
plistlib.dump(c, open(sys.argv[2], 'wb'))
c2 = plistlib.load(open(sys.argv[2], 'rb'))
for k in c:
    if k != 'ACPI': assert c2[k] == c[k], k
print('ACPI Add:', [(e['Path'], e['Enabled']) for e in c2['ACPI']['Add']])
