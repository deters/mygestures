use gtk::prelude::*;
use gtk::{cairo, gdk, glib};
use crate::system::is_osd_enabled;
use gtk4 as gtk;
use futures_util::StreamExt;
use std::rc::Rc;
use std::cell::RefCell;
use mygestures::protractor::Point2D;

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

pub fn build_overlay_ui(app: &gtk::Application) {
    let window = gtk::ApplicationWindow::builder()
        .application(app)
        .decorated(false)
        .focusable(false)
        .build();

    // CSS styling for transparency and OSD notification box
    let provider = gtk::CssProvider::new();
    provider.load_from_data(
        "window, window.background, drawingarea { background-color: rgba(0, 0, 0, 0); background: none; }\n\
         .osd-notification {\n\
             background-color: rgba(30, 30, 30, 0.4);\n\
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

                    window_clone.set_visible(true);
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
                        window_clone.set_visible(true);

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
    window.set_visible(true);
}
