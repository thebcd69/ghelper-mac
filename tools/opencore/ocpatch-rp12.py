#!/usr/bin/env python3
# Adds ACPI->Add entry SSDT-RP12PWR.aml (enabled) to an OpenCore config.plist. Idempotent.
# Usage: ocpatch-rp12.py <config.plist in> <config.plist out>
import plistlib, sys
src, dst = sys.argv[1], sys.argv[2]
with open(src, 'rb') as f:
    c = plistlib.load(f)
add = c['ACPI']['Add']
if any(e.get('Path') == 'SSDT-RP12PWR.aml' for e in add):
    for e in add:
        if e.get('Path') == 'SSDT-RP12PWR.aml':
            e['Enabled'] = True
else:
    add.append({'Comment': 'GU605: RP12 POFF/PON/NCHK (Eco: dGPU power off; MtlP_Rvp does not load in macOS)',
                'Enabled': True, 'Path': 'SSDT-RP12PWR.aml'})
with open(dst, 'wb') as f:
    plistlib.dump(c, f, fmt=plistlib.FMT_XML, sort_keys=False)
print('ACPI Add entries:', len(add))
