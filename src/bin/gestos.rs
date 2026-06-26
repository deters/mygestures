use gtk::prelude::*;
use gtk::{cairo, gdk, gio, glib};
use gtk4 as gtk;
use mygestures::config::{generate_unique_id, ActionType, Configuration, Gesture};
use mygestures::protractor::{match_gesture, Point2D};
use std::cell::RefCell;
use std::process::Command;
use std::rc::Rc;
use futures_util::StreamExt;

#[allow(dead_code)]
#[derive(Debug, Clone)]
struct EditorActionOption {
    category: usize,
    action_type: ActionType,
    name: String,
    tooltip: String,
}

const CATEGORY_NAMES: &[&str] = &[
    "Input Emulation",
    "Window Management",
    "Workspaces & Overview",
    "Media & Audio",
    "System & Settings",
    "Applications",
    "GNOME Actions (Native)",
    "Other/Internal",
    "KDE Actions (Native)",
];

fn action_matches(a: &ActionType, opt: &EditorActionOption) -> bool {
    match (a, &opt.action_type) {
        (ActionType::Gnome(k1), ActionType::Gnome(k2)) => k1 == k2,
        (ActionType::Kde(c1, s1), ActionType::Kde(c2, s2)) => c1 == c2 && s1 == s2,
        (ActionType::Execute(cmd1), ActionType::Execute(cmd2)) => {
            if opt.category == 6 {
                // Custom GNOME shortcut command must match exactly
                cmd1 == cmd2
            } else {
                // Generic execute matches any command
                opt.category == 7
            }
        }
        (ActionType::Keypress(_), ActionType::Keypress(_)) => opt.category == 0,
        (ActionType::Click(_), ActionType::Click(_)) => opt.category == 0,
        (at1, at2) => at1 == at2,
    }
}

fn get_static_action_options() -> Vec<EditorActionOption> {
    vec![
        // Input Emulation (0)
        EditorActionOption {
            category: 0,
            action_type: ActionType::Keypress(String::new()),
            name: "Keypress Shortcut".to_string(),
            tooltip: "Simulate keys like Control_L+Alt_L+t".to_string(),
        },
        // Window Management (1)
        EditorActionOption {
            category: 1,
            action_type: ActionType::Kill,
            name: "Close Window".to_string(),
            tooltip: "Close the active application window".to_string(),
        },
        EditorActionOption {
            category: 1,
            action_type: ActionType::ToggleMaximized,
            name: "Toggle Maximized".to_string(),
            tooltip: "Toggle maximize state of active window".to_string(),
        },
        EditorActionOption {
            category: 1,
            action_type: ActionType::Maximize,
            name: "Maximize Window".to_string(),
            tooltip: "Maximize active window".to_string(),
        },
        EditorActionOption {
            category: 1,
            action_type: ActionType::Restore,
            name: "Restore Window".to_string(),
            tooltip: "Restore window from maximized state".to_string(),
        },
        EditorActionOption {
            category: 1,
            action_type: ActionType::Iconify,
            name: "Minimize Window".to_string(),
            tooltip: "Minimize active window".to_string(),
        },
        EditorActionOption {
            category: 1,
            action_type: ActionType::Raise,
            name: "Raise Window".to_string(),
            tooltip: "Bring window to front".to_string(),
        },
        EditorActionOption {
            category: 1,
            action_type: ActionType::Lower,
            name: "Lower Window".to_string(),
            tooltip: "Send window to back".to_string(),
        },
        EditorActionOption {
            category: 1,
            action_type: ActionType::ToggleFullscreen,
            name: "Toggle Fullscreen".to_string(),
            tooltip: "Toggle fullscreen mode".to_string(),
        },
        EditorActionOption {
            category: 1,
            action_type: ActionType::ShowDesktop,
            name: "Show Desktop".to_string(),
            tooltip: "Minimize all windows or toggle show desktop".to_string(),
        },
        // Workspaces & Overview (2)
        EditorActionOption {
            category: 2,
            action_type: ActionType::WorkspaceLeft,
            name: "Workspace Left".to_string(),
            tooltip: "Switch to workspace on the left".to_string(),
        },
        EditorActionOption {
            category: 2,
            action_type: ActionType::WorkspaceRight,
            name: "Workspace Right".to_string(),
            tooltip: "Switch to workspace on the right".to_string(),
        },
        EditorActionOption {
            category: 2,
            action_type: ActionType::WorkspaceUp,
            name: "Workspace Up".to_string(),
            tooltip: "Workspace Up".to_string(),
        },
        EditorActionOption {
            category: 2,
            action_type: ActionType::WorkspaceDown,
            name: "Workspace Down".to_string(),
            tooltip: "Workspace Down".to_string(),
        },
        EditorActionOption {
            category: 2,
            action_type: ActionType::ShowOverview,
            name: "Show Overview".to_string(),
            tooltip: "Toggle workspace overview".to_string(),
        },
        EditorActionOption {
            category: 2,
            action_type: ActionType::ShowAppGrid,
            name: "Show App Grid".to_string(),
            tooltip: "Toggle applications menu/grid".to_string(),
        },
        // Media & Audio (3)
        EditorActionOption {
            category: 3,
            action_type: ActionType::VolumeUp,
            name: "Volume Up".to_string(),
            tooltip: "Increase audio volume".to_string(),
        },
        EditorActionOption {
            category: 3,
            action_type: ActionType::VolumeDown,
            name: "Volume Down".to_string(),
            tooltip: "Decrease audio volume".to_string(),
        },
        EditorActionOption {
            category: 3,
            action_type: ActionType::VolumeMute,
            name: "Volume Mute".to_string(),
            tooltip: "Mute/unmute audio volume".to_string(),
        },
        EditorActionOption {
            category: 3,
            action_type: ActionType::MediaPlay,
            name: "Play/Pause Media".to_string(),
            tooltip: "Toggle playback of media players".to_string(),
        },
        EditorActionOption {
            category: 3,
            action_type: ActionType::MediaNext,
            name: "Next Track".to_string(),
            tooltip: "Skip to next track".to_string(),
        },
        EditorActionOption {
            category: 3,
            action_type: ActionType::MediaPrev,
            name: "Previous Track".to_string(),
            tooltip: "Skip to previous track".to_string(),
        },
        // System & Settings (4)
        EditorActionOption {
            category: 4,
            action_type: ActionType::LockScreen,
            name: "Lock Screen".to_string(),
            tooltip: "Lock the computer screen".to_string(),
        },
        EditorActionOption {
            category: 4,
            action_type: ActionType::Terminal,
            name: "Open Terminal".to_string(),
            tooltip: "Launch default terminal emulator".to_string(),
        },
        EditorActionOption {
            category: 4,
            action_type: ActionType::ControlCenter,
            name: "Control Center".to_string(),
            tooltip: "Launch system settings panel".to_string(),
        },
        EditorActionOption {
            category: 4,
            action_type: ActionType::Logout,
            name: "Log Out".to_string(),
            tooltip: "Log out of session".to_string(),
        },
        EditorActionOption {
            category: 4,
            action_type: ActionType::Screenshot,
            name: "Take Screenshot".to_string(),
            tooltip: "Take screen capture".to_string(),
        },
        EditorActionOption {
            category: 4,
            action_type: ActionType::ScreenshotWindow,
            name: "Screenshot Window".to_string(),
            tooltip: "Take screenshot of active window".to_string(),
        },
        EditorActionOption {
            category: 4,
            action_type: ActionType::ScreenshotArea,
            name: "Screenshot Area".to_string(),
            tooltip: "Take screenshot of selection area".to_string(),
        },
        // Applications (5)
        EditorActionOption {
            category: 5,
            action_type: ActionType::Www,
            name: "Web Browser".to_string(),
            tooltip: "Launch web browser".to_string(),
        },
        EditorActionOption {
            category: 5,
            action_type: ActionType::Home,
            name: "Home Folder".to_string(),
            tooltip: "Open file manager in home directory".to_string(),
        },
        EditorActionOption {
            category: 5,
            action_type: ActionType::Email,
            name: "Email Client".to_string(),
            tooltip: "Launch email reader".to_string(),
        },
        EditorActionOption {
            category: 5,
            action_type: ActionType::Search,
            name: "System Search".to_string(),
            tooltip: "Open system search tool".to_string(),
        },
        EditorActionOption {
            category: 5,
            action_type: ActionType::Calculator,
            name: "Calculator".to_string(),
            tooltip: "Open calculator application".to_string(),
        },
        // Other/Internal (7)
        EditorActionOption {
            category: 7,
            action_type: ActionType::Execute(String::new()),
            name: "Run Command (Execute)".to_string(),
            tooltip: "Run custom shell command".to_string(),
        },
        EditorActionOption {
            category: 7,
            action_type: ActionType::Abort,
            name: "Abort Gesture".to_string(),
            tooltip: "Ignore gesture matching".to_string(),
        },
    ]
}

fn fetch_kde_action_options() -> Vec<EditorActionOption> {
    let mut options = Vec::new();

    // Check if KDE is running to avoid unnecessary D-Bus calls/timeouts on other DEs
    let is_kde = std::env::var("XDG_CURRENT_DESKTOP")
        .map(|d| d.to_lowercase().contains("kde"))
        .unwrap_or(false);
    if !is_kde {
        return options;
    }

    let conn = match zbus::blocking::Connection::session() {
        Ok(c) => c,
        Err(e) => {
            eprintln!("Warning: Failed to connect to D-Bus session bus: {}", e);
            return options;
        }
    };

    let kglobalaccel_proxy = match zbus::blocking::Proxy::new(
        &conn,
        "org.kde.kglobalaccel",
        "/kglobalaccel",
        "org.kde.KGlobalAccel",
    ) {
        Ok(p) => p,
        Err(_) => return options,
    };

    let component_paths: Vec<zbus::zvariant::OwnedObjectPath> = match kglobalaccel_proxy.call("allComponents", &()) {
        Ok(paths) => paths,
        Err(_) => {
            return options;
        }
    };

    for path in component_paths {
        let path_str = path.as_str();
        let comp_name = match path_str.rsplit('/').next() {
            Some(name) => name.to_string(),
            None => continue,
        };

        let proxy = match zbus::blocking::Proxy::new(
            &conn,
            "org.kde.kglobalaccel",
            path_str,
            "org.kde.kglobalaccel.Component",
        ) {
            Ok(p) => p,
            Err(_) => continue,
        };

        let friendly_name: String = proxy.get_property("friendlyName").unwrap_or_else(|_| comp_name.clone());

        let shortcuts: Vec<String> = match proxy.call("shortcutNames", &()) {
            Ok(s) => s,
            Err(_) => continue,
        };

        for shortcut in shortcuts {
            if shortcut.is_empty() {
                continue;
            }
            options.push(EditorActionOption {
                category: 8,
                action_type: ActionType::Kde(comp_name.clone(), shortcut.clone()),
                name: format!("{} - {}", friendly_name, shortcut),
                tooltip: format!("KDE action: {} of component {}", shortcut, comp_name),
            });
        }
    }

    options
}

fn fetch_gnome_action_options() -> Vec<EditorActionOption> {
    let mut options = Vec::new();

    let schemas = vec![
        "org.gnome.desktop.wm.keybindings",
        "org.gnome.settings-daemon.plugins.media-keys",
        "org.gnome.shell.keybindings",
    ];

    let source = match gio::SettingsSchemaSource::default() {
        Some(s) => s,
        None => {
            eprintln!("Warning: GSettings default schema source not found.");
            return options;
        }
    };

    for schema_id in schemas {
        let schema = match source.lookup(schema_id, true) {
            Some(s) => s,
            None => {
                eprintln!(
                    "Info: GSettings schema '{}' not found, skipping.",
                    schema_id
                );
                continue;
            }
        };

        let settings = gio::Settings::new(schema_id);

        for key in schema.list_keys() {
            let skey = schema.key(&key);
            let mut summary = skey.summary().map(|s| s.to_string()).unwrap_or_default();
            if summary.is_empty() {
                summary = skey
                    .description()
                    .map(|s| s.to_string())
                    .unwrap_or_default();
            }
            if summary.is_empty() {
                summary = key.to_string();
            }

            let val = settings.value(&key);
            let mut accel = String::new();
            if let Some(s) = val.get::<String>() {
                accel = s;
            } else if let Some(arr) = val.get::<Vec<String>>() {
                if !arr.is_empty() {
                    accel = arr[0].clone();
                }
            }

            let tooltip = if !accel.is_empty() && accel != "disabled" {
                format!("Schema key: {} (Shortcut: {})", key, accel)
            } else {
                format!("Schema key: {} (No shortcut configured)", key)
            };

            options.push(EditorActionOption {
                category: 6, // CAT_GNOME
                action_type: ActionType::Gnome(key.to_string()),
                name: summary,
                tooltip,
            });
        }

        // Handle custom keybindings
        if schema_id == "org.gnome.settings-daemon.plugins.media-keys"
            && schema.has_key("custom-keybindings")
        {
            let paths: Vec<String> = settings.get("custom-keybindings");
            for path in paths {
                let custom = gio::Settings::with_path(
                    "org.gnome.settings-daemon.plugins.media-keys.custom-keybinding",
                    &path,
                );
                let c_name: String = custom.get("name");
                let c_cmd: String = custom.get("command");

                if !c_name.is_empty() {
                    options.push(EditorActionOption {
                        category: 6, // CAT_GNOME
                        action_type: ActionType::Execute(c_cmd.clone()),
                        name: c_name,
                        tooltip: format!("Custom GNOME shortcut: {}", c_cmd),
                    });
                }
            }
        }
    }

    println!(
        "Fetched {} GNOME action options from GSettings.",
        options.len()
    );
    options
}

struct AppState {
    config: Configuration,
    main_list: gtk::ListBox,
    search_entry: gtk::SearchEntry,
    daemon_switch: gtk::Switch,
    window: gtk::ApplicationWindow,
    switch_handler_id: Option<glib::SignalHandlerId>,
    newly_added_gestures: Vec<String>,
    dbus_conn: Option<zbus::blocking::Connection>,
    empty_state_box: gtk::Box,
}

fn is_daemon_running(conn: Option<&zbus::blocking::Connection>) -> bool {
    let dbus_name = mygestures::config::get_dbus_name();

    let check_status = |c: &zbus::blocking::Connection| -> Option<bool> {
        let dbus_proxy = zbus::blocking::fdo::DBusProxy::new(c).ok()?;
        let bus_name = zbus::names::BusName::try_from(dbus_name.clone()).ok()?;
        dbus_proxy.name_has_owner(bus_name).ok()
    };

    if let Some(c) = conn {
        if let Some(running) = check_status(c) {
            return running;
        }
    }

    if let Ok(new_conn) = zbus::blocking::Connection::session() {
        if let Some(running) = check_status(&new_conn) {
            return running;
        }
    }
    false
}

fn show_error_dialog<W: IsA<gtk::Window>>(parent: &W, message: &str) {
    let dialog = gtk::Window::new();
    dialog.set_transient_for(Some(parent));
    dialog.set_modal(true);
    dialog.set_title(Some("Daemon Startup Error"));
    dialog.set_default_size(420, -1);

    let vbox = gtk::Box::new(gtk::Orientation::Vertical, 16);
    vbox.add_css_class("dialog-content");
    vbox.set_margin_top(16);
    vbox.set_margin_bottom(16);
    vbox.set_margin_start(16);
    vbox.set_margin_end(16);

    let content_box = gtk::Box::new(gtk::Orientation::Horizontal, 12);

    let error_icon = gtk::Image::from_icon_name("dialog-error-symbolic");
    error_icon.set_pixel_size(32);
    error_icon.set_valign(gtk::Align::Start);
    content_box.append(&error_icon);

    let text_box = gtk::Box::new(gtk::Orientation::Vertical, 6);
    text_box.set_hexpand(true);

    let title_label = gtk::Label::new(None);
    title_label.set_markup("<b>Failed to start MyGestures daemon</b>");
    title_label.set_halign(gtk::Align::Start);
    text_box.append(&title_label);

    let label = gtk::Label::new(Some(message));
    label.set_wrap(true);
    label.set_selectable(true);
    label.set_halign(gtk::Align::Start);
    label.set_hexpand(true);
    text_box.append(&label);

    content_box.append(&text_box);
    vbox.append(&content_box);

    let button_box = gtk::Box::new(gtk::Orientation::Horizontal, 0);
    button_box.set_halign(gtk::Align::End);
    let ok_button = gtk::Button::with_label("OK");
    ok_button.set_width_request(80);

    let dialog_clone = dialog.clone();
    ok_button.connect_clicked(move |_| {
        dialog_clone.destroy();
    });
    button_box.append(&ok_button);
    vbox.append(&button_box);

    dialog.set_child(Some(&vbox));
    dialog.present();
}

fn start_daemon(conn: Option<&zbus::blocking::Connection>) -> Result<(), String> {
    if is_daemon_running(conn) {
        return Ok(());
    }

    // Try local binary first, then path
    let cmd = if std::path::Path::new("./mygestures").exists() {
        "./mygestures"
    } else {
        "mygestures"
    };

    // Spawn the daemon process and pipe stderr
    let mut child = Command::new(cmd)
        .stderr(std::process::Stdio::piped())
        .spawn()
        .map_err(|e| format!("Failed to spawn daemon: {}", e))?;

    // Wait a short duration to see if the process exits immediately
    std::thread::sleep(std::time::Duration::from_millis(300));

    match child.try_wait() {
        Ok(Some(status)) => {
            // Process has exited, read stderr
            let mut stderr_str = String::new();
            if let Some(mut stderr) = child.stderr.take() {
                use std::io::Read;
                let _ = stderr.read_to_string(&mut stderr_str);
            }
            if stderr_str.trim().is_empty() {
                Err(format!("Daemon exited immediately with status {}", status))
            } else {
                Err(format!("Daemon startup error:\n{}", stderr_str.trim()))
            }
        }
        Ok(None) => {
            // Still running, which is good!
            // Spawn a background thread to forward stderr to the console so it doesn't block the child process
            if let Some(mut stderr) = child.stderr.take() {
                std::thread::spawn(move || {
                    let mut writer = std::io::stderr();
                    let _ = std::io::copy(&mut stderr, &mut writer);
                });
            }
            Ok(())
        }
        Err(e) => Err(format!("Failed to query daemon status: {}", e)),
    }
}

fn stop_daemon(conn: Option<&zbus::blocking::Connection>) {
    let dbus_name = mygestures::config::get_dbus_name();
    let stop_via_dbus = || -> zbus::Result<()> {
        let proxy = if let Some(c) = conn {
            zbus::blocking::Proxy::new(
                c,
                dbus_name.clone(),
                "/org/mygestures/Daemon",
                "org.mygestures.Daemon",
            )
        } else {
            let new_conn = zbus::blocking::Connection::session()?;
            zbus::blocking::Proxy::new(
                &new_conn,
                dbus_name.clone(),
                "/org/mygestures/Daemon",
                "org.mygestures.Daemon",
            )
        }?;
        let _: () = proxy.call("Stop", &())?;
        Ok(())
    };

    if stop_via_dbus().is_err() {
        let uid = nix::unistd::Uid::current();
        if let Ok(output) = Command::new("pgrep")
            .arg("-u")
            .arg(uid.to_string())
            .arg("-x")
            .arg("mygestures")
            .output()
        {
            if output.status.success() {
                let stdout = String::from_utf8_lossy(&output.stdout);
                for line in stdout.lines() {
                    if let Ok(pid_val) = line.trim().parse::<i32>() {
                        let _ = nix::sys::signal::kill(
                            nix::unistd::Pid::from_raw(pid_val),
                            nix::sys::signal::Signal::SIGTERM,
                        );
                    }
                }
            }
        }
    }
}

fn reload_daemon<W: IsA<gtk::Window>>(
    conn: Option<&zbus::blocking::Connection>,
    parent: Option<&W>,
) {
    let dbus_name = mygestures::config::get_dbus_name();
    let reload_via_dbus = || -> zbus::Result<()> {
        let proxy = if let Some(c) = conn {
            zbus::blocking::Proxy::new(
                c,
                dbus_name.clone(),
                "/org/mygestures/Daemon",
                "org.mygestures.Daemon",
            )
        } else {
            let new_conn = zbus::blocking::Connection::session()?;
            zbus::blocking::Proxy::new(
                &new_conn,
                dbus_name.clone(),
                "/org/mygestures/Daemon",
                "org.mygestures.Daemon",
            )
        }?;
        let _: () = proxy.call("Reload", &())?;
        Ok(())
    };

    if let Err(zbus::Error::FDO(ref fdo_err)) = reload_via_dbus() {
        if let Some(p) = parent {
            show_error_dialog(p, &fdo_err.to_string());
        } else {
            eprintln!("mygestures reload error: {}", fdo_err);
        }
    }
}

fn get_autostart_file_path() -> Option<std::path::PathBuf> {
    std::env::var("HOME").ok().map(|home| {
        std::path::PathBuf::from(home)
            .join(".config")
            .join("autostart")
            .join("mygestures.desktop")
    })
}

fn get_overlay_autostart_file_path() -> Option<std::path::PathBuf> {
    std::env::var("HOME").ok().map(|home| {
        std::path::PathBuf::from(home)
            .join(".config")
            .join("autostart")
            .join("gestos-overlay.desktop")
    })
}

fn set_autostart_enabled(enabled: bool) {
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

fn set_overlay_enabled(enabled: bool) {
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

fn get_osd_enabled_file_path() -> std::path::PathBuf {
    let base = if let Ok(xdg) = std::env::var("XDG_CONFIG_HOME") {
        std::path::PathBuf::from(xdg)
    } else if let Ok(home) = std::env::var("HOME") {
        std::path::PathBuf::from(home).join(".config")
    } else {
        std::path::PathBuf::from(".")
    };
    base.join("mygestures").join("osd_enabled")
}

fn is_osd_enabled() -> bool {
    let path = get_osd_enabled_file_path();
    if !path.exists() {
        true
    } else {
        let content = std::fs::read_to_string(path).unwrap_or_default();
        content.trim() == "true"
    }
}

fn set_osd_enabled(enabled: bool) {
    let path = get_osd_enabled_file_path();
    if let Some(parent) = path.parent() {
        let _ = std::fs::create_dir_all(parent);
    }
    let _ = std::fs::write(&path, if enabled { "true" } else { "false" });
}

fn get_action_category_icon(action: &ActionType) -> (&'static str, &'static str) {
    match action {
        ActionType::Execute(_) => ("utilities-terminal-symbolic", "icon-bg-purple"),
        ActionType::Keypress(_) => (
            "preferences-desktop-keyboard-shortcuts-symbolic",
            "icon-bg-orange",
        ),
        ActionType::Gnome(_) => ("preferences-system-symbolic", "icon-bg-blue"),
        ActionType::Kde(_, _) => ("preferences-system-symbolic", "icon-bg-blue"),
        ActionType::Iconify => ("window-minimize-symbolic", "icon-bg-blue"),
        ActionType::Kill => ("window-close-symbolic", "icon-bg-blue"),
        ActionType::Maximize | ActionType::ToggleMaximized => ("window-maximize-symbolic", "icon-bg-blue"),
        ActionType::Restore => ("window-restore-symbolic", "icon-bg-blue"),
        ActionType::ToggleFullscreen => ("view-fullscreen-symbolic", "icon-bg-blue"),
        ActionType::Lower | ActionType::Raise => ("window-new-symbolic", "icon-bg-blue"),
        ActionType::WorkspaceLeft
        | ActionType::WorkspaceRight
        | ActionType::WorkspaceUp
        | ActionType::WorkspaceDown => ("go-next-symbolic", "icon-bg-blue"),
        ActionType::VolumeUp | ActionType::VolumeDown | ActionType::VolumeMute => {
            ("audio-volume-high-symbolic", "icon-bg-green")
        }
        ActionType::MediaPlay | ActionType::MediaNext | ActionType::MediaPrev => {
            ("media-playback-start-symbolic", "icon-bg-green")
        }
        _ => ("system-run-symbolic", "icon-bg-blue"),
    }
}

fn draw_gesture_path(
    cr: &cairo::Context,
    points: &[Point2D],
    width: f64,
    height: f64,
    _draw_bg: bool,
    fit_to_canvas: bool,
) {
    // Background is transparent to naturally display the theme-dependent container background (e.g. @view_bg_color)

    if points.len() < 2 {
        return;
    }

    // Determine bounding box
    let mut min_x = points[0].x;
    let mut max_x = points[0].x;
    let mut min_y = points[0].y;
    let mut max_y = points[0].y;

    for p in points {
        if p.x < min_x {
            min_x = p.x;
        }
        if p.x > max_x {
            max_x = p.x;
        }
        if p.y < min_y {
            min_y = p.y;
        }
        if p.y > max_y {
            max_y = p.y;
        }
    }

    let w = max_x - min_x;
    let h = max_y - min_y;
    let max_dim = w.max(h);
    let scale = if fit_to_canvas {
        let size = width.min(height);
        if max_dim > 1.0 {
            (size * 0.62) / max_dim
        } else {
            1.0
        }
    } else {
        1.0
    };

    let cx = min_x + w / 2.0;
    let cy = min_y + h / 2.0;

    cr.save().unwrap();
    if fit_to_canvas {
        cr.translate(width / 2.0, height / 2.0);
        cr.scale(scale, scale);
        cr.translate(-cx, -cy);
    }

    cr.set_line_width(4.0 / scale);
    cr.set_line_cap(cairo::LineCap::Round);
    cr.set_line_join(cairo::LineJoin::Round);

    // Draw stroke gradient (violet to pink)
    let pat = cairo::LinearGradient::new(min_x, min_y, max_x, max_y);
    pat.add_color_stop_rgba(0.0, 0.49, 0.27, 0.90, 0.9);
    pat.add_color_stop_rgba(1.0, 0.98, 0.72, 0.80, 0.9);
    cr.set_source(&pat).unwrap();

    cr.move_to(points[0].x, points[0].y);
    for p in &points[1..] {
        cr.line_to(p.x, p.y);
    }
    cr.stroke().unwrap();

    // Draw end indicator (arrowhead pointing in the direction of the last segment)
    if points.len() >= 2 {
        let end = points.last().unwrap();
        let prev = &points[points.len() - 2];

        let dy = end.y - prev.y;
        let dx = end.x - prev.x;

        let len = (dx * dx + dy * dy).sqrt();
        if len > 0.001 {
            let angle = dy.atan2(dx);

            // Arrowhead size relative to scale
            let arrow_length = 12.0 / scale;

            // Calculate base corners perpendicular to the end of the line
            let half_width = 5.0 / scale;
            let perp_angle = angle + std::f64::consts::FRAC_PI_2;
            let x1 = end.x + half_width * perp_angle.cos();
            let y1 = end.y + half_width * perp_angle.sin();
            let x2 = end.x - half_width * perp_angle.cos();
            let y2 = end.y - half_width * perp_angle.sin();

            // Calculate the tip pointing forward from the end of the line
            let tip_x = end.x + arrow_length * angle.cos();
            let tip_y = end.y + arrow_length * angle.sin();

            cr.set_source_rgba(0.49, 0.27, 0.90, 1.0);
            cr.move_to(tip_x, tip_y);
            cr.line_to(x1, y1);
            cr.line_to(x2, y2);
            cr.close_path();
            cr.fill().unwrap();
        } else {
            // Fallback: draw a simple circle if the last segment has zero length
            cr.set_source_rgba(0.49, 0.27, 0.90, 1.0);
            cr.arc(end.x, end.y, 5.0 / scale, 0.0, 2.0 * std::f64::consts::PI);
            cr.fill().unwrap();
        }
    }

    cr.restore().unwrap();
}

fn get_action_human_readable(action: &ActionType) -> String {
    match action {
        ActionType::Iconify => "Minimize Window".to_string(),
        ActionType::Kill => "Close Window".to_string(),
        ActionType::Lower => "Lower Window".to_string(),
        ActionType::Raise => "Raise Window".to_string(),
        ActionType::Maximize => "Maximize Window".to_string(),
        ActionType::Restore => "Restore Window".to_string(),
        ActionType::ToggleMaximized => "Toggle Maximized".to_string(),
        ActionType::Keypress(keys) => {
            if keys.is_empty() {
                "Keypress Shortcut".to_string()
            } else {
                format!("Keypress Shortcut ({})", keys)
            }
        }
        ActionType::Execute(cmd) => {
            if cmd.is_empty() {
                "Run Command (Execute)".to_string()
            } else {
                format!("Run Command: {}", cmd)
            }
        }
        ActionType::WorkspaceLeft => "Workspace Left".to_string(),
        ActionType::WorkspaceRight => "Workspace Right".to_string(),
        ActionType::WorkspaceUp => "Workspace Up".to_string(),
        ActionType::WorkspaceDown => "Workspace Down".to_string(),
        ActionType::ShowOverview => "Show Overview".to_string(),
        ActionType::ShowAppGrid => "Show App Grid".to_string(),
        ActionType::Click(btn) => {
            if let Some(b) = btn {
                format!("Click Mouse Button {}", b)
            } else {
                "Click Mouse Button 1".to_string()
            }
        }
        ActionType::ToggleFullscreen => "Toggle Fullscreen".to_string(),
        ActionType::ShowDesktop => "Show Desktop".to_string(),
        ActionType::LockScreen => "Lock Screen".to_string(),
        ActionType::Terminal => "Open Terminal".to_string(),
        ActionType::VolumeUp => "Volume Up".to_string(),
        ActionType::VolumeDown => "Volume Down".to_string(),
        ActionType::VolumeMute => "Volume Mute".to_string(),
        ActionType::MediaPlay => "Play/Pause Media".to_string(),
        ActionType::MediaNext => "Next Track".to_string(),
        ActionType::MediaPrev => "Previous Track".to_string(),
        ActionType::Www => "Web Browser".to_string(),
        ActionType::Home => "Home Folder".to_string(),
        ActionType::Email => "Email Client".to_string(),
        ActionType::Search => "System Search".to_string(),
        ActionType::Calculator => "Calculator".to_string(),
        ActionType::ControlCenter => "Control Center".to_string(),
        ActionType::Logout => "Log Out".to_string(),
        ActionType::Screenshot => "Take Screenshot".to_string(),
        ActionType::ScreenshotWindow => "Screenshot Window".to_string(),
        ActionType::ScreenshotArea => "Screenshot Area".to_string(),
        ActionType::Gnome(key) => {
            let last_part = key.split('.').last().unwrap_or(key);
            let cleaned = last_part.replace('-', " ");
            let mut chars = cleaned.chars();
            match chars.next() {
                None => "GNOME Action".to_string(),
                Some(c) => c.to_uppercase().collect::<String>() + chars.as_str()
            }
        }
        ActionType::Kde(comp, short) => {
            format!("{} - {}", comp, short)
        }
        ActionType::Abort => "Abort Gesture".to_string(),
    }
}

fn create_gesture_row(gesture: &Gesture, state_rc: &Rc<RefCell<AppState>>) -> gtk::ListBoxRow {
    let row = gtk::ListBoxRow::new();
    row.add_css_class("gesture-row");

    let main_hbox = gtk::Box::new(gtk::Orientation::Horizontal, 16);
    main_hbox.set_margin_start(16);
    main_hbox.set_margin_end(16);
    main_hbox.set_margin_top(12);
    main_hbox.set_margin_bottom(12);

    let drag_handle = gtk::Image::from_icon_name("drag-handle-symbolic");
    drag_handle.add_css_class("drag-handle");
    drag_handle.set_valign(gtk::Align::Center);
    main_hbox.append(&drag_handle);

    let has_keypress = if !gesture.actions.is_empty() {
        if let ActionType::Keypress(ref keys) = gesture.actions[0] {
            Some(keys)
        } else {
            None
        }
    } else {
        None
    };

    // 1. Icon Category Holder (only if not a keypress action)
    if has_keypress.is_none() {
        let (icon_name, bg_class) = if !gesture.actions.is_empty() {
            get_action_category_icon(&gesture.actions[0])
        } else {
            ("system-run-symbolic", "icon-bg-blue")
        };

        let icon_box = gtk::Box::new(gtk::Orientation::Horizontal, 0);
        icon_box.add_css_class("icon-holder");
        icon_box.add_css_class(bg_class);
        icon_box.set_valign(gtk::Align::Center);
        let icon = gtk::Image::from_icon_name(icon_name);
        icon.set_icon_size(gtk::IconSize::Large);
        icon_box.append(&icon);
        main_hbox.append(&icon_box);
    }

    // 2. Info text label
    let vbox = gtk::Box::new(gtk::Orientation::Vertical, 0);
    vbox.set_hexpand(true);
    vbox.set_valign(gtk::Align::Center);

    if let Some(keys) = has_keypress {
        let row_hbox = gtk::Box::new(gtk::Orientation::Horizontal, 6);
        row_hbox.set_halign(gtk::Align::Start);
        row_hbox.set_valign(gtk::Align::Center);

        if !keys.is_empty() {
            let parts: Vec<&str> = keys.split('+').filter(|s| !s.trim().is_empty()).collect();
            for (i, part) in parts.iter().enumerate() {
                if i > 0 {
                    let plus = gtk::Label::new(Some("+"));
                    plus.set_opacity(0.6);
                    plus.set_valign(gtk::Align::Center);
                    row_hbox.append(&plus);
                }
                let keycap_label = gtk::Label::new(Some(part));
                keycap_label.add_css_class("keycap");
                keycap_label.set_valign(gtk::Align::Center);
                row_hbox.append(&keycap_label);
            }
        } else {
            let none_label = gtk::Label::new(Some("None"));
            none_label.set_opacity(0.5);
            none_label.set_valign(gtk::Align::Center);
            row_hbox.append(&none_label);
        }
        vbox.append(&row_hbox);
    } else {
        let action_desc = if !gesture.actions.is_empty() {
            get_action_human_readable(&gesture.actions[0])
        } else {
            "No Action Configured".to_string()
        };
        let title_label = gtk::Label::new(Some(&action_desc));
        title_label.set_halign(gtk::Align::Start);
        title_label.set_markup(&format!("<b>{}</b>", action_desc));
        vbox.append(&title_label);
    }

    main_hbox.append(&vbox);

    // 3. Mini Cairo preview canvas
    let preview_frame = gtk::Frame::new(None);
    preview_frame.add_css_class("gesture-preview-frame");
    preview_frame.set_size_request(60, 60);
    preview_frame.set_valign(gtk::Align::Center);

    let preview_canvas = gtk::DrawingArea::new();
    let pts_clone = gesture.points.clone();
    preview_canvas.set_draw_func(move |_, cr, width, height| {
        draw_gesture_path(cr, &pts_clone, width as f64, height as f64, true, true);
    });
    preview_frame.set_child(Some(&preview_canvas));
    main_hbox.append(&preview_frame);
    row.set_child(Some(&main_hbox));

    // 4. Drag and Drop Controllers for reordering
    let drag_source = gtk::DragSource::new();
    drag_source.set_actions(gdk::DragAction::MOVE);
    let id_str = gesture.id.clone();
    drag_source.connect_prepare(move |_, _, _| {
        let value = id_str.to_value();
        let provider = gdk::ContentProvider::for_value(&value);
        Some(provider)
    });
    let row_clone = row.clone();
    drag_source.connect_drag_begin(move |source, _| {
        let paintable = gtk::WidgetPaintable::new(Some(&row_clone));
        source.set_icon(Some(&paintable), 0, 0);
    });
    drag_handle.add_controller(drag_source);

    let drop_target = gtk::DropTarget::new(glib::Type::STRING, gdk::DragAction::MOVE);
    let state_clone = Rc::clone(state_rc);
    let target_id = gesture.id.clone();
    drop_target.connect_drop(move |_, value, _, _| {
        if let Ok(dragged_id) = value.get::<String>() {
            if dragged_id != target_id {
                let mut state = state_clone.borrow_mut();
                let source_idx = state.config.gestures.iter().position(|g| g.id == dragged_id);
                let dest_idx = state.config.gestures.iter().position(|g| g.id == target_id);
                if let (Some(s_idx), Some(d_idx)) = (source_idx, dest_idx) {
                    let item = state.config.gestures.remove(s_idx);
                    let name_clone = item.name.clone();
                    state.config.gestures.insert(d_idx, item);
                    if let Err(e) = state.config.save_to_file() {
                        println!("Failed to save config on reorder: {}", e);
                    }
                    let state_clone_inner = Rc::clone(&state_clone);
                    glib::idle_add_local_once(move || {
                        refresh_gesture_list(&state_clone_inner, Some(&name_clone));
                    });
                }
            }
        }
        true
    });
    row.add_controller(drop_target);

    row
}

fn get_visible_gestures(state: &AppState) -> Vec<Gesture> {
    let filter = state.search_entry.text().to_lowercase();

    let gestures: Vec<Gesture> = state
        .config
        .gestures
        .iter()
        .filter(|g| !g.is_deleted)
        .filter(|g| {
            if filter.is_empty() {
                true
            } else {
                g.name.to_lowercase().contains(&filter) || (
                    !g.actions.is_empty() && get_action_human_readable(&g.actions[0]).to_lowercase().contains(&filter)
                )
            }
        })
        .cloned()
        .collect();

    gestures
}

fn refresh_gesture_list(state_rc: &Rc<RefCell<AppState>>, select_name: Option<&str>) {
    let state = state_rc.borrow();

    // Clear list
    while let Some(child) = state.main_list.first_child() {
        state.main_list.remove(&child);
    }

    let visible = get_visible_gestures(&state);

    if visible.is_empty() {
        state.empty_state_box.set_visible(true);
        state.main_list.set_visible(false);
    } else {
        state.empty_state_box.set_visible(false);
        state.main_list.set_visible(true);

        for gesture in &visible {
            let row = create_gesture_row(gesture, state_rc);
            state.main_list.append(&row);

            if let Some(name) = select_name {
                if gesture.name == name {
                    state.main_list.select_row(Some(&row));
                }
            }
        }
    }
}

fn get_default_gesture_name(opt: &EditorActionOption, detail: &str) -> String {
    match &opt.action_type {
        ActionType::Keypress(_) => {
            if detail.trim().is_empty() {
                opt.name.clone()
            } else {
                format!("{} ({})", opt.name, detail.trim())
            }
        }
        ActionType::Execute(_) if opt.category == 7 => {
            if detail.trim().is_empty() {
                opt.name.clone()
            } else {
                format!("{} ({})", opt.name, detail.trim())
            }
        }
        ActionType::Click(_) => {
            if detail.trim().is_empty() {
                opt.name.clone()
            } else {
                format!("{} ({})", opt.name, detail.trim())
            }
        }
        _ => opt.name.clone(),
    }
}

fn open_shortcut_recorder(
    parent: &gtk::Window,
    gesture_name: &str,
    entry: &gtk::Entry,
    udn: Rc<dyn Fn() + 'static>,
) {
    let dialog = gtk::Window::new();
    dialog.set_transient_for(Some(parent));
    dialog.set_modal(true);
    dialog.set_default_size(440, 300);

    let header = gtk::HeaderBar::new();
    header.set_show_title_buttons(true); // Initially show close button
    let title_label = gtk::Label::new(Some("Set Shortcut"));
    title_label.add_css_class("title");
    header.set_title_widget(Some(&title_label));
    dialog.set_titlebar(Some(&header));

    // Cancel button (initially hidden)
    let cancel_btn = gtk::Button::with_label("Cancel");
    cancel_btn.set_visible(false);
    let dialog_cancel_clone = dialog.clone();
    cancel_btn.connect_clicked(move |_| {
        dialog_cancel_clone.destroy();
    });
    header.pack_start(&cancel_btn);

    // Set button (initially hidden and insensitive)
    let set_btn = gtk::Button::with_label("Set");
    set_btn.add_css_class("suggested-action");
    set_btn.set_visible(false);
    set_btn.set_sensitive(false);
    header.pack_end(&set_btn);

    let wrapper = gtk::Box::new(gtk::Orientation::Vertical, 0);
    wrapper.add_css_class("dialog-content");
    wrapper.set_vexpand(true);
    wrapper.set_hexpand(true);

    let vbox = gtk::Box::new(gtk::Orientation::Vertical, 16);
    vbox.set_margin_start(24);
    vbox.set_margin_end(24);
    vbox.set_margin_top(24);
    vbox.set_margin_bottom(24);
    vbox.set_valign(gtk::Align::Center);
    vbox.set_halign(gtk::Align::Center);
    vbox.set_vexpand(true);
    vbox.set_hexpand(true);

    wrapper.append(&vbox);

    let prompt_label = gtk::Label::new(None);
    let escaped_name = glib::markup_escape_text(gesture_name);
    prompt_label.set_markup(&format!(
        "Enter new shortcut to change <b>{}</b>",
        escaped_name
    ));
    prompt_label.set_wrap(true);
    prompt_label.set_justify(gtk::Justification::Center);
    vbox.append(&prompt_label);

    // Initial keyboard image
    let kb_image = gtk::Image::from_icon_name("input-keyboard-symbolic");
    kb_image.set_pixel_size(96);
    kb_image.set_opacity(0.8);
    kb_image.set_margin_top(8);
    kb_image.set_margin_bottom(8);
    vbox.append(&kb_image);

    // Container for displaying the keycaps combination
    let shortcut_display_box = gtk::Box::new(gtk::Orientation::Horizontal, 8);
    shortcut_display_box.set_halign(gtk::Align::Center);
    shortcut_display_box.set_valign(gtk::Align::Center);
    shortcut_display_box.set_margin_top(24);
    shortcut_display_box.set_margin_bottom(24);
    shortcut_display_box.set_visible(false);
    vbox.append(&shortcut_display_box);

    let hint_label = gtk::Label::new(Some(
        "Press Esc to cancel or Backspace to reset the shortcut",
    ));
    hint_label.add_css_class("action-label");
    hint_label.set_justify(gtk::Justification::Center);
    vbox.append(&hint_label);

    dialog.set_child(Some(&wrapper));

    // Captured key combination state
    let captured_combination = Rc::new(RefCell::new(None::<String>));

    // Set button click callback
    let captured_save = Rc::clone(&captured_combination);
    let entry_save = entry.clone();
    let udn_save = Rc::clone(&udn);
    let dialog_save = dialog.clone();
    set_btn.connect_clicked(move |_| {
        if let Some(ref combo_str) = *captured_save.borrow() {
            entry_save.set_text(combo_str);
            udn_save();
        }
        dialog_save.destroy();
    });

    // Keypress listener
    let key_controller = gtk::EventControllerKey::new();
    let entry_clone = entry.clone();
    let dialog_clone = dialog.clone();
    let udn_clone = Rc::clone(&udn);
    let captured_clone = Rc::clone(&captured_combination);
    let kb_image_clone = kb_image.clone();
    let display_box_clone = shortcut_display_box.clone();
    let set_btn_clone = set_btn.clone();
    let header_clone = header.clone();
    let cancel_btn_clone = cancel_btn.clone();
    let hint_label_clone = hint_label.clone();

    key_controller.connect_key_pressed(move |_, keyval, _keycode, state| {
        // Handle cancel
        if keyval == gdk::Key::Escape {
            dialog_clone.destroy();
            return glib::Propagation::Stop;
        }

        // Handle reset/disable
        if keyval == gdk::Key::BackSpace {
            entry_clone.set_text("");
            udn_clone();
            dialog_clone.destroy();
            return glib::Propagation::Stop;
        }

        // Identify modifiers
        let mut mods = Vec::new();
        let mut display_mods = Vec::new();
        if state.contains(gdk::ModifierType::CONTROL_MASK) {
            mods.push("Control");
            display_mods.push("Ctrl");
        }
        if state.contains(gdk::ModifierType::ALT_MASK) {
            mods.push("Alt");
            display_mods.push("Alt");
        }
        if state.contains(gdk::ModifierType::SHIFT_MASK) {
            mods.push("Shift");
            display_mods.push("Shift");
        }
        if state.contains(gdk::ModifierType::SUPER_MASK) {
            mods.push("Super");
            display_mods.push("Super");
        }

        let is_modifier = matches!(
            keyval,
            gdk::Key::Control_L
                | gdk::Key::Control_R
                | gdk::Key::Alt_L
                | gdk::Key::Alt_R
                | gdk::Key::Shift_L
                | gdk::Key::Shift_R
                | gdk::Key::Super_L
                | gdk::Key::Super_R
                | gdk::Key::Meta_L
                | gdk::Key::Meta_R
        );

        if is_modifier {
            return glib::Propagation::Stop;
        }

        let mut key_name = keyval.name().map(|s| s.to_string()).unwrap_or_default();
        let display_key = if key_name.len() == 1 {
            key_name.to_uppercase()
        } else {
            key_name.clone()
        };

        if key_name.len() == 1 {
            key_name = key_name.to_uppercase();
        }

        let mut combo = mods;
        if !key_name.is_empty() {
            combo.push(&key_name);
        }

        let combo_str = combo.join("+");
        *captured_clone.borrow_mut() = Some(combo_str);

        // Update UI
        header_clone.set_show_title_buttons(false);
        cancel_btn_clone.set_visible(true);
        set_btn_clone.set_visible(true);
        set_btn_clone.set_sensitive(true);
        hint_label_clone.set_visible(false);
        kb_image_clone.set_visible(false);

        // Clear previous visual keycaps
        while let Some(child) = display_box_clone.first_child() {
            display_box_clone.remove(&child);
        }

        // Append keycaps for modifiers
        for m in display_mods {
            let label = gtk::Label::new(Some(m));
            label.add_css_class("keycap");
            display_box_clone.append(&label);

            let plus = gtk::Label::new(Some("+"));
            plus.set_opacity(0.6);
            display_box_clone.append(&plus);
        }

        // Append keycap for the final key
        let label = gtk::Label::new(Some(&display_key));
        label.add_css_class("keycap");
        display_box_clone.append(&label);

        display_box_clone.set_visible(true);

        glib::Propagation::Stop
    });

    dialog.add_controller(key_controller);
    dialog.present();
}

fn get_category_icon(category: usize) -> &'static str {
    match category {
        0 => "preferences-desktop-keyboard-shortcuts-symbolic", // Input Emulation
        1 => "window-new-symbolic",                              // Window Management
        2 => "view-grid-symbolic",                               // Workspaces & Overview
        3 => "audio-volume-high-symbolic",                       // Media & Audio
        4 => "preferences-system-symbolic",                      // System & Settings
        5 => "application-x-executable-symbolic",                // Applications
        6 => "preferences-system-symbolic",                      // GNOME Actions
        8 => "preferences-system-symbolic",                      // KDE Actions
        _ => "system-run-symbolic",                              // Other/Internal
    }
}

fn open_gesture_editor(state_rc: &Rc<RefCell<AppState>>, target_gesture: Option<Gesture>) {
    let state = state_rc.borrow();
    let dialog = gtk::Window::new();
    dialog.set_transient_for(Some(&state.window));
    dialog.set_modal(true);
    dialog.set_title(Some(if target_gesture.is_some() {
        "Edit Gesture"
    } else {
        "Add Gesture"
    }));
    dialog.set_default_size(480, 620);

    let dialog_header = gtk::HeaderBar::new();
    dialog_header.set_show_title_buttons(false);
    let dialog_title = gtk::Label::new(Some(if target_gesture.is_some() {
        "Edit Gesture"
    } else {
        "Add Gesture"
    }));
    dialog_title.add_css_class("title");
    dialog_header.set_title_widget(Some(&dialog_title));
    dialog.set_titlebar(Some(&dialog_header));

    let main_box = gtk::Box::new(gtk::Orientation::Vertical, 12);
    main_box.add_css_class("dialog-content");
    main_box.set_margin_start(20);
    main_box.set_margin_end(20);
    main_box.set_margin_top(16);
    main_box.set_margin_bottom(16);
    dialog.set_child(Some(&main_box));

    // Scrollable area for all inputs
    let scrolled = gtk::ScrolledWindow::new();
    scrolled.set_hscrollbar_policy(gtk::PolicyType::Never);
    scrolled.set_vscrollbar_policy(gtk::PolicyType::Automatic);
    scrolled.set_vexpand(true);
    scrolled.set_hexpand(true);
    main_box.append(&scrolled);

    let scroll_content = gtk::Box::new(gtk::Orientation::Vertical, 12);
    scrolled.set_child(Some(&scroll_content));

    let name_entry = gtk::Entry::new();
    name_entry.set_placeholder_text(Some("e.g. Back Action"));
    if let Some(ref g) = target_gesture {
        name_entry.set_text(&g.name);
    }

    let is_name_customized = Rc::new(RefCell::new(target_gesture.is_some()));
    let is_updating_programmatically = Rc::new(RefCell::new(false));

    let cust_clone = Rc::clone(&is_name_customized);
    let prog_clone = Rc::clone(&is_updating_programmatically);
    name_entry.connect_changed(move |entry| {
        if *prog_clone.borrow() {
            return;
        }
        *cust_clone.borrow_mut() = !entry.text().trim().is_empty();
    });

    // --- GESTURE PATH SECTION ---
    let path_section_label = gtk::Label::new(Some("Gesture Path"));
    path_section_label.set_halign(gtk::Align::Start);
    path_section_label.add_css_class("section-header");
    scroll_content.append(&path_section_label);

    let canvas_frame = gtk::Frame::new(None);
    canvas_frame.add_css_class("gesture-preview-frame");
    canvas_frame.set_size_request(300, 150);
    canvas_frame.set_vexpand(true);

    let canvas = gtk::DrawingArea::new();
    let recorded_points: Rc<RefCell<Vec<Point2D>>> = Rc::new(RefCell::new(
        target_gesture
            .as_ref()
            .map(|g| g.points.clone())
            .unwrap_or_default(),
    ));
    let is_recording = Rc::new(RefCell::new(false));

    let pts_clone = Rc::clone(&recorded_points);
    let rec_clone = Rc::clone(&is_recording);
    canvas.set_draw_func(move |_, cr, width, height| {
        let pts = pts_clone.borrow();
        let active = *rec_clone.borrow();
        draw_gesture_path(cr, &pts, width as f64, height as f64, true, !active);
    });

    // GestureDrag controller for recording drawing
    let drag = gtk::GestureDrag::new();

    let conflict_box = gtk::Box::new(gtk::Orientation::Horizontal, 8);
    conflict_box.add_css_class("warning-box");
    conflict_box.set_visible(false);
    conflict_box.set_margin_top(4);
    conflict_box.set_margin_bottom(4);

    let conflict_icon = gtk::Image::from_icon_name("dialog-warning-symbolic");
    conflict_icon.set_icon_size(gtk::IconSize::Normal);
    conflict_box.append(&conflict_icon);

    let conflict_label = gtk::Label::new(None);
    conflict_label.set_halign(gtk::Align::Start);
    conflict_label.add_css_class("warning-label");
    conflict_label.set_wrap(true);
    conflict_box.append(&conflict_label);

    let canvas_clone = canvas.clone();
    let pts_drag = Rc::clone(&recorded_points);
    let rec_drag = Rc::clone(&is_recording);
    let conflict_box_begin = conflict_box.clone();
    drag.connect_drag_begin(move |_, start_x, start_y| {
        *rec_drag.borrow_mut() = true;
        let mut pts = pts_drag.borrow_mut();
        pts.clear();
        pts.push(Point2D {
            x: start_x,
            y: start_y,
        });
        println!(
            "GUI: Gesture drawing started at ({:.1}, {:.1})",
            start_x, start_y
        );
        canvas_clone.queue_draw();
        conflict_box_begin.set_visible(false);
    });

    let canvas_clone2 = canvas.clone();
    let pts_drag2 = Rc::clone(&recorded_points);
    let rec_drag2 = Rc::clone(&is_recording);
    drag.connect_drag_update(move |gesture, offset_x, offset_y| {
        if !*rec_drag2.borrow() {
            return;
        }
        if let Some((start_x, start_y)) = gesture.start_point() {
            let cx = start_x + offset_x;
            let cy = start_y + offset_y;
            let mut pts = pts_drag2.borrow_mut();

            // Filter jitter
            let add = match pts.last() {
                Some(lp) => {
                    let dx = cx - lp.x;
                    let dy = cy - lp.y;
                    dx * dx + dy * dy >= 9.0
                }
                None => true,
            };
            if add {
                pts.push(Point2D { x: cx, y: cy });
                canvas_clone2.queue_draw();
            }
        }
    });

    let canvas_clone3 = canvas.clone();
    let rec_drag3 = Rc::clone(&is_recording);
    let pts_drag_end = Rc::clone(&recorded_points);
    let state_clone_drag = Rc::clone(state_rc);
    let edited_gesture_name = target_gesture.as_ref().map(|g| g.name.clone());
    let conflict_box_end = conflict_box.clone();
    let conflict_label_end = conflict_label.clone();
    drag.connect_drag_end(move |_, _, _| {
        *rec_drag3.borrow_mut() = false;
        canvas_clone3.queue_draw();
        println!("GUI: Gesture drawing finished.");

        let pts = pts_drag_end.borrow();
        if pts.len() >= 2 {
            let state = state_clone_drag.borrow();
            let templates: Vec<(String, Vec<Point2D>)> = state
                .config
                .gestures
                .iter()
                .filter(|g| !g.is_deleted)
                .filter(|g| {
                    if let Some(ref name) = edited_gesture_name {
                        g.name != *name
                    } else {
                        true
                    }
                })
                .map(|g| (g.name.clone(), g.points.clone()))
                .collect();

            if let Some(matched_name) = match_gesture(&pts[..], &templates) {
                conflict_label_end.set_text(&format!(
                    "Warning: This path is very similar to the existing gesture '{}'.",
                    matched_name
                ));
                conflict_box_end.set_visible(true);
            } else {
                conflict_box_end.set_visible(false);
            }
        } else {
            conflict_box_end.set_visible(false);
        }
    });

    canvas.add_controller(drag);
    canvas_frame.set_child(Some(&canvas));
    scroll_content.append(&canvas_frame);
    scroll_content.append(&conflict_box);

    // --- ACTION SETTINGS SECTION ---
    let settings_section_label = gtk::Label::new(Some("Action Settings"));
    settings_section_label.set_halign(gtk::Align::Start);
    settings_section_label.add_css_class("section-header");
    scroll_content.append(&settings_section_label);

    let settings_list = gtk::ListBox::new();
    settings_list.add_css_class("boxed-list");
    settings_list.set_selection_mode(gtk::SelectionMode::None);
    scroll_content.append(&settings_list);



    // Row 2: Category
    let category_row = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    category_row.add_css_class("settings-row");

    let category_icon = gtk::Image::from_icon_name("open-menu-symbolic");
    category_icon.set_valign(gtk::Align::Center);
    category_row.append(&category_icon);

    let category_label = gtk::Label::new(Some("Category"));
    category_label.set_halign(gtk::Align::Start);
    category_label.add_css_class("status-label");
    category_row.append(&category_label);

    let category_spacer = gtk::Box::new(gtk::Orientation::Horizontal, 0);
    category_spacer.set_hexpand(true);
    category_row.append(&category_spacer);

    let category_dropdown = gtk::DropDown::from_strings(CATEGORY_NAMES);
    category_dropdown.set_halign(gtk::Align::End);
    category_dropdown.set_size_request(220, -1);
    category_row.append(&category_dropdown);

    settings_list.append(&category_row);

    // Row 3: Action
    let action_row = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    action_row.add_css_class("settings-row");

    let action_icon = gtk::Image::from_icon_name("system-run-symbolic");
    action_icon.set_valign(gtk::Align::Center);
    action_row.append(&action_icon);

    let action_label = gtk::Label::new(Some("Action"));
    action_label.set_halign(gtk::Align::Start);
    action_label.add_css_class("status-label");
    action_row.append(&action_label);

    let action_spacer = gtk::Box::new(gtk::Orientation::Horizontal, 0);
    action_spacer.set_hexpand(true);
    action_row.append(&action_spacer);

    let action_dropdown = gtk::DropDown::new(None::<gtk::StringList>, None::<gtk::Expression>);
    action_dropdown.set_halign(gtk::Align::End);
    action_dropdown.set_size_request(220, -1);
    action_row.append(&action_dropdown);

    settings_list.append(&action_row);

    // Row 4: Action Details Parameter
    let action_details_row = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    action_details_row.add_css_class("settings-row");

    let action_details_icon = gtk::Image::from_icon_name("system-run-symbolic");
    action_details_icon.set_valign(gtk::Align::Center);
    action_details_row.append(&action_details_icon);

    let action_details_label = gtk::Label::new(None);
    action_details_label.set_halign(gtk::Align::Start);
    action_details_label.add_css_class("status-label");
    action_details_row.append(&action_details_label);

    let action_details_spacer = gtk::Box::new(gtk::Orientation::Horizontal, 0);
    action_details_spacer.set_hexpand(true);
    action_details_row.append(&action_details_spacer);

    let entry_container = gtk::Box::new(gtk::Orientation::Horizontal, 6);
    entry_container.set_halign(gtk::Align::End);

    let action_details_entry = gtk::Entry::new();
    action_details_entry.set_size_request(160, -1);
    entry_container.append(&action_details_entry);

    // Add shortcut display box for keycaps representation
    let shortcut_display_box = gtk::Box::new(gtk::Orientation::Horizontal, 6);
    shortcut_display_box.set_halign(gtk::Align::End);
    shortcut_display_box.set_valign(gtk::Align::Center);
    shortcut_display_box.set_visible(false);
    entry_container.append(&shortcut_display_box);

    let record_btn = gtk::Button::from_icon_name("media-record-symbolic");
    record_btn.set_tooltip_text(Some("Record Keybinding"));
    entry_container.append(&record_btn);

    action_details_row.append(&entry_container);
    settings_list.append(&action_details_row);

    let current_options: Rc<RefCell<Vec<EditorActionOption>>> = Rc::new(RefCell::new(Vec::new()));

    // Custom List Item Factories for Category Dropdown
    let cat_list_factory = gtk::SignalListItemFactory::new();
    cat_list_factory.connect_setup(|_, list_item| {
        let box_ = gtk::Box::new(gtk::Orientation::Horizontal, 8);
        box_.set_margin_start(4);
        box_.set_margin_end(4);
        box_.set_margin_top(4);
        box_.set_margin_bottom(4);

        let img = gtk::Image::new();
        img.set_icon_size(gtk::IconSize::Normal);
        img.set_valign(gtk::Align::Center);

        let label = gtk::Label::new(None);
        label.set_valign(gtk::Align::Center);

        box_.append(&img);
        box_.append(&label);
        list_item.set_child(Some(&box_));
    });
    cat_list_factory.connect_bind(|_, list_item| {
        let child = list_item.child().unwrap();
        let box_ = child.downcast::<gtk::Box>().unwrap();
        let img = box_.first_child().unwrap().downcast::<gtk::Image>().unwrap();
        let label = img.next_sibling().unwrap().downcast::<gtk::Label>().unwrap();

        let pos = list_item.position() as usize;
        let icon_name = get_category_icon(pos);
        img.set_icon_name(Some(icon_name));
        if pos < CATEGORY_NAMES.len() {
            label.set_text(CATEGORY_NAMES[pos]);
        }
    });

    let cat_button_factory = gtk::SignalListItemFactory::new();
    cat_button_factory.connect_setup(|_, list_item| {
        let box_ = gtk::Box::new(gtk::Orientation::Horizontal, 8);
        let img = gtk::Image::new();
        img.set_icon_size(gtk::IconSize::Normal);
        img.set_valign(gtk::Align::Center);

        let label = gtk::Label::new(None);
        label.set_valign(gtk::Align::Center);

        box_.append(&img);
        box_.append(&label);
        list_item.set_child(Some(&box_));
    });
    cat_button_factory.connect_bind(|_, list_item| {
        let child = list_item.child().unwrap();
        let box_ = child.downcast::<gtk::Box>().unwrap();
        let img = box_.first_child().unwrap().downcast::<gtk::Image>().unwrap();
        let label = img.next_sibling().unwrap().downcast::<gtk::Label>().unwrap();

        let pos = list_item.position() as usize;
        let icon_name = get_category_icon(pos);
        img.set_icon_name(Some(icon_name));
        if pos < CATEGORY_NAMES.len() {
            label.set_text(CATEGORY_NAMES[pos]);
        }
    });

    category_dropdown.set_list_factory(Some(&cat_list_factory));
    category_dropdown.set_factory(Some(&cat_button_factory));

    // Custom List Item Factories for Action Dropdown
    let act_list_factory = gtk::SignalListItemFactory::new();
    act_list_factory.connect_setup(|_, list_item| {
        let box_ = gtk::Box::new(gtk::Orientation::Horizontal, 8);
        box_.set_margin_start(4);
        box_.set_margin_end(4);
        box_.set_margin_top(4);
        box_.set_margin_bottom(4);

        let img = gtk::Image::new();
        img.set_icon_size(gtk::IconSize::Normal);
        img.set_valign(gtk::Align::Center);

        let label = gtk::Label::new(None);
        label.set_valign(gtk::Align::Center);

        box_.append(&img);
        box_.append(&label);
        list_item.set_child(Some(&box_));
    });

    let current_opts_bind = Rc::clone(&current_options);
    act_list_factory.connect_bind(move |_, list_item| {
        let child = list_item.child().unwrap();
        let box_ = child.downcast::<gtk::Box>().unwrap();
        let img = box_.first_child().unwrap().downcast::<gtk::Image>().unwrap();
        let label = img.next_sibling().unwrap().downcast::<gtk::Label>().unwrap();

        let pos = list_item.position() as usize;
        let opts = current_opts_bind.borrow();
        if pos < opts.len() {
            let opt = &opts[pos];
            let (icon_name, _) = get_action_category_icon(&opt.action_type);
            img.set_icon_name(Some(icon_name));
            label.set_text(&opt.name);
        }
    });

    let act_button_factory = gtk::SignalListItemFactory::new();
    act_button_factory.connect_setup(|_, list_item| {
        let box_ = gtk::Box::new(gtk::Orientation::Horizontal, 8);
        let img = gtk::Image::new();
        img.set_icon_size(gtk::IconSize::Normal);
        img.set_valign(gtk::Align::Center);

        let label = gtk::Label::new(None);
        label.set_valign(gtk::Align::Center);

        box_.append(&img);
        box_.append(&label);
        list_item.set_child(Some(&box_));
    });

    let current_opts_btn_bind = Rc::clone(&current_options);
    act_button_factory.connect_bind(move |_, list_item| {
        let child = list_item.child().unwrap();
        let box_ = child.downcast::<gtk::Box>().unwrap();
        let img = box_.first_child().unwrap().downcast::<gtk::Image>().unwrap();
        let label = img.next_sibling().unwrap().downcast::<gtk::Label>().unwrap();

        let pos = list_item.position() as usize;
        let opts = current_opts_btn_bind.borrow();
        if pos < opts.len() {
            let opt = &opts[pos];
            let (icon_name, _) = get_action_category_icon(&opt.action_type);
            img.set_icon_name(Some(icon_name));
            label.set_text(&opt.name);
        }
    });

    action_dropdown.set_list_factory(Some(&act_list_factory));
    action_dropdown.set_factory(Some(&act_button_factory));

    // Helper to dynamically update the gesture name if not customized
    let update_default_name = Rc::new({
        let name_entry = name_entry.clone();
        let action_dropdown = action_dropdown.clone();
        let action_details_entry = action_details_entry.clone();
        let current_options = Rc::clone(&current_options);
        let is_name_customized = Rc::clone(&is_name_customized);
        let is_updating_programmatically = Rc::clone(&is_updating_programmatically);

        move || {
            if *is_name_customized.borrow() {
                return;
            }
            let act_idx = action_dropdown.selected();
            if act_idx == gtk::INVALID_LIST_POSITION {
                return;
            }
            let act_idx = act_idx as usize;
            let opts = current_options.borrow();
            if act_idx < opts.len() {
                let opt = &opts[act_idx];
                let detail = action_details_entry.text().to_string();
                let default_name = get_default_gesture_name(opt, &detail);

                *is_updating_programmatically.borrow_mut() = true;
                name_entry.set_text(&default_name);
                *is_updating_programmatically.borrow_mut() = false;
            }
        }
    });

    // Setup record button to open the "Set Shortcut" modal dialog
    let entry_click = action_details_entry.clone();
    let dialog_parent = dialog.clone();
    let name_entry_click = name_entry.clone();
    let udn_click = Rc::clone(&update_default_name);

    record_btn.connect_clicked(move |_| {
        let gesture_name = name_entry_click.text().to_string();
        open_shortcut_recorder(
            &dialog_parent,
            if gesture_name.trim().is_empty() {
                "Gesture"
            } else {
                &gesture_name
            },
            &entry_click,
            Rc::clone(&udn_click) as Rc<dyn Fn()>,
        );
    });

    // Helper to update shortcut display box with keycaps
    let update_shortcut_display = {
        let entry = action_details_entry.clone();
        let display_box = shortcut_display_box.clone();
        Rc::new(move || {
            // Clear previous children
            while let Some(child) = display_box.first_child() {
                display_box.remove(&child);
            }

            let text = entry.text().to_string();
            if text.trim().is_empty() {
                let label = gtk::Label::new(Some("None"));
                label.set_opacity(0.5);
                display_box.append(&label);
            } else {
                let parts: Vec<&str> = text.split('+').filter(|s| !s.trim().is_empty()).collect();
                for (i, part) in parts.iter().enumerate() {
                    if i > 0 {
                        let plus = gtk::Label::new(Some("+"));
                        plus.set_opacity(0.6);
                        display_box.append(&plus);
                    }
                    let label = gtk::Label::new(Some(part));
                    label.add_css_class("keycap");
                    display_box.append(&label);
                }
            }
        })
    };

    // Build options list
    let mut all_options = get_static_action_options();
    all_options.extend(fetch_gnome_action_options());
    all_options.extend(fetch_kde_action_options());

    // Sort all_options: first by category, then alphabetically by name (case-insensitive) within each category.
    all_options.sort_by(|a, b| {
        match a.category.cmp(&b.category) {
            std::cmp::Ordering::Equal => a.name.to_lowercase().cmp(&b.name.to_lowercase()),
            other => other,
        }
    });


    // Find initial matching option
    let mut selected_cat = 0;
    let mut selected_act = 0;

    if let Some(ref g) = target_gesture {
        if !g.actions.is_empty() {
            let a = &g.actions[0];
            if let Some(found_opt) = all_options.iter().find(|opt| action_matches(a, opt)) {
                selected_cat = found_opt.category;

                // Get the filtered options for this category
                let filtered: Vec<EditorActionOption> = all_options
                    .iter()
                    .filter(|opt| opt.category == selected_cat)
                    .cloned()
                    .collect();

                // Find index of option within the filtered list
                if let Some(act_idx) = filtered.iter().position(|opt| action_matches(a, opt)) {
                    selected_act = act_idx;
                }

                // If the action contains input text details, populate it
                match a {
                    ActionType::Keypress(combo) => {
                        action_details_entry.set_text(combo);
                    }
                    ActionType::Execute(cmd) => {
                        if selected_cat == 7 {
                            action_details_entry.set_text(cmd);
                        }
                    }
                    ActionType::Click(btn) => {
                        let btn_str = btn.map(|b| b.to_string()).unwrap_or_default();
                        action_details_entry.set_text(&btn_str);
                    }
                    _ => {}
                }
            }
        }
    }

    // Filter options for initial category and set model/selections
    let initial_filtered: Vec<EditorActionOption> = all_options
        .iter()
        .filter(|opt| opt.category == selected_cat)
        .cloned()
        .collect();

    *current_options.borrow_mut() = initial_filtered.clone();

    let action_names: Vec<String> = initial_filtered
        .iter()
        .map(|opt| opt.name.clone())
        .collect();
    let action_refs: Vec<&str> = action_names.iter().map(|s| s.as_str()).collect();
    let action_model = gtk::StringList::new(&action_refs);
    action_dropdown.set_model(Some(&action_model));

    category_dropdown.set_selected(selected_cat as u32);
    action_dropdown.set_selected(selected_act as u32);

    // Initialize details entry visibility, label, placeholder, and record button
    if selected_act < initial_filtered.len() {
        let opt = &initial_filtered[selected_act];
        let show_entry = match &opt.action_type {
            ActionType::Keypress(_) => true,
            ActionType::Execute(_) if opt.category == 7 => true,
            ActionType::Click(_) => true,
            _ => false,
        };
        action_details_row.set_visible(show_entry);

        let (act_icon, _) = get_action_category_icon(&opt.action_type);
        action_icon.set_icon_name(Some(act_icon));

        let details_icon_name = match &opt.action_type {
            ActionType::Keypress(_) => "preferences-desktop-keyboard-shortcuts-symbolic",
            ActionType::Execute(_) => "utilities-terminal-symbolic",
            ActionType::Click(_) => "input-mouse-symbolic",
            _ => "system-run-symbolic",
        };
        action_details_icon.set_icon_name(Some(details_icon_name));

        let is_keypress = matches!(&opt.action_type, ActionType::Keypress(_));
        action_details_entry.set_visible(!is_keypress);
        shortcut_display_box.set_visible(is_keypress);
        record_btn.set_visible(is_keypress);

        match &opt.action_type {
            ActionType::Keypress(_) => {
                action_details_label.set_text("Keys to Send");
            }
            ActionType::Execute(_) => {
                action_details_label.set_text("Command to Execute");
                action_details_entry.set_placeholder_text(Some("e.g. firefox"));
            }
            ActionType::Click(_) => {
                action_details_label.set_text("Mouse Button");
                action_details_entry
                    .set_placeholder_text(Some("e.g. 1 (Left), 2 (Middle), 3 (Right)"));
            }
            _ => {}
        }
    }

    let usd_init = Rc::clone(&update_shortcut_display);
    usd_init(); // Set initial keycaps if keys exist

    let udn_clone = Rc::clone(&update_default_name);
    if target_gesture.is_none() {
        udn_clone();
    }

    let udn_clone4 = Rc::clone(&update_default_name);
    let usd_clone_change = Rc::clone(&update_shortcut_display);
    action_details_entry.connect_changed(move |_| {
        udn_clone4();
        usd_clone_change();
    });

    // Connect category changed signal
    let all_options_clone = all_options.clone();
    let current_opts_clone = Rc::clone(&current_options);
    let action_dropdown_clone = action_dropdown.clone();
    let entry_clone = action_details_entry.clone();
    let label_clone = action_details_label.clone();
    let row_clone = action_details_row.clone();
    let record_btn_clone_cat = record_btn.clone();
    let udn_clone3 = Rc::clone(&update_default_name);
    let sdb_clone_cat = shortcut_display_box.clone();
    let action_icon_cat = action_icon.clone();
    let action_details_icon_cat = action_details_icon.clone();

    category_dropdown.connect_selected_notify(move |cat_dd| {
        let cat_idx = cat_dd.selected();
        if cat_idx == gtk::INVALID_LIST_POSITION {
            return;
        }
        let cat_idx = cat_idx as usize;
        let filtered: Vec<EditorActionOption> = all_options_clone
            .iter()
            .filter(|opt| opt.category == cat_idx)
            .cloned()
            .collect();

        *current_opts_clone.borrow_mut() = filtered.clone();

        let action_names: Vec<String> = filtered.iter().map(|opt| opt.name.clone()).collect();
        let action_refs: Vec<&str> = action_names.iter().map(|s| s.as_str()).collect();
        let action_model = gtk::StringList::new(&action_refs);
        action_dropdown_clone.set_model(Some(&action_model));

        action_dropdown_clone.set_selected(0);

        // Manually update details entry visibility/placeholder/label/record_btn for index 0
        if !filtered.is_empty() {
            let opt = &filtered[0];
            let show_entry = match &opt.action_type {
                ActionType::Keypress(_) => true,
                ActionType::Execute(_) if opt.category == 7 => true,
                ActionType::Click(_) => true,
                _ => false,
            };
            row_clone.set_visible(show_entry);

            let (act_icon, _) = get_action_category_icon(&opt.action_type);
            action_icon_cat.set_icon_name(Some(act_icon));

            let details_icon_name = match &opt.action_type {
                ActionType::Keypress(_) => "preferences-desktop-keyboard-shortcuts-symbolic",
                ActionType::Execute(_) => "utilities-terminal-symbolic",
                ActionType::Click(_) => "input-mouse-symbolic",
                _ => "system-run-symbolic",
            };
            action_details_icon_cat.set_icon_name(Some(details_icon_name));

            let is_keypress = matches!(&opt.action_type, ActionType::Keypress(_));
            entry_clone.set_visible(!is_keypress);
            sdb_clone_cat.set_visible(is_keypress);
            record_btn_clone_cat.set_visible(is_keypress);

            match &opt.action_type {
                ActionType::Keypress(_) => {
                    label_clone.set_text("Keys to Send");
                }
                ActionType::Execute(_) => {
                    label_clone.set_text("Command to Execute");
                    entry_clone.set_placeholder_text(Some("e.g. firefox"));
                }
                ActionType::Click(_) => {
                    label_clone.set_text("Mouse Button");
                    entry_clone.set_placeholder_text(Some("e.g. 1 (Left), 2 (Middle), 3 (Right)"));
                }
                _ => {}
            }
        }

        udn_clone3();
    });

    // Connect action changed signal
    let current_opts_clone2 = Rc::clone(&current_options);
    let entry_clone2 = action_details_entry.clone();
    let label_clone2 = action_details_label.clone();
    let row_clone2 = action_details_row.clone();
    let record_btn_clone_act = record_btn.clone();
    let udn_clone2 = Rc::clone(&update_default_name);
    let sdb_clone_act = shortcut_display_box.clone();
    let action_icon_act = action_icon.clone();
    let action_details_icon_act = action_details_icon.clone();

    action_dropdown.connect_selected_notify(move |act_dd| {
        let act_idx = act_dd.selected();
        if act_idx == gtk::INVALID_LIST_POSITION {
            return;
        }
        let act_idx = act_idx as usize;

        let opts = current_opts_clone2.borrow();
        if act_idx < opts.len() {
            let opt = &opts[act_idx];
            let show_entry = match &opt.action_type {
                ActionType::Keypress(_) => true,
                ActionType::Execute(_) if opt.category == 7 => true,
                ActionType::Click(_) => true,
                _ => false,
            };
            row_clone2.set_visible(show_entry);

            let (act_icon, _) = get_action_category_icon(&opt.action_type);
            action_icon_act.set_icon_name(Some(act_icon));

            let details_icon_name = match &opt.action_type {
                ActionType::Keypress(_) => "preferences-desktop-keyboard-shortcuts-symbolic",
                ActionType::Execute(_) => "utilities-terminal-symbolic",
                ActionType::Click(_) => "input-mouse-symbolic",
                _ => "system-run-symbolic",
            };
            action_details_icon_act.set_icon_name(Some(details_icon_name));

            let is_keypress = matches!(&opt.action_type, ActionType::Keypress(_));
            entry_clone2.set_visible(!is_keypress);
            sdb_clone_act.set_visible(is_keypress);
            record_btn_clone_act.set_visible(is_keypress);

            match &opt.action_type {
                ActionType::Keypress(_) => {
                    label_clone2.set_text("Keys to Send");
                }
                ActionType::Execute(_) => {
                    label_clone2.set_text("Command to Execute");
                    entry_clone2.set_placeholder_text(Some("e.g. firefox"));
                }
                ActionType::Click(_) => {
                    label_clone2.set_text("Mouse Button");
                    entry_clone2.set_placeholder_text(Some("e.g. 1 (Left), 2 (Middle), 3 (Right)"));
                }
                _ => {}
            }
        }

        udn_clone2();
    });

    // Save and Cancel buttons
    let cancel_btn = gtk::Button::with_label("Cancel");
    let dialog_clone = dialog.clone();
    cancel_btn.connect_clicked(move |_| {
        dialog_clone.destroy();
    });
    dialog_header.pack_start(&cancel_btn);

    let save_btn = gtk::Button::with_label("Save");
    save_btn.add_css_class("suggested-action");

    let state_clone = Rc::clone(state_rc);
    let is_edit = target_gesture.is_some();
    let target_id = target_gesture.as_ref().map(|g| g.id.clone());
    let dialog_clone2 = dialog.clone();

    let current_opts_save = Rc::clone(&current_options);
    save_btn.connect_clicked(move |_| {
        let name = name_entry.text().to_string();
        if name.trim().is_empty() {
            println!("Gesture save failed: Name is empty.");
            return;
        }

        let pts = recorded_points.borrow();
        if pts.len() < 2 {
            println!("Gesture save failed: Not enough points drawn.");
            return;
        }

        // Convert recorded points back to string coords representation
        let raw_movement = pts
            .iter()
            .map(|p| format!("{},{}", p.x as i32, p.y as i32))
            .collect::<Vec<_>>()
            .join(" ");

        let act_idx = action_dropdown.selected();
        if act_idx == gtk::INVALID_LIST_POSITION {
            println!("Gesture save failed: Invalid action selection.");
            return;
        }
        let act_idx = act_idx as usize;

        let opts = current_opts_save.borrow();
        if act_idx >= opts.len() {
            println!("Gesture save failed: Selected action index out of bounds.");
            return;
        }
        let opt = &opts[act_idx];
        let detail = action_details_entry.text().to_string();

        let action = match &opt.action_type {
            ActionType::Keypress(_) => ActionType::Keypress(detail),
            ActionType::Execute(_) if opt.category == 7 => ActionType::Execute(detail),
            ActionType::Click(_) => {
                let btn = if detail.trim().is_empty() {
                    None
                } else {
                    detail.trim().parse::<i32>().ok()
                };
                ActionType::Click(btn)
            }
            other => other.clone(),
        };

        let mut state = state_clone.borrow_mut();
        if is_edit {
            let lookup_id = target_id.as_ref().unwrap();
            if let Some(pos) = state
                .config
                .gestures
                .iter()
                .position(|g| g.id == *lookup_id)
            {
                let lookup_name = state.config.gestures[pos].name.clone();
                if name != lookup_name {
                    if state.config.gestures.iter().any(|g| g.name == name) {
                        println!(
                            "Gesture save failed: A gesture with the name '{}' already exists.",
                            name
                        );
                        return;
                    }
                    state.config.gestures[pos].name = name.clone();
                    state.config.gestures[pos].raw_movement = raw_movement;
                    state.config.gestures[pos].points = pts.clone();
                    state.config.gestures[pos].actions = vec![action];
                    println!("Gesture renamed successfully: {} -> {}", lookup_name, name);
                } else {
                    state.config.gestures[pos].raw_movement = raw_movement;
                    state.config.gestures[pos].points = pts.clone();
                    state.config.gestures[pos].actions = vec![action];
                    println!("Gesture modified successfully: {}", name);
                }
            } else {
                println!(
                    "Gesture modification failed: Could not find gesture with ID '{}'",
                    lookup_id
                );
            }
        } else {
            // Check if name conflict
            if state.config.gestures.iter().any(|g| g.name == name) {
                println!(
                    "Gesture save failed: A gesture with the name '{}' already exists.",
                    name
                );
                return;
            }
            println!("Gesture added successfully: {}", name);
            let new_id = generate_unique_id();
            state.config.gestures.push(Gesture {
                id: new_id,
                name: name.clone(),
                raw_movement,
                points: pts.clone(),
                actions: vec![action],
                is_custom: true,
                is_modified: false,
                is_deleted: false,
            });
            state.newly_added_gestures.push(name.clone());
        }

        if let Err(e) = state.config.save_to_file() {
            println!("Failed to save configuration to file: {}", e);
        }
        reload_daemon(state.dbus_conn.as_ref(), Some(&dialog_clone2));
        drop(state);

        refresh_gesture_list(&state_clone, Some(&name));
        dialog_clone2.destroy();
    });
    dialog_header.pack_end(&save_btn);

    if let Some(ref gest) = target_gesture {
        let delete_btn = gtk::Button::with_label("Delete Gesture");
        delete_btn.add_css_class("destructive-action");
        delete_btn.set_margin_top(8);

        let state_clone = Rc::clone(state_rc);
        let target_id = gest.id.clone();
        let name_clone = gest.name.clone();
        let dialog_clone3 = dialog.clone();
        delete_btn.connect_clicked(move |_| {
            let mut state = state_clone.borrow_mut();
            if let Some(pos) = state.config.gestures.iter().position(|g| g.id == target_id) {
                state.config.gestures.remove(pos);
                if let Err(e) = state.config.save_to_file() {
                    println!("Failed to save config to file on delete: {}", e);
                }
                println!("Gesture deleted successfully: {}", name_clone);
                reload_daemon(state.dbus_conn.as_ref(), Some(&dialog_clone3));
                drop(state);
                refresh_gesture_list(&state_clone, None);
                dialog_clone3.destroy();
            }
        });
        main_box.append(&delete_btn);
    }

    dialog.present();
}

fn show_confirm_dialog<W: IsA<gtk::Window>, F: FnOnce() + 'static>(
    parent: &W,
    title: &str,
    message: &str,
    confirm_label: &str,
    is_destructive: bool,
    on_confirm: F,
) {
    let dialog = gtk::Window::new();
    dialog.set_transient_for(Some(parent));
    dialog.set_modal(true);
    dialog.set_title(Some(title));
    dialog.set_default_size(320, -1);

    let main_box = gtk::Box::new(gtk::Orientation::Vertical, 16);
    main_box.add_css_class("dialog-content");
    main_box.set_margin_start(24);
    main_box.set_margin_end(24);
    main_box.set_margin_top(24);
    main_box.set_margin_bottom(24);
    dialog.set_child(Some(&main_box));

    let label = gtk::Label::new(Some(message));
    label.set_wrap(true);
    label.set_halign(gtk::Align::Center);
    main_box.append(&label);

    let btn_box = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    btn_box.set_halign(gtk::Align::End);

    let cancel_btn = gtk::Button::with_label("Cancel");
    let dialog_cancel_clone = dialog.clone();
    cancel_btn.connect_clicked(move |_| {
        dialog_cancel_clone.destroy();
    });
    btn_box.append(&cancel_btn);

    let confirm_btn = gtk::Button::with_label(confirm_label);
    if is_destructive {
        confirm_btn.add_css_class("destructive-action");
    } else {
        confirm_btn.add_css_class("suggested-action");
    }
    let dialog_confirm_clone = dialog.clone();
    let on_confirm_cell = Rc::new(RefCell::new(Some(on_confirm)));
    confirm_btn.connect_clicked(move |_| {
        if let Some(on_confirm) = on_confirm_cell.borrow_mut().take() {
            on_confirm();
        }
        dialog_confirm_clone.destroy();
    });
    btn_box.append(&confirm_btn);

    main_box.append(&btn_box);
    dialog.present();
}

fn open_settings_window(state_rc: &Rc<RefCell<AppState>>) {
    let parent = state_rc.borrow().window.clone();
    let dialog = gtk::Window::new();
    dialog.set_transient_for(Some(&parent));
    dialog.set_modal(true);
    dialog.set_title(Some("Settings"));
    dialog.set_default_size(380, -1);

    let header = gtk::HeaderBar::new();
    header.set_show_title_buttons(true);
    let title_label = gtk::Label::new(Some("Settings"));
    title_label.add_css_class("title");
    header.set_title_widget(Some(&title_label));
    dialog.set_titlebar(Some(&header));

    let main_box = gtk::Box::new(gtk::Orientation::Vertical, 12);
    main_box.add_css_class("dialog-content");
    main_box.set_margin_start(24);
    main_box.set_margin_end(24);
    main_box.set_margin_top(24);
    main_box.set_margin_bottom(24);
    dialog.set_child(Some(&main_box));

    // --- General Section ---
    let general_header = gtk::Label::new(Some("General"));
    general_header.add_css_class("section-header");
    general_header.set_halign(gtk::Align::Start);
    general_header.set_margin_bottom(4);
    main_box.append(&general_header);

    let general_list = gtk::ListBox::new();
    general_list.add_css_class("boxed-list");
    general_list.set_selection_mode(gtk::SelectionMode::None);
    main_box.append(&general_list);

    let autostart_row = gtk::ListBoxRow::new();
    let row_box = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    row_box.set_margin_start(16);
    row_box.set_margin_end(16);
    row_box.set_margin_top(12);
    row_box.set_margin_bottom(12);

    let text_vbox = gtk::Box::new(gtk::Orientation::Vertical, 2);
    text_vbox.set_hexpand(true);
    text_vbox.set_halign(gtk::Align::Start);

    let row_title = gtk::Label::new(Some("Start on Boot"));
    row_title.add_css_class("status-label");
    row_title.set_halign(gtk::Align::Start);
    text_vbox.append(&row_title);

    let row_subtitle = gtk::Label::new(Some("Automatically launch daemon at startup"));
    row_subtitle.add_css_class("action-label");
    row_subtitle.set_halign(gtk::Align::Start);
    text_vbox.append(&row_subtitle);

    row_box.append(&text_vbox);

    let autostart_switch = gtk::Switch::new();
    autostart_switch.set_valign(gtk::Align::Center);

    let autostart_file = get_autostart_file_path();
    let is_autostart = autostart_file.map(|p| p.exists()).unwrap_or(false);
    autostart_switch.set_active(is_autostart);

    autostart_switch.connect_state_set(move |_, state| {
        set_autostart_enabled(state);
        glib::Propagation::Proceed
    });

    row_box.append(&autostart_switch);
    autostart_row.set_child(Some(&row_box));
    general_list.append(&autostart_row);

    // Overlay (Gesture Trail) row
    let overlay_row = gtk::ListBoxRow::new();
    let overlay_box = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    overlay_box.set_margin_start(16);
    overlay_box.set_margin_end(16);
    overlay_box.set_margin_top(12);
    overlay_box.set_margin_bottom(12);

    let overlay_text_vbox = gtk::Box::new(gtk::Orientation::Vertical, 2);
    overlay_text_vbox.set_hexpand(true);
    overlay_text_vbox.set_halign(gtk::Align::Start);

    let overlay_title = gtk::Label::new(Some("Show Gesture Trail"));
    overlay_title.add_css_class("status-label");
    overlay_title.set_halign(gtk::Align::Start);
    overlay_text_vbox.append(&overlay_title);

    let overlay_subtitle = gtk::Label::new(Some("Show visual feedback trail while drawing"));
    overlay_subtitle.add_css_class("action-label");
    overlay_subtitle.set_halign(gtk::Align::Start);
    overlay_text_vbox.append(&overlay_subtitle);

    overlay_box.append(&overlay_text_vbox);

    let overlay_switch = gtk::Switch::new();
    overlay_switch.set_valign(gtk::Align::Center);

    let overlay_file = get_overlay_autostart_file_path();
    let is_overlay_enabled = overlay_file.map(|p| p.exists()).unwrap_or(false);
    overlay_switch.set_active(is_overlay_enabled);

    overlay_switch.connect_state_set(move |_, state| {
        set_overlay_enabled(state);
        glib::Propagation::Proceed
    });

    overlay_box.append(&overlay_switch);
    overlay_row.set_child(Some(&overlay_box));
    general_list.append(&overlay_row);

    // Action Notifications (OSD) row
    let osd_row = gtk::ListBoxRow::new();
    let osd_box = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    osd_box.set_margin_start(16);
    osd_box.set_margin_end(16);
    osd_box.set_margin_top(12);
    osd_box.set_margin_bottom(12);

    let osd_text_vbox = gtk::Box::new(gtk::Orientation::Vertical, 2);
    osd_text_vbox.set_hexpand(true);
    osd_text_vbox.set_halign(gtk::Align::Start);

    let osd_title_lbl = gtk::Label::new(Some("Action Notifications"));
    osd_title_lbl.add_css_class("status-label");
    osd_title_lbl.set_halign(gtk::Align::Start);
    osd_text_vbox.append(&osd_title_lbl);

    let osd_subtitle_lbl = gtk::Label::new(Some("Show a brief popup window when an action executes"));
    osd_subtitle_lbl.add_css_class("action-label");
    osd_subtitle_lbl.set_halign(gtk::Align::Start);
    osd_text_vbox.append(&osd_subtitle_lbl);

    osd_box.append(&osd_text_vbox);

    let osd_switch = gtk::Switch::new();
    osd_switch.set_valign(gtk::Align::Center);
    osd_switch.set_active(is_osd_enabled());

    osd_switch.connect_state_set(move |_, state| {
        set_osd_enabled(state);
        glib::Propagation::Proceed
    });

    osd_box.append(&osd_switch);
    osd_row.set_child(Some(&osd_box));
    general_list.append(&osd_row);

    // --- Backup & Restore Section ---
    let maintenance_header = gtk::Label::new(Some("Backup & Restore"));
    maintenance_header.add_css_class("section-header");
    maintenance_header.set_halign(gtk::Align::Start);
    maintenance_header.set_margin_top(12);
    maintenance_header.set_margin_bottom(4);
    main_box.append(&maintenance_header);

    let maintenance_list = gtk::ListBox::new();
    maintenance_list.add_css_class("boxed-list");
    maintenance_list.set_selection_mode(gtk::SelectionMode::None);
    main_box.append(&maintenance_list);

    // Export Row
    let export_row = gtk::ListBoxRow::new();
    let export_box = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    export_box.set_margin_start(16);
    export_box.set_margin_end(16);
    export_box.set_margin_top(12);
    export_box.set_margin_bottom(12);

    let export_text_vbox = gtk::Box::new(gtk::Orientation::Vertical, 2);
    export_text_vbox.set_hexpand(true);
    export_text_vbox.set_halign(gtk::Align::Start);

    let export_title = gtk::Label::new(Some("Export Configuration"));
    export_title.add_css_class("status-label");
    export_title.set_halign(gtk::Align::Start);
    export_text_vbox.append(&export_title);

    let export_subtitle = gtk::Label::new(Some("Save gestures settings to a YAML file"));
    export_subtitle.add_css_class("action-label");
    export_subtitle.set_halign(gtk::Align::Start);
    export_text_vbox.append(&export_subtitle);

    export_box.append(&export_text_vbox);

    let export_btn = gtk::Button::with_label("Export...");
    export_btn.set_valign(gtk::Align::Center);
    
    let parent_clone = parent.clone();
    export_btn.connect_clicked(move |_| {
        let chooser = gtk::FileChooserNative::new(
            Some("Export Configuration"),
            Some(&parent_clone),
            gtk::FileChooserAction::Save,
            Some("_Export"),
            Some("_Cancel"),
        );
        if let Ok(cwd) = std::env::current_dir() {
            let file = gio::File::for_path(cwd);
            let _ = chooser.set_current_folder(Some(&file));
        } else if let Ok(home) = std::env::var("HOME") {
            let file = gio::File::for_path(home);
            let _ = chooser.set_current_folder(Some(&file));
        }
        chooser.set_current_name("mygestures.yaml");
        
        let filter = gtk::FileFilter::new();
        filter.set_name(Some("YAML Files"));
        filter.add_pattern("*.yaml");
        filter.add_pattern("*.yml");
        chooser.add_filter(&filter);

        let parent_err_clone = parent_clone.clone();
        chooser.connect_response(move |c, response| {
            if response == gtk::ResponseType::Accept {
                if let Some(file) = c.file() {
                    if let Some(path) = file.path() {
                        let config_path = mygestures::config::get_default_config_path();
                        if config_path.exists() {
                            if let Err(e) = std::fs::copy(&config_path, &path) {
                                show_error_dialog(&parent_err_clone, &format!("Failed to export config: {}", e));
                            }
                        } else {
                            let default_cfg = Configuration::load_from_defaults();
                            let mut temp_cfg = default_cfg;
                            temp_cfg.user_config_path = path;
                            if let Err(e) = temp_cfg.save_to_file() {
                                show_error_dialog(&parent_err_clone, &format!("Failed to export config: {}", e));
                            }
                        }
                    }
                }
            }
            c.destroy();
        });
        chooser.show();
    });

    export_box.append(&export_btn);
    export_row.set_child(Some(&export_box));
    maintenance_list.append(&export_row);

    // Import Row
    let import_row = gtk::ListBoxRow::new();
    let import_box = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    import_box.set_margin_start(16);
    import_box.set_margin_end(16);
    import_box.set_margin_top(12);
    import_box.set_margin_bottom(12);

    let import_text_vbox = gtk::Box::new(gtk::Orientation::Vertical, 2);
    import_text_vbox.set_hexpand(true);
    import_text_vbox.set_halign(gtk::Align::Start);

    let import_title = gtk::Label::new(Some("Import Configuration"));
    import_title.add_css_class("status-label");
    import_title.set_halign(gtk::Align::Start);
    import_text_vbox.append(&import_title);

    let import_subtitle = gtk::Label::new(Some("Load gestures settings from a YAML file"));
    import_subtitle.add_css_class("action-label");
    import_subtitle.set_halign(gtk::Align::Start);
    import_text_vbox.append(&import_subtitle);

    import_box.append(&import_text_vbox);

    let import_btn = gtk::Button::with_label("Import...");
    import_btn.set_valign(gtk::Align::Center);

    let parent_clone = parent.clone();
    let state_rc_clone = Rc::clone(state_rc);
    import_btn.connect_clicked(move |_| {
        let chooser = gtk::FileChooserNative::new(
            Some("Import Configuration"),
            Some(&parent_clone),
            gtk::FileChooserAction::Open,
            Some("_Import"),
            Some("_Cancel"),
        );
        if let Ok(cwd) = std::env::current_dir() {
            let file = gio::File::for_path(cwd);
            let _ = chooser.set_current_folder(Some(&file));
        } else if let Ok(home) = std::env::var("HOME") {
            let file = gio::File::for_path(home);
            let _ = chooser.set_current_folder(Some(&file));
        }
        
        let filter = gtk::FileFilter::new();
        filter.set_name(Some("YAML Files"));
        filter.add_pattern("*.yaml");
        filter.add_pattern("*.yml");
        chooser.add_filter(&filter);

        let parent_err_clone = parent_clone.clone();
        let state_rc_inner = Rc::clone(&state_rc_clone);
        chooser.connect_response(move |c, response| {
            if response == gtk::ResponseType::Accept {
                if let Some(file) = c.file() {
                    if let Some(path) = file.path() {
                        if let Some(mut new_config) = Configuration::load_from_file(&path) {
                            let parent_confirm = parent_err_clone.clone();
                            let state_rc_confirm = Rc::clone(&state_rc_inner);
                            show_confirm_dialog(
                                &parent_err_clone,
                                "Confirm Import",
                                "Are you sure you want to import this configuration? This will overwrite your existing gestures.",
                                "Import",
                                false,
                                move || {
                                    let default_path = mygestures::config::get_default_config_path();
                                    if let Err(e) = std::fs::copy(&path, &default_path) {
                                        show_error_dialog(&parent_confirm, &format!("Failed to copy config file: {}", e));
                                    } else {
                                        new_config.user_config_path = default_path;
                                        let mut state = state_rc_confirm.borrow_mut();
                                        state.config = new_config;
                                        drop(state);
                                        refresh_gesture_list(&state_rc_confirm, None);
                                        let state = state_rc_confirm.borrow();
                                        reload_daemon(state.dbus_conn.as_ref(), Some(&parent_confirm));
                                    }
                                }
                            );
                        } else {
                            show_error_dialog(&parent_err_clone, "Invalid configuration file. Please select a valid YAML mygestures configuration.");
                        }
                    }
                }
            }
            c.destroy();
        });
        chooser.show();
    });

    import_box.append(&import_btn);
    import_row.set_child(Some(&import_box));
    maintenance_list.append(&import_row);

    // Reset Row
    let reset_row = gtk::ListBoxRow::new();
    let reset_box = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    reset_box.set_margin_start(16);
    reset_box.set_margin_end(16);
    reset_box.set_margin_top(12);
    reset_box.set_margin_bottom(12);

    let reset_text_vbox = gtk::Box::new(gtk::Orientation::Vertical, 2);
    reset_text_vbox.set_hexpand(true);
    reset_text_vbox.set_halign(gtk::Align::Start);

    let reset_title = gtk::Label::new(Some("Reset to Defaults"));
    reset_title.add_css_class("status-label");
    reset_title.set_halign(gtk::Align::Start);
    reset_text_vbox.append(&reset_title);

    let reset_subtitle = gtk::Label::new(Some("Restore default configuration values"));
    reset_subtitle.add_css_class("action-label");
    reset_subtitle.set_halign(gtk::Align::Start);
    reset_text_vbox.append(&reset_subtitle);

    reset_box.append(&reset_text_vbox);

    let reset_btn = gtk::Button::with_label("Reset...");
    reset_btn.set_valign(gtk::Align::Center);
    reset_btn.add_css_class("destructive-action");

    let parent_clone = parent.clone();
    let state_rc_clone2 = Rc::clone(state_rc);
    reset_btn.connect_clicked(move |_| {
        let parent_confirm = parent_clone.clone();
        let state_rc_inner = Rc::clone(&state_rc_clone2);
        show_confirm_dialog(
            &parent_clone,
            "Reset to Defaults",
            "Are you sure you want to reset all gesture settings to defaults? This action cannot be undone.",
            "Reset",
            true,
            move || {
                let default_path = mygestures::config::get_default_config_path();
                if default_path.exists() {
                    let _ = std::fs::remove_file(&default_path);
                }
                if let Err(e) = mygestures::config::initialize_user_config_if_missing() {
                    show_error_dialog(&parent_confirm, &format!("Failed to reset config: {}", e));
                    return;
                }
                let new_config = Configuration::load_from_defaults();
                let mut state = state_rc_inner.borrow_mut();
                state.config = new_config;
                drop(state);
                refresh_gesture_list(&state_rc_inner, None);
                let state = state_rc_inner.borrow();
                reload_daemon(state.dbus_conn.as_ref(), Some(&parent_confirm));
            }
        );
    });

    reset_box.append(&reset_btn);
    reset_row.set_child(Some(&reset_box));
    maintenance_list.append(&reset_row);

    dialog.present();
}

fn build_ui(app: &gtk::Application) {
    let window = gtk::ApplicationWindow::new(app);
    window.set_title(Some("Gestos"));
    window.set_default_size(650, 700);

    let header = gtk::HeaderBar::new();
    let title_label = gtk::Label::new(Some("Gestures"));
    title_label.add_css_class("title");
    header.set_title_widget(Some(&title_label));
    window.set_titlebar(Some(&header));

    // 1. Add gesture button (Primary action on the left)
    let add_gest_btn = gtk::Button::with_label("New Gesture");
    add_gest_btn.add_css_class("suggested-action");
    add_gest_btn.set_tooltip_text(Some("Add Gesture"));
    header.pack_start(&add_gest_btn);

    // 2. About button (Secondary action on the right)
    let about_btn = gtk::Button::from_icon_name("help-about-symbolic");
    header.pack_end(&about_btn);

    // 3. Settings button (Middle action on the right)
    let settings_btn = gtk::Button::from_icon_name("preferences-system-symbolic");
    settings_btn.set_tooltip_text(Some("Settings"));
    header.pack_end(&settings_btn);

    // 4. Daemon Switch (Leftmost action on the right)
    let daemon_switch = gtk::Switch::new();
    daemon_switch.set_valign(gtk::Align::Center);
    header.pack_end(&daemon_switch);

    // Content VBox
    let content_vbox = gtk::Box::new(gtk::Orientation::Vertical, 0);
    content_vbox.add_css_class("main-window-content");
    window.set_child(Some(&content_vbox));

    // 5. Search Entry
    let search_entry = gtk::SearchEntry::new();
    search_entry.set_halign(gtk::Align::Center);
    search_entry.set_width_request(360);
    search_entry.set_placeholder_text(Some("Search gestures..."));
    search_entry.set_margin_start(56);
    search_entry.set_margin_end(56);
    search_entry.set_margin_top(24);
    search_entry.set_margin_bottom(12);
    content_vbox.append(&search_entry);

    let scrolled = gtk::ScrolledWindow::new();
    scrolled.set_vexpand(true);
    content_vbox.append(&scrolled);

    let list_container = gtk::Box::new(gtk::Orientation::Vertical, 0);

    let main_list = gtk::ListBox::new();
    main_list.set_margin_start(56);
    main_list.set_margin_end(56);
    main_list.set_margin_bottom(56);
    main_list.add_css_class("boxed-list");
    main_list.set_selection_mode(gtk::SelectionMode::None);
    list_container.append(&main_list);

    let empty_state_box = gtk::Box::new(gtk::Orientation::Vertical, 16);
    empty_state_box.set_valign(gtk::Align::Center);
    empty_state_box.set_halign(gtk::Align::Center);
    empty_state_box.set_margin_top(80);
    empty_state_box.set_margin_bottom(80);

    let empty_icon = gtk::Image::from_icon_name("view-list-bullet-symbolic");
    empty_icon.set_pixel_size(80);
    empty_icon.set_opacity(0.4);
    empty_state_box.append(&empty_icon);

    let empty_title = gtk::Label::new(None);
    empty_title.set_markup("<span size='large' weight='bold'>No Gestures Found</span>");
    empty_state_box.append(&empty_title);

    let empty_subtitle = gtk::Label::new(Some("Configure some gestures or add a new one to get started."));
    empty_subtitle.set_opacity(0.65);
    empty_state_box.append(&empty_subtitle);

    list_container.append(&empty_state_box);
    scrolled.set_child(Some(&list_container));

    if let Err(e) = mygestures::config::initialize_user_config_if_missing() {
        eprintln!("Warning: Failed to initialize configuration: {}", e);
    }
    let config = Configuration::load_from_defaults();
    let dbus_conn = zbus::blocking::Connection::session().ok();

    let state = Rc::new(RefCell::new(AppState {
        config,
        main_list,
        search_entry,
        daemon_switch,
        window,
        switch_handler_id: None,
        newly_added_gestures: Vec::new(),
        dbus_conn,
        empty_state_box,
    }));

    // Refresh initially
    refresh_gesture_list(&state, None);

    // Connect search entry
    let state_clone = Rc::clone(&state);
    state
        .borrow()
        .search_entry
        .connect_search_changed(move |_| {
            refresh_gesture_list(&state_clone, None);
        });

    // Connect Add button
    let state_clone2 = Rc::clone(&state);
    add_gest_btn.connect_clicked(move |_| {
        open_gesture_editor(&state_clone2, None);
    });

    // Connect row activation for editing
    let state_clone3 = Rc::clone(&state);
    state
        .borrow()
        .main_list
        .connect_row_activated(move |_, row| {
            let idx = row.index();
            let state_borrow = state_clone3.borrow();

            let visible_gestures = get_visible_gestures(&state_borrow);

            if idx >= 0 && (idx as usize) < visible_gestures.len() {
                let gesture = visible_gestures[idx as usize].clone();
                drop(state_borrow);
                open_gesture_editor(&state_clone3, Some(gesture));
            }
        });

    // Connect about button
    let window_clone = state.borrow().window.clone();
    about_btn.connect_clicked(move |_| {
        let dialog = gtk::AboutDialog::new();
        dialog.set_transient_for(Some(&window_clone));
        dialog.set_program_name(Some("Gestos"));
        dialog.set_version(Some("4.2.1"));
        dialog.set_comments(Some(
            "A modern mouse gestures editor for Wayland desktop environments.",
        ));
        dialog.set_authors(&["Lucas Augusto Deters <lucasdeters@gmail.com>"]);
        dialog.present();
    });

    // Connect settings button
    let state_settings_clone = Rc::clone(&state);
    settings_btn.connect_clicked(move |_| {
        open_settings_window(&state_settings_clone);
    });

    // Connect daemon switch state controller
    let state_clone4 = Rc::clone(&state);
    let handler_id = state
        .borrow()
        .daemon_switch
        .connect_state_set(move |_, state| {
            if state {
                if let Err(err) = start_daemon(state_clone4.borrow().dbus_conn.as_ref()) {
                    show_error_dialog(&state_clone4.borrow().window, &err);
                }
            } else {
                stop_daemon(state_clone4.borrow().dbus_conn.as_ref());
            }

            let state_borrow = state_clone4.borrow();
            let running = is_daemon_running(state_borrow.dbus_conn.as_ref());
            if !running {
                // Toggle the switch back off immediately if daemon startup failed
                if let Some(ref hid) = state_borrow.switch_handler_id {
                    state_borrow.daemon_switch.block_signal(hid);
                    state_borrow.daemon_switch.set_active(false);
                    state_borrow.daemon_switch.unblock_signal(hid);
                }
            }
            glib::Propagation::Proceed
        });
    state.borrow_mut().switch_handler_id = Some(handler_id);

    // Setup periodic status check timer (every 1 second)
    let state_clone5 = Rc::clone(&state);
    glib::timeout_add_local(std::time::Duration::from_secs(1), move || {
        let state_borrow = state_clone5.borrow();
        let running = is_daemon_running(state_borrow.dbus_conn.as_ref());

        if let Some(ref hid) = state_borrow.switch_handler_id {
            state_borrow.daemon_switch.block_signal(hid);
            state_borrow.daemon_switch.set_active(running);
            state_borrow.daemon_switch.unblock_signal(hid);
        }
        glib::ControlFlow::Continue
    });

    // Stylesheet injection
    let provider = gtk::CssProvider::new();
    provider.load_from_data(
        "headerbar { background: @window_bg_color; border: none; box-shadow: none; }\n\
         .main-window-content, .dialog-content { background-color: @window_bg_color; }\n\
         scrolledwindow, viewport { background-color: transparent !important; background-image: none !important; }\n\
         .boxed-list, .boxed-list row, .boxed-list listrow, row, listrow { background-color: @card_bg_color !important; }\n\
         .gesture-preview-frame { background: @window_bg_color; border-radius: 6px; }\n\
         searchentry, entry.search { border-radius: 18px; }\n\
         .context-title { font-size: 1.5em; font-weight: bold; }\n\
         .gesture-row { padding: 6px; }\n\
         .icon-holder { padding: 4px; }\n\
         .action-label { font-size: 0.9em; opacity: 0.7; }\n\
         .status-dot-running { color: #10b981; }\n\
         .status-dot-stopped { color: #6b7280; }\n\
         .settings-row { padding: 8px; }\n\
         .section-header { font-weight: bold; margin-top: 8px; margin-bottom: 4px; }\n\
         .status-label { font-weight: bold; }\n\
         .warning-box { padding: 8px; background-color: alpha(@warning_color, 0.15); border: 1px solid alpha(@warning_color, 0.3); border-radius: 6px; }\n\
         .warning-label { color: @warning_color; font-size: 0.95em; }\n\
         .keycap { padding: 6px 12px; background-color: alpha(currentColor, 0.08); border: 1px solid alpha(currentColor, 0.15); border-radius: 6px; font-weight: bold; font-size: 1.1em; }\n\
         .drag-handle { opacity: 0.35; cursor: grab; margin-right: 4px; }\n\
         .drag-handle:hover { opacity: 0.85; }\n\
         .drag-handle:active { cursor: grabbing; }\n"
    );

    if let Some(display) = gdk::Display::default() {
        gtk::style_context_add_provider_for_display(
            &display,
            &provider,
            gtk::STYLE_PROVIDER_PRIORITY_APPLICATION,
        );
    }

    state.borrow().window.present();
}

enum OverlayEvent {
    Started(f64, f64),
    Updated(f64, f64),
    Ended,
    ActionExecuted(String, String, String),
}

#[zbus::proxy(
    interface = "org.mygestures.Daemon",
    default_service = "org.mygestures.Daemon",
    default_path = "/org/mygestures/Daemon"
)]
trait Daemon {
    #[zbus(signal)]
    fn gesture_started(&self, x: f64, y: f64) -> zbus::Result<()>;

    #[zbus(signal)]
    fn gesture_updated(&self, x: f64, y: f64) -> zbus::Result<()>;

    #[zbus(signal)]
    fn gesture_ended(&self) -> zbus::Result<()>;

    #[zbus(signal)]
    fn action_executed(&self, gesture_name: String, action_desc: String, icon_name: String) -> zbus::Result<()>;
}

struct TrailData {
    points: Vec<Point2D>,
    opacity: f64,
}

fn build_overlay_ui(app: &gtk::Application) {
    let window = gtk::ApplicationWindow::new(app);
    window.set_decorated(false);
    window.set_focusable(false);

    // CSS styling for transparency and OSD notification box
    let provider = gtk::CssProvider::new();
    provider.load_from_data(
        "window { background-color: rgba(0, 0, 0, 0); }\n\
         drawingarea { background-color: rgba(0, 0, 0, 0); }\n\
         .osd-notification {\n\
             background-color: rgba(30, 30, 30, 0.85);\n\
             border: 1px solid rgba(255, 255, 255, 0.15);\n\
             border-radius: 20px;\n\
             padding: 16px 24px;\n\
             margin-bottom: 80px;\n\
         }\n\
         .osd-title {\n\
             font-size: 16px;\n\
             font-weight: bold;\n\
             color: white;\n\
         }\n\
         .osd-subtitle {\n\
             font-size: 13px;\n\
             color: #b3b3b3;\n\
         }"
    );
    if let Some(display) = gdk::Display::default() {
        gtk::style_context_add_provider_for_display(
            &display,
            &provider,
            gtk::STYLE_PROVIDER_PRIORITY_APPLICATION,
        );
    }

    let overlay = gtk::Overlay::new();
    window.set_child(Some(&overlay));

    let drawing_area = gtk::DrawingArea::new();
    overlay.set_child(Some(&drawing_area));

    let osd_box = gtk::Box::builder()
        .orientation(gtk::Orientation::Vertical)
        .spacing(8)
        .valign(gtk::Align::End)
        .halign(gtk::Align::Center)
        .visible(false)
        .build();
    osd_box.add_css_class("osd-notification");

    let osd_icon = gtk::Image::builder()
        .pixel_size(48)
        .halign(gtk::Align::Center)
        .valign(gtk::Align::Center)
        .build();
    osd_icon.add_css_class("osd-icon");

    let osd_title = gtk::Label::builder()
        .use_markup(true)
        .halign(gtk::Align::Center)
        .valign(gtk::Align::Center)
        .build();
    osd_title.add_css_class("osd-title");

    let osd_subtitle = gtk::Label::builder()
        .use_markup(true)
        .halign(gtk::Align::Center)
        .valign(gtk::Align::Center)
        .build();
    osd_subtitle.add_css_class("osd-subtitle");

    osd_box.append(&osd_icon);
    osd_box.append(&osd_title);
    osd_box.append(&osd_subtitle);

    overlay.add_overlay(&osd_box);

    // Empty input region on realize (click-through)
    window.connect_realize(|w| {
        if let Some(surface) = w.surface() {
            let region = cairo::Region::create();
            surface.set_input_region(&region);
        }
    });

    window.fullscreen();

    let trail_data = Rc::new(RefCell::new(TrailData {
        points: Vec::new(),
        opacity: 0.0,
    }));

    let trail_draw = trail_data.clone();
    drawing_area.set_draw_func(move |_, cr, width, height| {
        let data = trail_draw.borrow();
        if data.points.len() < 2 {
            return;
        }

        cr.save().unwrap();

        let start_pt = &data.points[0];
        let anchor_x = width as f64 / 2.0;
        let anchor_y = height as f64 / 2.0;

        cr.set_source_rgba(0.0, 0.7, 1.0, data.opacity * 0.85); // Neon cyan glow
        cr.set_line_width(6.0);
        cr.set_line_cap(cairo::LineCap::Round);
        cr.set_line_join(cairo::LineJoin::Round);

        cr.move_to(anchor_x, anchor_y);
        for pt in &data.points[1..] {
            let dx = pt.x - start_pt.x;
            let dy = pt.y - start_pt.y;
            cr.line_to(anchor_x + dx, anchor_y + dy);
        }

        cr.stroke().unwrap();
        cr.restore().unwrap();
    });

    let (tx, mut rx) = futures_channel::mpsc::unbounded::<OverlayEvent>();

    let trail_clone = trail_data.clone();
    let area_clone = drawing_area.clone();
    let window_clone = window.clone();
    let fade_timeout_id = Rc::new(RefCell::new(None::<glib::SourceId>));
    let fade_timeout_clone = fade_timeout_id.clone();

    let osd_box_clone = osd_box.clone();
    let osd_icon_clone = osd_icon.clone();
    let osd_title_clone = osd_title.clone();
    let osd_subtitle_clone = osd_subtitle.clone();
    let osd_timeout_id = Rc::new(RefCell::new(None::<glib::SourceId>));
    let osd_timeout_clone = osd_timeout_id.clone();

    glib::MainContext::default().spawn_local(async move {
        while let Some(event) = rx.next().await {
            match event {
                OverlayEvent::Started(x, y) => {
                    if let Some(source_id) = fade_timeout_clone.borrow_mut().take() {
                        source_id.remove();
                    }

                    let mut data = trail_clone.borrow_mut();
                    data.points.clear();
                    data.points.push(Point2D { x, y });
                    data.opacity = 1.0;

                    window_clone.present();
                    area_clone.queue_draw();
                }
                OverlayEvent::Updated(x, y) => {
                    let mut data = trail_clone.borrow_mut();
                    data.points.push(Point2D { x, y });
                    area_clone.queue_draw();
                }
                OverlayEvent::Ended => {
                    let trail_timer = trail_clone.clone();
                    let area_timer = area_clone.clone();
                    let window_timer = window_clone.clone();
                    let fade_timeout_timer = fade_timeout_clone.clone();
                    let osd_box_timer_hide = osd_box_clone.clone();

                    let source_id = glib::timeout_add_local(std::time::Duration::from_millis(16), move || {
                        let mut data = trail_timer.borrow_mut();
                        data.opacity -= 0.05;
                        if data.opacity <= 0.0 {
                            data.points.clear();
                            if !osd_box_timer_hide.is_visible() {
                                window_timer.hide();
                            }
                            *fade_timeout_timer.borrow_mut() = None;
                            glib::ControlFlow::Break
                        } else {
                            area_timer.queue_draw();
                            glib::ControlFlow::Continue
                        }
                    });

                    *fade_timeout_clone.borrow_mut() = Some(source_id);
                }
                OverlayEvent::ActionExecuted(gesture_name, action_desc, icon_name) => {
                    if is_osd_enabled() {
                        if let Some(source_id) = osd_timeout_clone.borrow_mut().take() {
                            source_id.remove();
                        }

                        osd_icon_clone.set_icon_name(Some(&icon_name));
                        osd_title_clone.set_markup(&format!("Gesture: <b>{}</b>", gesture_name));
                        osd_subtitle_clone.set_markup(&format!("<i>{}</i>", action_desc));

                        osd_box_clone.set_opacity(1.0);
                        osd_box_clone.set_visible(true);
                        window_clone.present();

                        let osd_box_fade = osd_box_clone.clone();
                        let osd_timeout_fade = osd_timeout_clone.clone();
                        let window_fade = window_clone.clone();
                        let trail_fade = trail_clone.clone();

                        let fade_val = Rc::new(RefCell::new(1.0));
                        let source_id = glib::timeout_add_local(std::time::Duration::from_millis(1500), move || {
                            let osd_box_timer = osd_box_fade.clone();
                            let osd_timeout_timer = osd_timeout_fade.clone();
                            let window_timer = window_fade.clone();
                            let trail_timer = trail_fade.clone();
                            let fade_val_inner = fade_val.clone();

                            let fade_source_id = glib::timeout_add_local(std::time::Duration::from_millis(16), move || {
                                let mut v = fade_val_inner.borrow_mut();
                                *v -= 0.05;
                                if *v <= 0.0 {
                                    osd_box_timer.set_visible(false);
                                    *osd_timeout_timer.borrow_mut() = None;
                                    if trail_timer.borrow().opacity <= 0.0 {
                                        window_timer.hide();
                                    }
                                    glib::ControlFlow::Break
                                } else {
                                    osd_box_timer.set_opacity(*v);
                                    glib::ControlFlow::Continue
                                }
                            });
                            *osd_timeout_fade.borrow_mut() = Some(fade_source_id);
                            glib::ControlFlow::Break
                        });

                        *osd_timeout_clone.borrow_mut() = Some(source_id);
                    }
                }
            }
        }
    });

    // Spawn async receiver
    glib::MainContext::default().spawn_local(async move {
        let connection = match zbus::Connection::session().await {
            Ok(c) => c,
            Err(e) => {
                eprintln!("Failed to connect to D-Bus session: {}", e);
                return;
            }
        };

        let proxy = match DaemonProxy::new(&connection).await {
            Ok(p) => p,
            Err(e) => {
                eprintln!("Failed to create Daemon D-Bus proxy: {}", e);
                return;
            }
        };

        let mut started_stream = match proxy.receive_gesture_started().await {
            Ok(s) => s,
            Err(e) => {
                eprintln!("Failed to receive GestureStarted: {}", e);
                return;
            }
        };
        let tx_started = tx.clone();
        glib::MainContext::default().spawn_local(async move {
            while let Some(signal) = started_stream.next().await {
                if let Ok(args) = signal.args() {
                    let _ = tx_started.unbounded_send(OverlayEvent::Started(args.x, args.y));
                }
            }
        });

        let mut updated_stream = match proxy.receive_gesture_updated().await {
            Ok(s) => s,
            Err(e) => {
                eprintln!("Failed to receive GestureUpdated: {}", e);
                return;
            }
        };
        let tx_updated = tx.clone();
        glib::MainContext::default().spawn_local(async move {
            while let Some(signal) = updated_stream.next().await {
                if let Ok(args) = signal.args() {
                    let _ = tx_updated.unbounded_send(OverlayEvent::Updated(args.x, args.y));
                }
            }
        });

        let mut ended_stream = match proxy.receive_gesture_ended().await {
            Ok(s) => s,
            Err(e) => {
                eprintln!("Failed to receive GestureEnded: {}", e);
                return;
            }
        };
        let tx_ended = tx.clone();
        glib::MainContext::default().spawn_local(async move {
            while let Some(_signal) = ended_stream.next().await {
                let _ = tx_ended.unbounded_send(OverlayEvent::Ended);
            }
        });

        let mut action_executed_stream = match proxy.receive_action_executed().await {
            Ok(s) => s,
            Err(e) => {
                eprintln!("Failed to receive ActionExecuted: {}", e);
                return;
            }
        };
        let tx_action = tx.clone();
        glib::MainContext::default().spawn_local(async move {
            while let Some(signal) = action_executed_stream.next().await {
                if let Ok(args) = signal.args() {
                    let _ = tx_action.unbounded_send(OverlayEvent::ActionExecuted(
                        args.gesture_name.clone(),
                        args.action_desc.clone(),
                        args.icon_name.clone(),
                    ));
                }
            }
        });
    });
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let is_overlay = args.iter().any(|arg| arg == "--overlay");

    let app = gtk::Application::builder()
        .application_id(if is_overlay {
            "org.mygestures.gestos.overlay"
        } else {
            "org.mygestures.gestos"
        })
        .build();

    if is_overlay {
        app.connect_activate(build_overlay_ui);
    } else {
        app.connect_activate(build_ui);
    }
    app.run();
}
