MyGestures - Pure Wayland/Evdev mouse gestures for Linux
========================================================

Mouse gestures - "draw" commands using your mouse/touchscreen/touchpad.
Now written in Rust and completely independent of X11 and legacy drivers.

Installing from source:
-----------------------

### Install dependencies:

#### Ubuntu / Debian:
```bash
sudo apt install pkg-config libgtk-4-dev libevdev-dev git meson ninja-build cargo
```

#### Fedora:
```bash
sudo dnf install pkgconf-pkg-config gtk4-devel libevdev-devel git meson ninja-build cargo
```


#### Alpine:
```bash
apk add git meson cargo sudo
apk add glib-dev cairo-dev gdk-dev gdk-pixbuf-dev pango-dev gtk4.0-dev
```

### Build and Install:

You can compile and install `mygestures` either using the compatibility wrapper `Makefile` or directly using `meson`.

#### Option A: Using the Makefile wrapper (Recommended)
```bash
git clone https://github.com/deters/mygestures.git
cd mygestures/
make
sudo make install
```

#### Option B: Using Meson directly
```bash
git clone https://github.com/deters/mygestures.git
cd mygestures/
meson setup build
meson compile -C build
sudo meson install -C build
```

The installation process automatically installs the `mygestures` and `gestos` binaries, installs the udev rules, reloads the udev system, and adds your user to the `input` group if necessary.

Usage:
------

- `mygestures`: Start the daemon. Uses default button (button 3 / right-click) on the default mouse device.
- `gestos`: Start the GTK4 GUI configuration editor.

**To use gestures:** Hold the trigger button (default: right-click / button 3) and move the mouse to draw a gesture. Release the button to execute.

Troubleshooting:
----------------

### Gestures not working or no response:
1. **Wrong device:** The daemon auto-detects standard pointer devices. If yours is not captured, find its path in `/dev/input/by-path/` and run `mygestures -d '/dev/input/by-path/event-mouse'`.
2. **Permissions:** Ensure you have installed the udev rules (see above) and configured permissions. By default, the udev rules restrict access to the `input` group for security. You can configure `mygestures` with Set-Group-ID (SGID) privileges to run securely, or fallback to standard `uaccess`/`input` group setups. Running the binary will output detailed instructions on both options.
3. **Trigger Button:** If you are on a laptop with a touchpad, you might need to use button 1 (left-click) instead of 3. Try `mygestures -b 1`.
4. **Missing /dev/uinput (e.g. Alpine Linux):** On some distributions like Alpine Linux, the `uinput` kernel module is not loaded by default, and `/dev/uinput` will not exist. Load it manually using `sudo modprobe uinput`. To make this persistent on boot, add `uinput` to `/etc/modules`.

Gestures configuration:
-----------------------

On first execution, MyGestures will create a configuration file for your user:

`~/.config/mygestures/mygestures.yaml`

Gestures are defined using coordinate paths matched using the Protractor gesture recognition algorithm.

Example of `mygestures.yaml`:

```yaml
global:
  "Close window":
    move: "0,0 0,100 100,100"  # drawn as an 'L' shape
    do: kill

  "Maximize window":
    move: "50,100 50,0"        # drawn upwards
    do: toggle-maximized

  "Copy":
    move: "100,0 0,0 0,100 100,100"
    do: keypress Control_L+C

apps:
  - name: Terminal
    match: { class: ".*(Term|term).*" }
    gestures:
      "New tab":
        move: "0,0 100,0 50,0 50,100"
        do: keypress Control_L+Shift_L+T
```

Supported Actions:
------------------

### Window Management
- `do: maximize`          # Maximizes focused window
- `do: restore`           # Restores maximized window
- `do: iconify`           # Minimizes/hides window
- `do: toggle-maximized`  # Toggles window maximization state
- `do: raise`             # Raises window
- `do: lower`             # Lowers window
- `do: toggle-fullscreen` # Toggles window fullscreen state

### Program Operations
- `do: kill`              # Closes the active window
- `do: exec <cmd>`        # Executes a custom shell command (e.g. `exec gedit`)

### Keyboard/Mouse Simulation
- `do: keypress <keys>`   # Simulates a keyboard shortcut (e.g. `keypress Control_L+c`)
- `do: click <button>`    # Emulates a mouse click (e.g. `click 3` for right-click)

### Desktop/Compositor-Agnostic Actions
(Supported on Sway, Hyprland, GNOME, and KDE)
- `do: workspace-left`
- `do: workspace-right`
- `do: workspace-up`
- `do: workspace-down`
- `do: show-overview`     # Toggles activities overview
- `do: show-app-grid`     # Shows application view
- `do: show-desktop`
- `do: lock-screen`
- `do: terminal`
- `do: volume-up`
- `do: volume-down`
- `do: volume-mute`
- `do: media-play`
- `do: media-next`
- `do: media-prev`

License
-------
GPL-2.0-or-later
