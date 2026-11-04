//
//  ASUSATKD.hpp — minimal ASUS ATKD (WMNB) bridge for the ROG Zephyrus G16 GU605MY only.
//
//  SPDX-License-Identifier: GPL-3.0-only
//  Protocol knowledge derived from G-Helper (GPL-3.0, seerge/g-helper) and Linux asus-wmi.
//

#ifndef ASUSATKD_hpp
#define ASUSATKD_hpp

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>
#include <IOKit/acpi/IOACPIPlatformDevice.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>
#include "ATKDShared.h"

#define ATKDLOG(fmt, ...) IOLog("ATKD: " fmt "\n", ##__VA_ARGS__)

class ASUSATKD : public IOService {
	OSDeclareDefaultStructors(ASUSATKD)

public:
	bool      init(OSDictionary *dict = nullptr) APPLE_KEXT_OVERRIDE;
	IOService *probe(IOService *provider, SInt32 *score) APPLE_KEXT_OVERRIDE;
	bool      start(IOService *provider) APPLE_KEXT_OVERRIDE;
	void      stop(IOService *provider) APPLE_KEXT_OVERRIDE;
	void      free() APPLE_KEXT_OVERRIDE;
	IOReturn  message(UInt32 type, IOService *provider, void *argument = nullptr) APPLE_KEXT_OVERRIDE;

	// Called by ASUSATKDUserClient. All of them serialise on `lock`.
	void     getInfo(ATKDInfo *out);
	IOReturn dsts(uint32_t devId, uint32_t arg, uint32_t *out);
	IOReturn dstsBuffer(uint32_t devId, uint32_t arg, uint8_t *out, uint32_t outLen);
	IOReturn setFanCurve(uint32_t fan, const uint8_t *curve16, uint32_t *biosRet);
	IOReturn setPower(uint32_t which, uint32_t watts, uint32_t *biosRet);
	IOReturn setMode(uint32_t mode, uint32_t *biosRet);
	IOReturn readEC(uint32_t field, uint32_t *out);
	IOReturn readCurve(uint32_t fan, uint8_t *out16);

private:
	IOACPIPlatformDevice *atkd;
	IOACPIPlatformDevice *ec;      // \_SB.PC00.LPCB.EC0 if found (retained), else nullptr
	IOLock               *lock;
	IOWorkLoop           *workLoop;
	IOTimerEventSource   *notifyTimer;  // defers _WED draining out of the ACPI notify context

	bool     readOnlyArg;
	bool     forceArg;
	bool     dsdtMatch;
	bool     signatureOK;
	bool     writesEnabled;
	bool     cpuCurveActive;
	bool     modeIdPresent;
	bool     ecReadable;
	uint32_t dsdtOemRevision;
	uint32_t lastEvent;
	uint32_t eventCount;
	uint32_t writeCount;
	uint32_t lastBiosResult;

	uint64_t rateWindowStartNs;
	uint32_t rateWindowWrites;

	IOReturn callWMNBLocked(uint32_t method, const uint32_t args[5], OSObject **result);
	IOReturn wmnbIntegerLocked(uint32_t method, const uint32_t args[5], uint32_t *out);
	IOReturn dstsBufferLocked(uint32_t devId, uint32_t arg, uint8_t *out, uint32_t outLen, uint32_t *gotLen);
	IOReturn readECNameLocked(const char *name, uint32_t *out);
	bool     validateECObject(const char *name);
	void     findEC();
	void     checkDSDT();
	void     checkSignature();
	IOReturn writeGateLocked(const char *what);
	IOReturn validateCurveLocked(uint32_t fan, const uint8_t *c);

	void        notifyFired(IOTimerEventSource *sender);
	void        drainEvents();
};

class ASUSATKDUserClient : public IOUserClient {
	OSDeclareDefaultStructors(ASUSATKDUserClient)

public:
	bool     initWithTask(task_t owningTask, void *securityToken, UInt32 type, OSDictionary *properties) APPLE_KEXT_OVERRIDE;
	bool     start(IOService *provider) APPLE_KEXT_OVERRIDE;
	void     stop(IOService *provider) APPLE_KEXT_OVERRIDE;
	IOReturn clientClose() APPLE_KEXT_OVERRIDE;
	IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *arguments,
	                        IOExternalMethodDispatch *dispatch = nullptr,
	                        OSObject *target = nullptr, void *reference = nullptr) APPLE_KEXT_OVERRIDE;

private:
	ASUSATKD *drv;
	task_t    task;
	bool      mayWrite;

	static const IOExternalMethodDispatch sMethods[kATKDSelCount];
	static IOReturn sGetInfo(OSObject *target, void *ref, IOExternalMethodArguments *a);
	static IOReturn sDSTS(OSObject *target, void *ref, IOExternalMethodArguments *a);
	static IOReturn sDSTSBuffer(OSObject *target, void *ref, IOExternalMethodArguments *a);
	static IOReturn sSetFanCurve(OSObject *target, void *ref, IOExternalMethodArguments *a);
	static IOReturn sSetPower(OSObject *target, void *ref, IOExternalMethodArguments *a);
	static IOReturn sSetMode(OSObject *target, void *ref, IOExternalMethodArguments *a);
	static IOReturn sReadEC(OSObject *target, void *ref, IOExternalMethodArguments *a);
	static IOReturn sReadCurve(OSObject *target, void *ref, IOExternalMethodArguments *a);
};

#endif /* ASUSATKD_hpp */
