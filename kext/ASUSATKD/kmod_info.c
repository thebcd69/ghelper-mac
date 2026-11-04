// SPDX-License-Identifier: GPL-3.0-only
// kmod_info for ASUSATKD (what Xcode would generate). Link: *.o kmod_info.o -lkmod
#include <mach/mach_types.h>
#include <mach/kmod.h>

#ifndef ATKD_VERSION
#define ATKD_VERSION "0.1.0"
#endif

extern kern_return_t _start(kmod_info_t *ki, void *data);
extern kern_return_t _stop(kmod_info_t *ki, void *data);

__attribute__((visibility("default"))) KMOD_EXPLICIT_DECL(com.thebcd69.ASUSATKD, ATKD_VERSION, _start, _stop)
__private_extern__ kmod_start_func_t *_realmain = 0;
__private_extern__ kmod_stop_func_t *_antimain = 0;
#ifdef __APPLE_CC__
__private_extern__ int _kext_apple_cc = __APPLE_CC__;
#else
__private_extern__ int _kext_apple_cc = 6000;
#endif
