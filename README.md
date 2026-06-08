
MyGestures - mouse gestures for linux
=====================================

 Mouse gestures - "draw" commands using your mouse/touchscreen/touchpad.
 Now with multitouch gestures on synaptics touchpads (experimental).

  
Installing from source:
-----------------------

### Ubuntu / Debian:

    sudo apt install pkg-config autoconf automake libtool libx11-dev libxrender-dev libxtst-dev libxi-dev libyaml-dev libevdev-dev git make gcc libgtk-4-dev

### Fedora:

    sudo dnf install pkgconf-pkg-config autoconf automake libtool libX11-devel libXrender-devel libXtst-devel libXi-devel libyaml-devel libevdev-devel git make gcc gtk4-devel

### Build:

    git clone https://github.com/deters/mygestures.git
    cd mygestures/
    sh autogen.sh
    ./configure
    make
    sudo make install

Generating a .deb package (optional)
------------------------------------

  After installing from source:

    sudo apt install debhelper
    dpkg-buildpackage # it'll probably complain about not being able to sign the package. thats fine.
    ls ../mygestures*.deb

Usage:
------

    mygestures                       # use default button (button 3) on default device (Virtual core pointer)
    mygestures -l                    # list device names  
    mygestures -d 'elan touchscreen' # mygestures running against an specific device
    mygestures -m                    # experimental synaptics multitouch mode  *
                                     # * see next section

**To use gestures:** Hold the trigger button (default: right-click / button 3) and move the mouse to draw a gesture. Release the button to execute.

Troubleshooting:
----------------

### Gestures not working or no response:
1. **Wrong device:** Run `mygestures -l` to see available devices. If you are on Wayland, it might be picking the wrong `/dev/input/eventX`. Specify the correct one with `-d`.
2. **Permissions:** Ensure your user is in the `input` group and you have installed the udev rules (see below). **You must log out and log back in** for group changes to take effect.
3. **Trigger Button:** If you are on a laptop with a touchpad (but not using multitouch mode), you might need to use button 1 (left-click) instead of 3. Try `mygestures -b 1`.
4. **Wayland:** If you are on Wayland, MyGestures uses `evdev` mode. This requires access to `/dev/uinput` to simulate keys. Check if `/dev/uinput` exists and has the right permissions.
5. **Configuration Format:** MyGestures recently switched from XML to YAML. If you have an old `~/.config/mygestures/mygestures.yaml` that is actually XML, it will fail to load. Check the output for error messages.

### Mouse "dies" when MyGestures starts:
This happens if MyGestures grabs the device exclusively (needed for right-click gestures) but fails to initialize the virtual `uinput` device to forward other mouse events.
- Check if you have permissions for `/dev/uinput`.
- Ensure `99-mygestures.rules` is installed in `/etc/udev/rules.d/`.

Optional: If you want multitouch gestures on your synaptics touchpad
--------------------------------------------------------------------

 Installing the custom synaptics driver:

    # Ubuntu / Debian
    sudo apt-get install -y git build-essential libevdev-dev autoconf automake libmtdev-dev xorg-dev xutils-dev libtool

    # Fedora
    sudo dnf install git @development-tools libevdev-devel autoconf automake mtdev-devel xorg-x11-server-devel xorg-x11-util-macros libtool

    sudo apt-get remove -y xserver-xorg-input-synaptics # or dnf remove xorg-x11-drv-synaptics
    git clone https://github.com/Chosko/xserver-xorg-input-synaptics.git
    cd xserver-xorg-input-synaptics
    ./autogen.sh
    ./configure --exec_prefix=/usr
    make
    sudo make install

 Enable the option SHMConfig:

    sudo mkdir -p /etc/X11/xorg.conf.d/

    cat > /etc/X11/xorg.conf.d/50-synaptics.conf < EOL
    Section "InputClass"
    Identifier "evdev touchpad catchall"
    Driver "synaptics"
    MatchDevicePath "/dev/input/event*"
    MatchIsTouchpad "on"
    Option "Protocol" "event"
    Option "SHMConfig" "on"
    EndSection
    EOL

 Reboot the computer and then test the new driver:

    synclient -m 10

 And then in mygestures:

    mygestures -m

Gestures configuration:
-----------------------

  In the first execution mygestures will create a configuration file for your user:

    ~/.config/mygestures/mygestures.yaml

  Mygestures works by capturing your mouse movements and define them in terms of basic directions:
  
               U - (Up)
               D - (Down)
               R - (Right)
               L - (Left)
               7 - (Top left corner)
               9 - (Top right corner)
               1 - (Bottom left corner)
               3 - (Bottom right corner)

  A "movement" can be defined by composing this directions under a name using Regex:
  
    movements:
      T: RLD          # T is a sequence of right+left+down
      V: "39"         # V needs more precision to be defined.
      C: "U?LDRU?"    # C (notice the use of regex)

  Then you should define some contexts (used to filter applications):
    
    apps:
      - name: "Terminal windows"
        match: { class: ".*(Term|term).*" }
        gestures:
          # some gestures here
    
    global:
       # gestures for all applications

   Inside each context you can define the gestures:

    "Run gedit":
      move: G
      do: exec gedit
    
    "Copy (Ctrl+C)":
      move: C
      do: keypress Control_L+C
        
   Example of actions can be:
        
 __Window management__
           
    do: maximize           # put focused window to the maximized state
    do: restore            # restore window from maximized state
    do: iconify            # iconify window
    do: toggle-maximized   # toggle focused window from/to the maximized state
    do: raise              # raise current window
    do: lower              # lower current window
            
 __Program operation__
           
    do: kill               # kill the current application
    do: exec gedit         # launch some program
    
 __KeyPress__

    do: keypress Alt_L+Left # send key sequence

   More key names can be found on the file /usr/include/X11/keysymdef.h
