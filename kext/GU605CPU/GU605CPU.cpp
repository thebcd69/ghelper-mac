// SPDX-License-Identifier: GPL-3.0-only
// GU605CPU: reads the CPU frequency-control MSRs of every online logical CPU (HWP, EPB, PERF_CTL/STATUS, limits) and
// can write IA32_HWP_REQUEST / IA32_ENERGY_PERF_BIAS on all of them, optionally re-applying them twice a second.
// Diagnosis and fix for the GU605MY (Core Ultra 9 185H, macOS 15, XCPM with a spoofed Comet Lake CPUID), where under a
// full 12-thread load each P-core stayed at its own fixed clock: 3.96, 2.06, 0.86, 0.85, 0.57, 0.55 GHz (2026-11-17).
// v0.2, read only (2026-11-17): v0.1 could also write HWP_REQUEST on every CPU and re-apply it twice a second through
// mp_rendezvous_no_intrs, which stops all CPUs at once. At 12:09 one CPU sat with interrupts off in XeGfx (GuC command
// send) and the rendezvous panicked ("mp_rv_action() entry: 11 of 12 responses"). The fix moved into the kernel itself
// (OpenCore patch on XCPM's HWP request, docs/RESEARCH.md section 13); this kext only reads, and only when asked:
//   sysctl debug.gu605cpu_dump            every CPU's registers, read now (one rendezvous per read)
// v0.3 (2026-11-17), for GHelperMac's power display:
//   sysctl debug.gu605cpu_power           package-scope counters read on whichever CPU runs the sysctl, no rendezvous,
//                                         no IPI: RAPL units/energy (package, PSys, cores, graphics), TjMax, package
//                                         temperature, and the live PCIe link status of RP12 (the dGPU root port).
#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include <sys/sysctl.h>
#include <libkern/OSAtomic.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <kern/clock.h>

extern "C" {
void mp_rendezvous_no_intrs(void (*action)(void *), void *arg);
int rdmsr_carefully(uint32_t msr, uint32_t *lo, uint32_t *hi);
int cpu_number(void);
}

#define LOG(fmt, ...) IOLog("GU605CPU: " fmt "\n", ##__VA_ARGS__)
enum { kMaxCpu = 64 };
static const uint32_t kMsr[] = { 0x770, 0x771, 0x772, 0x774, 0x199, 0x198, 0x1B0, 0x64F, 0x1A0, 0xCE, 0x1AD, 0x610, 0x1FC, 0xE2, 0x19C };
static const char *const kName[] = { "PM_ENABLE", "HWP_CAP", "HWP_REQ_PKG", "HWP_REQ", "PERF_CTL", "PERF_STATUS", "EPB",
                                     "LIMIT_REASONS", "MISC_ENABLE", "PLATFORM_INFO", "TURBO_RATIOS", "PKG_POWER_LIMIT",
                                     "POWER_CTL", "PKG_CST_CONFIG", "THERM_STATUS" };
enum { kN = sizeof kMsr / sizeof kMsr[0] };

static struct {
    uint8_t seen[kMaxCpu];
    uint32_t apic[kMaxCpu];
    uint8_t ok[kMaxCpu][kN];
    uint64_t v[kMaxCpu][kN];
} gR;
static IOLock *gLock;
static char gBuf[16384];

static inline void cpuid(uint32_t l, uint32_t s, uint32_t r[4]) {
    __asm__ volatile("cpuid" : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3]) : "a"(l), "c"(s));
}
static bool rd(uint32_t msr, uint64_t *v) {
    uint32_t lo = 0, hi = 0;
    if (rdmsr_carefully(msr, &lo, &hi)) return false;
    *v = ((uint64_t)hi << 32) | lo;
    return true;
}

static void readAction(void *) {
    const int c = cpu_number();
    if (c < 0 || c >= kMaxCpu) return;
    uint32_t r[4]; cpuid(0xB, 0, r);
    gR.seen[c] = 1; gR.apic[c] = r[3];
    for (int i = 0; i < kN; i++) gR.ok[c][i] = rd(kMsr[i], &gR.v[c][i]);
}

static int dumpHandler SYSCTL_HANDLER_ARGS {
    IOLockLock(gLock);
    bzero(&gR, sizeof gR);
    mp_rendezvous_no_intrs(readAction, nullptr);
    size_t n = 0;
    n += snprintf(gBuf + n, sizeof gBuf - n, "GU605CPU 0.3 (read only)\n");
    for (int c = 0; c < kMaxCpu && n < sizeof gBuf - 512; c++) {
        if (!gR.seen[c]) continue;
        n += snprintf(gBuf + n, sizeof gBuf - n, "cpu %2d apic %3u", c, gR.apic[c]);
        for (int i = 0; i < kN && n < sizeof gBuf - 64; i++)
            n += gR.ok[c][i] ? snprintf(gBuf + n, sizeof gBuf - n, " %s=0x%llx", kName[i], gR.v[c][i])
                             : snprintf(gBuf + n, sizeof gBuf - n, " %s=-", kName[i]);
        n += snprintf(gBuf + n, sizeof gBuf - n, "\n");
    }
    int e = SYSCTL_OUT(req, gBuf, n + 1);
    IOLockUnlock(gLock);
    return e;
}
// ---- power (v0.3) ----
static IOPCIDevice *gRP12;
static uint32_t gRP12Cap;
static bool gRP12Tried;

// RP12 = PEG 0/1/0, the RTX root port (acpi-path IOACPIPlane:/_SB/PC00@0/RP12@10000). Looked up once.
static void findRP12() {
    gRP12Tried = true;
    OSDictionary *m = IOService::serviceMatching("IOPCIDevice");
    if (!m) return;
    OSIterator *it = IOService::getMatchingServices(m);
    m->release();
    if (!it) return;
    while (OSObject *o = it->getNextObject()) {
        IOPCIDevice *d = OSDynamicCast(IOPCIDevice, o);
        if (!d) continue;
        OSString *ap = OSDynamicCast(OSString, d->getProperty("acpi-path"));
        if (!ap || !ap->isEqualTo("IOACPIPlane:/_SB/PC00@0/RP12@10000")) continue;
        UInt8 off = 0;
        if (d->findPCICapability(kIOPCIPCIExpressCapability, &off) && off) {
            d->retain(); gRP12 = d; gRP12Cap = (uint32_t)off;
        }
        break;
    }
    it->release();
}

static char gPBuf[512];
static int powerHandler SYSCTL_HANDLER_ARGS {
    IOLockLock(gLock);
    uint64_t units = 0, pkg = 0, psys = 0, pp0 = 0, pp1 = 0, tt = 0, pth = 0;
    const bool okU = rd(0x606, &units), okP = rd(0x611, &pkg), okS = rd(0x64D, &psys), ok0 = rd(0x639, &pp0),
               ok1 = rd(0x641, &pp1), okT = rd(0x1A2, &tt), okH = rd(0x1B1, &pth);
    uint64_t ns = 0; absolutetime_to_nanoseconds(mach_absolute_time(), &ns);
    if (!gRP12Tried) findRP12();
    int link = -1;
    if (gRP12) link = gRP12->configRead16(gRP12Cap + 0x12);   // PCIe Link Status
    const unsigned tj = okT ? (unsigned)((tt >> 16) & 0xFF) : 0;
    const int ptemp = (okT && okH && (pth & (1u << 31))) ? (int)tj - (int)((pth >> 16) & 0x7F) : -1;
    size_t n = (size_t)snprintf(gPBuf, sizeof gPBuf,
        "v=0.3 ns=%llu units=%s0x%llx pkg=%s%llu psys=%s%llu pp0=%s%llu pp1=%s%llu tjmax=%u pkgtemp=%d rp12link=%d\n",
        ns, okU ? "" : "-", units, okP ? "" : "-", pkg & 0xFFFFFFFFull, okS ? "" : "-", psys & 0xFFFFFFFFull,
        ok0 ? "" : "-", pp0 & 0xFFFFFFFFull, ok1 ? "" : "-", pp1 & 0xFFFFFFFFull, tj, ptemp, link);
    int e = SYSCTL_OUT(req, gPBuf, n + 1);
    IOLockUnlock(gLock);
    return e;
}
SYSCTL_PROC(_debug, OID_AUTO, gu605cpu_power, CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_LOCKED, nullptr, 0, powerHandler, "A", "");

SYSCTL_PROC(_debug, OID_AUTO, gu605cpu_dump, CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_LOCKED, nullptr, 0, dumpHandler, "A", "");

class GU605CPU : public IOService {
    OSDeclareDefaultStructors(GU605CPU)
public:
    bool start(IOService *provider) override {
        if (!IOService::start(provider)) return false;
        gLock = IOLockAlloc();
        if (!gLock) return false;
        sysctl_register_oid(&sysctl__debug_gu605cpu_dump);
        sysctl_register_oid(&sysctl__debug_gu605cpu_power);
        LOG("started (0.3, read only; nothing is touched until debug.gu605cpu_dump / _power is read)");
        registerService();
        return true;
    }
    void stop(IOService *provider) override {
        sysctl_unregister_oid(&sysctl__debug_gu605cpu_power);
        sysctl_unregister_oid(&sysctl__debug_gu605cpu_dump);
        if (gRP12) { gRP12->release(); gRP12 = nullptr; }
        IOService::stop(provider);
    }
};
OSDefineMetaClassAndStructors(GU605CPU, IOService)
