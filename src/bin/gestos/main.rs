mod state;
mod actions;
mod daemon;
mod system;
mod ui;

use gtk::prelude::*;
use gtk::{gio, glib};
use gtk4 as gtk;
use std::rc::Rc;
use std::cell::RefCell;
use mygestures::config::Configuration;
use state::AppState;
use ui::main_window::build_ui;
use ui::overlay::build_overlay_ui;

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
    app.run_with_args::<glib::GString>(&[]);
}
