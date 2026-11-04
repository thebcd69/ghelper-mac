# STATUS

Last update: 2026-11-04 (session 1)

## Done
- DSDT decompiled and analysed; all IDs, byte formats, PL ranges recorded in `docs/RESEARCH.md`.
- LICENSE (GPL-3.0, from G-Helper).

- Kext sources `kext/ASUSATKD` (ASUSATKD.cpp/.hpp, ASUSATKDUserClient.cpp, ATKDShared.h ABI, Info.plist,
  kmod_info.c, build.sh). Passes clang -fsyntax-only against MacKernelSDK (not yet linked on CI).

## In progress
- Menu-bar app `app/GHelperMac` (SwiftUI) + CI workflow.

## Exact next step
- Write app sources, .github/workflows/build.yml, push milestone commit (no [skip ci]) and get CI green.
- Then: brightness research (coordinator request, lower priority) — see bottom.

## Risks / open points
- 0x00120075 (performance mode) goes through SMI on this BIOS (not visible in AML) — verify at runtime.
- fPPT (0x001200C1) is not settable (DEVS returns 0xFFFFFFFE) → not implemented.
- No mid fan on this model (0x00110031/0x00110032 unsupported).

## Queued (owner request, lower priority)
- Internal OLED brightness (Samsung ATNA60DL01-0, eDP, panel on NVIDIA via MUX, NullMoth driver):
  research ACPI _BCM/_BCL on dGPU, NVIDIA WMI-EC backlight, DPCD AUX; implement if feasible.
