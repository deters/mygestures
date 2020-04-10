
MyGestures - mouse gestures for linux
=====================================

 Mouse gestures - "draw" commands using your mouse/touchscreen/touchpad.
 Now with multitouch gestures on synaptics touchpads (experimental).

  
Installing from source:
-----------------------

    sudo apt install pkg-config autoconf libtool libx11-dev libxrender-dev libxtst-dev libxml2-dev git make
    git clone https://github.com/deters/mygestures.git
    cd mygestures/
    sh autogen.sh
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

Optional: If you want multitouch gestures on your synaptics touchpad
--------------------------------------------------------------------

 Installing the custom synaptics driver:

    sudo apt-get install -y git build-essential libevdev-dev autoconf automake libmtdev-dev xorg-dev xutils-dev libtool
    sudo apt-get remove -y xserver-xorg-input-synaptics
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

    ~/.config/mygestures/mygestures.xml

  Mygestures works by capturing your mouse movements and define them in terms of basic directions:
  
               U - (Up)
               D - (Down)
               R - (Right)
               L - (Left)
               7 - (Top left corner)
               9 - (Top right corner)
               1 - (Bottom left corner)
               3 - (Bottom right corner)

  A "movement" can be defined by composing this directions under a name.
  
    <movement name="T" value="RLD" />      <!-- T is a sequence of right+left+down -->
    <movement name="V" value="39" />       <!-- V needs more precision to be defined.-->
    <movement name="C" value="U?LDRU?" />  <!-- C (notice the use of regex) -->

  Then you should define some contexts (used to filter applications):
    
    <context name="Terminal windows" windowclass=".*(Term|term).*" windowtitle=".*">
       <!-- some gestures here -->
    </context>
    
    <context name="All applications" windowclass=".*" windowtitle=".*">
       <!-- some gestures here -->
    </context>

   Inside each context you can define the gestures:

    <gesture name="Run gedit" movement="G">
      <do action="exec" value="gedit" />
    </gesture>
    
    <gesture name="Copy (Ctrl+C)" movement="C">
      <do action="keypress" value="Control_L+C" />
    </gesture>
        
   Example of actions can be:
        
 __Window management__
           
    <do action="maximize" /> <!-- put focused window to the maximized state -->
    <do action="restore" /> <!-- restore window from maximized state -->
    <do action="iconify" /> <!-- iconify window -->
    <do action="toggle-maximized" /> <!-- toggle focused window from/to the maximized state -->
    <do action="raise" /> <!-- raise current window -->
    <do action="lower" /> <!-- lower current window -->
            
 __Program operation__
           
    <do action="kill" /> <!-- kill the current application -->
    <do action="exec" value="gedit" /> <!-- launch some program -->
    
 __KeyPress__

    <do action="keypress" value="Alt_L+Left" /> <!-- send key sequence -->

   More key names can be found on the file /usr/include/X11/keysymdef.h
