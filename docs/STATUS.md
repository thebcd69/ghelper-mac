# STATUS

Last update: 2026-11-04 (session 2)

## Done
- Research: DSDT decompiled/analysed; IDs, byte formats, PL ranges, brightness path → `docs/RESEARCH.md`.
- Kext `kext/ASUSATKD` (IOKit, Lilu-free): matches PNP0C14 with `_UID "ATK"`, DSDT OEM id/revision check,
  EC/AML signature check, whitelisted user client (DSTS reads, fan curves, PL1/PL2, mode), in-kernel validation,
  rate limit, `-atkdoff` / `-atkdro` / `-atkdforce`, `_WED` queue draining via IOTimerEventSource.
  CI-linked as MH_KEXT_BUNDLE x86_64; only public KPIs (build fails on known private ones).
- App `app/GHelperMac` (SwiftUI menu bar, macOS 13+, x86_64): mode, CPU temp, fan RPM, PL1/PL2 from EC,
  8-point CPU/GPU curve editor, PL sliders (AC/DC ranges), per-mode profiles, reapply on login/wake/mode change/
  FMAE drop (limited), "Varsayılana dön", login item (SMAppService), brightness slider (NullMoth IODisplay param).
- CI `.github/workflows/build.yml` (macos-14, Xcode 15.4) green; prereleases `v0.0.<run>-<sha>` with zip
  (kext + app + KURULUM.md + LICENSE).
- Docs: `docs/KURULUM.md` (Turkish install/test/backout/log meanings, brightness §9), README with attribution.

## Hardware test 1 (2026-11-04, v0.0.4-5df5737, `-atkdro`)
- Kext attached to `\_SB.ATKD`, `ATKD-DSDTMatch = Yes`, `WritesEnabled = No` (expected, `-atkdro`), app user client opened.
- **`ATKD-ECReadable = No`**: `evaluateInteger()` fails on the ERAM FieldUnits (EC0 nub exists at LPCB/EC0)
  → mode, PL1/PL2, FMAE unreadable; RPM only via DSTS (100 RPM steps).
- v0.0.5 (this session): ERAM/ERM2 mapped read-only (offsets from the DSDT Field lists, only with dsdtMatch),
  battery charge limit (DEVS 0x00120057, 40–100 %, runtime DSTS presence check, re-applied at launch/wake),
  ABI 2 (selector 8, flags ChargePresent/ERAMMapped, info.chargeLimit, EC field RSOC). See KURULUM §10–11.

## Not verified (no hardware access)
- Nothing has run on the real machine. First boot test per KURULUM §3 (read-only) is the next step.

## Exact next step (for a fresh session)
1. Get the owner's results: `log show --last boot --style compact --predicate 'eventMessage CONTAINS "ATKD:"'`
   and the app's diagnostics text.
2. If `_UID`/probe fails (no attach line): check `ioreg -l -w0 -p IOACPIPlane | grep -A5 ATKD`; adjust probe.
3. If `ec=0`: EC field reads via evaluateInteger don't work → PL/mode/FMAE readback unavailable; consider
   reading ERAM through a mapped IOMemoryDescriptor at 0xFE108300 (read-only) instead.
4. If `modeId=0` / mode DEVS returns 0xFFFFFFFE: mode write through SMI not available → hide mode picker.

## Risks / open points
- 0x00120075 (performance mode) is routed to SMI on this BIOS (not visible in AML) — runtime check only.
- `evaluateInteger` on EC FieldUnits (FTBL, PL01, PL02, FMAE, FRMS, CTP0..) assumed to work (ACPICA returns field
  values for non-method objects); if not, mode/PL readback and the PL fan-curve gate fall back (see step 3).
- PL "BIOS values" re-written when only custom fans are active (G-Helper PowerLimit Fix); fallback 80/80 W if
  never captured — owner decision whether to prefer a lower fallback.
- Static fan floor (≥70 °C ≥20 %, ≥90 °C ≥40 %) may reject a factory Silent curve; app then shows an error.
- fPPT (0x001200C1) not settable (DEVS → 0xFFFFFFFE) → not implemented. No mid fan.
- Brightness: depends on NullMoth `nvrmacpibl=1` + NBCI `_DSM` in OEM SSDT (unverified); NullMoth warns this
  `_DSM` call hung GPU start on some laptops.
