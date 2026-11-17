# VoodooGPIO: Meteor Lake-P (INTC1083) for the GU605MY

Drop-in class for VoodooGPIO (VoodooI2C/VoodooGPIO @ 44fc744, the 1.1 shipped with VoodooI2C 2.9.1):
`VoodooGPIOMeteorLakeP.{hpp,cpp}` go into `VoodooGPIO/MeteorLake-P/`, `build.sh` builds the whole kext with
clang + MacKernelSDK and adds the `INTC1083`/`INTC105E` personality to the installed kext's Info.plist.
Pin/community tables: Linux `drivers/pinctrl/intel/pinctrl-meteorlake.c`; they match the BIOS GPCM table
(group sizes and ACPI GPIO bases 0, 32, 64, 96, ... 448).

Status 2026-11-17 (first try): binds GPI0 (`VoodooGPIOMeteorLakeP`), VoodooI2C takes the touchpad's GpioInt (pin 352 = GPP_B0,
level low) and reports "Interrupt Mode = GPIO", but no touchpad events arrive. The BIOS sets the touchpad up for
IOxAPIC routing (_CRS also carries APIC IRQ 0x58, above the 0x2f VoodooI2C accepts). Under investigation with
GU605CPU 0.4 (`sysctl debug.gu605cpu_gpio`: HOSTSW_OWN, PADCFGLOCK, GPI_IS/IE, PADCFG of GPP_B). Polling is back
(`-vi2c-force-polling`) meanwhile.

Cause found 14:40 (GU605CPU 0.4 dump, polling boot): GPP_B0 DW0 = 0x90900102 (GPIO input, RXINV, level,
GPIROUTIOXAPIC set), DW1 INTSEL = 0x58, HOSTSW_OWN bit 0 = 1 (host), PADCFGLOCK bit 0 = 0 (unlocked), GPI_IE bit 0 = 1.
VoodooGPIO never clears GPIROUTIOXAPIC/SCI/SMI/NMI (Linux does in intel_gpio_set_gpio_mode()), so the touchpad's
events go to IOxAPIC IRQ 88 and never to the GPIO controller's IRQ 14. `VoodooGPIOIntel.cpp.patched` clears them
(and sets RX on / TX off) in intel_gpio_irq_set_type().

Second try 14:48 (routing cleared: GPP_B0 DW0 0x90800102): still no events. Sampling all GPP_B pads every 0.35 s
for 15 s while the touchpad was used showed no RX change on any of them, GPI_IS stayed 0. So the touchpad's
interrupt line did not move at all in GPIO mode (the device seems never to raise it, or it is not GPP_B0 on this
unit despite GpioInt 352 / INTSEL 0x58). Boot logs were already gone from the 1 MB message buffer. Parked;
polling restored (`-vi2c-force-polling`).
