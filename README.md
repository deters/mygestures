
MyGestures - mouse gestures for linux
=====================================

 Mouse gestures - "draw" commands using your mouse/touchscreen/touchpad.
 Now with multitouch gestures on synaptics touchpads (experimental).

  
Installing from source:
-----------------------

    sudo apt install pkg-config autoconf libtool libx11-dev libxrender-dev libxtst-dev libxml2-dev
    cd mygestures/
    sh autogen.sh
    make
    sudo make install

Usage:
------

    mygestures                       # use default button on default device 
    mygestures -l                    # list device names  
    mygestures -d 'elan touchscreen' # mygestures running against a touchscreen device.
    mygestures -d 'synaptics'        # experimental synaptics multitouch mode (3 finger gestures).

Configuration:
--------------

  Gestures configuration is done on "~/.config/mygestures/mygestures.xml".
  
  This file contains movements, contexts and gestures.
  
  __Movement__: a name assigned to a composition of elementar strokes: (U, D, R, L, 1, 3, 7, 9).
     
    <movement name="UpRight" value="UR" />
    <movement name="U" value="DRU" />
    <movement name="V" value="39" />
    <movement name="C" value="U?LDRU?" />         
         
  __Context__: used to filter applications
    
    <context name="Terminal windows" windowclass=".*(Term|term).*" windowtitle=".*">
       <!-- some gestures here -->
    </context>
    
    <context name="All applications" windowclass=".*" windowtitle=".*">
       <!-- some gestures here -->
    </context>

  __Gesture__: Will use a movement to trigger some actions

    <gesture name="Run gedit" movement="G">
      <do action="exec" value="gedit" />
    </gesture>
    
    <gesture name="Copy (Ctrl+C)" movement="C">
      <do action="keypress" value="Control_L+C" />
    </gesture>
        
Supported actions:
------------------
        
 __Window management__
           
    <do action="maximize" /> <!-- put focused window to the maximized state -->
    <do action="restore" /> <!-- restore window from maximized state -->
    <do action="iconify" /> <!-- iconify window -->
    <do action="toggle-maximized" /> <!-- toggle focused window from/to the maximized state -->
    <do action="raise" /> <!-- raise current window -->
    <do action="lower" /> <!-- lower current window -->
            
 __Program operation__
           
    <do action="kill" /> <!-- kill the program with the active window -->
    <do action="exec" value="gedit" /> <!-- execute command -->
    
 __KeyPress__

Key names can be found on /usr/include/X11/keysymdef.h

    <do action="keypress" value="Alt_L+Left" /> <!-- send key sequence -->

               
 Gestures are created inside contexts, so you can filter what applications will have any gesture. 
   

