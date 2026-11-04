# ghelper-mac

Minimal G-Helper-like tool for **one** machine: ASUS ROG Zephyrus G16 **GU605MY** (BIOS GU605MY.329)
running macOS Sequoia (x86_64, OpenCore). Not for any other model.

Features: performance mode (Silent/Balanced/Turbo), custom 8-point CPU/GPU fan curves (no mid fan on this
model), CPU PL1 (SPL) / PL2 (SPPT). fPPT is not settable on this BIOS. Shows CPU temp and fan RPM.

- `kext/ASUSATKD` — Lilu-free IOKit driver for `\_SB.ATKD` (PNP0C14, `_UID "ATK"`), strictly whitelisted
  user client with in-kernel validation.
- `app/GHelperMac` — SwiftUI menu-bar app (macOS 13+).
- `docs/KURULUM.md` — installation / first test (Turkish). `docs/RESEARCH.md` — IDs, byte formats, DSDT evidence.
  `docs/STATUS.md` — current state.

## Attribution / license
GPL-3.0 (see LICENSE). Protocol knowledge and behaviour derived from
[G-Helper](https://github.com/seerge/g-helper) by seerge (GPL-3.0) and the Linux `asus-wmi` / `asus-armoury`
drivers. Not affiliated with ASUS.

## Build
CI (`.github/workflows/build.yml`, macos-14) builds both and publishes a prerelease `v0.0.<run>-<sha>`.
Locally on a Mac: `kext/ASUSATKD/build.sh <MacKernelSDK> build` and `app/GHelperMac/build.sh build`.
Kernel syntax check on Linux:
`clang++ -fsyntax-only -target x86_64-apple-macos10.12 -std=gnu++17 -nostdinc -nostdinc++ -fno-builtin -fapple-kext -mkernel -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT -D__MACH__ -I MacKernelSDK/Headers kext/ASUSATKD/ASUSATKD.cpp`
