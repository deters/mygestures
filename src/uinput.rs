#[cfg(target_os = "linux")]
use evdev::uinput::VirtualDeviceBuilder;

pub fn name_to_keycode(name: &str) -> Option<u16> {
    let lower = name.to_lowercase();
    match lower.as_str() {
        "a" => Some(30), "b" => Some(48), "c" => Some(46), "d" => Some(32), "e" => Some(18),
        "f" => Some(33), "g" => Some(34), "h" => Some(35), "i" => Some(23), "j" => Some(36),
        "k" => Some(37), "l" => Some(38), "m" => Some(50), "n" => Some(49), "o" => Some(24),
        "p" => Some(25), "q" => Some(16), "r" => Some(19), "s" => Some(31), "t" => Some(20),
        "u" => Some(22), "v" => Some(47), "w" => Some(17), "x" => Some(45), "y" => Some(21),
        "z" => Some(44),
        "0" => Some(11), "1" => Some(2), "2" => Some(3), "3" => Some(4), "4" => Some(5),
        "5" => Some(6), "6" => Some(7), "7" => Some(8), "8" => Some(9), "9" => Some(10),
        "return" | "enter" => Some(28),
        "escape" | "esc" => Some(1),
        "backspace" => Some(14),
        "tab" => Some(15),
        "space" => Some(57),
        "delete" => Some(111),
        "home" => Some(102),
        "end" => Some(107),
        "left" => Some(105),
        "up" => Some(103),
        "right" => Some(106),
        "down" => Some(108),
        "page_up" => Some(104),
        "page_down" => Some(109),
        "control_l" | "ctrl" => Some(29),
        "control_r" => Some(97),
        "shift_l" | "shift" => Some(42),
        "shift_r" => Some(54),
        "alt_l" | "alt" => Some(56),
        "alt_r" => Some(100),
        "super_l" | "super" | "win" => Some(125),
        "super_r" => Some(126),
        "f1" => Some(59), "f2" => Some(60), "f3" => Some(61), "f4" => Some(62),
        "f5" => Some(63), "f6" => Some(64), "f7" => Some(65), "f8" => Some(66),
        "f9" => Some(67), "f10" => Some(68), "f11" => Some(87), "f12" => Some(88),
        "mute" | "audiomute" | "xf86audiomute" => Some(113),
        "volumedown" | "audiolowervolume" | "xf86audiolowervolume" => Some(114),
        "volumeup" | "audioraisevolume" | "xf86audioraisevolume" => Some(115),
        "playpause" | "audioplaypause" | "xf86audioplay" => Some(164),
        "play" | "audioplay" => Some(207),
        "pause" | "audiopause" => Some(119),
        "stop" | "audiostop" | "xf86audiostop" => Some(166),
        "next" | "audionext" | "xf86audionext" => Some(163),
        "prev" | "audioprev" | "xf86audioprev" => Some(165),
        "www" | "xf86www" => Some(150),
        "explorer" | "xf86explorer" | "homepage" => Some(172),
        "mail" | "xf86mail" => Some(155),
        "search" | "xf86search" => Some(217),
        "calc" | "xf86calculator" => Some(140),
        "print" | "printscreen" => Some(99),
        _ => None,
    }
}

#[cfg(target_os = "linux")]
pub struct UinputDevice {
    device: evdev::uinput::VirtualDevice,
}

#[cfg(target_os = "linux")]
impl UinputDevice {
    pub fn init_from_device(source_dev: &evdev::Device) -> Result<Self, std::io::Error> {
        let mut keys = evdev::AttributeSet::<evdev::Key>::new();
        // Enable standard keyboard keys so virtual device can type shortcuts
        for code in 1..255 {
            if code >= 0x110 && code <= 0x11f {
                continue; // Skip mouse button range to preserve source bindings
            }
            keys.insert(evdev::Key(code));
        }

        let mut builder = VirtualDeviceBuilder::new()?
            .name(source_dev.name().unwrap_or("Virtual MyGestures Forwarder"));

        // Add keyboard keys
        builder = builder.with_keys(&keys)?;

        // Copy source device mouse buttons
        if let Some(src_keys) = source_dev.supported_keys() {
            let mut mouse_keys = evdev::AttributeSet::<evdev::Key>::new();
            for k in src_keys.iter() {
                if k.0 >= 0x110 && k.0 <= 0x11f {
                    mouse_keys.insert(k);
                }
            }
            builder = builder.with_keys(&mouse_keys)?;
        }

        // Copy relative/absolute axes
        if let Some(src_rel) = source_dev.supported_relative_axes() {
            for code in src_rel.iter() {
                builder = builder.with_relative_axis(code)?;
            }
        }
        if let Some(src_abs) = source_dev.supported_absolute_axes() {
            for code in src_abs.iter() {
                if let Some(abs_info) = source_dev.get_abs_info(code) {
                    builder = builder.with_absolute_axis(code, &abs_info)?;
                }
            }
        }

        let device = builder.build()?;
        Ok(UinputDevice { device })
    }

    pub fn click(&mut self, button: i32) {
        let ev_button = match button {
            1 => evdev::Key::BTN_LEFT,
            2 => evdev::Key::BTN_MIDDLE,
            3 => evdev::Key::BTN_RIGHT,
            8 => evdev::Key::BTN_SIDE,
            9 => evdev::Key::BTN_EXTRA,
            other => evdev::Key(other as u16),
        };

        let press = evdev::InputEvent::new(evdev::EventType::KEY, ev_button.0, 1);
        let syn = evdev::InputEvent::new(evdev::EventType::SYNCHRONIZATION, 0, 0);
        let release = evdev::InputEvent::new(evdev::EventType::KEY, ev_button.0, 0);

        let _ = self.device.emit(&[press, syn]);
        std::thread::sleep(std::time::Duration::from_millis(50));
        let _ = self.device.emit(&[release, syn]);
    }

    pub fn keypress_string(&mut self, keys: &str) {
        let mut codes = Vec::new();
        for token in keys.split(|c| c == '+' || c == ' ') {
            if token.is_empty() {
                continue;
            }
            if let Some(code) = name_to_keycode(token) {
                codes.push(code);
            }
        }

        // Press all keys in sequence
        for &code in &codes {
            let press = evdev::InputEvent::new(evdev::EventType::KEY, code, 1);
            let syn = evdev::InputEvent::new(evdev::EventType::SYNCHRONIZATION, 0, 0);
            let _ = self.device.emit(&[press, syn]);
            std::thread::sleep(std::time::Duration::from_millis(10));
        }

        std::thread::sleep(std::time::Duration::from_millis(50));

        // Release in reverse order
        for &code in codes.iter().rev() {
            let release = evdev::InputEvent::new(evdev::EventType::KEY, code, 0);
            let syn = evdev::InputEvent::new(evdev::EventType::SYNCHRONIZATION, 0, 0);
            let _ = self.device.emit(&[release, syn]);
            std::thread::sleep(std::time::Duration::from_millis(10));
        }
    }

    pub fn forward_event(&mut self, ev_type: u16, ev_code: u16, ev_value: i32) {
        let ev = evdev::InputEvent::new(evdev::EventType(ev_type), ev_code, ev_value);
        let _ = self.device.emit(&[ev]);
    }
}

#[cfg(not(target_os = "linux"))]
pub struct UinputDevice;

#[cfg(not(target_os = "linux"))]
impl UinputDevice {
    pub fn click(&mut self, button: i32) {
        log::info!("uinput click stub: {}", button);
    }
    pub fn keypress_string(&mut self, keys: &str) {
        log::info!("uinput keypress string stub: {}", keys);
    }
    pub fn forward_event(&mut self, _ev_type: u16, _ev_code: u16, _ev_value: i32) {}
}
