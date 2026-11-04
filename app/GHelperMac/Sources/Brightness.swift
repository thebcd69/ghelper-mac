// SPDX-License-Identifier: GPL-3.0-only
// Internal OLED brightness through the IODisplay "brightness" parameter that NullMoth's NVRMFB publishes
// (AppleBacklightDisplay) when NVKMS registered a backlight — needs boot-arg nvrmacpibl=1. See RESEARCH.md §9.
// No direct hardware access: if the parameter is absent, brightness is simply unavailable.

import Foundation
import IOKit
import IOKit.graphics

enum Brightness {
    /// Lowest value this app will ever request (NullMoth additionally maps 0 -> hardware 5 %).
    static let minimum: Float = 0.05

    private static func withDisplayService<T>(_ body: (io_service_t) -> T?) -> T? {
        var iter: io_iterator_t = 0
        guard IOServiceGetMatchingServices(kIOMainPortDefault, IOServiceMatching("IODisplayConnect"), &iter) == KERN_SUCCESS else {
            return nil
        }
        defer { IOObjectRelease(iter) }
        var result: T?
        var svc = IOIteratorNext(iter)
        while svc != 0 {
            if result == nil { result = body(svc) }
            IOObjectRelease(svc)
            svc = IOIteratorNext(iter)
        }
        return result
    }

    /// Current brightness 0...1, or nil if no display exposes a brightness parameter.
    static func get() -> Float? {
        withDisplayService { svc -> Float? in
            var v: Float = 0
            let r = IODisplayGetFloatParameter(svc, 0, kIODisplayBrightnessKey as CFString, &v)
            return r == kIOReturnSuccess ? v : nil
        }
    }

    @discardableResult
    static func set(_ value: Float) -> Bool {
        let v = min(max(value, minimum), 1.0)
        return withDisplayService { svc -> Bool? in
            var cur: Float = 0
            guard IODisplayGetFloatParameter(svc, 0, kIODisplayBrightnessKey as CFString, &cur) == kIOReturnSuccess else { return nil }
            return IODisplaySetFloatParameter(svc, 0, kIODisplayBrightnessKey as CFString, v) == kIOReturnSuccess
        } ?? false
    }
}
