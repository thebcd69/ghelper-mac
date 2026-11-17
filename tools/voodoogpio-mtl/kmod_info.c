// kmod_info for VoodooGPIO (what Xcode would generate). Link: *.o kmod_info.o -lkmod
#include <mach/mach_types.h>
#include <mach/kmod.h>
#ifndef KEXT_VERSION
#define KEXT_VERSION "1.1"
#endif
extern kern_return_t _start(kmod_info_t *ki, void *data);
extern kern_return_t _stop(kmod_info_t *ki, void *data);
__attribute__((visibility("default"))) KMOD_EXPLICIT_DECL(org.coolstar.VoodooGPIO, KEXT_VERSION, _start, _stop)
__private_extern__ kmod_start_func_t *_realmain = 0;
__private_extern__ kmod_stop_func_t *_antimain = 0;
__private_extern__ int _kext_apple_cc = 6000;
