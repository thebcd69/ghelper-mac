// SPDX-License-Identifier: GPL-3.0-only
// Power readings for the GU605MY: battery (AppleSmartBattery via VirtualSMC/SMCBatteryManager), CPU RAPL counters and
// the dGPU root port's live PCIe link (GU605CPU.kext 0.3, sysctl debug.gu605cpu_power), and per-process CPU time.

import Darwin
import Foundation
import IOKit

/// What the menu-bar title shows.
enum MenuSource: String, CaseIterable, Identifiable, Codable {
    case auto, battery, cpu, platform, temp
    var id: String { rawValue }
    var name: String {
        switch self {
        case .auto: return "Otomatik (pilde sistem, adaptörde CPU)"
        case .battery: return "Pilden çekilen güç"
        case .cpu: return "CPU paketi"
        case .platform: return "Platform toplamı (PSys)"
        case .temp: return "CPU sıcaklığı"
        }
    }
}

struct BatteryInfo {
    var watts: Double?          // > 0 discharging, < 0 charging (power flowing into the battery)
    var amps: Int?              // mA, signed as the gauge reports it (negative = discharge)
    var volts: Int?             // mV
    var percent: Int?
    var rawCapacity: Int?       // mAh now
    var rawMax: Int?            // mAh full charge
    var design: Int?            // mAh design
    var cycles: Int?
    var minutesToEmpty: Int?
    var minutesToFull: Int?
    var external = false
    var charging = false
    var health: Int? { (rawMax != nil && design != nil && design! > 0) ? Int((Double(rawMax!) / Double(design!) * 100).rounded()) : nil }
}

enum BatteryReader {
    static func read() -> BatteryInfo {
        var b = BatteryInfo()
        let s = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("AppleSmartBattery"))
        guard s != 0 else { return b }
        defer { IOObjectRelease(s) }
        var props: Unmanaged<CFMutableDictionary>?
        guard IORegistryEntryCreateCFProperties(s, &props, kCFAllocatorDefault, 0) == KERN_SUCCESS,
              let d = props?.takeRetainedValue() as? [String: Any] else { return b }
        func int(_ k: String) -> Int? { (d[k] as? NSNumber).map { Int(truncatingIfNeeded: $0.int64Value) } }
        let a = int("InstantAmperage") ?? int("Amperage")
        b.amps = a
        b.volts = int("Voltage")
        b.percent = int("CurrentCapacity")
        b.rawCapacity = int("AppleRawCurrentCapacity")
        b.rawMax = int("AppleRawMaxCapacity") ?? int("NominalChargeCapacity")
        b.design = int("DesignCapacity") ?? (d["BatteryData"] as? [String: Any]).flatMap { ($0["DesignCapacity"] as? NSNumber)?.intValue }
        b.cycles = int("CycleCount")
        b.external = (d["ExternalConnected"] as? Bool) ?? false
        b.charging = (d["IsCharging"] as? Bool) ?? false
        if let t = int("AvgTimeToEmpty"), t > 0, t < 65535 { b.minutesToEmpty = t }
        if let t = int("AvgTimeToFull"), t > 0, t < 65535 { b.minutesToFull = t }
        // The gauge reports -4096 / 0 now and then (seen on this machine); ignore those.
        if let a = a, let v = b.volts, v > 0, a != -4096 { b.watts = -Double(a) * Double(v) / 1e6 }
        return b
    }
}

struct CPUPower {
    var pkgW: Double?
    var psysW: Double?
    var coresW: Double?
    var gfxW: Double?
    var pkgTemp: Int?
    var dgpuLink: Int?          // PCIe Link Status of RP12, nil = not readable
    var dgpuOn: Bool? { dgpuLink.map { $0 & 0x2000 != 0 } }   // Data Link Layer Link Active
}

/// Reads debug.gu605cpu_power and turns the RAPL energy counters into watts between two reads.
final class RAPLReader {
    private var last: [String: UInt64] = [:]
    private var lastNs: UInt64 = 0
    private(set) var available = true

    static func raw() -> String? {
        var len = 0
        guard sysctlbyname("debug.gu605cpu_power", nil, &len, nil, 0) == 0, len > 0 else { return nil }
        var buf = [CChar](repeating: 0, count: len + 1)
        guard sysctlbyname("debug.gu605cpu_power", &buf, &len, nil, 0) == 0 else { return nil }
        return String(cString: buf)
    }

    func sample() -> CPUPower? {
        guard let s = Self.raw() else { available = false; return nil }
        available = true
        var f: [String: String] = [:]
        for part in s.split(whereSeparator: { $0 == " " || $0 == "\n" }) {
            let kv = part.split(separator: "=", maxSplits: 1)
            if kv.count == 2 { f[String(kv[0])] = String(kv[1]) }
        }
        func u(_ k: String) -> UInt64? {
            guard let v = f[k], !v.hasPrefix("-") else { return nil }
            return v.hasPrefix("0x") ? UInt64(v.dropFirst(2), radix: 16) : UInt64(v)
        }
        var p = CPUPower()
        if let t = f["pkgtemp"].flatMap(Int.init), t > 0, t < 125 { p.pkgTemp = t }
        if let l = f["rp12link"].flatMap(Int.init), l >= 0, l != 0xFFFF { p.dgpuLink = l }
        guard let ns = u("ns"), let units = u("units") else { return p }
        let joule = 1.0 / Double(1 << ((units >> 8) & 0x1F))
        let dt = lastNs > 0 && ns > lastNs ? Double(ns - lastNs) / 1e9 : 0
        func w(_ k: String) -> Double? {
            guard let now = u(k) else { last[k] = nil; return nil }
            defer { last[k] = now }
            guard dt > 0.2, let prev = last[k] else { return nil }
            let d = now >= prev ? now - prev : now + (1 << 32) - prev
            return Double(d) * joule / dt
        }
        p.pkgW = w("pkg"); p.psysW = w("psys"); p.coresW = w("pp0"); p.gfxW = w("pp1")
        if let ps = p.psysW, ps < 0.5 { p.psysW = nil }      // PSys not set up by this BIOS -> stays 0
        lastNs = ns
        return p
    }
}

struct ProcLoad: Identifiable {
    let id: Int32
    let name: String
    let percent: Double          // of one CPU
}

/// CPU time per process between two calls (proc_pidinfo works for other users' processes too, like ps).
final class ProcSampler {
    private var last: [Int32: UInt64] = [:]
    private var lastT: UInt64 = 0
    private let tb: Double = {
        var i = mach_timebase_info_data_t(); mach_timebase_info(&i)
        return Double(i.numer) / Double(i.denom)
    }()

    func top(_ n: Int) -> [ProcLoad] {
        let cnt = proc_listallpids(nil, 0)
        guard cnt > 0 else { return [] }
        var pids = [Int32](repeating: 0, count: Int(cnt) + 64)
        let got = proc_listallpids(&pids, Int32(pids.count * MemoryLayout<Int32>.size))
        guard got > 0 else { return [] }
        let now = mach_absolute_time()
        var cur: [Int32: UInt64] = [:]
        var out: [ProcLoad] = []
        let dt = lastT > 0 ? Double(now - lastT) * tb : 0
        for pid in pids.prefix(Int(got)) where pid > 0 {
            var ti = proc_taskinfo()
            let r = proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &ti, Int32(MemoryLayout<proc_taskinfo>.size))
            guard r == Int32(MemoryLayout<proc_taskinfo>.size) else { continue }
            let t = ti.pti_total_user + ti.pti_total_system
            cur[pid] = t
            guard dt > 0, let p = last[pid], t >= p else { continue }
            let pct = Double(t - p) * tb / dt * 100
            if pct >= 1 {
                var nb = [CChar](repeating: 0, count: 256)
                proc_name(pid, &nb, UInt32(nb.count))
                out.append(ProcLoad(id: pid, name: String(cString: nb), percent: pct))
            }
        }
        last = cur; lastT = now
        return Array(out.sorted { $0.percent > $1.percent }.prefix(n))
    }
}
