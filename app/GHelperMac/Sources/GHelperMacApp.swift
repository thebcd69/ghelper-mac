// SPDX-License-Identifier: GPL-3.0-only
// GHelperMac — minimal G-Helper-like menu-bar tool for the ASUS ROG Zephyrus G16 GU605MY on macOS.
// Derived from G-Helper by seerge (https://github.com/seerge/g-helper), GPL-3.0.

import SwiftUI

@main
struct GHelperMacApp: App {
    @StateObject private var state = AppState()

    var body: some Scene {
        MenuBarExtra {
            MenuPanel().environmentObject(state)
        } label: {
            Text(state.menuTitle)
        }
        .menuBarExtraStyle(.window)

        Window("GHelperMac Ayarlar", id: "editor") {
            EditorView().environmentObject(state)
        }
        .defaultSize(width: 640, height: 820)
    }
}
