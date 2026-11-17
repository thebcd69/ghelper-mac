# VoodooGPIO: Meteor Lake-P (INTC1083) for the GU605MY

Drop-in class for VoodooGPIO (VoodooI2C/VoodooGPIO @ 44fc744, the 1.1 shipped with VoodooI2C 2.9.1):
`VoodooGPIOMeteorLakeP.{hpp,cpp}` go into `VoodooGPIO/MeteorLake-P/`, `build.sh` builds the whole kext with
clang + MacKernelSDK and adds the `INTC1083`/`INTC105E` personality to the installed kext's Info.plist.
Pin/community tables: Linux `drivers/pinctrl/intel/pinctrl-meteorlake.c`; they match the BIOS GPCM table
(group sizes and ACPI GPIO bases 0, 32, 64, 96, ... 448).

Status 2026-11-17: binds GPI0 (`VoodooGPIOMeteorLakeP`), VoodooI2C takes the touchpad's GpioInt (pin 352 = GPP_B0,
level low) and reports "Interrupt Mode = GPIO", but no touchpad events arrive. The BIOS sets the touchpad up for
IOxAPIC routing (_CRS also carries APIC IRQ 0x58, above the 0x2f VoodooI2C accepts). Under investigation with
GU605CPU 0.4 (`sysctl debug.gu605cpu_gpio`: HOSTSW_OWN, PADCFGLOCK, GPI_IS/IE, PADCFG of GPP_B). Polling is back
(`-vi2c-force-polling`) meanwhile.
