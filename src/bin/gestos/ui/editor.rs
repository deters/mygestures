use gtk::prelude::*;
use gtk::{gio, glib};
use mygestures::protractor::match_gesture;
use gtk4 as gtk;
use std::rc::Rc;
use std::cell::RefCell;
use mygestures::config::{ActionType, Gesture};
use mygestures::config::generate_unique_id;
use crate::state::{AppState, EditorActionOption, CATEGORY_NAMES};
use crate::actions::{fetch_gnome_action_options, fetch_kde_action_options, get_static_action_options, action_matches};
use crate::ui::shortcut_recorder::open_shortcut_recorder;
use crate::ui::components::{draw_gesture_path, get_action_category_icon};
use crate::daemon::reload_daemon;
use crate::ui::main_window::refresh_gesture_list;
use mygestures::protractor::Point2D;

pub fn get_default_gesture_name(opt: &EditorActionOption, detail: &str) -> String {
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
pub fn open_gesture_editor(state_rc: &Rc<RefCell<AppState>>, target_gesture: Option<Gesture>) {
    let state = state_rc.borrow();
    let dialog = gtk::Window::new();
    dialog.set_transient_for(Some(&state.window));
    dialog.set_modal(true);
    let title_text = if target_gesture.is_some() { "Edit Gesture" } else { "Add Gesture" };
    dialog.set_title(Some(title_text));
    dialog.set_default_size(700, 500);

    let esc_controller = gtk::EventControllerKey::new();
    let dialog_esc = dialog.clone();
    esc_controller.connect_key_pressed(move |_, keyval, _, _| {
        if keyval == gtk::gdk::Key::Escape {
            dialog_esc.destroy();
            glib::Propagation::Stop
        } else {
            glib::Propagation::Proceed
        }
    });
    dialog.add_controller(esc_controller);

    let dialog_header = gtk::HeaderBar::new();
    dialog_header.set_show_title_buttons(false);
    let dialog_title = gtk::Label::new(Some(title_text));
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



    // Row 2: Action Selection
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

    let action_select_btn = gtk::MenuButton::new();
    action_select_btn.set_halign(gtk::Align::End);
    action_select_btn.set_size_request(240, -1);
    
    let btn_content = gtk::Box::new(gtk::Orientation::Horizontal, 8);
    let action_select_icon = gtk::Image::new();
    let action_select_label = gtk::Label::new(Some("Select Action..."));
    action_select_label.set_ellipsize(gtk::pango::EllipsizeMode::End);
    action_select_label.set_hexpand(true);
    action_select_label.set_halign(gtk::Align::Start);
    
    let arrow_icon = gtk::Image::from_icon_name("pan-down-symbolic");
    
    btn_content.append(&action_select_icon);
    btn_content.append(&action_select_label);
    btn_content.append(&arrow_icon);
    
    action_select_btn.set_child(Some(&btn_content));
    action_row.append(&action_select_btn);

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

    let selected_action: Rc<RefCell<Option<EditorActionOption>>> = Rc::new(RefCell::new(None));

    let action_popover = gtk::Popover::new();
    action_popover.set_autohide(true);
    action_popover.set_position(gtk::PositionType::Bottom);

    let popover_vbox = gtk::Box::new(gtk::Orientation::Vertical, 6);
    popover_vbox.set_margin_top(6);
    popover_vbox.set_margin_bottom(6);
    popover_vbox.set_margin_start(6);
    popover_vbox.set_margin_end(6);

    let search_entry = gtk::SearchEntry::new();
    search_entry.set_hexpand(true);
    search_entry.set_placeholder_text(Some("Search actions..."));
    popover_vbox.append(&search_entry);

    let scrolled_window = gtk::ScrolledWindow::new();
    scrolled_window.set_policy(gtk::PolicyType::Never, gtk::PolicyType::Automatic);
    scrolled_window.set_min_content_height(350);
    scrolled_window.set_min_content_width(320);
    popover_vbox.append(&scrolled_window);

    let list_store = gio::ListStore::new::<glib::BoxedAnyObject>();

    let mut all_options = get_static_action_options();
    all_options.extend(fetch_gnome_action_options());
    all_options.extend(fetch_kde_action_options());
    all_options.sort_by(|a, b| {
        match a.category.cmp(&b.category) {
            std::cmp::Ordering::Equal => a.name.to_lowercase().cmp(&b.name.to_lowercase()),
            other => other,
        }
    });

    for opt in &all_options {
        list_store.append(&glib::BoxedAnyObject::new(opt.clone()));
    }

    let filter_text = Rc::new(RefCell::new(String::new()));
    let custom_filter = gtk::CustomFilter::new({
        let filter_text = Rc::clone(&filter_text);
        move |item| {
            let query = filter_text.borrow().to_lowercase();
            if query.is_empty() { return true; }
            let boxed = item.downcast_ref::<glib::BoxedAnyObject>().unwrap();
            let opt = boxed.borrow::<EditorActionOption>();
            let cat_name = CATEGORY_NAMES.get(opt.category).unwrap_or(&"").to_lowercase();
            let act_name = opt.name.to_lowercase();
            
            // Allow matching multiple words in any order across category and action names
            query.split_whitespace().all(|token| {
                cat_name.contains(token) || act_name.contains(token)
            })
        }
    });

    let search_entry_focus = search_entry.clone();
    action_popover.connect_map(move |_| {
        search_entry_focus.grab_focus();
    });

    let filter_model = gtk::FilterListModel::new(Some(list_store.clone()), Some(custom_filter.clone()));
    let selection_model = gtk::SingleSelection::new(Some(filter_model.clone()));
    selection_model.set_autoselect(false); // Don't auto select

    let factory = gtk::SignalListItemFactory::new();
    factory.connect_setup(|_, list_item| {
        let box_ = gtk::Box::new(gtk::Orientation::Horizontal, 8);
        box_.set_margin_top(6);
        box_.set_margin_bottom(6);
        box_.set_margin_start(6);
        box_.set_margin_end(6);

        let img = gtk::Image::new();
        img.set_icon_size(gtk::IconSize::Large);
        img.set_valign(gtk::Align::Center);

        let text_vbox = gtk::Box::new(gtk::Orientation::Vertical, 2);
        text_vbox.set_valign(gtk::Align::Center);

        let cat_label = gtk::Label::new(None);
        cat_label.set_halign(gtk::Align::Start);
        cat_label.add_css_class("action-label"); // Dimmed subtitle

        let act_label = gtk::Label::new(None);
        act_label.set_halign(gtk::Align::Start);
        act_label.add_css_class("status-label");
        act_label.set_wrap(true);
        act_label.set_max_width_chars(30);

        text_vbox.append(&cat_label);
        text_vbox.append(&act_label);

        box_.append(&img);
        box_.append(&text_vbox);
        list_item.set_child(Some(&box_));
    });

    factory.connect_bind(|_, list_item| {
        let item = list_item.item().unwrap();
        let boxed = item.downcast_ref::<glib::BoxedAnyObject>().unwrap();
        let opt = boxed.borrow::<EditorActionOption>();

        let child = list_item.child().unwrap();
        let box_ = child.downcast::<gtk::Box>().unwrap();
        let img = box_.first_child().unwrap().downcast::<gtk::Image>().unwrap();
        let text_vbox = img.next_sibling().unwrap().downcast::<gtk::Box>().unwrap();
        let cat_label = text_vbox.first_child().unwrap().downcast::<gtk::Label>().unwrap();
        let act_label = cat_label.next_sibling().unwrap().downcast::<gtk::Label>().unwrap();

        let (icon_name, _) = get_action_category_icon(&opt.action_type);
        img.set_icon_name(Some(icon_name));
        cat_label.set_text(CATEGORY_NAMES.get(opt.category).unwrap_or(&"Unknown"));
        act_label.set_text(&opt.name);
    });

    let list_view = gtk::ListView::new(Some(selection_model.clone()), Some(factory.clone()));
    list_view.add_css_class("boxed-list");
    let popover_activate_clone = action_popover.clone();
    list_view.connect_activate(move |_, _| {
        popover_activate_clone.popdown();
    });
    scrolled_window.set_child(Some(&list_view));
    action_popover.set_child(Some(&popover_vbox));
    action_select_btn.set_popover(Some(&action_popover));

    let filter_clone = custom_filter.clone();
    let text_clone = Rc::clone(&filter_text);
    search_entry.connect_search_changed(move |entry| {
        *text_clone.borrow_mut() = entry.text().to_string();
        filter_clone.changed(gtk::FilterChange::Different);
    });

    // Helper to dynamically update the gesture name if not customized
    let update_default_name = Rc::new({
        let name_entry = name_entry.clone();
        let action_details_entry = action_details_entry.clone();
        let selected_action = Rc::clone(&selected_action);
        let is_name_customized = Rc::clone(&is_name_customized);
        let is_updating_programmatically = Rc::clone(&is_updating_programmatically);

        move || {
            if *is_name_customized.borrow() {
                return;
            }
            if let Some(opt) = selected_action.borrow().as_ref() {
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

    let btn_label_clone = action_select_label.clone();
    let btn_icon_clone = action_select_icon.clone();
    let row_clone = action_details_row.clone();
    let action_icon_act = action_icon.clone();
    let action_details_icon_act = action_details_icon.clone();
    let entry_clone = action_details_entry.clone();
    let label_clone = action_details_label.clone();
    let sdb_clone_act = shortcut_display_box.clone();
    let record_btn_clone_act = record_btn.clone();
    let udn_clone = Rc::clone(&update_default_name);
    let selected_action_clone = Rc::clone(&selected_action);

    selection_model.connect_selection_changed(move |sel, _, _| {
        let item = match sel.selected_item() {
            Some(i) => i,
            None => return,
        };
        let boxed = item.downcast_ref::<glib::BoxedAnyObject>().unwrap();
        let opt = boxed.borrow::<EditorActionOption>().clone();
        
        btn_label_clone.set_text(&opt.name);
        let (icon_name, _) = get_action_category_icon(&opt.action_type);
        btn_icon_clone.set_icon_name(Some(icon_name));
        *selected_action_clone.borrow_mut() = Some(opt.clone());

        let show_entry = match &opt.action_type {
            ActionType::Keypress(_) => true,
            ActionType::Execute(_) if opt.category == 7 => true,
            ActionType::Click(_) => true,
            _ => false,
        };
        row_clone.set_visible(show_entry);

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
        entry_clone.set_visible(!is_keypress);
        sdb_clone_act.set_visible(is_keypress);
        record_btn_clone_act.set_visible(is_keypress);

        match &opt.action_type {
            ActionType::Keypress(_) => label_clone.set_text("Keys to Send"),
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
        
        udn_clone();
    });

    let udn_clone4 = Rc::clone(&update_default_name);
    let usd_clone_change = Rc::clone(&update_shortcut_display);
    action_details_entry.connect_changed(move |_| {
        udn_clone4();
        usd_clone_change();
    });

    // Find initial matching option
    let mut initial_opt = None;
    if let Some(ref g) = target_gesture {
        if !g.actions.is_empty() {
            let a = &g.actions[0];
            if let Some(pos) = all_options.iter().position(|opt| action_matches(a, opt)) {
                selection_model.set_selected(pos as u32);
                initial_opt = Some(all_options[pos].clone());
                
                match a {
                    ActionType::Keypress(combo) => action_details_entry.set_text(combo),
                    ActionType::Execute(cmd) => {
                        let opt = &all_options[pos];
                        if opt.category == 7 {
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
    } else {
        selection_model.set_selected(0);
        initial_opt = Some(all_options[0].clone());
        let udn_init = Rc::clone(&update_default_name);
        udn_init();
    }

    // Explicitly update UI visibility to prevent "text input instead of key presentation" bug
    if let Some(opt) = initial_opt {
        *selected_action.borrow_mut() = Some(opt.clone());
        let is_keypress = matches!(&opt.action_type, ActionType::Keypress(_));
        action_details_entry.set_visible(!is_keypress);
        shortcut_display_box.set_visible(is_keypress);
        record_btn.set_visible(is_keypress);

        match &opt.action_type {
            ActionType::Keypress(_) => action_details_label.set_text("Keys to Send"),
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
        action_select_label.set_text(&opt.name);
        let (icon_name, _) = get_action_category_icon(&opt.action_type);
        action_select_icon.set_icon_name(Some(icon_name));
    }

    let usd_init = Rc::clone(&update_shortcut_display);
    usd_init(); // Set initial keycaps if keys exist

    // Save and Cancel buttons
    let cancel_btn = gtk::Button::with_mnemonic("_Cancel");
    let dialog_clone = dialog.clone();
    cancel_btn.connect_clicked(move |_| {
        dialog_clone.destroy();
    });
    dialog_header.pack_start(&cancel_btn);

    let save_btn = gtk::Button::with_mnemonic("_Save");
    save_btn.add_css_class("suggested-action");
    save_btn.set_receives_default(true);

    let state_clone = Rc::clone(state_rc);
    let is_edit = target_gesture.is_some();
    let target_id = target_gesture.as_ref().map(|g| g.id.clone());
    let dialog_clone2 = dialog.clone();
    let selected_action_save = Rc::clone(&selected_action);

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

        let opt = if let Some(opt) = selected_action_save.borrow().as_ref() {
            opt.clone()
        } else {
            println!("Gesture save failed: No action selected.");
            return;
        };
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
        let delete_btn = gtk::Button::with_mnemonic("_Delete Gesture");
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

    dialog.set_default_widget(Some(&save_btn));
    dialog.present();
}
