use gtk::prelude::*;
use gtk::{gdk, glib};
use crate::ui::components::{get_action_human_readable, show_error_dialog};
use gtk4 as gtk;
use std::rc::Rc;
use std::cell::RefCell;
use mygestures::config::{Configuration, Gesture};
use crate::state::AppState;
use crate::ui::editor::open_gesture_editor;
use crate::ui::settings::open_settings_window;
use crate::ui::components::create_gesture_row;
use crate::daemon::{start_daemon, stop_daemon, is_daemon_running};

pub fn get_visible_gestures(state: &AppState) -> Vec<Gesture> {
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
pub fn refresh_gesture_list(state_rc: &Rc<RefCell<AppState>>, select_name: Option<&str>) {
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
pub fn build_ui(app: &gtk::Application) {
    let window = gtk::ApplicationWindow::new(app);
    window.set_title(Some("Gestos"));
    window.set_default_size(650, 700);

    let header = gtk::HeaderBar::new();
    let title_label = gtk::Label::new(Some("Gestures"));
    title_label.add_css_class("title");
    header.set_title_widget(Some(&title_label));
    window.set_titlebar(Some(&header));

    // 1. Add gesture button (Primary action on the left)
    let add_gest_btn = gtk::Button::with_mnemonic("_New Gesture");
    add_gest_btn.add_css_class("suggested-action");
    add_gest_btn.set_tooltip_text(Some("Add Gesture"));
    header.pack_start(&add_gest_btn);

    // 2. Primary Menu (Hamburger button)
    let menu_btn = gtk::MenuButton::new();
    menu_btn.set_icon_name("open-menu-symbolic");
    menu_btn.set_tooltip_text(Some("Main Menu"));
    
    let popover = gtk::Popover::new();
    let menu_vbox = gtk::Box::new(gtk::Orientation::Vertical, 4);
    menu_vbox.set_margin_start(6);
    menu_vbox.set_margin_end(6);
    menu_vbox.set_margin_top(6);
    menu_vbox.set_margin_bottom(6);
    
    // Daemon Row
    let daemon_row = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    daemon_row.set_margin_start(6);
    daemon_row.set_margin_end(6);
    daemon_row.set_margin_top(6);
    daemon_row.set_margin_bottom(6);
    let daemon_lbl = gtk::Label::new(Some("Background Daemon"));
    daemon_lbl.set_hexpand(true);
    daemon_lbl.set_halign(gtk::Align::Start);
    daemon_row.append(&daemon_lbl);
    
    let daemon_switch = gtk::Switch::new();
    daemon_switch.set_valign(gtk::Align::Center);
    daemon_row.append(&daemon_switch);
    menu_vbox.append(&daemon_row);
    
    menu_vbox.append(&gtk::Separator::new(gtk::Orientation::Horizontal));
    
    // Settings Button
    let settings_btn = gtk::Button::with_label("Preferences");
    settings_btn.add_css_class("flat");
    settings_btn.set_halign(gtk::Align::Fill);
    menu_vbox.append(&settings_btn);
    
    // About Button
    let about_btn = gtk::Button::with_label("About Gestos");
    about_btn.add_css_class("flat");
    about_btn.set_halign(gtk::Align::Fill);
    menu_vbox.append(&about_btn);
    
    popover.set_child(Some(&menu_vbox));
    menu_btn.set_popover(Some(&popover));
    
    header.pack_end(&menu_btn);

    // 3. Search button
    let search_toggle_btn = gtk::ToggleButton::new();
    search_toggle_btn.set_icon_name("system-search-symbolic");
    search_toggle_btn.set_tooltip_text(Some("Search"));
    header.pack_end(&search_toggle_btn);

    // Content VBox
    let content_vbox = gtk::Box::new(gtk::Orientation::Vertical, 0);
    content_vbox.add_css_class("main-window-content");
    window.set_child(Some(&content_vbox));

    // 5. Search Bar & Entry
    let search_bar = gtk::SearchBar::new();
    search_bar.set_key_capture_widget(Some(&window));
    
    let search_entry = gtk::SearchEntry::new();
    search_entry.set_halign(gtk::Align::Center);
    search_entry.set_width_request(400);
    search_entry.set_placeholder_text(Some("Search gestures..."));
    
    search_bar.set_child(Some(&search_entry));
    
    // Bind toggle button to search bar visibility
    search_bar
        .bind_property("search-mode-enabled", &search_toggle_btn, "active")
        .bidirectional()
        .build();
    
    content_vbox.append(&search_bar);

    let scrolled = gtk::ScrolledWindow::new();
    scrolled.set_vexpand(true);
    content_vbox.append(&scrolled);

    let list_container = gtk::Box::new(gtk::Orientation::Vertical, 0);

    let main_list = gtk::ListBox::new();
    main_list.set_halign(gtk::Align::Center);
    main_list.set_width_request(500);
    main_list.set_margin_top(24);
    main_list.set_margin_bottom(24);
    main_list.set_margin_start(24);
    main_list.set_margin_end(24);
    main_list.add_css_class("boxed-list");
    main_list.set_selection_mode(gtk::SelectionMode::None);
    list_container.append(&main_list);

    let empty_state_box = gtk::Box::new(gtk::Orientation::Vertical, 16);
    empty_state_box.set_valign(gtk::Align::Center);
    empty_state_box.set_halign(gtk::Align::Center);
    empty_state_box.set_margin_top(72);
    empty_state_box.set_margin_bottom(72);

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

    // Global Keyboard Shortcuts
    let key_controller = gtk::EventControllerKey::new();
    let state_shortcut_clone = Rc::clone(&state);
    let window_shortcut_clone = state.borrow().window.clone();
    key_controller.connect_key_pressed(move |_, keyval, _, state_modifier| {
        let is_ctrl = state_modifier.contains(gtk::gdk::ModifierType::CONTROL_MASK) 
                   || state_modifier.contains(gtk::gdk::ModifierType::META_MASK); // meta/cmd on mac
        
        if is_ctrl && keyval == gtk::gdk::Key::n {
            open_gesture_editor(&state_shortcut_clone, None);
            return glib::Propagation::Stop;
        }
        if is_ctrl && keyval == gtk::gdk::Key::comma {
            open_settings_window(&state_shortcut_clone);
            return glib::Propagation::Stop;
        }
        if is_ctrl && keyval == gtk::gdk::Key::q {
            window_shortcut_clone.destroy();
            return glib::Propagation::Stop;
        }
        glib::Propagation::Proceed
    });
    state.borrow().window.add_controller(key_controller);

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
