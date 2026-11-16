// SPDX-License-Identifier: GPL-3.0-only
// ghelperctl — command-line client of ASUSATKD.kext for scripts (GU605MY). Only the GPU mode for the next boot.
//   ghelperctl gpu status                      -> mode=<eco|standard|ultimate|unknown> target=<...> plan=0x.. abi=N
//   ghelperctl gpu eco|standard|ultimate|cancel   arms (or clears) the mode the kext writes at restart/shutdown
// Build: clang -O2 -target x86_64-apple-macos13.0 -framework IOKit -framework CoreFoundation ghelperctl.c -o ghelperctl
#include <IOKit/IOKitLib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { kSelGetInfo = 0, kSelSetGpuTarget = 9 };
#define FLAG_GPU_PRESENT (1u << 10)

static const char *name(uint32_t m) {
	switch (m) { case 1: return "eco"; case 2: return "standard"; case 3: return "ultimate"; default: return "none"; }
}

int main(int argc, char **argv) {
	if (argc != 3 || strcmp(argv[1], "gpu") != 0) {
		fprintf(stderr, "usage: ghelperctl gpu status|eco|standard|ultimate|cancel\n");
		return 2;
	}
	io_service_t svc = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("ASUSATKD"));
	if (!svc) { fprintf(stderr, "ASUSATKD.kext not found\n"); return 1; }
	io_connect_t c = 0;
	kern_return_t kr = IOServiceOpen(svc, mach_task_self(), 0, &c);
	IOObjectRelease(svc);
	if (kr != KERN_SUCCESS) { fprintf(stderr, "IOServiceOpen 0x%x\n", kr); return 1; }

	int target = -1;
	if (!strcmp(argv[2], "eco")) target = 1;
	else if (!strcmp(argv[2], "standard")) target = 2;
	else if (!strcmp(argv[2], "ultimate")) target = 3;
	else if (!strcmp(argv[2], "cancel")) target = 0;
	else if (strcmp(argv[2], "status") != 0) { fprintf(stderr, "unknown command %s\n", argv[2]); return 2; }

	int rc = 0;
	if (target >= 0) {
		uint64_t in = (uint64_t)target, out = 0;
		uint32_t n = 1;
		kr = IOConnectCallScalarMethod(c, kSelSetGpuTarget, &in, 1, &out, &n);
		if (kr != KERN_SUCCESS) { fprintf(stderr, "set target %s: 0x%x\n", argv[2], kr); rc = 1; }
		else printf("armed=%s plan=0x%llx\n", name((uint32_t)target), out);
	}
	uint32_t info[16] = {0};
	size_t len = sizeof(info);
	kr = IOConnectCallStructMethod(c, kSelGetInfo, NULL, 0, info, &len);
	if (kr != KERN_SUCCESS) { fprintf(stderr, "info 0x%x\n", kr); IOServiceClose(c); return 1; }
	const char *mode = "unknown";
	if (info[0] >= 3 && (info[1] & FLAG_GPU_PRESENT))
		mode = (info[12] & 1) ? "eco" : (info[13] & 1) ? "standard" : "ultimate";
	printf("mode=%s target=%s plan=0x%x abi=%u eco=0x%08x mux=0x%08x\n", mode, name(info[14]), info[15], info[0],
	       info[12], info[13]);
	IOServiceClose(c);
	return rc;
}
