//
//  ASUSATKDUserClient.cpp — strictly whitelisted user client for ASUSATKD.
//
//  SPDX-License-Identifier: GPL-3.0-only
//

#include "ASUSATKD.hpp"

OSDefineMetaClassAndStructors(ASUSATKDUserClient, IOUserClient)

// {function, scalarIn, structIn, scalarOut, structOut}
const IOExternalMethodDispatch ASUSATKDUserClient::sMethods[kATKDSelCount] = {
	{&ASUSATKDUserClient::sGetInfo,     0, 0,                    0, sizeof(ATKDInfo)},
	{&ASUSATKDUserClient::sDSTS,        2, 0,                    1, 0},
	{&ASUSATKDUserClient::sDSTSBuffer,  2, 0,                    0, ATKD_DSTS_BUF_BYTES},
	{&ASUSATKDUserClient::sSetFanCurve, 1, ATKD_FAN_CURVE_BYTES, 1, 0},
	{&ASUSATKDUserClient::sSetPower,    2, 0,                    1, 0},
	{&ASUSATKDUserClient::sSetMode,     1, 0,                    1, 0},
	{&ASUSATKDUserClient::sReadEC,      1, 0,                    1, 0},
	{&ASUSATKDUserClient::sReadCurve,   1, 0,                    0, ATKD_FAN_CURVE_BYTES},
};

bool ASUSATKDUserClient::initWithTask(task_t owningTask, void *securityToken, UInt32 type, OSDictionary *properties) {
	if (!IOUserClient::initWithTask(owningTask, securityToken, type, properties))
		return false;
	drv = nullptr;
	task = owningTask;
	// Reads: anyone. Writes: root or the user logged in at the local console.
	mayWrite = clientHasPrivilege(owningTask, kIOClientPrivilegeAdministrator) == kIOReturnSuccess ||
	           clientHasPrivilege(owningTask, kIOClientPrivilegeLocalUser) == kIOReturnSuccess;
	return true;
}

bool ASUSATKDUserClient::start(IOService *provider) {
	drv = OSDynamicCast(ASUSATKD, provider);
	if (!drv || !IOUserClient::start(provider))
		return false;
	drv->retain();
	return true;
}

void ASUSATKDUserClient::stop(IOService *provider) {
	OSSafeReleaseNULL(drv);
	IOUserClient::stop(provider);
}

IOReturn ASUSATKDUserClient::clientClose() {
	if (!isInactive())
		terminate();
	return kIOReturnSuccess;
}

IOReturn ASUSATKDUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments *arguments,
                                            IOExternalMethodDispatch *, OSObject *, void *) {
	if (selector >= kATKDSelCount || !drv)
		return kIOReturnUnsupported;
	return IOUserClient::externalMethod(selector, arguments,
	                                    const_cast<IOExternalMethodDispatch *>(&sMethods[selector]), this, nullptr);
}

static inline ASUSATKDUserClient *uc(OSObject *t) { return OSDynamicCast(ASUSATKDUserClient, t); }

IOReturn ASUSATKDUserClient::sGetInfo(OSObject *target, void *, IOExternalMethodArguments *a) {
	auto self = uc(target);
	if (!self || !a->structureOutput || a->structureOutputSize < sizeof(ATKDInfo))
		return kIOReturnBadArgument;
	self->drv->getInfo(static_cast<ATKDInfo *>(a->structureOutput));
	return kIOReturnSuccess;
}

IOReturn ASUSATKDUserClient::sDSTS(OSObject *target, void *, IOExternalMethodArguments *a) {
	auto self = uc(target);
	if (!self || a->scalarInput[0] > 0xFFFFFFFFULL || a->scalarInput[1] > 0xFFFFFFFFULL)
		return kIOReturnBadArgument;
	uint32_t v = 0;
	IOReturn r = self->drv->dsts(static_cast<uint32_t>(a->scalarInput[0]), static_cast<uint32_t>(a->scalarInput[1]), &v);
	a->scalarOutput[0] = v;
	return r;
}

IOReturn ASUSATKDUserClient::sDSTSBuffer(OSObject *target, void *, IOExternalMethodArguments *a) {
	auto self = uc(target);
	if (!self || !a->structureOutput || a->structureOutputSize < ATKD_DSTS_BUF_BYTES ||
	    a->scalarInput[0] > 0xFFFFFFFFULL || a->scalarInput[1] > 0xFFFFFFFFULL)
		return kIOReturnBadArgument;
	return self->drv->dstsBuffer(static_cast<uint32_t>(a->scalarInput[0]), static_cast<uint32_t>(a->scalarInput[1]),
	                             static_cast<uint8_t *>(a->structureOutput), ATKD_DSTS_BUF_BYTES);
}

IOReturn ASUSATKDUserClient::sSetFanCurve(OSObject *target, void *, IOExternalMethodArguments *a) {
	auto self = uc(target);
	if (!self || !a->structureInput || a->structureInputSize != ATKD_FAN_CURVE_BYTES || a->scalarInput[0] > 1)
		return kIOReturnBadArgument;
	if (!self->mayWrite)
		return kIOReturnNotPrivileged;
	uint8_t curve[ATKD_FAN_CURVE_BYTES];
	memcpy(curve, a->structureInput, sizeof(curve)); // copy once: no TOCTOU on user memory
	uint32_t ret = 0;
	IOReturn r = self->drv->setFanCurve(static_cast<uint32_t>(a->scalarInput[0]), curve, &ret);
	a->scalarOutput[0] = ret;
	return r;
}

IOReturn ASUSATKDUserClient::sSetPower(OSObject *target, void *, IOExternalMethodArguments *a) {
	auto self = uc(target);
	if (!self || a->scalarInput[0] > 1 || a->scalarInput[1] > 255)
		return kIOReturnBadArgument;
	if (!self->mayWrite)
		return kIOReturnNotPrivileged;
	uint32_t ret = 0;
	IOReturn r = self->drv->setPower(static_cast<uint32_t>(a->scalarInput[0]), static_cast<uint32_t>(a->scalarInput[1]), &ret);
	a->scalarOutput[0] = ret;
	return r;
}

IOReturn ASUSATKDUserClient::sSetMode(OSObject *target, void *, IOExternalMethodArguments *a) {
	auto self = uc(target);
	if (!self || a->scalarInput[0] > 2)
		return kIOReturnBadArgument;
	if (!self->mayWrite)
		return kIOReturnNotPrivileged;
	uint32_t ret = 0;
	IOReturn r = self->drv->setMode(static_cast<uint32_t>(a->scalarInput[0]), &ret);
	a->scalarOutput[0] = ret;
	return r;
}

IOReturn ASUSATKDUserClient::sReadEC(OSObject *target, void *, IOExternalMethodArguments *a) {
	auto self = uc(target);
	if (!self || a->scalarInput[0] >= kATKDECFieldCount)
		return kIOReturnBadArgument;
	uint32_t v = 0;
	IOReturn r = self->drv->readEC(static_cast<uint32_t>(a->scalarInput[0]), &v);
	a->scalarOutput[0] = v;
	return r;
}

IOReturn ASUSATKDUserClient::sReadCurve(OSObject *target, void *, IOExternalMethodArguments *a) {
	auto self = uc(target);
	if (!self || !a->structureOutput || a->structureOutputSize < ATKD_FAN_CURVE_BYTES || a->scalarInput[0] > 1)
		return kIOReturnBadArgument;
	return self->drv->readCurve(static_cast<uint32_t>(a->scalarInput[0]), static_cast<uint8_t *>(a->structureOutput));
}
