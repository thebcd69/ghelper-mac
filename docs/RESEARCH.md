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

## Sources
* DSDT: `EFI/OC/ACPI/DSDT.aml` (owner's USB), decompiled locally.
* G-Helper `app/AsusACPI.cs`, `app/Mode/ModeControl.cs`, `app/AppConfig.cs` (seerge/g-helper main, fetched 2026-11-04).
* Linux master `drivers/platform/x86/asus-wmi.c`, `asus-armoury.c`, `asus-armoury.h`,
  `include/linux/platform_data/x86/asus-wmi.h` (fetched 2026-11-04).
