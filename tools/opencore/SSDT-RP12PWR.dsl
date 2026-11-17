/*
 * SSDT-RP12PWR — GU605MY: dGPU (RP12, PEG 0/1/0) power-off for macOS Eco mode.
 *
 * Firmware SSDT "MtlP_Rvp" defines \_SB.PC00.RP12.POFF/PON/NCHK, but macOS's
 * ACPICA rejects that whole table (its Scope (\_SB.PC00.I2C3) is created by
 * DSDT module-level code that old ACPICA runs only after all tables load).
 * EC query _Q3F (Eco) calls RP12.DL23 then RP12.POFF; without POFF the method
 * aborts, the dGPU never loses power and UMFG/WOSR/UFON are never updated.
 * ATKD WAT2 (Eco off) calls RP12.NCHK, also missing.
 *
 * This table re-creates only those three methods, copied 1:1 from MtlP_Rvp
 * (OEM rev 0x1000), with the state they read.  Deliberately left out:
 *   - PXP power resource / _PR0 / _PR3 (no OS-driven power-up of the dGPU),
 *   - CEMP/RPSC (PS_ON bookkeeping: GPOM stays non-zero, no effect on Windows
 *     either), PSD0/PSD3 (only for PRTP==2; RP12 is PRTP 4),
 *   - DVES (only exists when DGBA != 0, Intel dGPU), PCPB (unset -> skipped).
 * Darwin only; on any other OS this table defines nothing.
 */
DefinitionBlock ("", "SSDT", 2, "GU605", "RP12PWR", 0x00001000)
{
    External (_SB_.PC00.RP12, DeviceObj)
    External (_SB_.PC00.RP12.DL23, MethodObj)
    External (_SB_.SHPO, MethodObj)    // 2 Arguments
    External (_SB_.CAGS, MethodObj)    // 1 Arguments
    External (SPCO, MethodObj)         // 2 Arguments (\SPCO)
    External (PIN_, PowerResObj)
    External (PIN_.ON__, MethodObj)    // 1 Arguments
    External (PIN_.OFF_, MethodObj)    // 1 Arguments
    External (TRSG, FieldUnitObj)
    External (TRSP, FieldUnitObj)
    External (TPWG, FieldUnitObj)
    External (TPWP, FieldUnitObj)
    External (TWGP, FieldUnitObj)
    External (PDOD, FieldUnitObj)
    External (P0DL, FieldUnitObj)
    External (PEP0, FieldUnitObj)
    External (HGMD, FieldUnitObj)
    External (VEND, FieldUnitObj)

    If (_OSI ("Darwin"))
    {
        Scope (\_SB.PC00.RP12)
        {
            Name (RSTG, Package (0x02) { Zero, Zero })
            Name (PWRG, Package (0x02) { Zero, Zero })
            Name (WAKG, Zero)
            Name (SCLK, 0x06)
            Name (WAKP, Zero)
            Name (WKEN, Zero)
            Name (WOFF, Zero)
            Name (LNRD, Zero)

            // MtlP_Rvp fills these at table load from GNVS; done per call here.
            Method (GPLD, 0, Serialized)
            {
                RSTG [Zero] = TRSG
                RSTG [One] = TRSP
                PWRG [Zero] = TPWG
                PWRG [One] = TPWP
                WAKG = TWGP
            }

            Method (GPPR, 0, NotSerialized)
            {
                If (((WAKP != Zero) && (WKEN == Zero)))
                {
                    Return (Zero)
                }

                Return (One)
            }

            Method (PON, 0, NotSerialized)
            {
                GPLD ()
                If ((WAKG != Zero))
                {
                    \_SB.SHPO (WAKG, One)
                    \_SB.CAGS (WAKG)
                }

                If ((WOFF != Zero))
                {
                    Local0 = ((Timer - WOFF) / 0x2710)
                    If ((Local0 < PDOD))
                    {
                        Sleep ((PDOD - Local0))
                    }

                    WOFF = Zero
                }

                \PIN.ON (PWRG)
                If (((HGMD == 0x02) && ((VEND == 0x10DE) || (VEND == 0x1002))))
                {
                    Sleep (P0DL)
                }
                Else
                {
                    Sleep (PEP0)
                }

                SPCO (SCLK, One)
                \PIN.OFF (RSTG)
                Sleep (0x64)
            }

            Method (POFF, 0, NotSerialized)
            {
                GPLD ()
                Local1 = (LNRD / 0x03E8)
                Sleep (Local1)
                \PIN.ON (RSTG)
                SPCO (SCLK, Zero)
                If ((GPPR () == One))
                {
                    \PIN.OFF (PWRG)
                }

                WOFF = Timer
                If (((WAKG != Zero) && WKEN))
                {
                    \_SB.SHPO (WAKG, Zero)
                }
            }

            Method (NCHK, 2, NotSerialized)
            {
                If ((Arg0 != 0x10DE))
                {
                    Return (Zero)
                }

                Local0 = ((Arg1 & 0xFFFF) >> 0x08)
                If (((Local0 == 0x24) || (Local0 == 0x25)))
                {
                    Return (0x03)
                }
                ElseIf ((Local0 <= 0x1D))
                {
                    Return (One)
                }
                Else
                {
                    Return (0x02)
                }
            }
        }
    }
}
