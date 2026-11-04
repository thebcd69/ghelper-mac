// SPDX-License-Identifier: GPL-3.0-only
// External monitor brightness over DDC/CI (VESA MCCS VCP 0x10) through the framebuffer's I2C interface
// (IOFBCopyI2CInterfaceForBus + IOI2CSendRequest), the Intel-Mac path ddcctl and MonitorControl use.
// It needs a framebuffer driver that publishes an I2C bus: the thebcd69 NullMoth fork (NVIDIA, branch ddc-ci-claude)
// or WTLIntelGfx (Intel, branch gu605-ddc). Without one, or with DDC/CI off in the monitor's menu, nothing answers
// and the slider stays hidden. See docs/KURULUM.md §12.

import Foundation
import IOKit
import IOKit.graphics
import IOKit.i2c

struct DDCMonitor: Equatable {
    let framebufferID: UInt64   // registry entry ID of the IOFramebuffer the monitor is on
    let name: String
    let preferred: Bool         // ASUS ROG PG27AQDM
}

enum DDC {
    static let vcpBrightness: UInt8 = 0x10
    /// Every bus access goes through this queue: one request at a time, never on the main thread.
    static let queue = DispatchQueue(label: "GHelperMac.ddc", qos: .userInitiated)

    private static let hostAddress: UInt8 = 0x51     // DDC/CI source address of the host
    private static let writeAddress: UInt32 = 0x6E   // 7-bit 0x37
    private static let readAddress: UInt32 = 0x6F
    private static let replyDelayNs: UInt64 = 50_000_000   // DDC/CI: >= 40 ms between request and reply

    private static func absoluteTime(ns: UInt64) -> UInt64 {
        var tb = mach_timebase_info_data_t()
        mach_timebase_info(&tb)
        return tb.numer == 0 ? ns : ns * UInt64(tb.denom) / UInt64(tb.numer)
    }

    private static func checksum(_ start: UInt8, _ bytes: ArraySlice<UInt8>) -> UInt8 {
        bytes.reduce(start) { $0 ^ $1 }
    }

    /// One request on bus 0 of `fb`: `send` to 0x6E, then (if replyLength > 0) a DDC/CI reply from 0x6F.
    private static func transfer(_ fb: io_service_t, send: [UInt8], replyLength: Int) -> [UInt8]? {
        var bus: io_service_t = 0
        guard IOFBCopyI2CInterfaceForBus(fb, 0, &bus) == kIOReturnSuccess, bus != 0 else { return nil }
        defer { IOObjectRelease(bus) }
        var connect: IOI2CConnectRef?
        guard IOI2CInterfaceOpen(bus, IOOptionBits(), &connect) == kIOReturnSuccess, let c = connect else { return nil }
        defer { IOI2CInterfaceClose(c, IOOptionBits()) }

        var s = send
        var r = [UInt8](repeating: 0, count: max(replyLength, 1))
        let ok: Bool = s.withUnsafeMutableBufferPointer { sp in
            r.withUnsafeMutableBufferPointer { rp in
                var req = IOI2CRequest()
                req.sendTransactionType = IOOptionBits(kIOI2CSimpleTransactionType)
                req.sendAddress = writeAddress
                req.sendBuffer = vm_address_t(bitPattern: sp.baseAddress)
                req.sendBytes = UInt32(sp.count)
                if replyLength > 0 {
                    req.replyTransactionType = IOOptionBits(kIOI2CDDCciReplyTransactionType)
                    req.replyAddress = readAddress
                    req.replyBuffer = vm_address_t(bitPattern: rp.baseAddress)
                    req.replyBytes = UInt32(replyLength)
                    req.minReplyDelay = absoluteTime(ns: replyDelayNs)
                }
                return IOI2CSendRequest(c, IOOptionBits(), &req) == kIOReturnSuccess && req.result == kIOReturnSuccess
            }
        }
        return ok ? Array(r.prefix(replyLength)) : nil
    }

    /// VCP read: (current, maximum), nil when the monitor gave no valid reply (3 tries).
    static func get(_ fb: io_service_t, code: UInt8) -> (current: Int, maximum: Int)? {
        var send: [UInt8] = [hostAddress, 0x82, 0x01, code, 0]
        send[4] = checksum(UInt8(writeAddress), send[0..<4])
        for attempt in 0..<3 {
            if attempt > 0 { usleep(60_000) }
            guard let r = transfer(fb, send: send, replyLength: 11), r.count == 11 else { continue }
            // 6E 88 02 <result> <code> <type> <maxH> <maxL> <curH> <curL> <checksum>, checksum seeded with 0x50
            guard checksum(0x50, r[0..<10]) == r[10], r[2] == 0x02, r[3] == 0x00, r[4] == code else { continue }
            let maximum = Int(r[6]) << 8 | Int(r[7]), current = Int(r[8]) << 8 | Int(r[9])
            guard maximum > 0, current <= maximum else { continue }
            return (current, maximum)
        }
        return nil
    }

    @discardableResult
    static func set(_ fb: io_service_t, code: UInt8, value: Int) -> Bool {
        let v = UInt16(clamping: max(0, value))
        var send: [UInt8] = [hostAddress, 0x84, 0x03, code, UInt8(v >> 8), UInt8(v & 0xFF), 0]
        send[6] = checksum(UInt8(writeAddress), send[0..<6])
        let ok = transfer(fb, send: send, replyLength: 0) != nil
        usleep(50_000)   // DDC/CI: let the monitor process a set before the next request
        return ok
    }

    static func withFramebuffer<T>(_ id: UInt64, _ body: (io_service_t) -> T?) -> T? {
        let fb = IOServiceGetMatchingService(kIOMainPortDefault, IORegistryEntryIDMatching(id))
        guard fb != 0 else { return nil }
        defer { IOObjectRelease(fb) }
        return body(fb)
    }

    /// The monitor's name from its EDID (IODisplayConnect below the framebuffer), or nil.
    private static func displayName(_ fb: io_service_t) -> String? {
        var iter: io_iterator_t = 0
        guard IORegistryEntryGetChildIterator(fb, kIOServicePlane, &iter) == KERN_SUCCESS else { return nil }
        defer { IOObjectRelease(iter) }
        var name: String?
        var child = IOIteratorNext(iter)
        while child != 0 {
            if name == nil, IOObjectConformsTo(child, "IODisplayConnect") != 0,
               let info = IODisplayCreateInfoDictionary(child, IOOptionBits(kIODisplayOnlyPreferredName))?
                   .takeRetainedValue() as? [String: Any],
               let names = info[kDisplayProductName] as? [String: String], let first = names.values.first {
                name = first
            }
            IOObjectRelease(child)
            child = IOIteratorNext(iter)
        }
        return name
    }

    /// Every framebuffer with an I2C bus whose monitor answers a brightness read; the PG27AQDM first. Bus traffic:
    /// one VCP read (up to 3 tries) per framebuffer that has a bus. Call on `queue`.
    static func discover() -> (DDCMonitor, current: Int, maximum: Int)? {
        var iter: io_iterator_t = 0
        guard IOServiceGetMatchingServices(kIOMainPortDefault, IOServiceMatching("IOFramebuffer"), &iter) == KERN_SUCCESS else {
            return nil
        }
        defer { IOObjectRelease(iter) }
        var found: [(DDCMonitor, Int, Int)] = []
        var fb = IOIteratorNext(iter)
        while fb != 0 {
            var bus: io_service_t = 0
            if IOFBCopyI2CInterfaceForBus(fb, 0, &bus) == kIOReturnSuccess, bus != 0 {
                IOObjectRelease(bus)
                if let v = get(fb, code: vcpBrightness) {
                    var id: UInt64 = 0
                    IORegistryEntryGetRegistryEntryID(fb, &id)
                    let n = displayName(fb) ?? "Harici monitör"
                    found.append((DDCMonitor(framebufferID: id, name: n, preferred: n.uppercased().contains("PG27AQDM")),
                                  v.current, v.maximum))
                }
            }
            IOObjectRelease(fb)
            fb = IOIteratorNext(iter)
        }
        guard let best = found.first(where: { $0.0.preferred }) ?? found.first else { return nil }
        return (best.0, current: best.1, maximum: best.2)
    }
}
