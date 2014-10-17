/*
  Copyright 2005 Nir Tzachar
  Copyright 2008, 2010, 2013, 2014 Lucas Augusto Deters

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.  */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <stdio.h>
#include "wm.h"
#include "helpers.h"


void generic_iconify(XButtonEvent *ev)
{
        Window w = get_window(ev, 0);
        if (w != None)
                XIconifyWindow(ev->display, w, 0);
                
        return;
}

void generic_kill(XButtonEvent *ev)
{
        Window w = get_window(ev, 0);

        /* dont kill root window */
        if (w == RootWindow(ev->display, DefaultScreen(ev->display)))
                return;

        XSync (ev->display, 0);	
        XKillClient(ev->display, w);
        XSync (ev->display, 0);	
        return;
}

void generic_raise(XButtonEvent *ev)
{
        Window w = get_window(ev, 0);
        XRaiseWindow(ev->display, w);
        return;
}

void generic_lower(XButtonEvent *ev)
{
        Window w = get_window(ev, 0);
        XLowerWindow(ev->display, w);
        return;
}

void generic_maximize(XButtonEvent *ev)
{
        Window w = get_window(ev, 0);
        int width = XDisplayWidth(ev->display, DefaultScreen(ev->display));
        int heigth = XDisplayHeight(ev->display, DefaultScreen(ev->display));
        
        XMoveResizeWindow(ev->display, w, 0, 0,
                          width-50, heigth-50);
        
        return;
}
struct wm_helper generic_wm_helper = {
        .iconify = generic_iconify,
        .kill = generic_kill,
        .raise = generic_raise,
        .lower = generic_lower,
        .maximize = generic_maximize,
};
