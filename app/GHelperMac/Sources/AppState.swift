// SPDX-License-Identifier: GPL-3.0-only
// Polling, apply/reapply logic (modelled on G-Helper ModeControl: mode -> fans -> power).

import AppKit
import Foundation
import IOKit.ps
import ServiceManagement

final class AppState: ObservableObject {
    let client = ATKDClient()

    @Published var connected = false
    @Published var info: ATKDInfo?
    @Published var mode: Mode?
    @Published var cpuRPM: Int?
    @Published var gpuRPM: Int?
    @Published var cpuTemp: Int?
    @Published var pl1Now: Int?
    @Published var pl2Now: Int?
    @Published var fmae: Bool?
    @Published var battery: Int?        // EC RSOC %
    @Published var onAC = true
    @Published var busy = false
    @Published var status = ""
    @Published var log: [String] = []
    @Published var store = Store.load()
    @Published var floors: [Fan: [Int]] = [:]
    @Published var loginItem = false
    @Published var brightness: Float?   // nil = NullMoth published no backlight (nvrmacpibl=1 needed)
    @Published var ddc: DDCMonitor?     // nil = no external monitor answers DDC/CI (slider hidden)
    @Published var ddcValue = 0
    @Published var ddcMax = 100

    private var timer: Timer?
    private var lastApply = Date.distantPast
    private var expectedMode: Mode?
    private var wakeObserver: NSObjectProtocol?
    var brightnessDragging = false
    var ddcDragging = false
    private var ddcPending: Int?          // latest slider value not yet written (written by one worker at a time)
    private var ddcWriting = false
    private var ddcSearching = false
    private var ddcLastLook = Date.distantPast
    private var autoReapplies: [Date] = []
    private var autoReapplyPaused = false

    var range: PowerRange { onAC ? .ac : .dc }
    var writesEnabled: Bool { info?.has(ATKD.flagWritesEnabled) ?? false }
    var chargeSupported: Bool { info?.has(ATKD.flagChargePresent) ?? false }

    var menuTitle: String {
        var s = cpuTemp.map { "\($0)°" } ?? "--°"
        if let m = mode { s += " " + String(m.name.prefix(1)) }
        if let m = mode, store.profile(m).customFans { s += "+" }
        return s
    }

    init() {
        if #available(macOS 13.0, *) { loginItem = SMAppService.mainApp.status == .enabled }
        connect()
        poll()
        timer = Timer.scheduledTimer(withTimeInterval: 2.0, repeats: true) { [weak self] _ in self?.poll() }
        NotificationCenter.default.addObserver(forName: NSApplication.didChangeScreenParametersNotification, object: nil,
                                               queue: .main) { [weak self] _ in
            DispatchQueue.main.asyncAfter(deadline: .now() + 3) { self?.ddcLook(force: true) }
        }
        wakeObserver = NSWorkspace.shared.notificationCenter.addObserver(
            forName: NSWorkspace.didWakeNotification, object: nil, queue: .main) { [weak self] _ in
            self?.note("Uykudan uyandı — 5 sn sonra profil yeniden uygulanacak")
            DispatchQueue.main.asyncAfter(deadline: .now() + 5) {
                self?.reapplyChargeLimit(reason: "uyanma")
                self?.reapplyCurrent(reason: "uyanma")
                self?.ddcLook(force: true)
            }
        }
        // Startup: like G-Helper, set the saved mode and re-apply its profile.
        DispatchQueue.main.asyncAfter(deadline: .now() + 3) { [weak self] in self?.startupApply() }
    }

    // MARK: - logging

    func note(_ s: String) {
        let f = DateFormatter()
        f.dateFormat = "HH:mm:ss"
        log.append("\(f.string(from: Date())) \(s)")
        if log.count > 200 { log.removeFirst(log.count - 200) }
        status = s
        NSLog("GHelperMac: %@", s)
    }

    // MARK: - connection / polling

    func connect() {
        connected = client.open()
        if connected {
            info = client.info()
            if let i = info, i.abiVersion != ATKD.abiVersion {
                note("Kext ABI \(i.abiVersion) ≠ uygulama ABI \(ATKD.abiVersion) — sürümler uyuşmuyor")
            }
            loadFloors()
        }
    }

    private func loadFloors() {
        for fan in Fan.allCases {
            let id = fan == .cpu ? ATKD.devCPUFanFloor : ATKD.devGPUFanFloor
            if let b = client.dstsBuffer(id, arg: 0), b.count >= 8 { floors[fan] = b.prefix(8).map { Int($0) } }
        }
    }

    func poll() {
        onAC = Self.isOnAC()
        if !brightnessDragging { brightness = Brightness.get() }
        ddcLook(force: false)
        if !connected { connect(); if !connected { return } }
        info = client.info()
        if info == nil { client.close(); connected = false; return }

        if let v = client.dsts(ATKD.devCPUFan), v != ATKD.unsupported, v & ATKD.presentBit != 0 {
            let h = Int(v & 0xFFFF); cpuRPM = h <= 120 ? h * 100 : nil
        }
        if let v = client.dsts(ATKD.devGPUFan), v != ATKD.unsupported, v & ATKD.presentBit != 0 {
            let h = Int(v & 0xFFFF); gpuRPM = h <= 120 ? h * 100 : nil
        }
        if let v = client.dsts(ATKD.devCPUTemp), v != ATKD.unsupported, v & ATKD.presentBit != 0 {
            cpuTemp = Int(v & 0xFF)
        }
        // Exact RPM from EC fan period if readable (RPM = 2156250 / FRMS)
        if let f = client.readEC(ATKD.ecFRMS) { cpuRPM = Self.rpm(fromPeriod: f) }
        if let f = client.readEC(ATKD.ecFRS2) { gpuRPM = Self.rpm(fromPeriod: f) }

        pl1Now = client.readEC(ATKD.ecPL01).map { Int($0) }
        pl2Now = client.readEC(ATKD.ecPL02).map { Int($0) }
        fmae = client.readEC(ATKD.ecFMAE).map { $0 & 1 == 1 }
        battery = client.readEC(ATKD.ecRSOC).flatMap { $0 <= 100 ? Int($0) : nil }

        let newMode = client.readEC(ATKD.ecFTBL).flatMap { Mode(rawValue: Int($0)) }
        if let nm = newMode {
            let old = mode
            mode = nm
            captureBiosPL(nm)
            if let o = old, o != nm, expectedMode != nm, !busy {
                note("Mod dışarıdan değişti (\(o.name) → \(nm.name))")
                store.lastMode = nm.rawValue
                store.save()
                reapplyCurrent(reason: "mod değişimi")
            }
        }

        // BIOS dropped the custom curve (sleep, Fn key, EC reset) -> re-apply, at most every 30 s.
        if let m = mode, !busy, store.profile(m).customFans || store.profile(m).customPower,
           fmae == false, Date().timeIntervalSince(lastApply) > 30 {
            autoReapplies = autoReapplies.filter { Date().timeIntervalSince($0) < 300 }
            if autoReapplies.count < 3 {
                autoReapplies.append(Date())
                reapplyCurrent(reason: "BIOS eğriyi sıfırladı")
            } else if !autoReapplyPaused {
                autoReapplyPaused = true
                note("FMAE sürekli 0 okunuyor — otomatik yeniden uygulama 5 dk durduruldu (log'u paylaşın)")
            }
        } else if fmae == true {
            autoReapplyPaused = false
        }
    }

    /// Remember the BIOS PL values of a mode while no custom curve is active.
    private func captureBiosPL(_ m: Mode) {
        guard fmae == false, let a = pl1Now, let b = pl2Now, a > 0, b > 0 else { return }
        var p = store.profile(m)
        if p.biosPL1 != a || p.biosPL2 != b {
            p.biosPL1 = a
            p.biosPL2 = b
            if !p.customPower { p.pl1 = a; p.pl2 = b }
            store.setProfile(m, p)
            store.save()
        }
    }

    /// EC fan period -> RPM (DSTS 0x00110013 uses 0x0020E6DA / FRMS / 100). 0 / 0xFFFF = stopped.
    static func rpm(fromPeriod f: UInt32) -> Int {
        if f == 0 || f >= 0xFFFF { return 0 }
        return Int(2_156_250 / f)
    }

    static func isOnAC() -> Bool {
        guard let blob = IOPSCopyPowerSourcesInfo()?.takeRetainedValue() else { return true }
        guard let type = IOPSGetProvidingPowerSourceType(blob)?.takeUnretainedValue() else { return true }
        return (type as String) == kIOPMACPowerKey
    }

    // MARK: - factory curves

    func factoryCurve(_ fan: Fan, mode: Mode) -> FanCurve {
        let id = fan == .cpu ? ATKD.devCPUFanCurve : ATKD.devGPUFanCurve
        if let b = client.dstsBuffer(id, arg: mode.curveIndex), let c = FanCurve(bytes: b) {
            return c.fixed(biosFloor: floors[fan])
        }
        return FanCurve.fallback.fixed(biosFloor: floors[fan])
    }

    /// Ensure the stored profile has curves initialised from the BIOS factory curve.
    func profileForEditing(_ m: Mode) -> ModeProfile {
        var p = store.profile(m)
        if !p.curvesInitialised && connected {
            p.cpu = factoryCurve(.cpu, mode: m)
            p.gpu = factoryCurve(.gpu, mode: m)
            p.curvesInitialised = true
            store.setProfile(m, p)
            store.save()
        }
        return p
    }

    func updateProfile(_ m: Mode, _ p: ModeProfile) {
        store.setProfile(m, p)
        store.save()
    }

    // MARK: - apply

    private func runSteps(_ steps: [(Double, () -> Bool)], done: @escaping () -> Void) {
        guard let first = steps.first else { done(); return }
        DispatchQueue.main.asyncAfter(deadline: .now() + first.0) { [weak self] in
            guard let self = self else { return }
            if first.1() { self.runSteps(Array(steps.dropFirst()), done: done) } else { done() }
        }
    }

    /// G-Helper order: set mode -> 0.1 s -> fan curves -> 1 s -> PL1 (A3), PL2 (A0).
    func apply(mode m: Mode, setMode: Bool, reason: String) {
        guard connected, !busy else { return }
        guard writesEnabled else { note("Yazma kapalı (kext salt-okunur). Bkz. KURULUM.md"); return }
        busy = true
        lastApply = Date()
        let p = profileForEditing(m)
        var steps: [(Double, () -> Bool)] = []

        if setMode {
            steps.append((0, { [weak self] in
                guard let self = self else { return false }
                self.expectedMode = m
                switch self.client.setMode(m.rawValue) {
                case .success: self.note("Mod: \(m.name)"); return true
                case .failure(let e): self.note("Mod ayarlanamadı: \(e)"); return false
                }
            }))
        }

        let needFans = p.customFans || p.customPower // GU605: PPT needs an active custom curve
        if needFans {
            let cpu = p.customFans ? p.cpu : factoryCurve(.cpu, mode: m)
            let gpu = p.customFans ? p.gpu : factoryCurve(.gpu, mode: m)
            steps.append((setMode ? 0.5 : 0, { [weak self] in
                guard let self = self else { return false }
                for (fan, c) in [(Fan.cpu, cpu), (Fan.gpu, gpu)] {
                    if let err = c.validationError(biosFloor: self.floors[fan]) {
                        self.note("\(fan.name) eğrisi geçersiz: \(err)"); return false
                    }
                    switch self.client.setFanCurve(fan: fan.rawValue, bytes: c.bytes) {
                    case .success: break
                    case .failure(let e): self.note("\(fan.name) eğrisi yazılamadı: \(e)"); return false
                    }
                }
                self.note("Fan eğrileri uygulandı (\(m.name))")
                return true
            }))

            // PL: custom values, or (G-Helper "PowerLimit Fix" for GU605M) re-write BIOS values
            // because a custom curve switches the EC to manual and it then uses PL01/PL02 as-is.
            let r = range
            var pl1 = p.customPower ? p.pl1 : (p.biosPL1 ?? 80)
            var pl2 = p.customPower ? p.pl2 : (p.biosPL2 ?? 80)
            pl1 = min(max(pl1, r.pl1.lowerBound), r.pl1.upperBound)
            pl2 = min(max(pl2, max(pl1, r.pl2.lowerBound)), r.pl2.upperBound)
            let fpl1 = pl1, fpl2 = pl2
            steps.append((1.0, { [weak self] in
                guard let self = self else { return false }
                for (lim, w, name) in [(0, fpl1, "PL1"), (1, fpl2, "PL2")] {
                    switch self.client.setPower(limit: lim, watts: w) {
                    case .success: break
                    case .failure(let e): self.note("\(name) yazılamadı: \(e)"); return false
                    }
                }
                self.note("Güç: PL1 \(fpl1) W, PL2 \(fpl2) W\(p.customPower ? "" : " (BIOS değerleri)")")
                return true
            }))
        }

        runSteps(steps) { [weak self] in
            guard let self = self else { return }
            self.busy = false
            self.lastApply = Date()
            self.store.lastMode = m.rawValue
            self.store.save()
            DispatchQueue.main.asyncAfter(deadline: .now() + 1) { self.expectedMode = nil; self.poll() }
            if !reason.isEmpty { self.note("Uygulandı (\(reason))") }
        }
    }

    func selectMode(_ m: Mode) { apply(mode: m, setMode: true, reason: "") }

    func reapplyCurrent(reason: String) {
        guard let m = mode else { return }
        let p = store.profile(m)
        guard p.customFans || p.customPower else { return }
        apply(mode: m, setMode: false, reason: reason)
    }

    private func startupApply() {
        guard connected else { return }
        reapplyChargeLimit(reason: "başlangıç")
        if let lm = store.lastMode, let m = Mode(rawValue: lm), m != mode {
            apply(mode: m, setMode: true, reason: "başlangıç")
        } else {
            reapplyCurrent(reason: "başlangıç")
        }
    }

    /// "Varsayılana dön": drop custom fans/power for the mode and let the BIOS restore defaults
    /// by re-writing the same performance mode (Linux/G-Helper approach).
    func resetToDefault(_ m: Mode) {
        var p = store.profile(m)
        p.customFans = false
        p.customPower = false
        p.cpu = factoryCurve(.cpu, mode: m)
        p.gpu = factoryCurve(.gpu, mode: m)
        p.curvesInitialised = true
        if let a = p.biosPL1, let b = p.biosPL2 { p.pl1 = a; p.pl2 = b }
        updateProfile(m, p)
        apply(mode: m, setMode: true, reason: "varsayılana dönüldü")
    }

    // MARK: - external monitor brightness (DDC/CI VCP 0x10, DDC.swift)

    /// Find the DDC monitor (every 60 s while none, re-read its value every 30 s), off the main thread.
    func ddcLook(force: Bool) {
        let every: TimeInterval = ddc == nil ? 60 : 30
        guard !ddcSearching, !ddcWriting, !ddcDragging, force || Date().timeIntervalSince(ddcLastLook) >= every else { return }
        ddcSearching = true
        ddcLastLook = Date()
        let known = ddc
        DDC.queue.async { [weak self] in
            var result: (DDCMonitor, current: Int, maximum: Int)?
            if let k = known, !force,
               let v = DDC.withFramebuffer(k.framebufferID, { DDC.get($0, code: DDC.vcpBrightness) }) {
                result = (k, current: v.current, maximum: v.maximum)
            } else {
                result = DDC.discover()
            }
            DispatchQueue.main.async {
                guard let self else { return }
                self.ddcSearching = false
                if let r = result {
                    if self.ddc != r.0 { self.note("DDC/CI: \(r.0.name) bulundu (parlaklık \(r.current)/\(r.maximum))") }
                    self.ddc = r.0
                    self.ddcMax = r.maximum
                    if !self.ddcDragging && self.ddcPending == nil { self.ddcValue = r.current }
                } else if self.ddc != nil {
                    self.note("DDC/CI: monitör artık yanıt vermiyor, kaydırıcı gizlendi")
                    self.ddc = nil
                }
            }
        }
    }

    /// Slider value: written by one worker, the latest value wins, at most one write per ~150 ms.
    func setDDC(_ v: Int) {
        guard let m = ddc else { return }
        let c = min(max(v, 0), ddcMax)
        ddcValue = c
        ddcPending = c
        guard !ddcWriting else { return }
        ddcWriting = true
        func next() {
            guard let value = ddcPending else { ddcWriting = false; return }
            ddcPending = nil
            DDC.queue.asyncAfter(deadline: .now() + 0.1) { [weak self] in
                let ok = DDC.withFramebuffer(m.framebufferID) { DDC.set($0, code: DDC.vcpBrightness, value: value) } ?? false
                DispatchQueue.main.async {
                    if !ok { self?.note("DDC/CI: parlaklık yazılamadı (\(m.name))") }
                    next()
                }
            }
        }
        next()
    }

    // MARK: - battery charge limit (DEVS 0x00120057, like G-Helper / Linux charge_control_end_threshold)

    /// Write and remember a charge limit. 100 = no limit (BIOS default).
    func setChargeLimit(_ percent: Int) {
        let v = min(max(percent, ATKD.chargeMin), ATKD.chargeMax)
        store.chargeLimit = v
        store.save()
        writeChargeLimit(v, reason: "elle")
    }

    /// Stop managing the limit: write 100 % once so the battery charges fully again.
    func clearChargeLimit() {
        store.chargeLimit = nil
        store.save()
        writeChargeLimit(ATKD.chargeMax, reason: "kapatıldı")
    }

    func reapplyChargeLimit(reason: String) {
        guard let v = store.chargeLimit else { return }
        writeChargeLimit(v, reason: reason)
    }

    private func writeChargeLimit(_ v: Int, reason: String) {
        guard connected else { return }
        guard chargeSupported else { note("Şarj limiti: BIOS bu aygıtı bildirmiyor (DSTS 0x00120057)"); return }
        guard writesEnabled else { note("Şarj limiti kaydedildi (%\(v)) ama yazma kapalı (-atkdro)"); return }
        switch client.setChargeLimit(v) {
        case .success: note("Şarj limiti: %\(v) (\(reason))")
        case .failure(let e): note("Şarj limiti yazılamadı: \(e)")
        }
        info = client.info()
    }

    // MARK: - brightness (NullMoth IODisplay parameter, never below Brightness.minimum)

    func setBrightness(_ v: Float) {
        let c = max(v, Brightness.minimum)
        brightness = c
        if !Brightness.set(c) { note("Parlaklık ayarlanamadı (NullMoth arka ışığı yok mu? nvrmacpibl=1)") }
    }

    // MARK: - login item

    func setLoginItem(_ on: Bool) {
        if #available(macOS 13.0, *) {
            do {
                if on { try SMAppService.mainApp.register() } else { try SMAppService.mainApp.unregister() }
            } catch {
                note("Giriş öğesi ayarlanamadı: \(error.localizedDescription)")
            }
            loginItem = SMAppService.mainApp.status == .enabled
        }
    }
}
