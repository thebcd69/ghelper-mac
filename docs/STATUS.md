# STATUS

Last update: 2026-11-04 (session 1)

## Done
- DSDT decompiled and analysed; all IDs, byte formats, PL ranges recorded in `docs/RESEARCH.md`.
- LICENSE (GPL-3.0, from G-Helper).

- Kext sources `kext/ASUSATKD` (ASUSATKD.cpp/.hpp, ASUSATKDUserClient.cpp, ATKDShared.h ABI, Info.plist,
  kmod_info.c, build.sh). Passes clang -fsyntax-only against MacKernelSDK (not yet linked on CI).

- App sources `app/GHelperMac` (ATKDClient/Model/AppState/Views/GHelperMacApp.swift, Info.plist, build.sh).
- CI workflow `.github/workflows/build.yml`, docs/KURULUM.md, README.md.

## In progress
- First CI run (milestone commit). Swift never compiled locally (no toolchain) — fix CI errors.

## Exact next step
- `gh api repos/thebcd69/ghelper-mac/actions/runs?per_page=5`; read failing job log via
  `gh api repos/thebcd69/ghelper-mac/actions/jobs/<id>/logs`; fix; repeat until green.
- Then: brightness research (coordinator request, lower priority) — see bottom.

## Risks / open points
- 0x00120075 (performance mode) goes through SMI on this BIOS (not visible in AML) — verify at runtime.
- fPPT (0x001200C1) is not settable (DEVS returns 0xFFFFFFFE) → not implemented.
- No mid fan on this model (0x00110031/0x00110032 unsupported).

## Brightness (owner request)
- Researched (RESEARCH.md §9): only feasible path = NullMoth's own opt-in `nvrmacpibl=1` (NBCI _DSM -> NVKMS
  backlight -> AppleBacklightDisplay). App has a menu slider via IODisplay "brightness" (min 5 %), hidden if absent.
  Owner must add the boot-arg and test; NBCI support for this panel unconfirmed.
