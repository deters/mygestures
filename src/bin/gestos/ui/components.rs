use gtk::prelude::*;
use gtk::{cairo, gdk, glib};
use gtk4 as gtk;
use std::rc::Rc;
use std::cell::RefCell;
use mygestures::config::{ActionType, Gesture};
use mygestures::protractor::Point2D;
use crate::state::AppState;
use crate::ui::main_window::refresh_gesture_list;

pub fn show_error_dialog<W: IsA<gtk::Window>>(parent: &W, message: &str) {
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
pub fn get_action_category_icon(action: &ActionType) -> (&'static str, &'static str) {
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
pub fn draw_gesture_path(
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
pub fn get_action_human_readable(action: &ActionType) -> String {
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
pub fn create_gesture_row(gesture: &Gesture, state_rc: &Rc<RefCell<AppState>>) -> gtk::ListBoxRow {
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
pub fn show_confirm_dialog<W: IsA<gtk::Window>, F: FnOnce() + 'static>(
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
