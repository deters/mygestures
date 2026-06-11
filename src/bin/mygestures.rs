use std::fs;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, Ordering};
use mygestures::config::{Configuration, ActionType};
use mygestures::wayland::WaylandContext;
use mygestures::ipc::DaemonIpc;
use zbus::interface;

static RELOAD_REQUESTED: AtomicBool = AtomicBool::new(false);
static IS_MANAGER: AtomicBool = AtomicBool::new(false);
static RELOAD_MANAGER: AtomicBool = AtomicBool::new(false);
static CHILD_PIDS: once_cell::sync::Lazy<std::sync::Mutex<Vec<u32>>> =
    once_cell::sync::Lazy::new(|| std::sync::Mutex::new(Vec::new()));

extern "C" fn sigusr1_handler(_sig: std::os::raw::c_int) {}

fn register_sigusr1() {
    unsafe {
        let sig_action = nix::sys::signal::SigAction::new(
            nix::sys::signal::SigHandler::Handler(sigusr1_handler),
            nix::sys::signal::SaFlags::empty(),
            nix::sys::signal::SigSet::empty(),
        );
        let _ = nix::sys::signal::sigaction(nix::sys::signal::Signal::SIGUSR1, &sig_action);
    }
}

fn kill_children() {
    let pids = CHILD_PIDS.lock().unwrap();
    for &pid in pids.iter() {
        let _ = nix::sys::signal::kill(
            nix::unistd::Pid::from_raw(pid as i32),
            nix::sys::signal::Signal::SIGTERM,
        );
    }
}

struct DaemonDbus;

#[interface(name = "org.mygestures.Daemon")]
impl DaemonDbus {
    fn reload(&self) -> Result<(), zbus::fdo::Error> {
        if IS_MANAGER.load(Ordering::Relaxed) {
            let path = mygestures::config::get_default_config_path();
            if mygestures::config::Configuration::load_from_file(&path).is_none() {
                return Err(zbus::fdo::Error::Failed("Invalid configuration file format".to_string()));
            }
            RELOAD_MANAGER.store(true, Ordering::Relaxed);
            kill_children();
        } else {
            let path = mygestures::config::get_default_config_path();
            if mygestures::config::Configuration::load_from_file(&path).is_none() {
                return Err(zbus::fdo::Error::Failed("Invalid configuration file format".to_string()));
            }
            RELOAD_REQUESTED.store(true, Ordering::Relaxed);
            let _ = nix::sys::signal::kill(
                nix::unistd::Pid::this(),
                nix::sys::signal::Signal::SIGUSR1,
            );
        }
        Ok(())
    }

    fn stop(&self) {
        if IS_MANAGER.load(Ordering::Relaxed) {
            kill_children();
        }
        std::process::exit(0);
    }
}

fn find_mouse_device() -> Option<PathBuf> {
    let input_dir = Path::new("/dev/input/by-path");
    if !input_dir.exists() {
        return None;
    }
    let mut fallback = None;
    if let Ok(entries) = fs::read_dir(input_dir) {
        for entry in entries.flatten() {
            let name = entry.file_name().to_string_lossy().into_owned();
            if name.contains("event-mouse") {
                return Some(input_dir.join(name));
            }
            if name.contains("-mouse") && fallback.is_none() {
                fallback = Some(input_dir.join(name));
            }
        }
    }
    fallback
}

fn check_permissions(device: &str) -> bool {
    let mut uinput_ok = false;
    if let Ok(file) = std::fs::OpenOptions::new().write(true).open("/dev/uinput") {
        uinput_ok = true;
        drop(file);
    } else if let Ok(file) = std::fs::OpenOptions::new().write(true).open("/dev/misc/uinput") {
        uinput_ok = true;
        drop(file);
    }

    let uinput_exists = std::path::Path::new("/dev/uinput").exists() || std::path::Path::new("/dev/misc/uinput").exists();
    let dev_ok = std::fs::OpenOptions::new().read(true).open(device).is_ok();

    if !uinput_ok || !dev_ok {
        eprintln!("\n=========================================================================");
        eprintln!("ERROR: Missing permissions or kernel module to run mygestures.");
        if !dev_ok {
            eprintln!(" - Cannot read mouse input device '{}'.", device);
        }
        if !uinput_ok {
            if !uinput_exists {
                eprintln!(" - /dev/uinput virtual device creator does not exist. The uinput kernel module is likely not loaded.");
            } else {
                eprintln!(" - Cannot write to /dev/uinput virtual device creator.");
            }
        }

        if !uinput_exists {
            eprintln!("\nTo resolve the missing kernel module:\n");
            eprintln!("1. Load the uinput kernel module:");
            eprintln!("   sudo modprobe uinput");
            eprintln!("   (To load it automatically on boot, add 'uinput' to /etc/modules)\n");
            eprintln!("=========================================================================\n");
            return false;
        }

        eprintln!("\nTo resolve permissions, choose one of the following setups:\n");
        eprintln!("OPTION A: SECURE SETUP (Recommended - Only mygestures has access)");
        eprintln!("  1. Install the mygestures udev rules (restricts access to the 'input' group):");
        eprintln!("     sudo cp 99-mygestures.rules /etc/udev/rules.d/");
        eprintln!("     sudo udevadm control --reload-rules && sudo udevadm trigger");
        eprintln!("  2. Configure the mygestures binary to run as Set-Group-ID (SGID) to 'input':");
        eprintln!("     sudo chown root:input /usr/local/bin/mygestures");
        eprintln!("     sudo chmod 2755 /usr/local/bin/mygestures");
        eprintln!("     (Note: This allows only this binary to capture inputs/inject actions)");
        
        eprintln!("\nOPTION B: STANDARD SETUP (Allows all user applications to capture inputs)");
        eprintln!("  1. Add your user to the 'input' group:");
        eprintln!("     sudo usermod -aG input $USER");
        eprintln!("  2. Install the udev rules:");
        eprintln!("     sudo cp 99-mygestures.rules /etc/udev/rules.d/");
        eprintln!("     sudo udevadm control --reload-rules && sudo udevadm trigger");
        eprintln!("=========================================================================\n");
        return false;
    }
    true
}

#[cfg(target_os = "linux")]
fn run_grabber(
    device_path: &str,
    trigger_button: i32,
    sensitivity: i32,
    mut config: mygestures::config::Configuration,
    wayland_ctx: mygestures::wayland::WaylandContext,
    ipc: mygestures::ipc::DaemonIpc,
) -> Result<(), std::io::Error> {
    use evdev::{Device, EventType, KeyCode};
    use mygestures::protractor::{Point2D, match_gesture};
    use mygestures::uinput::UinputDevice;

    println!("Attempting to open device: {}", device_path);
    let mut dev = Device::open(device_path)?;

    // Adjust sensitivity threshold if relative device vs absolute device
    let mut threshold = sensitivity as f64;
    let abs_x = dev.get_absinfo().ok()
        .and_then(|mut iter| iter.find(|(code, _)| *code == evdev::AbsoluteAxisCode::ABS_X))
        .map(|(_, info)| info);
    if let Some(abs_info) = abs_x {
        if sensitivity <= 30 {
            let range = abs_info.maximum() - abs_info.minimum();
            let resolution = abs_info.resolution();
            if resolution > 0 {
                threshold = (resolution * 4) as f64;
            } else if range > 0 {
                threshold = (range / 25) as f64;
            }
        }
    }

    // Translate trigger button to evdev code
    let target_button = match trigger_button {
        1 => KeyCode::BTN_LEFT,
        2 => KeyCode::BTN_MIDDLE,
        3 => KeyCode::BTN_RIGHT,
        8 => KeyCode::BTN_SIDE,
        9 => KeyCode::BTN_EXTRA,
        other => KeyCode(other as u16),
    };

    // Exclusive grab (only for button 3 by default)
    let mut grabbed = false;
    if trigger_button == 3 {
        if let Err(e) = dev.grab() {
            eprintln!("mygestures: Failed to grab device exclusively: {}", e);
        } else {
            println!("mygestures: Grabbed device exclusively.");
            grabbed = true;
        }
    }

    // Initialize uinput virtual clone
    let mut uinput = match UinputDevice::init_from_device(&dev) {
        Ok(u) => Some(u),
        Err(e) => {
            eprintln!("mygestures: Failed to initialize uinput: {}", e);
            None
        }
    };

    // Drop elevated GID privileges if running as setgid (SGID)
    let rgid = nix::unistd::getgid();
    let egid = nix::unistd::getegid();
    if egid != rgid {
        if nix::unistd::setegid(rgid).is_ok() {
            println!("mygestures: Successfully dropped elevated GID from {} to {}", egid, rgid);
        } else {
            eprintln!("mygestures: Warning: Failed to drop elevated GID.");
        }
    }

    println!("Listening for events from device using libevdev (button {})", trigger_button);

    let mut is_drawing = false;
    let mut captured_points = Vec::new();
    let mut virtual_x = 0.0;
    let mut virtual_y = 0.0;
    let mut moved = false;

    // Convert templates for matching
    let mut templates: Vec<(String, Vec<Point2D>)> = config.gestures.iter()
        .filter(|g| !g.is_deleted)
        .map(|g| (g.name.clone(), g.points.clone()))
        .collect();

    // Event loop
    loop {
        if RELOAD_REQUESTED.swap(false, Ordering::Relaxed) {
            println!("mygestures: Reloading configuration dynamically...");
            if let Some(new_config) = mygestures::config::Configuration::load_from_file(&config.user_config_path) {
                config = new_config;
                templates = config.gestures.iter()
                    .filter(|g| !g.is_deleted)
                    .map(|g| (g.name.clone(), g.points.clone()))
                    .collect();
                println!("mygestures: Configuration reloaded successfully.");
            } else {
                eprintln!("mygestures: Failed to reload configuration from {}", config.user_config_path.display());
            }
        }

        // Check if another instance requested us to exit via shared memory
        if ipc.is_kill_requested() {
            println!("Mygestures asked me to exit via IPC.");
            break;
        }

        // Fetch events blocking
        let events = match dev.fetch_events() {
            Ok(events) => events,
            Err(e) if e.kind() == std::io::ErrorKind::Interrupted => continue,
            Err(e) => return Err(e),
        };

        for ev in events {
            let ev_type = ev.event_type();
            let ev_code = ev.code();
            let ev_value = ev.value();

            if ev_type == EventType::KEY && ev_code == target_button.0 {
                if ev_value == 1 {
                    // Start gesture movement
                    is_drawing = true;
                    captured_points.clear();
                    captured_points.push(Point2D { x: virtual_x, y: virtual_y });
                    println!("Gesture drawing started at ({:.1}, {:.1})", virtual_x, virtual_y);
                } else if ev_value == 0 {
                    // End gesture movement
                    is_drawing = false;
                    
                    // Final coordinate
                    if moved {
                        captured_points.push(Point2D { x: virtual_x, y: virtual_y });
                    }

                    println!("Gesture drawing finished. Path points: {}", captured_points.len());

                    // Calculate path length
                    let len = mygestures::protractor::path_length(&captured_points);
                    if captured_points.len() < 5 || len < 15.0 {
                        println!("Gesture failed: path too short or too few points (points: {}, length: {:.1})", captured_points.len(), len);
                        // Click emulation
                        if grabbed {
                            if let Some(ref mut u) = uinput {
                                u.click(trigger_button);
                            }
                        }
                    } else {
                        // Match gesture
                        if let Some(matched_name) = match_gesture(&captured_points, &templates) {
                            if let Some(gesture) = config.gestures.iter().find(|g| g.name == matched_name) {
                                println!("Matched gesture: {}", gesture.name);
                                for action in &gesture.actions {
                                    if let ActionType::Click(btn) = action {
                                        if let Some(ref mut u) = uinput {
                                            u.click(btn.unwrap_or(1));
                                        }
                                    } else {
                                        let ui_cell = std::cell::RefCell::new(uinput.as_mut());
                                        wayland_ctx.execute_action(action, &|keys_str| {
                                            if let Some(ref mut u) = *ui_cell.borrow_mut() {
                                                u.keypress_string(keys_str);
                                            }
                                        });
                                    }
                                }
                            }
                        } else {
                            println!("Gesture failed: did not match any known template.");
                        }
                    }
                    moved = false;
                }
            } else {
                // Forward events to virtual device if grabbed
                if grabbed {
                    if let Some(ref mut u) = uinput {
                        u.forward_event(ev_type.0, ev_code, ev_value);
                    }
                }

                // Update coordinate tracking
                if ev_type == EventType::RELATIVE {
                    if ev_code == 0 { // REL_X
                        virtual_x += ev_value as f64;
                        moved = true;
                    } else if ev_code == 1 { // REL_Y
                        virtual_y += ev_value as f64;
                        moved = true;
                    }
                } else if ev_type == EventType::ABSOLUTE {
                    if ev_code == 0 { // ABS_X
                        virtual_x = ev_value as f64;
                        moved = true;
                    } else if ev_code == 1 { // ABS_Y
                        virtual_y = ev_value as f64;
                        moved = true;
                    }
                } else if ev_type == EventType::SYNCHRONIZATION && ev_code == 0 { // SYN_REPORT
                    if moved && is_drawing {
                        let last = captured_points.last();
                        let add_point = match last {
                            Some(lp) => {
                                let dx = virtual_x - lp.x;
                                let dy = virtual_y - lp.y;
                                dx * dx + dy * dy >= threshold * threshold / 100.0 // check sensitivity threshold
                            }
                            None => true,
                        };
                        if add_point {
                            captured_points.push(Point2D { x: virtual_x, y: virtual_y });
                        }
                    }
                    moved = false;
                }
            }
        }
    }

    if grabbed {
        let _ = dev.ungrab();
    }
    Ok(())
}

#[cfg(not(target_os = "linux"))]
fn run_grabber(
    _device_path: &str,
    _trigger_button: i32,
    _sensitivity: i32,
    _config: mygestures::config::Configuration,
    _wayland_ctx: mygestures::wayland::WaylandContext,
    _ipc: mygestures::ipc::DaemonIpc,
) -> Result<(), std::io::Error> {
    println!("evdev grabbing is only supported on Linux. Mock daemon sleeping...");
    loop {
        std::thread::sleep(std::time::Duration::from_secs(60));
    }
}

fn main() {
    let mut args = std::env::args().skip(1);
    let mut devices = Vec::new();
    let mut button = 3;
    let mut sensitivity = 30;
    let mut help = false;
    let mut custom_config = None;

    while let Some(arg) = args.next() {
        if arg == "-h" || arg == "--help" {
            help = true;
        } else if arg == "-b" || arg == "--button" {
            if let Some(val) = args.next() {
                button = val.parse::<i32>().unwrap_or(3);
            }
        } else if arg == "-s" || arg == "--sensitivity" {
            if let Some(val) = args.next() {
                sensitivity = val.parse::<i32>().unwrap_or(30);
            }
        } else if arg == "-d" || arg == "--device" {
            if let Some(val) = args.next() {
                devices.push(val);
            }
        } else if arg.starts_with("--device=") {
            devices.push(arg.trim_start_matches("--device=").to_string());
        } else if arg.starts_with("--button=") {
            button = arg.trim_start_matches("--button=").parse::<i32>().unwrap_or(3);
        } else if arg.starts_with("--sensitivity=") {
            sensitivity = arg.trim_start_matches("--sensitivity=").parse::<i32>().unwrap_or(30);
        } else if !arg.starts_with('-') {
            custom_config = Some(arg);
        }
    }

    if help {
        println!("Usage: mygestures [OPTIONS] [CONFIG_FILE]");
        println!();
        println!("CONFIG_FILE:");
        println!("  Default: {}", mygestures::config::get_default_config_path().display());
        println!();
        println!("OPTIONS:");
        println!("  -d, --device <DEVICENAME>  : Device to grab (can specify multiple times)");
        println!("  -b, --button <BUTTON>      : Button used to draw the gesture");
        println!("                              Default: '3' (Right Click)");
        println!("  -s, --sensitivity <VAL>    : Sensitivity threshold (pixels)");
        println!("                              Default: '30'");
        println!("  -h, --help                 : Help");
        return;
    }

    // Resolve target device list
    if devices.is_empty() {
        if let Some(default_mouse) = find_mouse_device() {
            devices.push(default_mouse.to_string_lossy().into_owned());
        } else {
            eprintln!("Could not find default evdev mouse device.");
            std::process::exit(1);
        }
    }

    // If NOT spawned as a child process of the manager, run D-Bus registration
    if std::env::var("MYGESTURES_CHILD").is_err() {
        register_sigusr1();
        std::thread::spawn(move || {
            let dbus_name = mygestures::config::get_dbus_name();
            match zbus::blocking::connection::Builder::session()
                .and_then(|b| b.name(dbus_name))
                .and_then(|b| b.serve_at("/org/mygestures/Daemon", DaemonDbus))
                .and_then(|b| b.build())
            {
                Ok(_conn) => {
                    println!("mygestures: Registered D-Bus service.");
                    loop {
                        std::thread::sleep(std::time::Duration::from_secs(3600));
                    }
                }
                Err(e) => {
                    eprintln!("mygestures: D-Bus initialization error: {}", e);
                }
            }
        });
    }

    // If multiple devices are specified, run process manager
    if devices.len() > 1 {
        IS_MANAGER.store(true, Ordering::Relaxed);
        
        loop {
            let exe = std::env::current_exe().unwrap();
            let mut children = Vec::new();

            for dev in &devices {
                let mut cmd = std::process::Command::new(&exe);
                cmd.env("MYGESTURES_CHILD", "1");
                cmd.arg("-d").arg(dev);
                cmd.arg("-b").arg(button.to_string());
                cmd.arg("-s").arg(sensitivity.to_string());
                if let Some(ref cfg) = custom_config {
                    cmd.arg(cfg);
                }

                match cmd.spawn() {
                    Ok(child) => {
                        children.push(child);
                    }
                    Err(e) => {
                        eprintln!("Failed to spawn listener process for {}: {}", dev, e);
                    }
                }
            }

            {
                let mut pids = CHILD_PIDS.lock().unwrap();
                pids.clear();
                for child in &children {
                    pids.push(child.id());
                }
            }

            for mut child in children {
                let _ = child.wait();
            }

            if RELOAD_MANAGER.swap(false, Ordering::Relaxed) {
                println!("mygestures parent: Config reload triggered. Restarting child processes...");
                continue;
            }
            break;
        }
        return;
    }

    // Single device execution
    let device_path = &devices[0];
    if !check_permissions(device_path) {
        std::process::exit(1);
    }

    // Load configuration
    let config = if let Some(ref path) = custom_config {
        if !std::path::Path::new(path).exists() {
            eprintln!("Error: Configuration file not found at {}.", path);
            std::process::exit(1);
        }
        if let Some(c) = Configuration::load_from_file(path) {
            c
        } else {
            eprintln!("Failed to load config from: {}", path);
            std::process::exit(1);
        }
    } else {
        let user_path = mygestures::config::get_default_config_path();
        if !user_path.exists() {
            eprintln!("Error: Configuration file not found at {}.", user_path.display());
            eprintln!("Please run gestos first to initialize the configuration.");
            std::process::exit(1);
        }
        Configuration::load_from_defaults()
    };

    // Initialize IPC and kill previous instance of mygestures running on this device
    let ipc = match DaemonIpc::new(device_path) {
        Ok(ipc_inst) => {
            ipc_inst.send_kill_message();
            ipc_inst
        }
        Err(e) => {
            eprintln!("Failed to allocate shared memory segment: {}", e);
            std::process::exit(1);
        }
    };

    let wayland_ctx = WaylandContext::discover();

    println!("Trigger button: {}", button);
    println!("Capture method: evdev");
    println!("Starting grabber...");

    if let Err(e) = run_grabber(device_path, button, sensitivity, config, wayland_ctx, ipc) {
        eprintln!("mygestures: Daemon error: {}", e);
        std::process::exit(1);
    }
}
