# STATUS

Last update: 2026-11-04 (session 1)

## Done
- DSDT decompiled and analysed; all IDs, byte formats, PL ranges recorded in `docs/RESEARCH.md`.
- LICENSE (GPL-3.0, from G-Helper).

## In progress
- Kext `kext/ASUSATKD` (IOKit, Lilu-free).

## Exact next step
- Write kext sources + Info.plist + build script; syntax-check with MacKernelSDK clang command in README.

## Risks / open points
- 0x00120075 (performance mode) goes through SMI on this BIOS (not visible in AML) — verify at runtime.
- fPPT (0x001200C1) is not settable (DEVS returns 0xFFFFFFFE) → not implemented.
- No mid fan on this model (0x00110031/0x00110032 unsupported).
