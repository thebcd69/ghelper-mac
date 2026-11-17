// SPDX-License-Identifier: GPL-3.0-only
// GU605CPU: reads the CPU frequency-control MSRs of every online logical CPU (HWP, EPB, PERF_CTL/STATUS, limits) and
// can write IA32_HWP_REQUEST / IA32_ENERGY_PERF_BIAS on all of them, optionally re-applying them twice a second.
// Diagnosis and fix for the GU605MY (Core Ultra 9 185H, macOS 15, XCPM with a spoofed Comet Lake CPUID), where under a
// full 12-thread load each P-core stayed at its own fixed clock: 3.96, 2.06, 0.86, 0.85, 0.57, 0.55 GHz (2026-11-17).
// Nothing is written unless root writes a sysctl; power limits (MSR 0x610 and friends) are only ever read.
//   sysctl debug.gu605cpu_dump            every CPU's registers, read now
//   sysctl -w debug.gu605cpu_hwp=0x...    IA32_HWP_REQUEST for every CPU (bits 0-31 and 32-41 only); 0 = stop
//   sysctl -w debug.gu605cpu_hold=1       re-apply it every 500 ms when something else changed it
//   sysctl -w debug.gu605cpu_epb=N        IA32_ENERGY_PERF_BIAS (0 = performance .. 15 = power saving); -1 = leave
#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include <sys/sysctl.h>
#include <kern/thread_call.h>
#include <libkern/OSAtomic.h>

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
enum { kN = sizeof kMsr / sizeof kMsr[0], kPmEnable = 0, kHwpReq = 3, kEpb = 6 };

static struct {
    uint8_t seen[kMaxCpu];
    uint32_t apic[kMaxCpu];
    uint8_t ok[kMaxCpu][kN];
    uint64_t v[kMaxCpu][kN];
} gR;
static IOLock *gLock;
static char gBuf[16384];
static uint64_t gHwp;           // what debug.gu605cpu_hwp asks for; 0 = nothing
static int gEpb = -1, gHold;
static volatile SInt32 gRewrites, gWrites, gFaults;
static thread_call_t gCall;

static inline void cpuid(uint32_t l, uint32_t s, uint32_t r[4]) {
    __asm__ volatile("cpuid" : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3]) : "a"(l), "c"(s));
}
static inline void wrmsr64(uint32_t msr, uint64_t v) {
    __asm__ volatile("wrmsr" : : "c"(msr), "a"((uint32_t)v), "d"((uint32_t)(v >> 32)));
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

struct WriteArg { uint64_t hwp; int epb; bool onlyIfDiff; };
static void writeAction(void *p) {
    const WriteArg *a = (const WriteArg *)p;
    uint64_t pm = 0, cur = 0;
    if (a->hwp && rd(0x770, &pm) && (pm & 1) && rd(0x774, &cur)) {
        const uint64_t want = (cur & ~0x3FFFFFFFFFFULL) | (a->hwp & 0x3FFFFFFFFFFULL);   // bits 0-41; 42+ kept
        if (cur != want) { wrmsr64(0x774, want); OSIncrementAtomic(a->onlyIfDiff ? &gRewrites : &gWrites); }
    } else if (a->hwp) OSIncrementAtomic(&gFaults);
    if (a->epb >= 0 && a->epb <= 15 && rd(0x1B0, &cur)) {
        const uint64_t want = (cur & ~0xFULL) | (uint64_t)a->epb;
        if (cur != want) { wrmsr64(0x1B0, want); OSIncrementAtomic(a->onlyIfDiff ? &gRewrites : &gWrites); }
    }
}

static void applyNow(bool hold) {
    WriteArg a = { gHwp, gEpb, hold };
    if (!a.hwp && a.epb < 0) return;
    mp_rendezvous_no_intrs(writeAction, &a);
}

static void tick(thread_call_param_t, thread_call_param_t) {
    if (!gHold || (!gHwp && gEpb < 0)) return;
    applyNow(true);
    uint64_t dl; clock_interval_to_deadline(500, kMillisecondScale, &dl);
    thread_call_enter_delayed(gCall, dl);
}

static int dumpHandler SYSCTL_HANDLER_ARGS {
    IOLockLock(gLock);
    bzero(&gR, sizeof gR);
    mp_rendezvous_no_intrs(readAction, nullptr);
    size_t n = 0;
    n += snprintf(gBuf + n, sizeof gBuf - n, "hwp asked 0x%llx epb %d hold %d | writes %d rewrites %d no-hwp %d\n",
                  gHwp, gEpb, gHold, gWrites, gRewrites, gFaults);
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
static int hwpHandler SYSCTL_HANDLER_ARGS {
    uint64_t v = gHwp;
    int e = sysctl_handle_quad(oidp, &v, 0, req);
    if (e || !req->newptr) return e;
    IOLockLock(gLock); gHwp = v; applyNow(false); IOLockUnlock(gLock);
    LOG("HWP_REQUEST asked 0x%llx (writes %d)", v, gWrites);
    if (gHold) thread_call_enter(gCall);
    return 0;
}
static int epbHandler SYSCTL_HANDLER_ARGS {
    int v = gEpb;
    int e = sysctl_handle_int(oidp, &v, 0, req);
    if (e || !req->newptr) return e;
    if (v < -1 || v > 15) return EINVAL;
    IOLockLock(gLock); gEpb = v; applyNow(false); IOLockUnlock(gLock);
    LOG("EPB asked %d", v);
    if (gHold) thread_call_enter(gCall);
    return 0;
}
static int holdHandler SYSCTL_HANDLER_ARGS {
    int v = gHold;
    int e = sysctl_handle_int(oidp, &v, 0, req);
    if (e || !req->newptr) return e;
    gHold = v ? 1 : 0;
    if (gHold) thread_call_enter(gCall); else thread_call_cancel(gCall);
    return 0;
}

SYSCTL_PROC(_debug, OID_AUTO, gu605cpu_dump, CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_LOCKED, nullptr, 0, dumpHandler, "A", "");
SYSCTL_PROC(_debug, OID_AUTO, gu605cpu_hwp, CTLTYPE_QUAD | CTLFLAG_RW | CTLFLAG_LOCKED, nullptr, 0, hwpHandler, "Q", "");
SYSCTL_PROC(_debug, OID_AUTO, gu605cpu_epb, CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED, nullptr, 0, epbHandler, "I", "");
SYSCTL_PROC(_debug, OID_AUTO, gu605cpu_hold, CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED, nullptr, 0, holdHandler, "I", "");

class GU605CPU : public IOService {
    OSDeclareDefaultStructors(GU605CPU)
public:
    bool start(IOService *provider) override {
        if (!IOService::start(provider)) return false;
        gLock = IOLockAlloc();
        gCall = thread_call_allocate(tick, nullptr);
        if (!gLock || !gCall) return false;
        sysctl_register_oid(&sysctl__debug_gu605cpu_dump);
        sysctl_register_oid(&sysctl__debug_gu605cpu_hwp);
        sysctl_register_oid(&sysctl__debug_gu605cpu_epb);
        sysctl_register_oid(&sysctl__debug_gu605cpu_hold);
        int n = 0;
        IOLockLock(gLock); bzero(&gR, sizeof gR); mp_rendezvous_no_intrs(readAction, nullptr);
        for (int c = 0; c < kMaxCpu; c++) if (gR.seen[c]) n++;
        IOLockUnlock(gLock);
        LOG("started: %d CPUs; cpu0 PM_ENABLE 0x%llx HWP_REQ 0x%llx EPB 0x%llx", n, gR.v[0][kPmEnable], gR.v[0][kHwpReq], gR.v[0][kEpb]);
        registerService();
        return true;
    }
    void stop(IOService *provider) override {
        gHold = 0;
        if (gCall) { thread_call_cancel(gCall); while (!thread_call_free(gCall)) { thread_call_cancel(gCall); IOSleep(1); } gCall = nullptr; }
        sysctl_unregister_oid(&sysctl__debug_gu605cpu_dump);
        sysctl_unregister_oid(&sysctl__debug_gu605cpu_hwp);
        sysctl_unregister_oid(&sysctl__debug_gu605cpu_epb);
        sysctl_unregister_oid(&sysctl__debug_gu605cpu_hold);
        IOService::stop(provider);
    }
};
OSDefineMetaClassAndStructors(GU605CPU, IOService)
