// SPDX-License-Identifier: GPL-3.0-only
// SwiftUI views: menu-bar panel and the editor window (Turkish UI).

import AppKit
import Charts
import SwiftUI

struct MenuPanel: View {
    @EnvironmentObject var state: AppState
    @Environment(\.openWindow) private var openWindow

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("GHelperMac — GU605MY").font(.headline)
            if !state.connected {
                Text("ASUSATKD kext bulunamadı.\nKURULUM.md adımlarını kontrol edin.")
                    .foregroundColor(.red).font(.callout)
            } else {
                Grid(alignment: .leading, horizontalSpacing: 12, verticalSpacing: 3) {
                    GridRow { Text("CPU sıcaklık"); Text(state.cpuTemp.map { "\($0) °C" } ?? "?") }
                    GridRow { Text("CPU fan"); Text(state.cpuRPM.map { "\($0) RPM" } ?? "?") }
                    GridRow { Text("GPU fan"); Text(state.gpuRPM.map { "\($0) RPM" } ?? "?") }
                    GridRow { Text("PL1 / PL2 (EC)"); Text("\(state.pl1Now.map(String.init) ?? "?") / \(state.pl2Now.map(String.init) ?? "?") W") }
                    GridRow { Text("Özel eğri (FMAE)"); Text(state.fmae.map { $0 ? "aktif" : "kapalı" } ?? "?") }
                    GridRow { Text("Güç kaynağı"); Text(state.onAC ? "Adaptör" : "Pil") }
                    GridRow {
                        Text("Pil / şarj limiti")
                        Text("\(state.battery.map { "%\($0)" } ?? "?") / \(state.store.chargeLimit.map { "%\($0)" } ?? "kapalı")")
                    }
                }.font(.callout)

                Picker("Mod", selection: Binding(
                    get: { state.mode ?? .balanced },
                    set: { state.selectMode($0) })) {
                    ForEach(Mode.allCases) { m in Text(m.name).tag(m) }
                }
                .pickerStyle(.segmented)
                .disabled(state.busy || !state.writesEnabled)

                if !state.writesEnabled {
                    Text("Salt-okunur: kext yazmaya izin vermiyor (-atkdro / DSDT uyuşmazlığı).")
                        .font(.caption).foregroundColor(.orange)
                }
            }
            Divider()
            if let b = state.brightness {
                HStack {
                    Image(systemName: "sun.min")
                    Slider(value: Binding(get: { Double(b) }, set: { state.setBrightness(Float($0)) }),
                           in: Double(Brightness.minimum)...1.0,
                           onEditingChanged: { editing in state.brightnessDragging = editing })
                    Image(systemName: "sun.max")
                }
            } else {
                Text("Parlaklık: NullMoth arka ışık denetimi kapalı (boot-arg nvrmacpibl=1, bkz. KURULUM §9)")
                    .font(.caption).foregroundColor(.secondary)
            }
            if let m = state.ddc {
                HStack {
                    Image(systemName: "display")
                    Slider(value: Binding(get: { Double(state.ddcValue) }, set: { state.setDDC(Int($0.rounded())) }),
                           in: 0...Double(max(state.ddcMax, 1)), step: 1,
                           onEditingChanged: { editing in state.ddcDragging = editing })
                    Text("\(state.ddcValue)").font(.caption.monospacedDigit()).frame(width: 30, alignment: .trailing)
                }
                .help("\(m.name): monitörün kendi parlaklığı (DDC/CI, VCP 0x10)")
                Text("\(m.name) — DDC/CI").font(.caption).foregroundColor(.secondary)
            }
            HStack {
                Button("Ayarlar…") {
                    openWindow(id: "editor")
                    NSApp.activate(ignoringOtherApps: true)
                }
                Button("Yeniden uygula") { state.reapplyCurrent(reason: "elle") }
                    .disabled(!state.connected || state.busy)
                Spacer()
                Button("Çıkış") { NSApp.terminate(nil) }
            }
            if !state.status.isEmpty {
                Text(state.status).font(.caption).foregroundColor(.secondary).lineLimit(3)
            }
        }
        .padding(12)
        .frame(width: 320)
    }
}

struct CurveEditor: View {
    let fan: Fan
    @Binding var curve: FanCurve
    let floor: [Int]?

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(fan.name).font(.headline)
            Chart {
                ForEach(0..<FanCurve.points, id: \.self) { i in
                    LineMark(x: .value("°C", curve.temps[i]), y: .value("%", curve.pcts[i]))
                    PointMark(x: .value("°C", curve.temps[i]), y: .value("%", curve.pcts[i]))
                }
                if let f = floor, f.count == 8 {
                    ForEach(0..<8, id: \.self) { i in
                        AreaMark(x: .value("°C", 30 + 10 * i), y: .value("Taban", f[i]))
                            .foregroundStyle(Color.gray.opacity(0.25))
                    }
                }
            }
            .chartXScale(domain: 30...110)
            .chartYScale(domain: 0...100)
            .frame(height: 130)

            ForEach(0..<FanCurve.points, id: \.self) { i in
                HStack {
                    Text("\(i + 1)").frame(width: 14)
                    Stepper(value: tempBinding(i), in: FanCurve.window(i)) {
                        Text("\(curve.temps[i]) °C").monospacedDigit().frame(width: 52, alignment: .trailing)
                    }
                    Slider(value: pctBinding(i), in: 0...100, step: 1)
                    Text("%\(curve.pcts[i])").monospacedDigit().frame(width: 40, alignment: .trailing)
                }
            }
        }
    }

    private func tempBinding(_ i: Int) -> Binding<Int> {
        Binding(get: { curve.temps[i] }, set: { v in
            var c = curve; c.temps[i] = v; curve = c.fixed(biosFloor: floor)
        })
    }

    private func pctBinding(_ i: Int) -> Binding<Double> {
        Binding(get: { Double(curve.pcts[i]) }, set: { v in
            var c = curve
            c.pcts[i] = Int(v.rounded())
            // keep monotonic: lowering a point lowers the earlier ones, raising raises later ones
            for j in 0..<i where c.pcts[j] > c.pcts[i] { c.pcts[j] = c.pcts[i] }
            curve = c.fixed(biosFloor: floor)
        })
    }
}

struct EditorView: View {
    @EnvironmentObject var state: AppState
    @State private var editMode: Mode = .balanced
    @State private var profile = ModeProfile()
    @State private var loaded = false
    @State private var showLog = false
    @State private var chargeDraft = 80

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 14) {
                HStack {
                    Picker("Düzenlenen mod", selection: $editMode) {
                        ForEach(Mode.allCases) { m in Text(m.name).tag(m) }
                    }
                    .pickerStyle(.segmented)
                    .onChange(of: editMode) { m in profile = state.profileForEditing(m) }
                    Text("Etkin: \(state.mode?.name ?? "?")").foregroundColor(.secondary)
                }
                Text("Not: BIOS her mod değişiminde ve uykudan sonra eğrileri sıfırlar; uygulama bunları otomatik yeniden yazar. Eğriler mod başına ayrı kaydedilir.")
                    .font(.caption).foregroundColor(.secondary)

                GroupBox {
                    VStack(alignment: .leading, spacing: 10) {
                        Toggle("Özel fan eğrisi kullan", isOn: $profile.customFans)
                        CurveEditor(fan: .cpu, curve: $profile.cpu, floor: state.floors[.cpu])
                        Divider()
                        CurveEditor(fan: .gpu, curve: $profile.gpu, floor: state.floors[.gpu])
                        Text("Kurallar: nokta i sıcaklığı 30+10i…40+10i °C, artan; fan % azalmaz; ≥70 °C en az %20, ≥90 °C en az %40; BIOS tabanının (gri) altına inmez. Orta fan bu modelde yok.")
                            .font(.caption).foregroundColor(.secondary)
                    }.padding(6)
                } label: { Text("Fan eğrileri").font(.title3) }

                GroupBox {
                    VStack(alignment: .leading, spacing: 8) {
                        Toggle("Özel güç limiti kullan", isOn: $profile.customPower)
                        let r = state.range
                        HStack {
                            Text("PL1 (SPL)").frame(width: 80, alignment: .leading)
                            Slider(value: Binding(get: { Double(profile.pl1) }, set: { v in
                                profile.pl1 = Int(v.rounded())
                                if profile.pl2 < profile.pl1 { profile.pl2 = min(profile.pl1, r.pl2.upperBound) }
                            }), in: Double(r.pl1.lowerBound)...Double(r.pl1.upperBound), step: 1)
                            Text("\(profile.pl1) W").monospacedDigit().frame(width: 50, alignment: .trailing)
                        }
                        HStack {
                            Text("PL2 (SPPT)").frame(width: 80, alignment: .leading)
                            Slider(value: Binding(get: { Double(profile.pl2) }, set: { v in
                                profile.pl2 = max(Int(v.rounded()), profile.pl1)
                            }), in: Double(r.pl2.lowerBound)...Double(r.pl2.upperBound), step: 1)
                            Text("\(profile.pl2) W").monospacedDigit().frame(width: 50, alignment: .trailing)
                        }
                        Text("Aralık (\(state.onAC ? "adaptör" : "pil")): PL1 \(r.pl1.lowerBound)–\(r.pl1.upperBound) W, PL2 \(r.pl2.lowerBound)–\(r.pl2.upperBound) W (Linux asus-armoury GU605M). BIOS değeri: PL1 \(profile.biosPL1.map(String.init) ?? "?") W, PL2 \(profile.biosPL2.map(String.init) ?? "?") W. fPPT bu BIOS'ta ayarlanamıyor.")
                            .font(.caption).foregroundColor(.secondary)
                        Text("Uyarı: Yüksek PL değerleri sıcaklığı ve fan sesini artırır. Bu modelde güç limiti yalnızca özel fan eğrisi etkinken uygulanır (uygulama gerekirse BIOS eğrisini yazar).")
                            .font(.caption).foregroundColor(.orange)
                    }.padding(6)
                } label: { Text("CPU güç limitleri").font(.title3) }

                GroupBox {
                    VStack(alignment: .leading, spacing: 8) {
                        Toggle("Şarj limitini kullan", isOn: Binding(
                            get: { state.store.chargeLimit != nil },
                            set: { on in
                                if on { state.setChargeLimit(chargeDraft) } else { state.clearChargeLimit() }
                            }))
                        .disabled(!state.connected || !state.chargeSupported)
                        HStack {
                            Slider(value: Binding(get: { Double(chargeDraft) }, set: { chargeDraft = Int($0.rounded()) }),
                                   in: Double(ATKD.chargeMin)...Double(ATKD.chargeMax), step: 5)
                            Text("%\(chargeDraft)").monospacedDigit().frame(width: 44, alignment: .trailing)
                            Button("Uygula") { state.setChargeLimit(chargeDraft) }
                                .disabled(!state.connected || !state.chargeSupported)
                        }
                        Text(state.chargeSupported
                             ? "Pil bu yüzdeye kadar şarj olur (Windows/Linux G-Helper ile aynı ASUS ayarı). Açılışta ve uyanınca yeniden yazılır. Kapatınca %100'e döner."
                             : "BIOS şarj limiti aygıtını (0x00120057) bildirmedi — bu makinede kullanılamıyor.")
                            .font(.caption).foregroundColor(.secondary)
                    }.padding(6)
                } label: { Text("Pil şarj limiti").font(.title3) }

                HStack {
                    Button("Kaydet ve uygula") { saveAndApply() }
                        .keyboardShortcut(.defaultAction)
                        .disabled(!state.connected || state.busy)
                    Button("Varsayılana dön") {
                        state.resetToDefault(editMode)
                        profile = state.store.profile(editMode)
                    }
                    .disabled(!state.connected || state.busy)
                    Spacer()
                    Toggle("Girişte başlat", isOn: Binding(get: { state.loginItem }, set: { state.setLoginItem($0) }))
                }
                if let err = profile.cpu.validationError(biosFloor: state.floors[.cpu]) ?? profile.gpu.validationError(biosFloor: state.floors[.gpu]) {
                    Text("Eğri hatası: \(err)").foregroundColor(.red).font(.callout)
                }
                Text(state.status).font(.callout).foregroundColor(.secondary)

                DisclosureGroup("Tanılama / günlük", isExpanded: $showLog) {
                    VStack(alignment: .leading, spacing: 2) {
                        if let i = state.info {
                            Text(String(format: "kext ABI %u, flags 0x%02X, DSDT oemrev 0x%08X, olay %u (son 0x%X), yazma %u, son BIOS 0x%X",
                                        i.abiVersion, i.flags, i.dsdtOemRevision, i.eventCount, i.lastEvent, i.writeCount, i.lastBiosResult))
                                .font(.caption.monospaced())
                            Text("yazma \(i.has(ATKD.flagWritesEnabled) ? "açık" : "kapalı") · DSDT \(i.has(ATKD.flagDSDTMatch) ? "eşleşti" : "eşleşmedi") · EC \(i.has(ATKD.flagECFound) ? "okunuyor" : "yok") · mod-ID \(i.has(ATKD.flagModeIdPresent) ? "var" : "yok") · ERAM \(i.has(ATKD.flagERAMMapped) ? "eşlendi" : "yok") · şarj \(i.has(ATKD.flagChargePresent) ? "var" : "yok")\(i.chargeLimit > 0 ? " (%\(i.chargeLimit))" : "")")
                                .font(.caption)
                        }
                        ForEach(Array(state.log.suffix(60).enumerated()), id: \.offset) { _, line in
                            Text(line).font(.caption.monospaced())
                        }
                    }.frame(maxWidth: .infinity, alignment: .leading)
                }
            }
            .padding(16)
        }
        .frame(minWidth: 600, minHeight: 600)
        .onAppear {
            if !loaded {
                editMode = state.mode ?? .balanced
                profile = state.profileForEditing(editMode)
                chargeDraft = state.store.chargeLimit ?? 80
                loaded = true
            }
        }
    }

    private func saveAndApply() {
        if profile.customFans {
            for fan in Fan.allCases {
                if let e = profile.curve(fan).validationError(biosFloor: state.floors[fan]) {
                    state.note("\(fan.name): \(e)")
                    return
                }
            }
        }
        state.updateProfile(editMode, profile)
        if editMode == state.mode {
            if profile.customFans || profile.customPower {
                state.apply(mode: editMode, setMode: false, reason: "kaydedildi")
            } else {
                state.apply(mode: editMode, setMode: true, reason: "özel ayar kapatıldı, BIOS varsayılanı")
            }
        } else {
            state.note("\(editMode.name) kaydedildi — bu mod seçildiğinde uygulanır")
        }
    }
}
