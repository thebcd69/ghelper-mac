# RESEARCH — GU605MY ATKD / WMNB facts

Target: ASUS ROG Zephyrus G16 **GU605MY**, BIOS GU605MY.329, Core Ultra 9 185H, RTX 4090 Laptop.
Everything below is for this one machine. Nothing here is meant to generalise.

Line numbers refer to `DSDT.dsl` produced by `iasl -d` (ACPICA 20230628) from
`EFI/OC/ACPI/DSDT.aml` (the table OpenCore actually injects: `ACPI > Add > DSDT.aml`, Enabled=true).
DSDT header: `DefinitionBlock ("", "DSDT", 2, "_ASUS_", "Notebook", 0x01072009)`, length 0x6A237.

## 1. The WMI device

| Item | Value | Evidence |
|---|---|---|
| Device | `\_SB.ATKD` | L77151 |
| `_HID` | `"PNP0C14"` (string) | L77153 |
| `_UID` | `"ATK"` (string) | L77154 |
| `_WDG` GUID #1 | `97845ED0-4E6D-11DE-8A39-0800200C9A66`, object id `"NB"`, flags 0x02 (method) → `WMNB` | L77229-77231 |
| `_WDG` GUID #2 | `0B3CBB35-E3C2-45ED-91C2-4C5A6D195D1C`, notify id 0xFF → `_WED(0xFF)` | L77231-77233 |
| ATK0100 | **absent** → AsusSMC (hieplpvip) cannot be used | grep |

Other PNP0C14 devices exist (L49040, L49241, `\UAT2` `_UID "AHWM"` L78938) — the kext must check `_UID == "ATK"`.

### WMNB(Arg0, Arg1, Arg2) — L77355
```
CreateDWordField (Arg2, 0x00, IIA0)   // device id
CreateDWordField (Arg2, 0x04, IIA1)
CreateDWordField (Arg2, 0x08, IIA2)
CreateDWordField (Arg2, 0x0C, IIA3)
CreateDWordField (Arg2, 0x10, IIA4)   // => Arg2 MUST be a Buffer of >= 20 bytes
Local0 = Arg1 & 0xFFFFFFFF            // method id
```
* Arg0 = WMI instance (0). Arg1 = method id. Arg2 = Buffer. Linux passes `struct bios_args` = 6×u32 = **24 bytes**
  (`asus-wmi.c` L181). Our kext always passes a zero-padded 24-byte buffer.
  A shorter buffer would make `CreateDWordField(…,0x10,…)` raise AE_AML_BUFFER_LIMIT.
* Method ids handled: `INIT 0x54494E49` (sets `ATKP=1`, L77248/77364), `BSTS`, `SFUN` (returns 0x21),
  `SCDG`, `VERS`, `MWGF`, `GLCD`, `WDOG`, `OSVR`, `ANVI`, **`DSTS 0x53545344` (L77415)**, **`DEVS 0x53564544` (L78367)**.
* **INIT is NOT required** for DSTS/DEVS: neither branch tests `ATKP`. INIT only enables hotkey
  notifications (EC `_Qxx` → `IANE` gated by `If (ATKP)`). We do **not** call INIT (calling it would change
  how the BIOS routes Fn keys and fill the 16-entry `ATKQ` queue, whose producer `IANQ` busy-waits up to 1 s
  when full).
* Unknown ids: DSTS → `WCHK(0,id)`→ SMI (`W15H`, `WSCP=0xAA`), DEVS → `WCHK(1,id)` → SMI. Return
  `0xFFFFFFFE` = unsupported.
* DSTS return convention: bit 16 (`0x00010000`) = "present", low 16 bits = value.
* DEVS return: `1` = OK, `0xFFFFFFFE` = unsupported.

## 2. Device ids — confirmed vs not

| ID | Name (Linux / G-Helper) | DSTS in this DSDT | DEVS in this DSDT | Use |
|---|---|---|---|---|
| 0x00110013 | CPU fan RPM (`CPU_FAN_CTRL` / `CPU_Fan`) | L77817: `(0x0020E6DA / EC0.FRMS) / 100 \| 0x10000` → value in **hundreds of RPM** | L78543 (reads FTBL, returns 1; harmless) | read |
| 0x00110014 | GPU fan RPM | L77835: same with `EC0.FRS2` | L78561 | read |
| 0x00110031 | Mid fan RPM | **0xFFFFFFFE** (L78720 area for DEVS; DSTS L78101) | – | **no mid fan** |
| 0x00110024 | CPU fan curve | L78041: IIA1=0 → `EC0.CFCV` (CMB0..F), 1 → `CSFV` (CSB0..F), 2 → `CTFV` (CTB0..F); 32-byte buffer, 16 bytes used | **L78694**: `FMAE=1; SFTC(IIA1..IIA4); CPUP(0x140); SMSM(); Sleep 100; Notify(NPCF,0xC0); Sleep 100; WMSH(); ODV0=4; Notify(IETM,0x88); Return 1` | read default / write |
| 0x00110025 | GPU fan curve | L78061: 0 → `GFCV`, 1 → `GSCV`, 2 → `GTCV` | **L78709**: `SFTG(IIA1..IIA4); Return 1` (no FMAE) | read default / write |
| 0x00110032 | Mid fan curve | **0xFFFFFFFE** (L78106) | **0xFFFFFFFE** (L78725) | **not present** |
| 0x00110026 | CPU fan floor (G-Helper `DevsCPUFanFloor`) | L78081 → `EC0.CFLB` (CBD0..7, 8 bytes) | – | read (validation) |
| 0x00110027 | GPU fan floor | L78086 → `EC0.GFLB` (GBD0..7) | – | read (validation) |
| 0x00120094 | CPU temp | L77903: `EC0.CTMP \| 0x10000` (°C) | – | read |
| 0x00120097 | GPU temp | 0xFFFFFFFE (L78204) | – | not available |
| **0x001200A3** | **PL1 / SPL** (`PPT_PL1_SPL`, G-Helper `PPT_APUA3`) | L78239: `0x00010000` (present, value not reported) | **L78773: `EC0.PL01 = IIA1`** | write |
| **0x001200A0** | **PL2 / SPPT** (`PPT_PL2_SPPT`, G-Helper `PPT_APUA0`) | L78224: `0x00010000` | **L78757: `EC0.PL02 = IIA1`** | write |
| 0x001200A1 | CPU EDC (G-Helper `PPT_EDCA1`) — **not PL2** | 0xFFFFFFFE | 0xFFFFFFFE | – |
| 0x001200C1 | fPPT (`PPT_PL3_FPPT`) | L78259: `0x00010000` | **L78813: 0xFFFFFFFE** | **not settable → fPPT N/A on this Intel BIOS** |
| 0x001200C0 / C2 | NV dynamic boost / NV temp target | present | L78789 / L78818 | out of scope |
| 0x00120075 | Throttle thermal policy (`PerformanceMode`) | not in AML list → `WCHK(0,…)` → SMI | not in AML list → `WCHK(1,…)` → SMI | **unconfirmable statically**; G-Helper/Linux use it on GU605; kext checks DSTS presence bit at runtime |
| 0x00110019 | Vivobook thermal policy | not present (→ SMI) | – | not used |

> Note: the task brief listed PL1=0x001200A0 / PL2=0x001200A1. The DSDT, Linux `asus-wmi.h`
> (`PPT_PL2_SPPT 0x001200A0`, `PPT_PL1_SPL 0x001200A3`) and G-Helper (`PPT_APUA0` = sPPT/PL2,
> `PPT_APUA3` = SPL/PL1) all agree: **PL1 = 0x001200A3, PL2 = 0x001200A0**. 0x001200A1 is unsupported here.

## 3. Fan curve byte format

Write (DEVS 0x00110024 / 0x00110025), `EC0.SFTC/SFTG(Arg0..Arg3)` L81155/L81175:
```
IIA1 = t0 | t1<<8 | t2<<16 | t3<<24      -> CTP0..CTP3   (°C)
IIA2 = t4 | t5<<8 | t6<<16 | t7<<24      -> CTP4..CTP7
IIA3 = p0 | p1<<8 | p2<<16 | p3<<24      -> SCT0..SCT3   (fan %, 0..100)
IIA4 = p4 | p5<<8 | p6<<16 | p7<<24      -> SCT4..SCT7
```
i.e. the WMNB Arg2 buffer is `[devid u32][16 bytes: 8 temps, 8 percents][4 zero bytes]` — identical to
G-Helper `DeviceSet(id, byte[16])` and Linux `fan_curve_write()` (which converts pwm 0-255 → %).

Read (DSTS id, IIA1=mode) returns a 32-byte Buffer: bytes 0..7 temps, 8..15 percents (factory curve for
that mode). Mode index for curve reads is **swapped** vs the thermal policy (Linux
`fan_curve_get_factory_default`: "switched for OVERBOOST vs SILENT"; G-Helper `GetFanCurve`):
policy Balanced 0 → idx 0 (`CFCV`, "M"), Turbo 1 → idx 2 (`CTFV`, "T"), Silent 2 → idx 1 (`CSFV`, "S").

Current custom curve is readable straight from EC RAM (SystemMemory, no EC transactions):
`EC0.CTP0..7, SCT0..7, GTP0..7, SGT0..7` in `ERM2` @ 0xFE108B00 (L75924).

### Validation rules used (kext + app)
* 8 points; point i temperature in **[30+10i, 40+10i] °C** (G-Helper `FixFanCurve` with default
  `fan_clamp` — the curve G-Helper sends to this model), strictly increasing. Covers 30..110 °C.
* percent 0..100, non-decreasing (Linux `fan_curve_validate`).
* percent[i] ≥ BIOS floor[i] (DSTS 0x00110026/27; G-Helper plots floor[i] at 30+10i °C).
* Extra static floor (ours): any point ≥ 70 °C must be ≥ 20 %, any point ≥ 90 °C must be ≥ 40 %.

## 4. Performance mode

EC field `EC0.FTBL` (ERAM @0xFE108300 + 6, L75657) holds the active thermal table; `EC0.SFMN` L81504:
`FTBL 0` → `SPDM` (Balanced), `1` → `STDM` (Turbo, NV AMAT 0xA0 / ACBT 0x78), `2` → `SSSM` (Silent, GPU temp
target 75 °C). Matches G-Helper `AsusMode {Balanced=0, Turbo=1, Silent=2}`. Mode change → EC `_QD4` → `SFMN`.
Kext reads mode from `FTBL` (memory-mapped, read-only) and from DSTS 0x00120075 if present.
Writes allowed: 0, 1, 2 only (no 3 FullSpeed / 4 Manual).

Fan curves are **cleared by a mode change** (Linux `throttle_thermal_policy_write` disables curves;
G-Helper reapplies `AutoFans()` 100 ms after `SetPerformanceMode`, then `AutoPower()` 1000 ms later).
`FMAE` (ERAM 0xCA bit 2, L75887) = "fan manual enable", set by the CPU-curve DEVS; PEP deep-standby
entry copies it to `MNMS` (L53034).

## 5. Power limits (PL1/PL2)

* Written as plain bytes into `EC0.PL01` (ERAM+0x9B) / `EC0.PL02` (ERAM+0x9D) → watts 0..255, EC applies.
* DSTS does not report current values (always `0x00010000`); kext reads `PL01`/`PL02` directly from EC RAM
  (read-only) so the sliders can start from the BIOS values. `PL1V`/`PL2V` (ERM2 @0xFE108B00 +0x8E/0x8F) are also read
  for diagnostics; meaning unconfirmed.
* **Ranges (source: Linux `drivers/platform/x86/asus-armoury.h`, entry `DMI_BOARD_NAME "GU605M"`,
  which matches GU605MY by substring):**

| | PL1 (SPL) | PL2 (SPPT) |
|---|---|---|
| AC | 28 – 90 W | 28 – 135 W |
| DC (battery) | 25 – 35 W | 38 – 53 W |

  No `_def` given → Linux treats default = max. G-Helper generic bounds for non-HX Intel: MinTotal 5,
  MaxTotal 150, DefaultTotal 80 (not model-specific; Linux table is stricter → we use Linux).
  Kext hard clamp: PL1 25..90, PL2 28..135 (union of AC+DC mins, AC max). App: AC or DC range by power source,
  and PL2 ≥ PL1.
* `requires_fan_curve = true` for GU605M in asus-armoury: "PPT changes require an active fan curve"
  (asus-armoury.c L218). G-Helper agrees: `IsFanRequired()` contains GU605 → `AutoFans(true)` before
  `SetPower()`. And `IsPowerRequired()` contains "GU605M": after applying custom fans without custom power,
  G-Helper forces `A0=80, A3=80` ("PowerLimit Fix"). → Kext refuses PL writes unless a CPU curve was
  written since the last mode change; app always writes curves first, then PL1 (A3) then PL2 (A0)
  (G-Helper order).

## 6. Fan RPM / temperature
* RPM ≈ (DSTS 0x00110013 & 0xFFFF) × 100 (G-Helper `GetFan`; invalid if > 120).
  Exact: RPM = 2156250 / `EC0.FRMS` (0x0020E6DA = 2156250), FRMS 16-bit in ERAM.
* CPU temp: DSTS 0x00120094 low byte = `EC0.CTMP` °C. (SMCProcessor also exposes package temp via SMC.)

## 7. Reset to defaults
* Fans: re-issue DEVS 0x00120075 with the current mode (Linux `fan_curve_enable_store` case 2/3 with throttle
  policy dev → `throttle_thermal_policy_write`; G-Helper "Factory defaults" re-applies the mode).
* PL: same mode re-write; whether SMM restores PL01/PL02 is **unconfirmed** — app re-reads PL01/PL02
  afterwards and shows them.

## 8. Notifications
`Notify(ATKD, 0xFF)` (L77224) after `IANQ` enqueues into 16-slot `ATKQ`. Windows/Linux WMI then calls
`_WED(0xFF)` (L77238) → `GANQ` dequeues. Kext does the same on `kIOACPIMessageDeviceNotification` (0xFF)
so the queue never fills; last event code is exposed for diagnostics only.

## 9. Internal OLED brightness (Samsung ATNA60DL01-0, eDP, panel on NVIDIA via MUX)

Findings (2026-11-04):
* **ASUS ATKD has no backlight path here.** No brightness device id is used by WMNB for the panel; EC `_Q` brightness
  hotkeys only call `BRTN(0x86/0x87)` (L30046), which `Notify`s `\_SB.PC00.GFX0.DD1F/DD2F`,
  `…RP12.PXSX.PEGB.PEGE.DEDP` and `…RP12.PXSX.EDP1` (CondRefOf) — notifications, not control.
* **ACPI `_BCM/_BCL` exists only on `\_SB.PC00.RP12.PXSX.PEGB.PEGE.DEDP`** (L27553, L27639-27763), inside
  `If (DGBA != Zero)` — an Intel discrete-graphics (DG) template. Its `_BCM` writes `BCLP`/`ASLC` (Intel OpRegion
  ASLE mailbox, external `\_SB.PC00.RP12.PXSX.BCLP/ASLC`). Only an Intel graphics driver services that mailbox;
  with an NVIDIA GPU at RP12 it does nothing (and the device is likely absent). → not usable.
* **NVIDIA WMI-EC backlight (Linux `nvidia-wmi-ec-backlight`, WMI GUID 603E9613-EF25-4338-A3D0-C46177516DB7 /
  `WMAA`) is not present**: DSDT `_WDG`s are ATKD (97845ED0…), `\_SB.WFDE`/`WFTE` (ASUS test devices) and
  `\UAT2` (AHWM). No `WMAA` method. → not applicable.
* **DPCD AUX from our kext is not safe**: the panel is driven by NVIDIA's GSP firmware through NullMoth's NVRM
  (open-gpu-kernel-modules 615.78.08 + NVKMS). AUX transactions from a second driver would race GSP/NVKMS link
  management. DDC/CI on an internal eDP OLED is not implemented by such panels. → rejected.
* **The feasible path already exists in NullMoth 2.6.6** (verified in tag `v2.6.6`):
  `kexts/NVRM/os-xnu2.cpp` implements `nv_acpi_dsm_method`/`nv_acpi_dod_method` for the **NBCI `_DSM`**
  (GUID D4A50B75-65C7-46F7-BFB7-41514CEA0244, functions GETOBJBYTYPE 0x10, GETBACKLIGHT 0x14) and `_DOD` on the GPU's
  ACPI node — **only with boot-arg `nvrmacpibl=1`** (opt-in since NullMoth 2.0.3 because on some laptops the
  `_DSM` call hung GPU start). With it, RM answers `NV0073_CTRL_CMD_SPECIFIC_GET_BACKLIGHT_BRIGHTNESS`, NVKMS calls
  `nvkms_register_backlight()`, and `kexts/NVRMFB/fb/nvrm-fb.cpp::publishBacklight()` publishes a "backlight"
  service → `AppleBacklightDisplay` → native macOS brightness slider and keys (BrightnessKeys.kext). NVRMFB maps
  logical 0 → hardware 5 % (never fully black) and persists the level in NVRAM `/options:nullmoth-backlight`.
  The owner's current boot-args do **not** contain `nvrmacpibl=1` → explains "no brightness control".
* The NBCI `_DSM` is not in the DSDT; it would be in an OEM SSDT (OC log lists 41 firmware tables, e.g. one with
  OEM table id `OemNv2T`, 12766 bytes). Not available here → whether GETBACKLIGHT is implemented for this panel is
  **unconfirmed** until the owner tests. NullMoth logs: `NVRM-xnu: NBCI _DSM 0x14 -> …`, `NVRM-xnu: backlight
  registered: …`, `fb0: panel backlight at N% …` or `fb0: internal panel, but NVKMS registered no backlight`.
* Our app: a brightness slider in the menu that uses the public `IODisplayGet/SetFloatParameter("brightness")` on
  the display NullMoth publishes; hidden (with a hint) when no such parameter exists; never requests < 5 %.
  No kext code is involved.

Sources: nullmoth/nvidia-macos-driver tag v2.6.6 `kexts/NVRM/os-xnu2.cpp`, `kexts/NVRMFB/fb/nvrm-fb.cpp`,
main `docs/NVRMFB-BACKLIGHT-POWER.md`, `CHANGELOG.md` (2.0.3, 2.3.2, 2.6.3); Linux `nvidia-wmi-ec-backlight.c` (GUID).

## Sources
* DSDT: `EFI/OC/ACPI/DSDT.aml` (owner's USB), decompiled locally.
* G-Helper `app/AsusACPI.cs`, `app/Mode/ModeControl.cs`, `app/AppConfig.cs` (seerge/g-helper main, fetched 2026-11-04).
* Linux master `drivers/platform/x86/asus-wmi.c`, `asus-armoury.c`, `asus-armoury.h`,
  `include/linux/platform_data/x86/asus-wmi.h` (fetched 2026-11-04).

## 12. GPU mode (Eco / Standard / Ultimate) — added 2026-11-16

Sources: GU605MY.329 DSDT (line numbers of `iasl -d` 20230628), Linux `asus-wmi` (`dgpu_disable`, `gpu_mux_mode`),
`utajum/g-helper-linux` (`src/Gpu/GPUModeControl.cs`, `MuxLatch.cs`, `Platform/Linux/Asus/LinuxAsusWmi.cs`).

| ID | Read (DSTS) | Write (DEVS) |
|---|---|---|
| 0x00090020 dGPU disable ("Eco") | L78323: `0x00010000 \| CUMA` (CUMA 1 = dGPU off) | **in AML, L78863** (below) |
| 0x00090016 GPU MUX | L78339: `Return (DGPM)`; bit 0: 1 = hybrid, 0 = dGPU drives the panel | not in AML → `WCHK` → SMI (`W15H`); latched, read-back stays old until the next boot |

DEVS 0x00090020 = 1: `CUMA = 1; WOSR = 1; GPCE = 1; EC0.UMAF = 1; EC0.PWMD = 0; Notify (RP12.PXSX, 0x81);
Notify (RP12.PXSX, 3 /* eject */); WAT1 ()` — WAT1 waits up to 15 s (150 × 100 ms) for `WOSR == 0`, which EC query
`_Q3F` clears after `RP12.DL23 (); RP12.POFF ()` once the EC raised `UFON` (L76445). So the firmware really cuts the
dGPU's power, live. The flag the BIOS reads at the next POST is EC `UMAF`, set in the first statements.

DEVS 0x00090020 = 0: `EC0.UMAF = 0; EC0.PWMD = 1; UMFG = 0; Notify (PXSX, 0); EC0.FGA9 = 1; WAT2 ()` — WAT2 waits up
to 7.5 s (500 × 15 ms) for the device to answer on the bus.

Rules taken over from Linux / g-helper-linux (and the owner's Windows experience):
* never dGPU off while a GPU driver is bound (kernel panic on Linux; macOS has no hot-removal either) → **nothing is
  written while macOS runs**;
* never dGPU off while the MUX is on the dGPU (no display at boot, CMOS reset needed);
* the firmware rejects MUX writes while the dGPU is off → Eco → Ultimate = dGPU on first, then MUX;
* Ultimate → Eco takes two boots (first boot is Standard), on Windows too.

Implementation (`ASUSATKD.cpp`, "GPU mode for the next boot"): selector 9 only arms a target. The kext sets the
property `IOPlatformHaltRestartAction`; xnu (`iokit/Kernel/IOPlatformActions.cpp`, `IOPMrootDomain::
handlePlatformHaltRestart`) then calls `callPlatformFunction("IOPlatformHaltRestartAction", …, pe_type)` after all
drivers got their shutdown notification, right before the platform reset. Only `kPEHaltCPU` / `kPERestartCPU` count
(macOS also runs the actions for `kPEPagingOff`). The DEVS calls run on a `thread_call`; the hook waits 4–10 s
(xnu forces the reset 30 s after the shutdown began) and leaves the outcome in NVRAM `ghelper-gpu-last`.

| now → target | written at restart | boots as |
|---|---|---|
| Standard → Eco | dGPU off | Eco |
| Eco → Standard | dGPU on | Standard |
| Standard → Ultimate | MUX = 0 | Ultimate |
| Eco → Ultimate | dGPU on, then MUX = 0 | Ultimate |
| Ultimate → Standard | MUX = 1 | Standard |
| Ultimate → Eco | MUX = 1 (first restart: Standard), app arms dGPU off (second restart) | Eco after two |

The Standard boot between Ultimate and Eco needs nobody at the keyboard: the app leaves `eco <epoch>` in
`/Users/Shared/GHelperMac/gpu-next`; the GU605 boot runner (`gu605-auto.sh`, root, WTLIntelGfx branch `gu605-mac`)
finds it in a Standard boot, arms Eco through `tools/ghelperctl gpu eco` and runs `shutdown -r now`, once (the flag
is moved into the run's log folder first; `gu605.conf` `GPU_NEXT=0` switches this off).

## 13. CPU clocks under XCPM (GU605CPU.kext) — 2026-11-17

Symptom: under a 12-thread load each P-core stayed at its own fixed clock (3.96 / 2.06 / 0.86 / 0.85 / 0.57 / 0.55 GHz),
package ~9.7 W, no thermal or power limit; one core never above 3.96 GHz.

Cause (MSR dump, `sysctl debug.gu605cpu_dump`): HWP is on (IA32_PM_ENABLE = 1) and XCPM (Comet Lake CPUID spoof,
MacBookPro16,2) writes IA32_HWP_REQUEST per CPU with max = 0x33 and a desired value of its own (0x08 .. 0x33) in
100 MHz units. On this hybrid CPU HWP counts ~78 MHz per unit (HWP_CAPABILITIES highest 0x40 on the two favoured
cores, 0x3C on the others), so max 0x33 = ~4.0 GHz and desired 0x08 = ~0.6 GHz; and XCPM did not raise the desired
value on most cores under load.

Fix: IA32_HWP_REQUEST = 0x80004004 on every CPU (min 4, max 0x40, desired 0 = hardware chooses, EPP 0x80),
re-applied every 500 ms (XCPM rewrites it ~140 times a second when idle, rarely under load).
Measured (powermetrics): one thread 4.99 GHz; six cores 4.55 GHz at the 35 W limit set in G-Helper (was 1.47 GHz at
9.7 W). BCD probe (hiz-olcumu.sh): single core 1.90 -> 1.10 s, 12 cores 5.17 -> 2.79 s, 3 at once 2.50 -> 1.54 s;
3000 process starts 10.4 -> 8.9 s (not clock-bound).

~~Install: kext in /Library/Extensions + auxiliary collection; tools/gu605cpu/ LaunchDaemon + script apply it at boot.~~

**Withdrawn the same day (12:09 panic).** Re-applying meant `mp_rendezvous_no_intrs` twice a second, which stops every
CPU at once. One CPU was spinning with interrupts off in XeGfx (`intel_guc_ct_send`) and could not join in time:
`mp_rv_action() entry: 11 of 12 responses`. The per-CPU queued call (`mp_cpus_call` NOSYNC) would avoid that but is a
Private export. GU605CPU 0.2 is read only (dump on demand), the LaunchDaemon is disabled
(`/Library/XeGfx/gu605/com.thebcd69.gu605cpu.plist.disabled`).

### The fix in the kernel instead (OpenCore Kernel->Patch, macOS 15.8.1 / Darwin 24.6.0 only)

XCPM's HWP request builder (unnamed function at kernel vmaddr 0xffffff800046a250, `edi` = its P-state) assembles
`min | max<<8 | desired<<16 | EPP<<24` and writes 0x774 (or 0x772). Two patches, each found once in the kernel:

| what | find | replace |
|---|---|---|
| shared request: desired 0, max 0x40 | `89f0 c1e310 0fb6f0 c1e618 4889d0 48c1e008` | `89f0 31db90 0fb6f0 c1e618 b800400000 6690` |
| per-CPU request: desired 0 | `478b8418c0000000 41c1e110 41c1e018` | `478b8418c0000000 4531c990 41c1e018` |

min (4) and EPP (XCPM's, 0x80) are left as they are: idle stays low, the hardware picks the clock under load
(the same as Windows' balanced plan). No periodic work, no cross-CPU calls. tools/opencore/ocpatch-xcpm.py adds them.

## 14. E-cores (MADT) — 2026-11-17

The firmware MADT (dumped from Windows with GetSystemFirmwareTable, 604 bytes, SHA-256 8D293504...) lists 22 enabled
LAPICs: BSP 0x20 (uid 12), P-core threads 0x10/11, 0x18/19, 0x20/21, 0x28/29, 0x30/31, 0x38/39 (uid 8-19), E-cores
0x00-0x0E even (uid 0-7), LP-E cores 0x40/0x42 (uid 20-21). The installer had not got through with all 22, so the EFI
has run APIC-BSP0.aml (BSP first as uid 0, the 12 P-core threads as uid 0-11, the rest disabled).

APIC-E8.aml = APIC-BSP0.aml + the 8 E-cores enabled as uid 12-19 (SSDT-PLUG-ALT has Processor objects for 0-47);
LP-E cores left out. tools/opencore/ocpatch-e8.py switches ACPI->Add from APIC-BSP0 to APIC-E8.
macOS on Intel has no hybrid-aware scheduler (no Thread Director): threads land on P and E cores alike.

Result on the GU605 (macOS 15.8.1), 2026-11-17 — **E-cores dropped, the machine stays at 6 P-cores / 12 threads**:
- APIC-E8 alone: panic during CPU bring-up, `processor_boot() missing for cpu 1 @processor.c:876` (raised on cpu 18, so
  the E-cores had started; the first P-core sibling never did). Most likely why the installer had not got through with
  all 22 threads either.
- APIC-E8 + CpuTopologyRebuild 2.0.2 (after Lilu; its E/P split by 8-wide APIC groups fits this layout): hang at the
  same point, after the AppleACPICPU list, no panic text. The kext replaces x86_validate_topology; the failure is
  earlier, in starting a CPU.
- Both rolled back by the Windows-side agent to config.plist.before-ctr-20261117-125023 (= the XCPM-patched, P-only
  config, SHA-256 7BEE2BCBFE40B40E...). CpuTopologyRebuild.kext and APIC-E8.aml stay on the EFI, unused.
