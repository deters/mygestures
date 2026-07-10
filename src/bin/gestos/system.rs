use std::path::PathBuf;
use std::process::Command;

pub fn get_autostart_file_path() -> Option<std::path::PathBuf> {
    std::env::var("HOME").ok().map(|home| {
        std::path::PathBuf::from(home)
            .join(".config")
            .join("autostart")
            .join("mygestures.desktop")
    })
}

pub fn get_overlay_autostart_file_path() -> Option<std::path::PathBuf> {
    std::env::var("HOME").ok().map(|home| {
        std::path::PathBuf::from(home)
            .join(".config")
            .join("autostart")
            .join("gestos-overlay.desktop")
    })
}

pub fn set_autostart_enabled(enabled: bool) {
    if let Some(path) = get_autostart_file_path() {
        if enabled {
            if let Some(parent) = path.parent() {
                let _ = std::fs::create_dir_all(parent);
            }
            let content = "[Desktop Entry]\n\
                           Type=Application\n\
                           Name=MyGestures Daemon\n\
                           Comment=Gesture recognition daemon\n\
                           Exec=mygestures\n\
                           Icon=mygestures\n\
                           Terminal=false\n\
                           X-GNOME-Autostart-enabled=true\n";
            let _ = std::fs::write(&path, content);
        } else if path.exists() {
            let _ = std::fs::remove_file(&path);
        }
    }
}

pub fn set_overlay_enabled(enabled: bool) {
    if let Some(path) = get_overlay_autostart_file_path() {
        if enabled {
            if let Some(parent) = path.parent() {
                let _ = std::fs::create_dir_all(parent);
            }
            let content = "[Desktop Entry]\n\
                           Type=Application\n\
                           Name=Gestos Overlay\n\
                           Comment=Gesture visualization overlay\n\
                           Exec=gestos --overlay\n\
                           Icon=gestos\n\
                           Terminal=false\n\
                           X-GNOME-Autostart-enabled=true\n";
            let _ = std::fs::write(&path, content);

            // Start the overlay process immediately
            let _ = Command::new("gestos").arg("--overlay").spawn();
        } else {
            if path.exists() {
                let _ = std::fs::remove_file(&path);
            }
            // Kill any running overlay instance
            let _ = Command::new("pkill").args(["-f", "gestos --overlay"]).status();
        }
    }
}

pub fn get_osd_enabled_file_path() -> std::path::PathBuf {
    let base = if let Ok(xdg) = std::env::var("XDG_CONFIG_HOME") {
        std::path::PathBuf::from(xdg)
    } else if let Ok(home) = std::env::var("HOME") {
        std::path::PathBuf::from(home).join(".config")
    } else {
        std::path::PathBuf::from(".")
    };
    base.join("mygestures").join("osd_enabled")
}

pub fn is_osd_enabled() -> bool {
    let args: Vec<String> = std::env::args().collect();
    if args.iter().any(|arg| arg == "--no-osd") {
        return false;
    }
    if args.iter().any(|arg| arg == "--osd") {
        return true;
    }
    let path = get_osd_enabled_file_path();
    if !path.exists() {
        true
    } else {
        let content = std::fs::read_to_string(path).unwrap_or_default();
        content.trim() == "true"
    }
}

pub fn set_osd_enabled(enabled: bool) {
    let path = get_osd_enabled_file_path();
    if let Some(parent) = path.parent() {
        let _ = std::fs::create_dir_all(parent);
    }
    let _ = std::fs::write(&path, if enabled { "true" } else { "false" });
}
