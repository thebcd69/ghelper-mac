// SPDX-License-Identifier: GPL-3.0-only
// IOKit client for ASUSATKD.kext. ABI mirrors kext/ASUSATKD/ATKDShared.h — keep in sync.

import Foundation
import IOKit

enum ATKD {
    static let serviceClass = "ASUSATKD"
    static let abiVersion: UInt32 = 2

    // selectors
    static let selGetInfo: UInt32 = 0
    static let selDSTS: UInt32 = 1
    static let selDSTSBuffer: UInt32 = 2
    static let selSetFanCurve: UInt32 = 3
    static let selSetPower: UInt32 = 4
    static let selSetMode: UInt32 = 5
    static let selReadEC: UInt32 = 6
    static let selReadCurve: UInt32 = 7
    static let selSetChargeLimit: UInt32 = 8

    // device ids (docs/RESEARCH.md)
    static let devCPUFan: UInt32 = 0x0011_0013
    static let devGPUFan: UInt32 = 0x0011_0014
    static let devCPUFanCurve: UInt32 = 0x0011_0024
    static let devGPUFanCurve: UInt32 = 0x0011_0025
    static let devCPUFanFloor: UInt32 = 0x0011_0026
    static let devGPUFanFloor: UInt32 = 0x0011_0027
    static let devCPUTemp: UInt32 = 0x0012_0094
    static let devPerfMode: UInt32 = 0x0012_0075
    static let devChargeLimit: UInt32 = 0x0012_0057
    static let chargeMin = 40
    static let chargeMax = 100
    static let unsupported: UInt32 = 0xFFFF_FFFE
    static let presentBit: UInt32 = 0x0001_0000

    // EC fields
    static let ecFTBL: UInt64 = 0
    static let ecPL01: UInt64 = 1
    static let ecPL02: UInt64 = 2
    static let ecFMAE: UInt64 = 3
    static let ecCTMP: UInt64 = 4
    static let ecFRMS: UInt64 = 5
    static let ecFRS2: UInt64 = 6
    static let ecPL1V: UInt64 = 7
    static let ecPL2V: UInt64 = 8
    static let ecRSOC: UInt64 = 9

    // info flags
    static let flagAttached: UInt32 = 1 << 0
    static let flagWritesEnabled: UInt32 = 1 << 1
    static let flagReadOnlyArg: UInt32 = 1 << 2
    static let flagCpuCurveActive: UInt32 = 1 << 3
    static let flagDSDTMatch: UInt32 = 1 << 4
    static let flagECFound: UInt32 = 1 << 5
    static let flagModeIdPresent: UInt32 = 1 << 6
    static let flagForceArg: UInt32 = 1 << 7
    static let flagChargePresent: UInt32 = 1 << 8
    static let flagERAMMapped: UInt32 = 1 << 9
}

struct ATKDInfo {
    var abiVersion: UInt32 = 0
    var flags: UInt32 = 0
    var dsdtOemRevision: UInt32 = 0
    var lastEvent: UInt32 = 0
    var eventCount: UInt32 = 0
    var writeCount: UInt32 = 0
    var lastBiosResult: UInt32 = 0
    var pl1Min: UInt32 = 0, pl1Max: UInt32 = 0, pl2Min: UInt32 = 0, pl2Max: UInt32 = 0
    var chargeLimit: UInt32 = 0

    init(bytes: [UInt8]) {
        func u32(_ i: Int) -> UInt32 {
            let o = i * 4
            guard o + 3 < bytes.count else { return 0 }
            return UInt32(bytes[o]) | UInt32(bytes[o + 1]) << 8 | UInt32(bytes[o + 2]) << 16 | UInt32(bytes[o + 3]) << 24
        }
        abiVersion = u32(0); flags = u32(1); dsdtOemRevision = u32(2); lastEvent = u32(3)
        eventCount = u32(4); writeCount = u32(5); lastBiosResult = u32(6)
        pl1Min = u32(7); pl1Max = u32(8); pl2Min = u32(9); pl2Max = u32(10)
        chargeLimit = u32(11)
    }

    func has(_ f: UInt32) -> Bool { flags & f != 0 }
}

struct ATKDError: Error, CustomStringConvertible {
    let code: kern_return_t
    var hex: String { String(format: "0x%08X", UInt32(bitPattern: code)) }
    var description: String {
        switch UInt32(bitPattern: code) {
        case 0xE000_02E2: return "izin yok / kext reddetti (\(hex)) — log: ATKD:"
        case 0xE000_02C2: return "geçersiz değer, kext reddetti (\(hex))"
        case 0xE000_02C7: return "BIOS desteklemiyor (\(hex))"
        case 0xE000_02C1: return "yetki yok (\(hex))"
        case 0xE000_02D5: return "çok sık yazma, bekleyin (\(hex))"
        case 0xE000_02D8: return "BIOS fan tabanı okunamadı (\(hex))"
        default: return "IOKit hatası \(hex)"
        }
    }
}

final class ATKDClient {
    private var conn: io_connect_t = 0

    var isOpen: Bool { conn != 0 }

    @discardableResult
    func open() -> Bool {
        if conn != 0 { return true }
        let svc = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching(ATKD.serviceClass))
        guard svc != 0 else { return false }
        defer { IOObjectRelease(svc) }
        var c: io_connect_t = 0
        let kr = IOServiceOpen(svc, mach_task_self_, 0, &c)
        guard kr == KERN_SUCCESS else { return false }
        conn = c
        return true
    }

    func close() {
        if conn != 0 {
            IOServiceClose(conn)
            conn = 0
        }
    }

    private func call(_ sel: UInt32, _ scalarsIn: [UInt64] = [], structIn: [UInt8] = [],
                      scalarOut: Int = 0, structOut: Int = 0) -> Result<([UInt64], [UInt8]), ATKDError> {
        guard conn != 0 else { return .failure(ATKDError(code: kern_return_t(bitPattern: 0xE000_02CD))) }
        var inScalars = scalarsIn.isEmpty ? [UInt64(0)] : scalarsIn
        var inStruct = structIn.isEmpty ? [UInt8(0)] : structIn
        var outScalars = [UInt64](repeating: 0, count: 16)
        var outScalarCnt = UInt32(scalarOut)
        var outStruct = [UInt8](repeating: 0, count: max(structOut, 1))
        var outStructCnt = structOut
        let nIn = UInt32(scalarsIn.count)
        let nStructIn = structIn.count
        let kr: kern_return_t = inScalars.withUnsafeMutableBufferPointer { sp in
            inStruct.withUnsafeMutableBytes { ip in
                outScalars.withUnsafeMutableBufferPointer { op in
                    outStruct.withUnsafeMutableBytes { os in
                        IOConnectCallMethod(conn, sel,
                                            nIn == 0 ? nil : UnsafePointer(sp.baseAddress), nIn,
                                            nStructIn == 0 ? nil : UnsafeRawPointer(ip.baseAddress), nStructIn,
                                            scalarOut == 0 ? nil : op.baseAddress, &outScalarCnt,
                                            structOut == 0 ? nil : os.baseAddress, &outStructCnt)
                    }
                }
            }
        }
        if kr != KERN_SUCCESS { return .failure(ATKDError(code: kr)) }
        return .success((Array(outScalars.prefix(Int(outScalarCnt))), Array(outStruct.prefix(outStructCnt))))
    }

    func info() -> ATKDInfo? {
        if case let .success((_, s)) = call(ATKD.selGetInfo, structOut: 64) { return ATKDInfo(bytes: s) }
        return nil
    }

    func dsts(_ dev: UInt32) -> UInt32? {
        if case let .success((o, _)) = call(ATKD.selDSTS, [UInt64(dev), 0], scalarOut: 1), let v = o.first {
            return UInt32(truncatingIfNeeded: v)
        }
        return nil
    }

    func dstsBuffer(_ dev: UInt32, arg: UInt32) -> [UInt8]? {
        if case let .success((_, s)) = call(ATKD.selDSTSBuffer, [UInt64(dev), UInt64(arg)], structOut: 32) { return s }
        return nil
    }

    func readEC(_ field: UInt64) -> UInt32? {
        if case let .success((o, _)) = call(ATKD.selReadEC, [field], scalarOut: 1), let v = o.first {
            return UInt32(truncatingIfNeeded: v)
        }
        return nil
    }

    func readCurve(fan: Int) -> [UInt8]? {
        if case let .success((_, s)) = call(ATKD.selReadCurve, [UInt64(fan)], structOut: 16) { return s }
        return nil
    }

    func setFanCurve(fan: Int, bytes: [UInt8]) -> Result<UInt32, ATKDError> {
        precondition(bytes.count == 16)
        return call(ATKD.selSetFanCurve, [UInt64(fan)], structIn: bytes, scalarOut: 1)
            .map { UInt32(truncatingIfNeeded: $0.0.first ?? 0) }
    }

    func setPower(limit: Int, watts: Int) -> Result<UInt32, ATKDError> {
        call(ATKD.selSetPower, [UInt64(limit), UInt64(watts)], scalarOut: 1)
            .map { UInt32(truncatingIfNeeded: $0.0.first ?? 0) }
    }

    func setMode(_ mode: Int) -> Result<UInt32, ATKDError> {
        call(ATKD.selSetMode, [UInt64(mode)], scalarOut: 1)
            .map { UInt32(truncatingIfNeeded: $0.0.first ?? 0) }
    }

    func setChargeLimit(_ percent: Int) -> Result<UInt32, ATKDError> {
        call(ATKD.selSetChargeLimit, [UInt64(percent)], scalarOut: 1)
            .map { UInt32(truncatingIfNeeded: $0.0.first ?? 0) }
    }
}
