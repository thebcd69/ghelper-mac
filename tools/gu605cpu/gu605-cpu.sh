#!/bin/bash
# GU605CPU: XCPM asks the P-cores for clocks in 100 MHz steps, the hardware counts ~78 MHz steps, and most cores sat at
# 0.6-0.9 GHz under load (2026-11-17). Hardware-chosen clocks instead, re-applied twice a second. Values: gu605.conf CPU_HWP.
# Installed as /Library/XeGfx/gu605/gu605-cpu.sh, started by /Library/LaunchDaemons/com.thebcd69.gu605cpu.plist (RunAtLoad).
C=/Library/XeGfx/gu605/gu605.conf
v=$(sed -n 's/^CPU_HWP=//p' "$C" 2>/dev/null | tail -1); v=${v:-0x80004004}
[ "$v" = 0 ] && exit 0
for i in $(seq 1 120); do sysctl -n debug.gu605cpu_hold > /dev/null 2>&1 && break; sleep 1; done
sysctl -w debug.gu605cpu_hwp="$v" && sysctl -w debug.gu605cpu_hold=1
