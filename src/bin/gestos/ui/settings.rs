use gtk::prelude::*;
use gtk::{gio, glib};
use mygestures::config::Configuration;
use crate::ui::main_window::refresh_gesture_list;
use crate::ui::components::{show_error_dialog, show_confirm_dialog};
use gtk4 as gtk;
use std::rc::Rc;
use std::cell::RefCell;
use crate::state::AppState;
use crate::daemon::reload_daemon;
use crate::system::{is_osd_enabled, set_osd_enabled, get_autostart_file_path, get_overlay_autostart_file_path, set_autostart_enabled, set_overlay_enabled};

pub fn open_settings_window(state_rc: &Rc<RefCell<AppState>>) {
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
    overlay_row.set_visible(false); // HIDE FOR NOW
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
    osd_row.set_visible(false); // HIDE FOR NOW
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
