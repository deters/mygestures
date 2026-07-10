use gtk::prelude::*;
use gtk::{gdk, glib};
use gtk4 as gtk;
use std::rc::Rc;
use std::cell::RefCell;

pub fn open_shortcut_recorder(
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
