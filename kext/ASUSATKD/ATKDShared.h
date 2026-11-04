//
//  ATKDShared.h — user/kernel ABI of ASUSATKD.kext (GU605MY only).
//  Keep in sync with app/GHelperMac/Sources/ATKDClient.swift.
//
//  SPDX-License-Identifier: GPL-3.0-only
//  Derived from G-Helper (seerge/g-helper) and Linux asus-wmi research, see docs/RESEARCH.md.
//

#ifndef ATKDShared_h
#define ATKDShared_h

#include <stdint.h>

#define ATKD_ABI_VERSION        2u
#define ATKD_SERVICE_CLASS      "ASUSATKD"

// ---- IOUserClient selectors ----
enum {
	kATKDSelGetInfo         = 0, // out struct ATKDInfo
	kATKDSelDSTS            = 1, // in: scalar[0]=device id, scalar[1]=arg ; out: scalar[0]=raw u32
	kATKDSelDSTSBuffer      = 2, // in: scalar[0]=device id, scalar[1]=arg ; out: struct 32 bytes
	kATKDSelSetFanCurve     = 3, // in: scalar[0]=fan (0 CPU,1 GPU), struct 16 bytes (8 temps, 8 %) ; out: scalar[0]=BIOS ret
	kATKDSelSetPower        = 4, // in: scalar[0]=limit (0 PL1/SPL, 1 PL2/SPPT), scalar[1]=watts ; out: scalar[0]=BIOS ret
	kATKDSelSetMode         = 5, // in: scalar[0]=mode (0 Balanced,1 Turbo,2 Silent) ; out: scalar[0]=BIOS ret
	kATKDSelReadEC          = 6, // in: scalar[0]=ATKDECField ; out: scalar[0]=value
	kATKDSelReadCurve       = 7, // in: scalar[0]=fan (0 CPU,1 GPU) ; out: struct 16 bytes (current EC curve)
	kATKDSelSetChargeLimit  = 8, // in: scalar[0]=percent (ATKD_CHARGE_MIN..100) ; out: scalar[0]=BIOS ret
	kATKDSelCount
};

// ---- Fans ----
enum { kATKDFanCPU = 0, kATKDFanGPU = 1 };

// ---- Power limits ----
enum { kATKDLimitPL1 = 0, kATKDLimitPL2 = 1 };

// ---- Modes (ASUS throttle thermal policy / EC FTBL) ----
enum { kATKDModeBalanced = 0, kATKDModeTurbo = 1, kATKDModeSilent = 2 };

// ---- Read-only EC RAM fields (\_SB.PC00.LPCB.EC0.*), memory-mapped on this BIOS ----
enum {
	kATKDECFieldFTBL = 0, // active thermal table == mode (0/1/2)
	kATKDECFieldPL01 = 1, // PL1 watts last requested
	kATKDECFieldPL02 = 2, // PL2 watts last requested
	kATKDECFieldFMAE = 3, // fan manual enable (custom curve active)
	kATKDECFieldCTMP = 4, // CPU temp degC
	kATKDECFieldFRMS = 5, // CPU fan period; RPM = 2156250 / FRMS
	kATKDECFieldFRS2 = 6, // GPU fan period
	kATKDECFieldPL1V = 7, // diagnostics, meaning unconfirmed
	kATKDECFieldPL2V = 8, // diagnostics, meaning unconfirmed
	kATKDECFieldRSOC = 9, // battery relative state of charge, % (ABI 2)
	kATKDECFieldCount
};

// ---- Device IDs (confirmed in GU605MY DSDT unless noted) ----
#define ATKD_METHOD_DSTS        0x53545344u
#define ATKD_METHOD_DEVS        0x53564544u
#define ATKD_DEV_CPU_FAN        0x00110013u // DSTS: hundreds of RPM | 0x10000
#define ATKD_DEV_GPU_FAN        0x00110014u
#define ATKD_DEV_CPU_FAN_CURVE  0x00110024u
#define ATKD_DEV_GPU_FAN_CURVE  0x00110025u
#define ATKD_DEV_CPU_FAN_FLOOR  0x00110026u
#define ATKD_DEV_GPU_FAN_FLOOR  0x00110027u
#define ATKD_DEV_CPU_TEMP       0x00120094u
#define ATKD_DEV_PERF_MODE      0x00120075u // routed to SMI on this BIOS (not visible in AML)
#define ATKD_DEV_PPT_PL2_SPPT   0x001200A0u
#define ATKD_DEV_PPT_PL1_SPL    0x001200A3u
#define ATKD_DEV_PPT_FPPT       0x001200C1u // DSTS present, DEVS unsupported -> read only
#define ATKD_DEV_CHARGE_LIMIT   0x00120057u // RSOC charge end threshold; not in AML -> SMI (WCHK), runtime presence check
#define ATKD_UNSUPPORTED        0xFFFFFFFEu
#define ATKD_PRESENT_BIT        0x00010000u

// ---- Hard limits enforced in the kext (docs/RESEARCH.md §3, §5) ----
#define ATKD_PL1_MIN_W          25u   // asus-armoury GU605M DC min (AC min is 28)
#define ATKD_PL1_MAX_W          90u   // asus-armoury GU605M AC max
#define ATKD_PL2_MIN_W          28u   // asus-armoury GU605M AC min
#define ATKD_PL2_MAX_W          135u  // asus-armoury GU605M AC max
#define ATKD_CHARGE_MIN         40u   // G-Helper minimum
#define ATKD_CHARGE_MAX         100u  // 100 = no limit
#define ATKD_FAN_POINTS         8u
#define ATKD_FAN_CURVE_BYTES    16u
#define ATKD_DSTS_BUF_BYTES     32u

// ---- Info flags ----
enum {
	kATKDFlagAttached       = 1u << 0,
	kATKDFlagWritesEnabled  = 1u << 1,
	kATKDFlagReadOnlyArg    = 1u << 2, // -atkdro
	kATKDFlagCpuCurveActive = 1u << 3, // a CPU curve was written since last mode change
	kATKDFlagDSDTMatch      = 1u << 4, // DSDT OEM ids + revision match GU605MY.329 dump
	kATKDFlagECFound        = 1u << 5, // EC0 fields readable
	kATKDFlagModeIdPresent  = 1u << 6, // DSTS 0x00120075 has presence bit
	kATKDFlagForceArg       = 1u << 7, // -atkdforce
	kATKDFlagChargePresent  = 1u << 8, // DSTS 0x00120057 has presence bit (ABI 2)
	kATKDFlagERAMMapped     = 1u << 9, // EC fields read from the mapped ERAM/ERM2 window (ABI 2)
};

typedef struct {
	uint32_t abiVersion;
	uint32_t flags;
	uint32_t dsdtOemRevision;
	uint32_t lastEvent;      // last _WED(0xFF) code
	uint32_t eventCount;
	uint32_t writeCount;
	uint32_t lastBiosResult;
	uint32_t pl1MinW, pl1MaxW, pl2MinW, pl2MaxW;
	uint32_t chargeLimit;    // last charge limit written by the kext since boot, 0 = none (ABI 2)
	uint32_t reserved[4];
} ATKDInfo; // 64 bytes

#endif /* ATKDShared_h */
