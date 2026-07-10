use gtk::prelude::*;
use std::process::Command;
use crate::ui::components::show_error_dialog;
use gtk4 as gtk;

pub fn is_daemon_running(conn: Option<&zbus::blocking::Connection>) -> bool {
    let dbus_name = mygestures::config::get_dbus_name();

    let check_status = |c: &zbus::blocking::Connection| -> Option<bool> {
        let dbus_proxy = zbus::blocking::fdo::DBusProxy::new(c).ok()?;
        let bus_name = zbus::names::BusName::try_from(dbus_name.clone()).ok()?;
        dbus_proxy.name_has_owner(bus_name).ok()
    };

    if let Some(c) = conn {
        if let Some(running) = check_status(c) {
            return running;
        }
    }

    if let Ok(new_conn) = zbus::blocking::Connection::session() {
        if let Some(running) = check_status(&new_conn) {
            return running;
        }
    }
    false
}
pub fn start_daemon(conn: Option<&zbus::blocking::Connection>) -> Result<(), String> {
    if is_daemon_running(conn) {
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
        Err(e) => Err(format!("Failed to query daemon status: {}", e)),
    }
}

pub fn stop_daemon(conn: Option<&zbus::blocking::Connection>) {
    let dbus_name = mygestures::config::get_dbus_name();
    let stop_via_dbus = || -> zbus::Result<()> {
        let proxy = if let Some(c) = conn {
            zbus::blocking::Proxy::new(
                c,
                dbus_name.clone(),
                "/org/mygestures/Daemon",
                "org.mygestures.Daemon",
            )
        } else {
            let new_conn = zbus::blocking::Connection::session()?;
            zbus::blocking::Proxy::new(
                &new_conn,
                dbus_name.clone(),
                "/org/mygestures/Daemon",
                "org.mygestures.Daemon",
            )
        }?;
        let _: () = proxy.call("Stop", &())?;
        Ok(())
    };

    if stop_via_dbus().is_err() {
        let uid = nix::unistd::Uid::current();
        if let Ok(output) = Command::new("pgrep")
            .arg("-u")
            .arg(uid.to_string())
            .arg("-x")
            .arg("mygestures")
            .output()
        {
            if output.status.success() {
                let stdout = String::from_utf8_lossy(&output.stdout);
                for line in stdout.lines() {
                    if let Ok(pid_val) = line.trim().parse::<i32>() {
                        let _ = nix::sys::signal::kill(
                            nix::unistd::Pid::from_raw(pid_val),
                            nix::sys::signal::Signal::SIGTERM,
                        );
                    }
                }
            }
        }
    }
}

pub fn reload_daemon<W: IsA<gtk::Window>>(
    conn: Option<&zbus::blocking::Connection>,
    parent: Option<&W>,
) {
    let dbus_name = mygestures::config::get_dbus_name();
    let reload_via_dbus = || -> zbus::Result<()> {
        let proxy = if let Some(c) = conn {
            zbus::blocking::Proxy::new(
                c,
                dbus_name.clone(),
                "/org/mygestures/Daemon",
                "org.mygestures.Daemon",
            )
        } else {
            let new_conn = zbus::blocking::Connection::session()?;
            zbus::blocking::Proxy::new(
                &new_conn,
                dbus_name.clone(),
                "/org/mygestures/Daemon",
                "org.mygestures.Daemon",
            )
        }?;
        let _: () = proxy.call("Reload", &())?;
        Ok(())
    };

    if let Err(zbus::Error::FDO(ref fdo_err)) = reload_via_dbus() {
        if let Some(p) = parent {
            show_error_dialog(p, &fdo_err.to_string());
        } else {
            eprintln!("mygestures reload error: {}", fdo_err);
        }
    }
}
