#!/usr/bin/env python3
# Adds two OpenCore Kernel->Patch entries to config.plist (path in argv[1], written to argv[2]):
# XCPM's HWP request builder (kernel 24.6.0, macOS 15.8.1) -> desired 0 (hardware chooses) and max 0x40 (real ceiling).
import plistlib, sys
src, dst = sys.argv[1], sys.argv[2]
c = plistlib.load(open(src, 'rb'))
P = [
 ("GU605 XCPM HWP request: desired 0 (hardware chooses), max 0x40 (shared request; was max 0x33 = 4.0 GHz in ~78 MHz units)",
  "89f0c1e3100fb6f0c1e6184889d048c1e008", "89f031db900fb6f0c1e618b8004000006690"),
 ("GU605 XCPM HWP request: desired 0 (per-CPU request; XCPM left most P-cores at 0.6-0.9 GHz under load)",
  "478b8418c000000041c1e11041c1e018", "478b8418c00000004531c99041c1e018"),
]
pl = c['Kernel']['Patch']
pl[:] = [e for e in pl if not str(e.get('Comment', '')).startswith('GU605 XCPM HWP')]
for com, f, r in P:
    assert len(f) == len(r)
    pl.append({'Arch': 'x86_64', 'Base': '', 'Comment': com, 'Count': 1, 'Enabled': True, 'Find': bytes.fromhex(f),
               'Identifier': 'kernel', 'Limit': 0, 'Mask': b'', 'MaxKernel': '24.6.99', 'MinKernel': '24.6.0',
               'Replace': bytes.fromhex(r), 'ReplaceMask': b'', 'Skip': 0})
plistlib.dump(c, open(dst, 'wb'))
c2 = plistlib.load(open(dst, 'rb'))
assert [e for e in c2['Kernel']['Patch'] if str(e.get('Comment','')).startswith('GU605 XCPM HWP')] == pl[-2:]
for k in c:
    if k != 'Kernel': assert c2[k] == c[k], k
print('patches now:', [(e['Comment'][:40], e['Enabled']) for e in c2['Kernel']['Patch']])
