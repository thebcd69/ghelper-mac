// SPDX-License-Identifier: GPL-3.0-only
// Profiles, limits and fan-curve rules for the GU605MY (docs/RESEARCH.md §3, §5).

import Foundation

enum Mode: Int, CaseIterable, Codable, Identifiable {
    case balanced = 0, turbo = 1, silent = 2
    var id: Int { rawValue }
    var name: String {
        switch self {
        case .balanced: return "Dengeli"
        case .turbo: return "Turbo"
        case .silent: return "Sessiz"
        }
    }
    /// Factory-curve table index for DSTS 0x00110024/25 (swapped vs policy; RESEARCH §3).
    var curveIndex: UInt32 {
        switch self {
        case .balanced: return 0
        case .turbo: return 2
        case .silent: return 1
        }
    }
}

enum Fan: Int, CaseIterable, Identifiable {
    case cpu = 0, gpu = 1
    var id: Int { rawValue }
    var name: String { self == .cpu ? "CPU fanı" : "GPU fanı" }
}

/// Power-limit ranges, Linux asus-armoury.h entry "GU605M" (matches GU605MY).
struct PowerRange {
    let pl1: ClosedRange<Int>
    let pl2: ClosedRange<Int>
    static let ac = PowerRange(pl1: 28...90, pl2: 28...135)
    static let dc = PowerRange(pl1: 25...35, pl2: 38...53)
}

struct FanCurve: Codable, Equatable {
    var temps: [Int]   // 8 points, degC
    var pcts: [Int]    // 8 points, fan %

    static let points = 8
    static func window(_ i: Int) -> ClosedRange<Int> { (30 + 10 * i)...(40 + 10 * i) }

    /// Used only if the BIOS factory curve cannot be read.
    static let fallback = FanCurve(temps: [35, 45, 55, 65, 75, 85, 95, 105],
                                   pcts: [0, 10, 20, 30, 45, 60, 80, 100])

    init(temps: [Int], pcts: [Int]) {
        self.temps = temps
        self.pcts = pcts
    }

    init?(bytes: [UInt8]) {
        guard bytes.count >= 16, !bytes.prefix(16).allSatisfy({ $0 == 0 }) else { return nil }
        temps = bytes[0..<8].map { Int($0) }
        pcts = bytes[8..<16].map { Int($0) }
    }

    var bytes: [UInt8] { temps.map { UInt8(clamping: $0) } + pcts.map { UInt8(clamping: $0) } }

    static func staticFloor(temp: Int) -> Int {
        if temp >= 90 { return 40 }
        if temp >= 70 { return 20 }
        return 0
    }

    /// Make the curve satisfy every rule the kext enforces (like G-Helper FixFanCurve + floors).
    func fixed(biosFloor: [Int]?) -> FanCurve {
        var t = temps.count == 8 ? temps : FanCurve.fallback.temps
        var p = pcts.count == 8 ? pcts : FanCurve.fallback.pcts
        for i in 0..<8 {
            let w = FanCurve.window(i)
            t[i] = min(max(t[i], w.lowerBound), w.upperBound)
            if i > 0 && t[i] <= t[i - 1] { t[i] = min(t[i - 1] + 1, w.upperBound) }
            p[i] = min(max(p[i], 0), 100)
            p[i] = max(p[i], FanCurve.staticFloor(temp: t[i]))
            if let f = biosFloor, f.count == 8, f.allSatisfy({ $0 <= 100 }) { p[i] = max(p[i], f[i]) }
            if i > 0 { p[i] = max(p[i], p[i - 1]) }
        }
        return FanCurve(temps: t, pcts: p)
    }

    /// Same checks as ASUSATKD::validateCurveLocked. Returns a Turkish error or nil.
    func validationError(biosFloor: [Int]?) -> String? {
        guard temps.count == 8, pcts.count == 8 else { return "8 nokta gerekli" }
        for i in 0..<8 {
            let w = FanCurve.window(i)
            if !w.contains(temps[i]) { return "Nokta \(i + 1): sıcaklık \(w.lowerBound)–\(w.upperBound) °C olmalı" }
            if i > 0 && temps[i] <= temps[i - 1] { return "Nokta \(i + 1): sıcaklık artmalı" }
            if !(0...100).contains(pcts[i]) { return "Nokta \(i + 1): fan %0–100 olmalı" }
            if i > 0 && pcts[i] < pcts[i - 1] { return "Nokta \(i + 1): fan yüzdesi azalamaz" }
            if pcts[i] < FanCurve.staticFloor(temp: temps[i]) {
                return "Nokta \(i + 1): \(temps[i]) °C için en az %\(FanCurve.staticFloor(temp: temps[i]))"
            }
            if let f = biosFloor, f.count == 8, f.allSatisfy({ $0 <= 100 }), pcts[i] < f[i] {
                return "Nokta \(i + 1): BIOS tabanı %\(f[i])"
            }
        }
        return nil
    }
}

struct ModeProfile: Codable, Equatable {
    var customFans = false
    var cpu = FanCurve.fallback
    var gpu = FanCurve.fallback
    var customPower = false
    var pl1 = 45
    var pl2 = 65
    /// PL01/PL02 read from the EC while this mode had no custom curve (BIOS values).
    var biosPL1: Int?
    var biosPL2: Int?
    var curvesInitialised = false

    func curve(_ fan: Fan) -> FanCurve { fan == .cpu ? cpu : gpu }
    mutating func setCurve(_ fan: Fan, _ c: FanCurve) { if fan == .cpu { cpu = c } else { gpu = c } }
}

struct Store: Codable {
    var lastMode: Int?
    /// Battery charge end threshold in % (ATKD.chargeMin...100); nil = not managed by the app.
    var chargeLimit: Int?
    var profiles: [String: ModeProfile] = [:]

    func profile(_ m: Mode) -> ModeProfile { profiles[String(m.rawValue)] ?? ModeProfile() }
    mutating func setProfile(_ m: Mode, _ p: ModeProfile) { profiles[String(m.rawValue)] = p }

    static var url: URL {
        let base = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        return base.appendingPathComponent("GHelperMac", isDirectory: true).appendingPathComponent("profiles.json")
    }

    static func load() -> Store {
        guard let d = try? Data(contentsOf: url), let s = try? JSONDecoder().decode(Store.self, from: d) else { return Store() }
        return s
    }

    func save() {
        let u = Store.url
        try? FileManager.default.createDirectory(at: u.deletingLastPathComponent(), withIntermediateDirectories: true)
        let enc = JSONEncoder()
        enc.outputFormatting = [.prettyPrinted, .sortedKeys]
        if let d = try? enc.encode(self) { try? d.write(to: u, options: .atomic) }
    }
}
