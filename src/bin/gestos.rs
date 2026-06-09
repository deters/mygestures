use std::cell::RefCell;
use std::collections::HashMap;
use std::rc::Rc;
use std::process::Command;
use gtk4 as gtk;
use gtk::prelude::*;
use mygestures::config::{Configuration, Gesture, ActionType, get_default_config_path, parse_points};
use mygestures::protractor::Point2D;

struct AppState {
    config: Configuration,
    main_list: gtk::ListBox,
    search_entry: gtk::SearchEntry,
    status_label: gtk::Label,
    status_dot: gtk::Image,
    daemon_switch: gtk::Switch,
    window: gtk::ApplicationWindow,
    switch_handler_id: Option<glib::SignalHandlerId>,
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

fn start_daemon() {
    if is_daemon_running() {
        return;
    }
    // Try local binary first, then path
    let cmd = if std::path::Path::new("./mygestures").exists() {
        "./mygestures"
    } else {
        "mygestures"
    };
    let _ = Command::new("sh")
        .arg("-c")
        .arg(format!("{} &", cmd))
        .spawn();
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
        start_daemon();
    }
}

fn get_action_category_icon(action: &ActionType) -> (&'static str, &'static str) {
    match action {
        ActionType::Execute(_) => ("utilities-terminal-symbolic", "icon-bg-purple"),
        ActionType::Keypress(_) => ("preferences-desktop-keyboard-shortcuts-symbolic", "icon-bg-orange"),
        ActionType::WorkspaceLeft | ActionType::WorkspaceRight | ActionType::WorkspaceUp | ActionType::WorkspaceDown => {
            ("go-next-symbolic", "icon-bg-blue")
        }
        ActionType::VolumeUp | ActionType::VolumeDown | ActionType::VolumeMute => ("audio-volume-high-symbolic", "icon-bg-green"),
        ActionType::MediaPlay | ActionType::MediaNext | ActionType::MediaPrev => ("media-playback-start-symbolic", "icon-bg-green"),
        _ => ("system-run-symbolic", "icon-bg-blue"),
    }
}

fn draw_gesture_path(cr: &cairo::Context, points: &[Point2D], width: f64, height: f64, draw_bg: bool) {
    if draw_bg {
        // Subtle background
        cr.set_source_rgba(0.95, 0.96, 0.98, 0.5);
        cr.paint().unwrap();
    }

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
    let scale = if max_dim > 1.0 { (width * 0.7) / max_dim } else { 1.0 };

    let cx = min_x + w / 2.0;
    let cy = min_y + h / 2.0;

    cr.save().unwrap();
    cr.translate(width / 2.0, height / 2.0).unwrap();
    cr.scale(scale, scale);
    cr.translate(-cx, -cy).unwrap();

    cr.set_line_width(4.0 / scale);
    cr.set_line_cap(cairo::LineCap::Round);
    cr.set_line_join(cairo::LineJoin::Round);

    // Draw stroke gradient (violet to pink)
    let pat = cairo::LinearGradient::new(min_x, min_y, max_x, max_y);
    pat.add_color_stop_rgba(0.0, 0.49, 0.27, 0.90, 0.9).unwrap();
    pat.add_color_stop_rgba(1.0, 0.98, 0.72, 0.80, 0.9).unwrap();
    cr.set_source(&pat).unwrap();

    cr.move_to(points[0].x, points[0].y);
    for p in &points[1..] {
        cr.line_to(p.x, p.y);
    }
    cr.stroke().unwrap();

    // Draw end indicator
    let end = points.last().unwrap();
    cr.set_source_rgba(0.49, 0.27, 0.90, 1.0).unwrap();
    cr.arc(end.x, end.y, 5.0 / scale, 0.0, 2.0 * std::f64::consts::PI).unwrap();
    cr.fill().unwrap();

    cr.restore().unwrap();
}

fn create_gesture_row(state_rc: &Rc<RefCell<AppState>>, gesture: &Gesture) -> gtk::ListBoxRow {
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
        draw_gesture_path(cr, &pts_clone, width as f64, height as f64, true);
    });
    preview_frame.set_child(Some(&preview_canvas));
    main_hbox.append(&preview_frame);

    // 4. Delete button
    let del_btn = gtk::Button::from_icon_name("user-trash-symbolic");
    del_btn.add_css_class("destructive-action");
    del_btn.set_valign(gtk::Align::Center);
    
    let state_clone = Rc::clone(state_rc);
    let name_clone = gesture.name.clone();
    del_btn.connect_clicked(move |_| {
        let mut state = state_clone.borrow_mut();
        if let Some(pos) = state.config.gestures.iter_mut().position(|g| g.name == name_clone) {
            if state.config.gestures[pos].is_custom {
                state.config.gestures.remove(pos);
            } else {
                state.config.gestures[pos].is_deleted = true;
            }
            let _ = state.config.save_to_file();
            reload_daemon();
            drop(state);
            refresh_gesture_list(&state_clone);
        }
    });
    main_hbox.append(&del_btn);

    row.set_child(Some(&main_hbox));
    row
}

fn refresh_gesture_list(state_rc: &Rc<RefCell<AppState>>) {
    let state = state_rc.borrow();
    
    // Clear list
    while let Some(child) = state.main_list.first_child() {
        state.main_list.remove(&child);
    }

    let filter = state.search_entry.text().to_lowercase();

    for gesture in &state.config.gestures {
        if gesture.is_deleted {
            continue;
        }
        if !filter.is_empty() && !gesture.name.to_lowercase().contains(&filter) {
            continue;
        }
        let row = create_gesture_row(state_rc, gesture);
        state.main_list.append(&row);
    }
}

fn open_gesture_editor(state_rc: &Rc<RefCell<AppState>>, target_gesture: Option<Gesture>) {
    let state = state_rc.borrow();
    let dialog = gtk::Window::new();
    dialog.set_transient_for(Some(&state.window));
    dialog.set_modal(true);
    dialog.set_title(Some(if target_gesture.is_some() { "Edit Gesture" } else { "Add Gesture" }));
    dialog.set_default_size(420, 560);

    let main_box = gtk::Box::new(gtk::Orientation::Vertical, 16);
    main_box.set_margin_start(24);
    main_box.set_margin_end(24);
    main_box.set_margin_top(24);
    main_box.set_margin_bottom(24);
    dialog.set_child(Some(&main_box));

    // Name entry
    let name_label = gtk::Label::new(Some("Gesture Name"));
    name_label.set_halign(gtk::Align::Start);
    name_label.add_css_class("status-label");
    main_box.append(&name_label);

    let name_entry = gtk::Entry::new();
    name_entry.set_placeholder_text(Some("e.g. Back Action"));
    if let Some(ref g) = target_gesture {
        name_entry.set_text(&g.name);
        name_entry.set_sensitive(false); // Can't change name of existing gesture
    }
    main_box.append(&name_entry);

    // Canvas Frame and Drawing area
    let canvas_label = gtk::Label::new(Some("Draw gesture below"));
    canvas_label.set_halign(gtk::Align::Start);
    canvas_label.add_css_class("status-label");
    main_box.append(&canvas_label);

    let canvas_frame = gtk::Frame::new(None);
    canvas_frame.add_css_class("gesture-preview-frame");
    canvas_frame.set_size_request(300, 200);
    canvas_frame.set_vexpand(true);

    let canvas = gtk::DrawingArea::new();
    let recorded_points: Rc<RefCell<Vec<Point2D>>> = Rc::new(RefCell::new(
        target_gesture.as_ref().map(|g| g.points.clone()).unwrap_or_default()
    ));

    let pts_clone = Rc::clone(&recorded_points);
    canvas.set_draw_func(move |_, cr, width, height| {
        let pts = pts_clone.borrow();
        draw_gesture_path(cr, &pts, width as f64, height as f64, true);
    });

    // GestureDrag controller for recording drawing
    let drag = gtk::GestureDrag::new();
    let is_recording = Rc::new(RefCell::new(false));
    
    let canvas_clone = canvas.clone();
    let pts_drag = Rc::clone(&recorded_points);
    let rec_drag = Rc::clone(&is_recording);
    drag.connect_drag_begin(move |_, start_x, start_y| {
        *rec_drag.borrow_mut() = true;
        let mut pts = pts_drag.borrow_mut();
        pts.clear();
        pts.push(Point2D { x: start_x, y: start_y });
        canvas_clone.queue_draw();
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

    let rec_drag3 = Rc::clone(&is_recording);
    drag.connect_drag_end(move |_, _, _| {
        *rec_drag3.borrow_mut() = false;
    });

    canvas.add_controller(drag);
    canvas_frame.set_child(Some(&canvas));
    main_box.append(&canvas_frame);

    // Action config dropdown and text entries
    let action_label = gtk::Label::new(Some("Action Mapping"));
    action_label.set_halign(gtk::Align::Start);
    action_label.add_css_class("status-label");
    main_box.append(&action_label);

    let action_types = vec![
        "Keypress Shortcut",
        "Run Command (Execute)",
        "Workspace Left",
        "Workspace Right",
        "Workspace Up",
        "Workspace Down",
        "Close Window (Kill)",
        "Minimize Window (Iconify)",
        "Volume Up",
        "Volume Down",
        "Volume Mute",
    ];

    let action_dropdown = gtk::DropDown::from_strings(&action_types);
    main_box.append(&action_dropdown);

    let action_details_entry = gtk::Entry::new();
    action_details_entry.set_placeholder_text(Some("e.g. Control_L+Alt_L+t or firefox"));
    main_box.append(&action_details_entry);

    // Setup initial editor values if editing an existing gesture
    if let Some(ref g) = target_gesture {
        if !g.actions.is_empty() {
            match &g.actions[0] {
                ActionType::Keypress(combo) => {
                    action_dropdown.set_selected(0);
                    action_details_entry.set_text(combo);
                }
                ActionType::Execute(cmd) => {
                    action_dropdown.set_selected(1);
                    action_details_entry.set_text(cmd);
                }
                ActionType::WorkspaceLeft => action_dropdown.set_selected(2),
                ActionType::WorkspaceRight => action_dropdown.set_selected(3),
                ActionType::WorkspaceUp => action_dropdown.set_selected(4),
                ActionType::WorkspaceDown => action_dropdown.set_selected(5),
                ActionType::Kill => action_dropdown.set_selected(6),
                ActionType::Iconify => action_dropdown.set_selected(7),
                ActionType::VolumeUp => action_dropdown.set_selected(8),
                ActionType::VolumeDown => action_dropdown.set_selected(9),
                ActionType::VolumeMute => action_dropdown.set_selected(10),
                _ => {}
            }
        }
    }

    // Connect dropdown selection to details field visibility
    let entry_clone = action_details_entry.clone();
    action_dropdown.connect_selected_notify(move |dd| {
        let sel = dd.selected();
        entry_clone.set_visible(sel == 0 || sel == 1);
    });
    // Trigger initial notify
    let sel = action_dropdown.selected();
    action_details_entry.set_visible(sel == 0 || sel == 1);

    // Save and Cancel buttons
    let button_box = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    button_box.set_halign(gtk::Align::End);

    let cancel_btn = gtk::Button::with_label("Cancel");
    let dialog_clone = dialog.clone();
    cancel_btn.connect_clicked(move |_| {
        dialog_clone.destroy();
    });
    button_box.append(&cancel_btn);

    let save_btn = gtk::Button::with_label("Save");
    save_btn.add_css_class("suggested-action");
    
    let state_clone = Rc::clone(state_rc);
    let is_edit = target_gesture.is_some();
    let dialog_clone2 = dialog.clone();
    
    save_btn.connect_clicked(move |_| {
        let name = name_entry.text().to_string();
        if name.trim().is_empty() {
            return;
        }

        let pts = recorded_points.borrow();
        if pts.len() < 2 {
            return;
        }

        // Convert recorded points back to string coords representation
        let raw_movement = pts.iter()
            .map(|p| format!("{},{}", p.x as i32, p.y as i32))
            .collect::<Vec<_>>()
            .join(" ");

        let sel_idx = action_dropdown.selected();
        let detail = action_details_entry.text().to_string();

        let action = match sel_idx {
            0 => ActionType::Keypress(detail),
            1 => ActionType::Execute(detail),
            2 => ActionType::WorkspaceLeft,
            3 => ActionType::WorkspaceRight,
            4 => ActionType::WorkspaceUp,
            5 => ActionType::WorkspaceDown,
            6 => ActionType::Kill,
            7 => ActionType::Iconify,
            8 => ActionType::VolumeUp,
            9 => ActionType::VolumeDown,
            _ => ActionType::VolumeMute,
        };

        let mut state = state_clone.borrow_mut();
        if is_edit {
            if let Some(existing) = state.config.gestures.iter_mut().find(|g| g.name == name) {
                existing.raw_movement = raw_movement;
                existing.points = pts.clone();
                existing.actions = vec![action];
                existing.is_modified = true;
                existing.is_deleted = false;
            }
        } else {
            // Check if name conflict
            if state.config.gestures.iter().any(|g| g.name == name) {
                return;
            }
            state.config.gestures.push(Gesture {
                name: name.clone(),
                raw_movement,
                points: pts.clone(),
                actions: vec![action],
                is_custom: true,
                is_modified: false,
                is_deleted: false,
            });
        }

        let _ = state.config.save_to_file();
        reload_daemon();
        drop(state);

        refresh_gesture_list(&state_clone);
        dialog_clone2.destroy();
    });
    
    button_box.append(&save_btn);
    main_box.append(&button_box);

    dialog.present();
}

fn build_ui(app: &gtk::Application) {
    let window = gtk::ApplicationWindow::new(app);
    window.set_title(Some("Gestos"));
    window.set_default_size(650, 700);

    let header = gtk::HeaderBar::new();
    window.set_titlebar(Some(&header));

    // 1. Add gesture button
    let add_gest_btn = gtk::Button::from_icon_name("list-add-symbolic");
    add_gest_btn.set_tooltip_text(Some("Add Gesture"));
    header.pack_end(&add_gest_btn);

    // 2. Status controller box
    let ctrl_box = gtk::Box::new(gtk::Orientation::Horizontal, 8);
    ctrl_box.set_valign(gtk::Align::Center);
    ctrl_box.set_margin_start(12);
    ctrl_box.set_margin_end(12);

    let status_dot = gtk::Image::from_icon_name("media-record-symbolic");
    status_dot.add_css_class("status-dot-stopped");
    ctrl_box.append(&status_dot);

    let status_label = gtk::Label::new(Some("Daemon Off"));
    status_label.add_css_class("status-label");
    ctrl_box.append(&status_label);

    let daemon_switch = gtk::Switch::new();
    ctrl_box.append(&daemon_switch);
    header.pack_end(&ctrl_box);

    // 3. About button
    let about_btn = gtk::Button::from_icon_name("help-about-symbolic");
    header.pack_end(&about_btn);

    // Content VBox
    let content_vbox = gtk::Box::new(gtk::Orientation::Vertical, 0);
    window.set_child(Some(&content_vbox));

    let content_header = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    content_header.set_margin_top(24);
    content_header.set_margin_bottom(24);
    content_header.set_margin_start(24);
    content_header.set_margin_end(24);
    content_vbox.append(&content_header);

    let gestos_title = gtk::Label::new(Some("Gestures"));
    gestos_title.add_css_class("context-title");
    gestos_title.set_hexpand(true);
    gestos_title.set_halign(gtk::Align::Start);
    content_header.append(&gestos_title);

    let search_entry = gtk::SearchEntry::new();
    content_header.append(&search_entry);

    let scrolled = gtk::ScrolledWindow::new();
    scrolled.set_vexpand(true);
    content_vbox.append(&scrolled);

    let main_list = gtk::ListBox::new();
    main_list.set_margin_start(24);
    main_list.set_margin_end(24);
    main_list.add_css_class("boxed-list");
    main_list.set_selection_mode(gtk::SelectionMode::None);
    scrolled.set_child(Some(&main_list));

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
    }));

    // Refresh initially
    refresh_gesture_list(&state);

    // Connect search entry
    let state_clone = Rc::clone(&state);
    state.borrow().search_entry.connect_search_changed(move |_| {
        refresh_gesture_list(&state_clone);
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
        
        // Find matched gesture matching filter
        let filter = state_borrow.search_entry.text().to_lowercase();
        let visible_gestures: Vec<Gesture> = state_borrow.config.gestures.iter()
            .filter(|g| !g.is_deleted)
            .filter(|g| filter.is_empty() || g.name.to_lowercase().contains(&filter))
            .cloned()
            .collect();

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
        dialog.set_version(Some("4.0.0"));
        dialog.set_comments(Some("A modern mouse gestures editor for Wayland desktop environments."));
        dialog.set_authors(&["Lucas Augusto Deters <lucasdeters@gmail.com>"]);
        dialog.present();
    });

    // Connect daemon switch state controller
    let state_clone4 = Rc::clone(&state);
    let handler_id = state.borrow().daemon_switch.connect_state_set(move |_, state| {
        if state {
            start_daemon();
        } else {
            stop_daemon();
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
    provider.load_from_string(
        "window { background-color: @theme_bg_color; }\n\
         .context-title { font-size: 2.2em; font-weight: 800; letter-spacing: -0.5px; margin-bottom: 4px; }\n\
         entry { border-radius: 10px; padding: 8px 12px; border: 1px solid alpha(currentColor, 0.15); background: @view_bg_color; transition: all 0.2s ease; }\n\
         entry:focus { border-color: #6366f1; }\n\
         dropdown { border-radius: 10px; padding: 6px 12px; border: 1px solid alpha(currentColor, 0.15); background: @view_bg_color; transition: all 0.2s ease; }\n\
         dropdown:focus { border-color: #6366f1; }\n\
         scrolledwindow { border: none; }\n\
         .boxed-list { background: @view_bg_color; border: 1px solid alpha(currentColor, 0.08); border-radius: 16px; }\n\
         .gesture-row { background: transparent; border-bottom: 1px solid alpha(currentColor, 0.06); }\n\
         .gesture-row:last-child { border-bottom: none; }\n\
         .gesture-row:hover { background: alpha(currentColor, 0.03); }\n\
         .icon-holder { border-radius: 10px; padding: 6px; }\n\
         .icon-bg-purple { background: rgba(139, 92, 246, 0.12); color: #8b5cf6; }\n\
         .icon-bg-orange { background: rgba(249, 115, 22, 0.12); color: #f97316; }\n\
         .icon-bg-blue { background: rgba(59, 130, 246, 0.12); color: #3b82f6; }\n\
         .icon-bg-green { background: rgba(16, 185, 129, 0.12); color: #10b981; }\n\
         .gesture-preview-frame { border: 1px solid alpha(currentColor, 0.08); border-radius: 8px; overflow: hidden; background: transparent; }\n\
         .action-label { font-size: 0.82em; opacity: 0.6; }\n\
         headerbar { background: @theme_bg_color; border-bottom: 1px solid alpha(currentColor, 0.06); padding: 8px 12px; }\n\
         button { border-radius: 8px; padding: 6px 12px; }\n\
         .suggested-action { background: linear-gradient(135deg, #6366f1, #4f46e5) !important; color: white !important; border: none !important; font-weight: 600 !important; }\n\
         .destructive-action { color: alpha(currentColor, 0.4); }\n\
         .destructive-action:hover { color: #ef4444 !important; background: rgba(239, 68, 68, 0.08) !important; }\n\
         .status-dot-running { color: #10b981 !important; }\n\
         .status-dot-stopped { color: #6b7280 !important; opacity: 0.6; }\n\
         .status-label { font-size: 0.85em; font-weight: 600; }\n"
    );

    if let Some(display) = gdk4::Display::default() {
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
