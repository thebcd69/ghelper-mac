//
//  ASUSATKD.cpp — minimal ASUS ATKD (WMNB) bridge for the ROG Zephyrus G16 GU605MY only.
//
//  SPDX-License-Identifier: GPL-3.0-only
//  Protocol knowledge derived from G-Helper (GPL-3.0, seerge/g-helper) and Linux asus-wmi.
//  Every id / byte layout used here is documented with DSDT evidence in docs/RESEARCH.md.
//

#include "ASUSATKD.hpp"
#include <IOKit/IOPlatformExpert.h>
#include <libkern/libkern.h>
#include <kern/clock.h>
#include <pexpert/pexpert.h>

OSDefineMetaClassAndStructors(ASUSATKD, IOService)

// GU605MY.329 DSDT as injected by the owner's OpenCore (EFI/OC/ACPI/DSDT.aml)
static const char     kExpectedOemId[6]      = {'_', 'A', 'S', 'U', 'S', '_'};
static const char     kExpectedOemTableId[8] = {'N', 'o', 't', 'e', 'b', 'o', 'o', 'k'};
static const uint32_t kExpectedOemRevision   = 0x01072009;

static const char *const kECAbsPrefix = "\\_SB.PC00.LPCB.EC0.";

static const char *const kECFieldNames[kATKDECFieldCount] = {
	"FTBL", "PL01", "PL02", "FMAE", "CTMP", "FRMS", "FRS2", "PL1V", "PL2V", "RSOC",
};

// EC RAM windows of this DSDT (OperationRegion ERAM/ERM2, SystemMemory). Offsets taken from the
// Field lists of the GU605MY.329 DSDT (EFI/OC/ACPI/DSDT.aml) and only used when dsdtMatch is set.
// Needed because AppleACPIPlatform's evaluateInteger() fails on these FieldUnits (test 1, 2026-11-04).
static const IOPhysicalAddress kERAMBase = 0xFE108300;
static const IOPhysicalAddress kERM2Base = 0xFE108B00;
static const IOByteCount       kERAMLen  = 0x100;

struct ERAMField {
	char    name[5];
	uint8_t region; // 0 ERAM, 1 ERM2
	uint8_t offset;
	uint8_t bytes;  // 1 or 2 (little endian); 0 = single bit
	uint8_t bit;
};

static const ERAMField kERAMFields[] = {
	{"FTBL", 0, 0x06, 1, 0}, {"CTMP", 0, 0x58, 1, 0}, {"FRMS", 0, 0x66, 2, 0}, {"FRS2", 0, 0x68, 2, 0},
	{"RSOC", 0, 0x77, 1, 0}, {"PL01", 0, 0x9B, 1, 0}, {"PL02", 0, 0x9D, 1, 0}, {"FMAE", 0, 0xCA, 0, 2},
	{"PL1V", 1, 0x8E, 1, 0}, {"PL2V", 1, 0x8F, 1, 0},
	{"CTP0", 1, 0x00, 1, 0}, {"CTP1", 1, 0x01, 1, 0}, {"CTP2", 1, 0x02, 1, 0}, {"CTP3", 1, 0x03, 1, 0},
	{"CTP4", 1, 0x04, 1, 0}, {"CTP5", 1, 0x05, 1, 0}, {"CTP6", 1, 0x06, 1, 0}, {"CTP7", 1, 0x07, 1, 0},
	{"SCT0", 1, 0x08, 1, 0}, {"SCT1", 1, 0x09, 1, 0}, {"SCT2", 1, 0x0A, 1, 0}, {"SCT3", 1, 0x0B, 1, 0},
	{"SCT4", 1, 0x0C, 1, 0}, {"SCT5", 1, 0x0D, 1, 0}, {"SCT6", 1, 0x0E, 1, 0}, {"SCT7", 1, 0x0F, 1, 0},
	{"GTP0", 1, 0x10, 1, 0}, {"GTP1", 1, 0x11, 1, 0}, {"GTP2", 1, 0x12, 1, 0}, {"GTP3", 1, 0x13, 1, 0},
	{"GTP4", 1, 0x14, 1, 0}, {"GTP5", 1, 0x15, 1, 0}, {"GTP6", 1, 0x16, 1, 0}, {"GTP7", 1, 0x17, 1, 0},
	{"SGT0", 1, 0x18, 1, 0}, {"SGT1", 1, 0x19, 1, 0}, {"SGT2", 1, 0x1A, 1, 0}, {"SGT3", 1, 0x1B, 1, 0},
	{"SGT4", 1, 0x1C, 1, 0}, {"SGT5", 1, 0x1D, 1, 0}, {"SGT6", 1, 0x1E, 1, 0}, {"SGT7", 1, 0x1F, 1, 0},
};

// Current custom curve as written by SFTC/SFTG (ERM2 @ 0xFE108B00), docs/RESEARCH.md §3
static const char *const kCpuCurveNames[ATKD_FAN_CURVE_BYTES] = {
	"CTP0", "CTP1", "CTP2", "CTP3", "CTP4", "CTP5", "CTP6", "CTP7",
	"SCT0", "SCT1", "SCT2", "SCT3", "SCT4", "SCT5", "SCT6", "SCT7",
};
static const char *const kGpuCurveNames[ATKD_FAN_CURVE_BYTES] = {
	"GTP0", "GTP1", "GTP2", "GTP3", "GTP4", "GTP5", "GTP6", "GTP7",
	"SGT0", "SGT1", "SGT2", "SGT3", "SGT4", "SGT5", "SGT6", "SGT7",
};

// Rate limit for writes: at most kRateMaxWrites in kRateWindowNs.
static const uint64_t kRateWindowNs  = 10ULL * 1000ULL * 1000ULL * 1000ULL;
static const uint32_t kRateMaxWrites = 24;

static bool containsStr(const char *hay, size_t hayLen, const char *needle) {
	size_t n = strlen(needle);
	size_t h = strnlen(hay, hayLen);
	if (n == 0 || n > h)
		return n == 0;
	for (size_t i = 0; i + n <= h; i++)
		if (memcmp(hay + i, needle, n) == 0)
			return true;
	return false;
}

static bool bootArg(const char *name) {
	char tmp[16];
	return PE_parse_boot_argn(name, tmp, sizeof(tmp));
}

static uint64_t uptimeNs() {
	uint64_t abs = 0, ns = 0;
	clock_get_uptime(&abs);
	absolutetime_to_nanoseconds(abs, &ns);
	return ns;
}

// ---------------------------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------------------------

bool ASUSATKD::init(OSDictionary *dict) {
	if (!IOService::init(dict))
		return false;
	atkd = nullptr;
	ec = nullptr;
	lock = nullptr;
	workLoop = nullptr;
	notifyTimer = nullptr;
	eramMap = erm2Map = nullptr;
	chargePresent = false;
	chargeLimit = 0;
	readOnlyArg = forceArg = dsdtMatch = signatureOK = writesEnabled = false;
	cpuCurveActive = modeIdPresent = ecReadable = false;
	dsdtOemRevision = 0;
	lastEvent = eventCount = writeCount = 0;
	lastBiosResult = 0;
	rateWindowStartNs = 0;
	rateWindowWrites = 0;
	return true;
}

IOService *ASUSATKD::probe(IOService *provider, SInt32 *score) {
	if (bootArg("-atkdoff")) {
		ATKDLOG("disabled by -atkdoff");
		return nullptr;
	}

	auto dev = OSDynamicCast(IOACPIPlatformDevice, provider);
	if (!dev)
		return nullptr;

	// Several PNP0C14 devices exist on this DSDT; only \_SB.ATKD has _UID "ATK".
	OSObject *uid = nullptr;
	bool isATK = false;
	if (dev->evaluateObject("_UID", &uid) == kIOReturnSuccess && uid) {
		if (auto s = OSDynamicCast(OSString, uid))
			isATK = s->isEqualTo("ATK");
		else if (auto d = OSDynamicCast(OSData, uid)) // some ACPI platform versions return strings as data
			isATK = d->getLength() >= 3 && memcmp(d->getBytesNoCopy(), "ATK", 3) == 0 &&
			        (d->getLength() == 3 || static_cast<const char *>(d->getBytesNoCopy())[3] == 0);
	}
	OSSafeReleaseNULL(uid);
	if (!isATK)
		return nullptr;

	if (dev->validateObject("WMNB") != kIOReturnSuccess) {
		ATKDLOG("ATK device has no WMNB, not attaching");
		return nullptr;
	}

	return IOService::probe(provider, score);
}

bool ASUSATKD::start(IOService *provider) {
	if (!IOService::start(provider))
		return false;

	atkd = OSDynamicCast(IOACPIPlatformDevice, provider);
	if (!atkd)
		return false;
	atkd->retain();

	lock = IOLockAlloc();
	workLoop = IOWorkLoop::workLoop();
	if (workLoop) {
		notifyTimer = IOTimerEventSource::timerEventSource(
			this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &ASUSATKD::notifyFired));
		if (notifyTimer && workLoop->addEventSource(notifyTimer) != kIOReturnSuccess)
			OSSafeReleaseNULL(notifyTimer);
	}
	if (!lock || !workLoop || !notifyTimer) {
		ATKDLOG("allocation failed");
		stop(provider);
		return false;
	}

	readOnlyArg = bootArg("-atkdro");
	forceArg    = bootArg("-atkdforce");

	checkDSDT();
	findEC();
	if (dsdtMatch)
		mapERAM();
	checkSignature();

	writesEnabled = signatureOK && (dsdtMatch || forceArg) && !readOnlyArg;

	IOLockLock(lock);
	uint32_t args[5] = {ATKD_DEV_PERF_MODE, 0, 0, 0, 0};
	uint32_t v = 0;
	if (wmnbIntegerLocked(ATKD_METHOD_DSTS, args, &v) == kIOReturnSuccess && v != ATKD_UNSUPPORTED &&
	    (v & ATKD_PRESENT_BIT))
		modeIdPresent = true;
	uint32_t ftbl = 0xFF;
	ecReadable = readECNameLocked("FTBL", &ftbl) == kIOReturnSuccess;
	uint32_t cargs[5] = {ATKD_DEV_CHARGE_LIMIT, 0, 0, 0, 0};
	uint32_t cv = 0;
	if (wmnbIntegerLocked(ATKD_METHOD_DSTS, cargs, &cv) == kIOReturnSuccess && cv != ATKD_UNSUPPORTED &&
	    (cv & ATKD_PRESENT_BIT))
		chargePresent = true;
	IOLockUnlock(lock);

	ATKDLOG("attached to \\_SB.ATKD abi=%u dsdtMatch=%d (oemrev 0x%08x) signature=%d ec=%d (eram=%d) "
	        "modeId=%d(0x%08x) ftbl=%u charge=%d(0x%08x) ro=%d force=%d -> writes %s",
	        ATKD_ABI_VERSION, dsdtMatch, dsdtOemRevision, signatureOK, ecReadable, eramMap != nullptr,
	        modeIdPresent, v, ftbl, chargePresent, cv, readOnlyArg, forceArg, writesEnabled ? "ENABLED" : "DISABLED");

	setProperty("ATKD-WritesEnabled", writesEnabled);
	setProperty("ATKD-DSDTMatch", dsdtMatch);
	setProperty("ATKD-ECReadable", ecReadable);
	registerService();
	return true;
}

void ASUSATKD::stop(IOService *provider) {
	if (notifyTimer) {
		notifyTimer->cancelTimeout();
		if (workLoop)
			workLoop->removeEventSource(notifyTimer); // waits for an in-flight action (gated)
		OSSafeReleaseNULL(notifyTimer);
	}
	OSSafeReleaseNULL(workLoop);
	unmapERAM();
	OSSafeReleaseNULL(ec);
	OSSafeReleaseNULL(atkd);
	ATKDLOG("stopped");
	IOService::stop(provider);
}

void ASUSATKD::free() {
	if (lock) {
		IOLockFree(lock);
		lock = nullptr;
	}
	IOService::free();
}

// ---------------------------------------------------------------------------------------------
// Machine checks
// ---------------------------------------------------------------------------------------------

void ASUSATKD::checkDSDT() {
	const OSData *dsdt = atkd->getACPITableData("DSDT", 0);
	OSData *fromPE = nullptr;
	if (!dsdt) {
		// Fallback: AppleACPIPlatformExpert publishes "ACPI Tables" = { DSDT = <...> }
		IOService *pe = getPlatform();
		if (pe) {
			auto tables = OSDynamicCast(OSDictionary, pe->getProperty("ACPI Tables"));
			if (tables) {
				fromPE = OSDynamicCast(OSData, tables->getObject("DSDT"));
				if (fromPE)
					fromPE->retain();
			}
		}
		dsdt = fromPE;
	}
	if (!dsdt || dsdt->getLength() < 36) {
		ATKDLOG("DSDT header not readable -> treating as mismatch");
		OSSafeReleaseNULL(fromPE);
		return;
	}
	const uint8_t *h = static_cast<const uint8_t *>(dsdt->getBytesNoCopy());
	uint32_t rev = 0;
	memcpy(&rev, h + 24, sizeof(rev));
	dsdtOemRevision = rev;
	bool ids = memcmp(h, "DSDT", 4) == 0 && memcmp(h + 10, kExpectedOemId, 6) == 0 &&
	           memcmp(h + 16, kExpectedOemTableId, 8) == 0;
	dsdtMatch = ids && rev == kExpectedOemRevision;
	if (!dsdtMatch)
		ATKDLOG("DSDT mismatch: ids=%d oemrev=0x%08x (expected 0x%08x)", ids, rev, kExpectedOemRevision);
	OSSafeReleaseNULL(fromPE);
}

void ASUSATKD::findEC() {
	OSDictionary *m = IOService::serviceMatching("IOACPIPlatformDevice");
	if (!m)
		return;
	IOService::nameMatching("PNP0C09", m);
	OSIterator *it = IOService::getMatchingServices(m);
	m->release();
	if (!it)
		return;
	const IORegistryPlane *acpiPlane = IORegistryEntry::getPlane("IOACPIPlane");
	while (OSObject *o = it->getNextObject()) {
		auto dev = OSDynamicCast(IOACPIPlatformDevice, o);
		if (!dev)
			continue;
		char path[256];
		int len = sizeof(path);
		if (acpiPlane && dev->getPath(path, &len, acpiPlane) && containsStr(path, sizeof(path), "/LPCB") && containsStr(path, sizeof(path), "/EC0")) {
			ec = dev;
			ec->retain();
			ATKDLOG("EC found at %s", path);
			break;
		}
	}
	it->release();
	if (!ec)
		ATKDLOG("EC0 nub not found, using absolute ACPI paths");
}

bool ASUSATKD::validateECObject(const char *name) {
	if (ec && ec->validateObject(name) == kIOReturnSuccess)
		return true;
	char abs[48];
	snprintf(abs, sizeof(abs), "%s%s", kECAbsPrefix, name);
	return atkd->validateObject(abs) == kIOReturnSuccess;
}

void ASUSATKD::checkSignature() {
	// The objects our whitelist relies on (docs/RESEARCH.md §2-§5).
	static const char *const ecObjs[] = {"SFTC", "SFTG", "CFCV", "GFCV", "CFLB", "GFLB", "FTBL", "PL01", "PL02", "FMAE"};
	bool ok = atkd->validateObject("_WED") == kIOReturnSuccess;
	for (auto name : ecObjs) {
		if (!validateECObject(name)) {
			ATKDLOG("signature: EC0.%s missing", name);
			ok = false;
		}
	}
	signatureOK = ok;
}

// ---------------------------------------------------------------------------------------------
// WMNB plumbing
// ---------------------------------------------------------------------------------------------

IOReturn ASUSATKD::callWMNBLocked(uint32_t method, const uint32_t args[5], OSObject **result) {
	// WMNB does CreateDWordField(Arg2, 0x10, IIA4): Arg2 must be >= 20 bytes. Linux passes 24.
	uint32_t buf[6] = {args[0], args[1], args[2], args[3], args[4], 0};
	OSNumber *a0 = OSNumber::withNumber(0ULL, 32);
	OSNumber *a1 = OSNumber::withNumber(static_cast<unsigned long long>(method), 32);
	OSData   *a2 = OSData::withBytes(buf, sizeof(buf));
	IOReturn  r  = kIOReturnNoMemory;
	if (a0 && a1 && a2) {
		OSObject *params[3] = {a0, a1, a2};
		r = atkd->evaluateObject("WMNB", result, params, 3);
	}
	OSSafeReleaseNULL(a0);
	OSSafeReleaseNULL(a1);
	OSSafeReleaseNULL(a2);
	return r;
}

IOReturn ASUSATKD::wmnbIntegerLocked(uint32_t method, const uint32_t args[5], uint32_t *out) {
	OSObject *res = nullptr;
	IOReturn r = callWMNBLocked(method, args, &res);
	if (r == kIOReturnSuccess) {
		auto n = OSDynamicCast(OSNumber, res);
		if (n)
			*out = static_cast<uint32_t>(n->unsigned64BitValue() & 0xFFFFFFFFULL);
		else
			r = kIOReturnBadArgument;
	}
	OSSafeReleaseNULL(res);
	return r;
}

IOReturn ASUSATKD::dstsBufferLocked(uint32_t devId, uint32_t arg, uint8_t *out, uint32_t outLen, uint32_t *gotLen) {
	uint32_t args[5] = {devId, arg, 0, 0, 0};
	OSObject *res = nullptr;
	IOReturn r = callWMNBLocked(ATKD_METHOD_DSTS, args, &res);
	*gotLen = 0;
	if (r == kIOReturnSuccess) {
		auto d = OSDynamicCast(OSData, res);
		if (d && d->getLength() > 0) {
			uint32_t n = d->getLength() < outLen ? d->getLength() : outLen;
			memcpy(out, d->getBytesNoCopy(), n);
			*gotLen = n;
		} else {
			r = kIOReturnUnsupported; // integer (0 / 0xFFFFFFFE) instead of a buffer
		}
	}
	OSSafeReleaseNULL(res);
	return r;
}

static IOMemoryMap *mapPhysReadOnly(IOPhysicalAddress pa, IOByteCount len) {
	IOMemoryDescriptor *md = IOMemoryDescriptor::withPhysicalAddress(pa, len, kIODirectionIn);
	if (!md)
		return nullptr;
	IOMemoryMap *m = md->map(kIOMapInhibitCache | kIOMapReadOnly);
	md->release(); // the map keeps its own reference
	return m;
}

void ASUSATKD::mapERAM() {
	eramMap = mapPhysReadOnly(kERAMBase, kERAMLen);
	erm2Map = mapPhysReadOnly(kERM2Base, kERAMLen);
	if (!eramMap || !erm2Map) {
		ATKDLOG("ERAM map failed (eram=%p erm2=%p), falling back to ACPI evaluation", eramMap, erm2Map);
		unmapERAM();
		return;
	}
	ATKDLOG("ERAM 0x%llx / ERM2 0x%llx mapped read-only", static_cast<unsigned long long>(kERAMBase),
	        static_cast<unsigned long long>(kERM2Base));
}

void ASUSATKD::unmapERAM() {
	OSSafeReleaseNULL(eramMap);
	OSSafeReleaseNULL(erm2Map);
}

bool ASUSATKD::readERAMLocked(const char *name, uint32_t *out) {
	if (!eramMap || !erm2Map)
		return false;
	for (const auto &f : kERAMFields) {
		if (memcmp(f.name, name, 4) != 0 || name[4] != 0)
			continue;
		IOMemoryMap *m = f.region == 0 ? eramMap : erm2Map;
		auto base = reinterpret_cast<volatile const uint8_t *>(m->getVirtualAddress());
		if (!base || f.offset + (f.bytes ? f.bytes : 1) > m->getLength())
			return false;
		if (f.bytes == 0)
			*out = (base[f.offset] >> f.bit) & 1u;
		else if (f.bytes == 1)
			*out = base[f.offset];
		else
			*out = static_cast<uint32_t>(base[f.offset]) | (static_cast<uint32_t>(base[f.offset + 1]) << 8);
		return true;
	}
	return false;
}

IOReturn ASUSATKD::readECNameLocked(const char *name, uint32_t *out) {
	if (readERAMLocked(name, out))
		return kIOReturnSuccess;
	UInt32 v = 0;
	if (ec && ec->evaluateInteger(name, &v) == kIOReturnSuccess) {
		*out = v;
		return kIOReturnSuccess;
	}
	char abs[48];
	snprintf(abs, sizeof(abs), "%s%s", kECAbsPrefix, name);
	IOReturn r = atkd->evaluateInteger(abs, &v);
	if (r == kIOReturnSuccess)
		*out = v;
	return r;
}

// ---------------------------------------------------------------------------------------------
// Public (user client) API
// ---------------------------------------------------------------------------------------------

void ASUSATKD::getInfo(ATKDInfo *out) {
	IOLockLock(lock);
	bzero(out, sizeof(*out));
	out->abiVersion = ATKD_ABI_VERSION;
	uint32_t f = kATKDFlagAttached;
	if (writesEnabled)  f |= kATKDFlagWritesEnabled;
	if (readOnlyArg)    f |= kATKDFlagReadOnlyArg;
	if (cpuCurveActive) f |= kATKDFlagCpuCurveActive;
	if (dsdtMatch)      f |= kATKDFlagDSDTMatch;
	if (ecReadable)     f |= kATKDFlagECFound;
	if (modeIdPresent)  f |= kATKDFlagModeIdPresent;
	if (forceArg)       f |= kATKDFlagForceArg;
	if (chargePresent)  f |= kATKDFlagChargePresent;
	if (eramMap)        f |= kATKDFlagERAMMapped;
	out->flags = f;
	out->chargeLimit = chargeLimit;
	out->dsdtOemRevision = dsdtOemRevision;
	out->lastEvent = lastEvent;
	out->eventCount = eventCount;
	out->writeCount = writeCount;
	out->lastBiosResult = lastBiosResult;
	out->pl1MinW = ATKD_PL1_MIN_W;
	out->pl1MaxW = ATKD_PL1_MAX_W;
	out->pl2MinW = ATKD_PL2_MIN_W;
	out->pl2MaxW = ATKD_PL2_MAX_W;
	IOLockUnlock(lock);
}

IOReturn ASUSATKD::dsts(uint32_t devId, uint32_t arg, uint32_t *out) {
	switch (devId) {
	case ATKD_DEV_CPU_FAN:
	case ATKD_DEV_GPU_FAN:
	case ATKD_DEV_CPU_TEMP:
	case ATKD_DEV_PERF_MODE:
	case ATKD_DEV_PPT_PL1_SPL:
	case ATKD_DEV_PPT_PL2_SPPT:
	case ATKD_DEV_PPT_FPPT:
	case ATKD_DEV_CHARGE_LIMIT:
		break;
	default:
		return kIOReturnNotPermitted;
	}
	if (arg != 0)
		return kIOReturnBadArgument;
	uint32_t args[5] = {devId, 0, 0, 0, 0};
	IOLockLock(lock);
	IOReturn r = wmnbIntegerLocked(ATKD_METHOD_DSTS, args, out);
	IOLockUnlock(lock);
	return r;
}

IOReturn ASUSATKD::dstsBuffer(uint32_t devId, uint32_t arg, uint8_t *out, uint32_t outLen) {
	switch (devId) {
	case ATKD_DEV_CPU_FAN_CURVE:
	case ATKD_DEV_GPU_FAN_CURVE:
		if (arg > 2) // curve table index 0 (Balanced), 1 (Silent), 2 (Turbo) — swapped, see RESEARCH §3
			return kIOReturnBadArgument;
		break;
	case ATKD_DEV_CPU_FAN_FLOOR:
	case ATKD_DEV_GPU_FAN_FLOOR:
		if (arg != 0)
			return kIOReturnBadArgument;
		break;
	default:
		return kIOReturnNotPermitted;
	}
	bzero(out, outLen);
	uint32_t got = 0;
	IOLockLock(lock);
	IOReturn r = dstsBufferLocked(devId, arg, out, outLen, &got);
	IOLockUnlock(lock);
	return r;
}

IOReturn ASUSATKD::readEC(uint32_t field, uint32_t *out) {
	if (field >= kATKDECFieldCount)
		return kIOReturnBadArgument;
	IOLockLock(lock);
	IOReturn r = readECNameLocked(kECFieldNames[field], out);
	IOLockUnlock(lock);
	return r;
}

IOReturn ASUSATKD::readCurve(uint32_t fan, uint8_t *out16) {
	if (fan > kATKDFanGPU)
		return kIOReturnBadArgument;
	const char *const *names = fan == kATKDFanCPU ? kCpuCurveNames : kGpuCurveNames;
	IOReturn r = kIOReturnSuccess;
	IOLockLock(lock);
	for (uint32_t i = 0; i < ATKD_FAN_CURVE_BYTES && r == kIOReturnSuccess; i++) {
		uint32_t v = 0;
		r = readECNameLocked(names[i], &v);
		out16[i] = static_cast<uint8_t>(v & 0xFF);
	}
	IOLockUnlock(lock);
	return r;
}

IOReturn ASUSATKD::writeGateLocked(const char *what) {
	if (!writesEnabled) {
		ATKDLOG("%s refused: writes disabled (dsdtMatch=%d signature=%d ro=%d)", what, dsdtMatch, signatureOK, readOnlyArg);
		return kIOReturnNotPermitted;
	}
	uint64_t now = uptimeNs();
	if (now - rateWindowStartNs > kRateWindowNs) {
		rateWindowStartNs = now;
		rateWindowWrites = 0;
	}
	if (rateWindowWrites >= kRateMaxWrites) {
		ATKDLOG("%s refused: rate limit (%u writes / 10 s)", what, kRateMaxWrites);
		return kIOReturnBusy;
	}
	rateWindowWrites++;
	return kIOReturnSuccess;
}

IOReturn ASUSATKD::validateCurveLocked(uint32_t fan, const uint8_t *c) {
	const uint8_t *t = c;
	const uint8_t *p = c + ATKD_FAN_POINTS;
	for (uint32_t i = 0; i < ATKD_FAN_POINTS; i++) {
		uint32_t lo = 30 + 10 * i, hi = 40 + 10 * i; // G-Helper fan_clamp windows (30..110 degC)
		if (t[i] < lo || t[i] > hi) {
			ATKDLOG("curve reject: point %u temp %u outside [%u,%u]", i, t[i], lo, hi);
			return kIOReturnBadArgument;
		}
		if (i > 0 && t[i] <= t[i - 1]) {
			ATKDLOG("curve reject: temps not strictly increasing at %u", i);
			return kIOReturnBadArgument;
		}
		if (p[i] > 100) {
			ATKDLOG("curve reject: point %u fan %u%% > 100", i, p[i]);
			return kIOReturnBadArgument;
		}
		if (i > 0 && p[i] < p[i - 1]) {
			ATKDLOG("curve reject: fan %% decreases at %u", i);
			return kIOReturnBadArgument;
		}
		if ((t[i] >= 70 && p[i] < 20) || (t[i] >= 90 && p[i] < 40)) {
			ATKDLOG("curve reject: point %u (%u C, %u%%) below safety floor (>=70C:20%%, >=90C:40%%)", i, t[i], p[i]);
			return kIOReturnBadArgument;
		}
	}

	// BIOS-reported floor (DSTS 0x00110026 / 0x00110027 -> EC CBD0..7 / GBD0..7).
	uint8_t floorBuf[ATKD_DSTS_BUF_BYTES];
	bzero(floorBuf, sizeof(floorBuf));
	uint32_t got = 0;
	uint32_t floorId = fan == kATKDFanCPU ? ATKD_DEV_CPU_FAN_FLOOR : ATKD_DEV_GPU_FAN_FLOOR;
	IOReturn r = dstsBufferLocked(floorId, 0, floorBuf, sizeof(floorBuf), &got);
	if (r != kIOReturnSuccess || got < ATKD_FAN_POINTS) {
		ATKDLOG("curve reject: BIOS fan floor unreadable (0x%x, %u bytes)", r, got);
		return kIOReturnNotReady;
	}
	bool sane = true;
	for (uint32_t i = 0; i < ATKD_FAN_POINTS; i++)
		if (floorBuf[i] > 100)
			sane = false;
	if (!sane) {
		ATKDLOG("BIOS fan floor looks invalid, ignoring it (static floor still applies)");
		return kIOReturnSuccess;
	}
	for (uint32_t i = 0; i < ATKD_FAN_POINTS; i++) {
		if (p[i] < floorBuf[i]) {
			ATKDLOG("curve reject: point %u fan %u%% below BIOS floor %u%%", i, p[i], floorBuf[i]);
			return kIOReturnBadArgument;
		}
	}
	return kIOReturnSuccess;
}

IOReturn ASUSATKD::setFanCurve(uint32_t fan, const uint8_t *c, uint32_t *biosRet) {
	if (fan > kATKDFanGPU)
		return kIOReturnBadArgument;
	IOLockLock(lock);
	IOReturn r = writeGateLocked(fan == kATKDFanCPU ? "CPU curve" : "GPU curve");
	if (r == kIOReturnSuccess)
		r = validateCurveLocked(fan, c);
	if (r == kIOReturnSuccess) {
		uint32_t args[5];
		args[0] = fan == kATKDFanCPU ? ATKD_DEV_CPU_FAN_CURVE : ATKD_DEV_GPU_FAN_CURVE;
		memcpy(&args[1], c, ATKD_FAN_CURVE_BYTES); // temps t0..t7 then percents p0..p7, little endian
		uint32_t ret = 0;
		r = wmnbIntegerLocked(ATKD_METHOD_DEVS, args, &ret);
		*biosRet = ret;
		lastBiosResult = ret;
		writeCount++;
		ATKDLOG("DEVS %s curve T=%u/%u/%u/%u/%u/%u/%u/%u P=%u/%u/%u/%u/%u/%u/%u/%u -> 0x%x (ior 0x%x)",
		        fan == kATKDFanCPU ? "CPU" : "GPU", c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7],
		        c[8], c[9], c[10], c[11], c[12], c[13], c[14], c[15], ret, r);
		if (r == kIOReturnSuccess && ret != 1)
			r = ret == ATKD_UNSUPPORTED ? kIOReturnUnsupported : kIOReturnError;
		if (r == kIOReturnSuccess && fan == kATKDFanCPU)
			cpuCurveActive = true;
	}
	IOLockUnlock(lock);
	return r;
}

IOReturn ASUSATKD::setPower(uint32_t which, uint32_t watts, uint32_t *biosRet) {
	uint32_t devId, lo, hi;
	const char *name;
	if (which == kATKDLimitPL1) {
		devId = ATKD_DEV_PPT_PL1_SPL;  lo = ATKD_PL1_MIN_W; hi = ATKD_PL1_MAX_W; name = "PL1";
	} else if (which == kATKDLimitPL2) {
		devId = ATKD_DEV_PPT_PL2_SPPT; lo = ATKD_PL2_MIN_W; hi = ATKD_PL2_MAX_W; name = "PL2";
	} else {
		return kIOReturnBadArgument;
	}
	if (watts < lo || watts > hi) {
		ATKDLOG("%s reject: %u W outside [%u,%u]", name, watts, lo, hi);
		return kIOReturnBadArgument;
	}
	IOLockLock(lock);
	IOReturn r = writeGateLocked(name);
	if (r == kIOReturnSuccess) {
		// asus-armoury: GU605M requires_fan_curve -> PPT only with an active custom curve.
		uint32_t fmae = 0;
		bool active = cpuCurveActive;
		if (readECNameLocked("FMAE", &fmae) == kIOReturnSuccess)
			active = (fmae & 1) != 0;
		if (!active) {
			ATKDLOG("%s refused: no active custom fan curve (FMAE=0) - apply curves first", name);
			r = kIOReturnNotPermitted;
		}
	}
	if (r == kIOReturnSuccess) {
		uint32_t args[5] = {devId, watts, 0, 0, 0};
		uint32_t ret = 0;
		r = wmnbIntegerLocked(ATKD_METHOD_DEVS, args, &ret);
		*biosRet = ret;
		lastBiosResult = ret;
		writeCount++;
		ATKDLOG("DEVS %s (0x%08x) = %u W -> 0x%x (ior 0x%x)", name, devId, watts, ret, r);
		if (r == kIOReturnSuccess && ret != 1)
			r = ret == ATKD_UNSUPPORTED ? kIOReturnUnsupported : kIOReturnError;
	}
	IOLockUnlock(lock);
	return r;
}

IOReturn ASUSATKD::setMode(uint32_t mode, uint32_t *biosRet) {
	if (mode > kATKDModeSilent)
		return kIOReturnBadArgument;
	IOLockLock(lock);
	IOReturn r = writeGateLocked("mode");
	if (r == kIOReturnSuccess) {
		uint32_t args[5] = {ATKD_DEV_PERF_MODE, mode, 0, 0, 0};
		uint32_t ret = 0;
		r = wmnbIntegerLocked(ATKD_METHOD_DEVS, args, &ret);
		*biosRet = ret;
		lastBiosResult = ret;
		writeCount++;
		cpuCurveActive = false; // BIOS drops custom curves on mode change
		ATKDLOG("DEVS mode = %u -> 0x%x (ior 0x%x)", mode, ret, r);
		if (r == kIOReturnSuccess && ret != 1)
			r = ret == ATKD_UNSUPPORTED ? kIOReturnUnsupported : kIOReturnError;
	}
	IOLockUnlock(lock);
	return r;
}

IOReturn ASUSATKD::setChargeLimit(uint32_t percent, uint32_t *biosRet) {
	if (percent < ATKD_CHARGE_MIN || percent > ATKD_CHARGE_MAX) {
		ATKDLOG("charge limit reject: %u%% outside [%u,%u]", percent, ATKD_CHARGE_MIN, ATKD_CHARGE_MAX);
		return kIOReturnBadArgument;
	}
	IOLockLock(lock);
	IOReturn r = writeGateLocked("charge limit");
	if (r == kIOReturnSuccess && !chargePresent) {
		ATKDLOG("charge limit refused: DSTS 0x%08x has no presence bit", ATKD_DEV_CHARGE_LIMIT);
		r = kIOReturnUnsupported;
	}
	if (r == kIOReturnSuccess) {
		uint32_t args[5] = {ATKD_DEV_CHARGE_LIMIT, percent, 0, 0, 0};
		uint32_t ret = 0;
		r = wmnbIntegerLocked(ATKD_METHOD_DEVS, args, &ret);
		*biosRet = ret;
		lastBiosResult = ret;
		writeCount++;
		ATKDLOG("DEVS charge limit = %u%% -> 0x%x (ior 0x%x)", percent, ret, r);
		if (r == kIOReturnSuccess && ret != 1)
			r = ret == ATKD_UNSUPPORTED ? kIOReturnUnsupported : kIOReturnError;
		if (r == kIOReturnSuccess)
			chargeLimit = percent;
	}
	IOLockUnlock(lock);
	return r;
}

// ---------------------------------------------------------------------------------------------
// Notifications: drain ATKQ like Windows/Linux WMI does (Notify(ATKD,0xFF) -> _WED(0xFF)).
// ---------------------------------------------------------------------------------------------

IOReturn ASUSATKD::message(UInt32 type, IOService *provider, void *argument) {
	if (type == kIOACPIMessageDeviceNotification && provider == atkd) {
		uint32_t code = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(argument));
		if (code == 0xFF && notifyTimer)
			notifyTimer->setTimeoutMS(1);
		return kIOReturnSuccess;
	}
	return IOService::message(type, provider, argument);
}

void ASUSATKD::notifyFired(IOTimerEventSource *) {
	drainEvents();
}

void ASUSATKD::drainEvents() {
	if (!atkd || !lock)
		return;
	IOLockLock(lock);
	for (int i = 0; i < 16; i++) {
		OSNumber *arg = OSNumber::withNumber(0xFFULL, 32);
		if (!arg)
			break;
		OSObject *params[1] = {arg};
		OSObject *res = nullptr;
		IOReturn r = atkd->evaluateObject("_WED", &res, params, 1);
		arg->release();
		auto n = OSDynamicCast(OSNumber, res);
		uint64_t v = n ? n->unsigned64BitValue() : ~0ULL;
		OSSafeReleaseNULL(res);
		if (r != kIOReturnSuccess || v >= 0xFFFFFFFFULL)
			break;
		lastEvent = static_cast<uint32_t>(v);
		eventCount++;
	}
	IOLockUnlock(lock);
}
