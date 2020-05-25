
Gestos - Mouse/Touchpad Gestures
================================

  Gestos - mouse & touchpad gestures on linux.

Basics:
-------

 With Gestos you can use your mouse or touchpad to perform custom actions.
 
 Mouse gestures are activated when you click the right button and start doing a movement.
 
 Touchpad gestures are activated when you swipe three or more fingers on your compatible touchpad.

 To start using gestos, just start one or both the programs provided:

    gestos-mouse
    gestos-touchpad

Installing from source:
-----------------------

    sudo apt install pkg-config autoconf libtool libx11-dev libxrender-dev libxtst-dev libxml2-dev make xdotool wmctrl
    git clone https://github.com/deters/mygestures.git
    cd mygestures/
    sh autogen.sh
    make
    sudo make install

Generating a .deb package (optional)
------------------------------------

  After installing from source:

    sudo apt install debhelper
    dpkg-buildpackage # it'll probably complain about not being able to sign the package. That's fine.
    ls ../mygestures*.deb

