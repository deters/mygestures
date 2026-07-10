use crate::state::*;
use mygestures::config::ActionType;
use gtk::prelude::*;
use gtk4 as gtk;
use gtk::gio;

pub fn action_matches(a: &ActionType, opt: &EditorActionOption) -> bool {
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

pub fn get_static_action_options() -> Vec<EditorActionOption> {
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

pub fn fetch_kde_action_options() -> Vec<EditorActionOption> {
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

pub fn fetch_gnome_action_options() -> Vec<EditorActionOption> {
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
