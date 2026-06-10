use std::cell::RefCell;
use std::rc::Rc;
use std::process::Command;
use gtk4 as gtk;
use gtk::prelude::*;
use gtk::{cairo, glib, gdk, gio};
use mygestures::config::{Configuration, Gesture, ActionType, generate_unique_id};
use mygestures::protractor::{Point2D, match_gesture};

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
];

fn action_matches(a: &ActionType, opt: &EditorActionOption) -> bool {
    match (a, &opt.action_type) {
        (ActionType::Gnome(k1), ActionType::Gnome(k2)) => k1 == k2,
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
            name: "Close Window (Kill)".to_string(),
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
            name: "Minimize Window (Iconify)".to_string(),
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
                eprintln!("Info: GSettings schema '{}' not found, skipping.", schema_id);
                continue;
            }
        };

        let settings = gio::Settings::new(schema_id);

        for key in schema.list_keys() {
            let skey = schema.key(&key);
            let mut summary = skey.summary().map(|s| s.to_string()).unwrap_or_default();
            if summary.is_empty() {
                summary = skey.description().map(|s| s.to_string()).unwrap_or_default();
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
        if schema_id == "org.gnome.settings-daemon.plugins.media-keys" {
            if schema.has_key("custom-keybindings") {
                let paths: Vec<String> = settings.get("custom-keybindings");
                for path in paths {
                    let custom = gio::Settings::with_path("org.gnome.settings-daemon.plugins.media-keys.custom-keybinding", &path);
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
    }

    println!("Fetched {} GNOME action options from GSettings.", options.len());
    options
}

struct AppState {
    config: Configuration,
    main_list: gtk::ListBox,
    search_entry: gtk::SearchEntry,
    status_label: gtk::Label,
    status_dot: gtk::Image,
    daemon_switch: gtk::Switch,
    window: gtk::ApplicationWindow,
    switch_handler_id: Option<glib::SignalHandlerId>,
    newly_added_gestures: Vec<String>,
}

fn is_daemon_running() -> bool {
    let uid = unsafe { libc::getuid() };
    let output = Command::new("pgrep")
        .arg("-u")
        .arg(uid.to_string())
        .arg("-x")
        .arg("mygestures")
        .output();
    
    if let Ok(out) = output {
        out.status.success() && !out.stdout.is_empty()
    } else {
        false
    }
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

fn start_daemon() -> Result<(), String> {
    if is_daemon_running() {
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
        Err(e) => {
            Err(format!("Failed to query daemon status: {}", e))
        }
    }
}

fn stop_daemon() {
    let uid = unsafe { libc::getuid() };
    let _ = Command::new("pkill")
        .arg("-u")
        .arg(uid.to_string())
        .arg("-x")
        .arg("mygestures")
        .status();
}

fn reload_daemon() {
    if is_daemon_running() {
        stop_daemon();
        std::thread::sleep(std::time::Duration::from_millis(150));
        let _ = start_daemon();
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

fn get_action_category_icon(action: &ActionType) -> (&'static str, &'static str) {
    match action {
        ActionType::Execute(_) => ("utilities-terminal-symbolic", "icon-bg-purple"),
        ActionType::Keypress(_) => ("preferences-desktop-keyboard-shortcuts-symbolic", "icon-bg-orange"),
        ActionType::Gnome(_) => ("preferences-system-symbolic", "icon-bg-blue"),
        ActionType::WorkspaceLeft | ActionType::WorkspaceRight | ActionType::WorkspaceUp | ActionType::WorkspaceDown => {
            ("go-next-symbolic", "icon-bg-blue")
        }
        ActionType::VolumeUp | ActionType::VolumeDown | ActionType::VolumeMute => ("audio-volume-high-symbolic", "icon-bg-green"),
        ActionType::MediaPlay | ActionType::MediaNext | ActionType::MediaPrev => ("media-playback-start-symbolic", "icon-bg-green"),
        _ => ("system-run-symbolic", "icon-bg-blue"),
    }
}

fn draw_gesture_path(cr: &cairo::Context, points: &[Point2D], width: f64, height: f64, _draw_bg: bool, fit_to_canvas: bool) {
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
        if p.x < min_x { min_x = p.x; }
        if p.x > max_x { max_x = p.x; }
        if p.y < min_y { min_y = p.y; }
        if p.y > max_y { max_y = p.y; }
    }

    let w = max_x - min_x;
    let h = max_y - min_y;
    let max_dim = w.max(h);
    let scale = if fit_to_canvas {
        let size = width.min(height);
        if max_dim > 1.0 { (size * 0.62) / max_dim } else { 1.0 }
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

fn create_gesture_row(gesture: &Gesture) -> gtk::ListBoxRow {
    let row = gtk::ListBoxRow::new();
    row.add_css_class("gesture-row");
    
    let main_hbox = gtk::Box::new(gtk::Orientation::Horizontal, 16);
    main_hbox.set_margin_start(16);
    main_hbox.set_margin_end(16);
    main_hbox.set_margin_top(12);
    main_hbox.set_margin_bottom(12);

    // 1. Icon Category Holder
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

    // 2. Info text label
    let vbox = gtk::Box::new(gtk::Orientation::Vertical, 4);
    vbox.set_hexpand(true);
    vbox.set_valign(gtk::Align::Center);

    let title_label = gtk::Label::new(Some(&gesture.name));
    title_label.set_halign(gtk::Align::Start);
    title_label.set_markup(&format!("<b>{}</b>", gesture.name));
    vbox.append(&title_label);

    let action_desc = if !gesture.actions.is_empty() {
        gesture.actions[0].to_string()
    } else {
        "No Action Configured".to_string()
    };
    let action_label = gtk::Label::new(Some(&action_desc));
    action_label.set_halign(gtk::Align::Start);
    action_label.add_css_class("action-label");
    vbox.append(&action_label);
    
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
    row
}

fn get_visible_gestures(state: &AppState) -> Vec<Gesture> {
    let filter = state.search_entry.text().to_lowercase();
    let newly_added = &state.newly_added_gestures;
    
    let mut gestures: Vec<Gesture> = state.config.gestures.iter()
        .filter(|g| !g.is_deleted)
        .filter(|g| filter.is_empty() || g.name.to_lowercase().contains(&filter))
        .cloned()
        .collect();
        
    gestures.sort_by(|a, b| {
        let a_new_idx = newly_added.iter().position(|name| name == &a.name);
        let b_new_idx = newly_added.iter().position(|name| name == &b.name);
        
        match (a_new_idx, b_new_idx) {
            (Some(a_idx), Some(b_idx)) => b_idx.cmp(&a_idx), // most recently added first
            (Some(_), None) => std::cmp::Ordering::Less,
            (None, Some(_)) => std::cmp::Ordering::Greater,
            (None, None) => a.name.to_lowercase().cmp(&b.name.to_lowercase()),
        }
    });
    
    gestures
}

fn refresh_gesture_list(state_rc: &Rc<RefCell<AppState>>, select_name: Option<&str>) {
    let state = state_rc.borrow();
    
    // Clear list
    while let Some(child) = state.main_list.first_child() {
        state.main_list.remove(&child);
    }

    let visible = get_visible_gestures(&state);

    for gesture in &visible {
        let row = create_gesture_row(gesture);
        state.main_list.append(&row);
        
        if let Some(name) = select_name {
            if gesture.name == name {
                state.main_list.select_row(Some(&row));
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
    udn: Rc<dyn Fn() + 'static>
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
    prompt_label.set_markup(&format!("Enter new shortcut to change <b>{}</b>", escaped_name));
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

    let hint_label = gtk::Label::new(Some("Press Esc to cancel or Backspace to reset the shortcut"));
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

        let is_modifier = match keyval {
            gdk::Key::Control_L | gdk::Key::Control_R |
            gdk::Key::Alt_L | gdk::Key::Alt_R |
            gdk::Key::Shift_L | gdk::Key::Shift_R |
            gdk::Key::Super_L | gdk::Key::Super_R |
            gdk::Key::Meta_L | gdk::Key::Meta_R => true,
            _ => false,
        };

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

fn open_gesture_editor(state_rc: &Rc<RefCell<AppState>>, target_gesture: Option<Gesture>) {
    let state = state_rc.borrow();
    let dialog = gtk::Window::new();
    dialog.set_transient_for(Some(&state.window));
    dialog.set_modal(true);
    dialog.set_title(Some(if target_gesture.is_some() { "Edit Gesture" } else { "Add Gesture" }));
    dialog.set_default_size(480, 620);

    let dialog_header = gtk::HeaderBar::new();
    dialog_header.set_show_title_buttons(false);
    let dialog_title = gtk::Label::new(Some(if target_gesture.is_some() { "Edit Gesture" } else { "Add Gesture" }));
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
        if entry.text().trim().is_empty() {
            *cust_clone.borrow_mut() = false;
        } else {
            *cust_clone.borrow_mut() = true;
        }
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
        target_gesture.as_ref().map(|g| g.points.clone()).unwrap_or_default()
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
        pts.push(Point2D { x: start_x, y: start_y });
        println!("GUI: Gesture drawing started at ({:.1}, {:.1})", start_x, start_y);
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
            let templates: Vec<(String, Vec<Point2D>)> = state.config.gestures.iter()
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

    // Row 1: Name
    let name_row = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    name_row.add_css_class("settings-row");

    let name_label = gtk::Label::new(Some("Gesture Name"));
    name_label.set_halign(gtk::Align::Start);
    name_label.add_css_class("status-label");
    name_row.append(&name_label);

    let name_spacer = gtk::Box::new(gtk::Orientation::Horizontal, 0);
    name_spacer.set_hexpand(true);
    name_row.append(&name_spacer);

    name_entry.set_halign(gtk::Align::End);
    name_entry.set_size_request(220, -1);
    name_row.append(&name_entry);

    settings_list.append(&name_row);

    // Row 2: Category
    let category_row = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    category_row.add_css_class("settings-row");

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
            if gesture_name.trim().is_empty() { "Gesture" } else { &gesture_name }, 
            &entry_click, 
            Rc::clone(&udn_click) as Rc<dyn Fn()>
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
 
    // Find initial matching option
    let mut selected_cat = 0;
    let mut selected_act = 0;
 
    if let Some(ref g) = target_gesture {
        if !g.actions.is_empty() {
            let a = &g.actions[0];
            if let Some(found_opt) = all_options.iter().find(|opt| action_matches(a, opt)) {
                selected_cat = found_opt.category;
 
                // Get the filtered options for this category
                let filtered: Vec<EditorActionOption> = all_options.iter()
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
    let initial_filtered: Vec<EditorActionOption> = all_options.iter()
        .filter(|opt| opt.category == selected_cat)
        .cloned()
        .collect();
 
    let action_names: Vec<String> = initial_filtered.iter().map(|opt| opt.name.clone()).collect();
    let action_refs: Vec<&str> = action_names.iter().map(|s| s.as_str()).collect();
    let action_model = gtk::StringList::new(&action_refs);
    action_dropdown.set_model(Some(&action_model));
 
    *current_options.borrow_mut() = initial_filtered.clone();
 
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
                action_details_entry.set_placeholder_text(Some("e.g. 1 (Left), 2 (Middle), 3 (Right)"));
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
 
    category_dropdown.connect_selected_notify(move |cat_dd| {
        let cat_idx = cat_dd.selected();
        if cat_idx == gtk::INVALID_LIST_POSITION {
            return;
        }
        let cat_idx = cat_idx as usize;
        let filtered: Vec<EditorActionOption> = all_options_clone.iter()
            .filter(|opt| opt.category == cat_idx)
            .cloned()
            .collect();
 
        let action_names: Vec<String> = filtered.iter().map(|opt| opt.name.clone()).collect();
        let action_refs: Vec<&str> = action_names.iter().map(|s| s.as_str()).collect();
        let action_model = gtk::StringList::new(&action_refs);
        action_dropdown_clone.set_model(Some(&action_model));
 
        *current_opts_clone.borrow_mut() = filtered.clone();
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
        let raw_movement = pts.iter()
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
            if let Some(pos) = state.config.gestures.iter().position(|g| g.id == *lookup_id) {
                let lookup_name = state.config.gestures[pos].name.clone();
                if name != lookup_name {
                    if state.config.gestures.iter().any(|g| g.name == name) {
                        println!("Gesture save failed: A gesture with the name '{}' already exists.", name);
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
                println!("Gesture modification failed: Could not find gesture with ID '{}'", lookup_id);
            }
        } else {
            // Check if name conflict
            if state.config.gestures.iter().any(|g| g.name == name) {
                println!("Gesture save failed: A gesture with the name '{}' already exists.", name);
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
        reload_daemon();
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
                reload_daemon();
                drop(state);
                refresh_gesture_list(&state_clone, None);
                dialog_clone3.destroy();
            }
        });
        main_box.append(&delete_btn);
    }

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
    let add_gest_btn = gtk::Button::from_icon_name("list-add-symbolic");
    add_gest_btn.set_tooltip_text(Some("Add Gesture"));
    header.pack_start(&add_gest_btn);

    // 2. About button (Secondary action on the right)
    let about_btn = gtk::Button::from_icon_name("help-about-symbolic");
    header.pack_end(&about_btn);

    // Content VBox
    let content_vbox = gtk::Box::new(gtk::Orientation::Vertical, 0);
    content_vbox.add_css_class("main-window-content");
    window.set_child(Some(&content_vbox));

    // 3. Status Banner (for daemon status controls)
    let banner_box = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    banner_box.set_margin_start(56);
    banner_box.set_margin_end(56);
    banner_box.set_margin_top(16);
    banner_box.set_margin_bottom(8);
    banner_box.add_css_class("status-banner");

    let status_dot = gtk::Image::from_icon_name("media-record-symbolic");
    status_dot.add_css_class("status-dot-stopped");
    banner_box.append(&status_dot);

    let status_label = gtk::Label::new(Some("Daemon Off"));
    status_label.add_css_class("status-label");
    status_label.set_hexpand(true);
    status_label.set_halign(gtk::Align::Start);
    banner_box.append(&status_label);

    let daemon_switch = gtk::Switch::new();
    daemon_switch.set_valign(gtk::Align::Center);
    banner_box.append(&daemon_switch);
    content_vbox.append(&banner_box);

    // 4. Search Entry
    let search_entry = gtk::SearchEntry::new();
    search_entry.set_halign(gtk::Align::Center);
    search_entry.set_width_request(360);
    search_entry.set_placeholder_text(Some("Search shortcuts..."));
    search_entry.set_margin_start(56);
    search_entry.set_margin_end(56);
    search_entry.set_margin_top(8);
    search_entry.set_margin_bottom(12);
    content_vbox.append(&search_entry);

    let scrolled = gtk::ScrolledWindow::new();
    scrolled.set_vexpand(true);
    content_vbox.append(&scrolled);

    let main_list = gtk::ListBox::new();
    main_list.set_margin_start(56);
    main_list.set_margin_end(56);
    main_list.set_margin_bottom(56);
    main_list.add_css_class("boxed-list");
    main_list.set_selection_mode(gtk::SelectionMode::None);
    scrolled.set_child(Some(&main_list));

    if let Err(e) = mygestures::config::initialize_user_config_if_missing() {
        eprintln!("Warning: Failed to initialize configuration: {}", e);
    }
    let config = Configuration::load_from_defaults();

    let state = Rc::new(RefCell::new(AppState {
        config,
        main_list,
        search_entry,
        status_label,
        status_dot,
        daemon_switch,
        window,
        switch_handler_id: None,
        newly_added_gestures: Vec::new(),
    }));

    // Refresh initially
    refresh_gesture_list(&state, None);

    // Connect search entry
    let state_clone = Rc::clone(&state);
    state.borrow().search_entry.connect_search_changed(move |_| {
        refresh_gesture_list(&state_clone, None);
    });

    // Connect Add button
    let state_clone2 = Rc::clone(&state);
    add_gest_btn.connect_clicked(move |_| {
        open_gesture_editor(&state_clone2, None);
    });

    // Connect row activation for editing
    let state_clone3 = Rc::clone(&state);
    state.borrow().main_list.connect_row_activated(move |_, row| {
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
        dialog.set_version(Some("4.1.9"));
        dialog.set_comments(Some("A modern mouse gestures editor for Wayland desktop environments."));
        dialog.set_authors(&["Lucas Augusto Deters <lucasdeters@gmail.com>"]);
        dialog.present();
    });

    // Connect daemon switch state controller
    let state_clone4 = Rc::clone(&state);
    let handler_id = state.borrow().daemon_switch.connect_state_set(move |_, state| {
        if state {
            if let Err(err) = start_daemon() {
                show_error_dialog(&state_clone4.borrow().window, &err);
            }
            set_autostart_enabled(true);
        } else {
            stop_daemon();
            set_autostart_enabled(false);
        }
        
        let state_borrow = state_clone4.borrow();
        let running = is_daemon_running();
        if running {
            state_borrow.status_label.set_text("Daemon Active");
            state_borrow.status_dot.remove_css_class("status-dot-stopped");
            state_borrow.status_dot.add_css_class("status-dot-running");
        } else {
            state_borrow.status_label.set_text("Daemon Off");
            state_borrow.status_dot.remove_css_class("status-dot-running");
            state_borrow.status_dot.add_css_class("status-dot-stopped");
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
        let running = is_daemon_running();

        if let Some(ref hid) = state_borrow.switch_handler_id {
            state_borrow.daemon_switch.block_signal(hid);
            state_borrow.daemon_switch.set_active(running);
            state_borrow.daemon_switch.unblock_signal(hid);
        }

        if running {
            state_borrow.status_label.set_text("Daemon Active");
            state_borrow.status_dot.remove_css_class("status-dot-stopped");
            state_borrow.status_dot.add_css_class("status-dot-running");
        } else {
            state_borrow.status_label.set_text("Daemon Off");
            state_borrow.status_dot.remove_css_class("status-dot-running");
            state_borrow.status_dot.add_css_class("status-dot-stopped");
        }
        glib::ControlFlow::Continue
    });

    // Stylesheet injection
    let provider = gtk::CssProvider::new();
    provider.load_from_data(
        "headerbar { background: @window_bg_color; border: none; box-shadow: none; }\n\
         .main-window-content, .dialog-content { background-color: @window_bg_color; }\n\
         scrolledwindow, viewport { background-color: transparent !important; background-image: none !important; }\n\
         .status-banner { padding: 12px 16px; background-color: @card_bg_color !important; border-radius: 8px; }\n\
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
         .keycap { padding: 6px 12px; background-color: alpha(currentColor, 0.08); border: 1px solid alpha(currentColor, 0.15); border-radius: 6px; font-weight: bold; font-size: 1.1em; }\n"
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

fn main() {
    let app = gtk::Application::builder()
        .application_id("org.mygestures.gestos")
        .build();

    app.connect_activate(build_ui);
    app.run();
}
