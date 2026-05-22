/*
 Copyright 2026 Lucas Augusto Deters
 Copyright 2005 Nir Tzachar

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 */

#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>

#include "x11_window.h"
#include "configuration.h"

Status fetch_window_title(Display *dpy, Window w, char **out_window_title)
{
	int status;
	XTextProperty text_prop;
	char **list = NULL;
	int num = 0;

	status = XGetWMName(dpy, w, &text_prop);
	if (!status || !text_prop.value || !text_prop.nitems)
	{
		*out_window_title = strdup("");
		return 1;
	}
	status = Xutf8TextPropertyToTextList(dpy, &text_prop, &list, &num);

	if (status < Success || !num || !*list)
	{
		*out_window_title = strdup("");
	}
	else
	{
		*out_window_title = (char *)strdup(*list);
	}
	XFree(text_prop.value);
	if (list)
	{
		XFreeStringList(list);
	}

	return 1;
}

ActiveWindowInfo *get_active_window_info(Display *dpy, Window win)
{
	ActiveWindowInfo *ans = malloc(sizeof(ActiveWindowInfo));
	bzero(ans, sizeof(ActiveWindowInfo));

	if (!dpy || win == 0)
	{
		ans->class = strdup("");
		ans->title = strdup("");
		return ans;
	}

	char *win_title = NULL;
	fetch_window_title(dpy, win, &win_title);

	char *win_class = NULL;
	XClassHint class_hints;

	int result = XGetClassHint(dpy, win, &class_hints);

	if (result)
	{
		if (class_hints.res_class != NULL)
			win_class = strdup(class_hints.res_class);

		if (class_hints.res_name != NULL)
			XFree(class_hints.res_name);

		if (class_hints.res_class != NULL)
			XFree(class_hints.res_class);
	}

	ans->class = win_class ? win_class : strdup("");
	ans->title = win_title ? win_title : strdup("");

	return ans;
}

Window get_parent_window(Display *dpy, Window w)
{
	Window root_return, parent_return, *child_return;
	unsigned int nchildren_return;
	int ret;
	ret = XQueryTree(dpy, w, &root_return, &parent_return, &child_return,
					 &nchildren_return);

	return parent_return;
}

Window get_focused_window(Display *dpy)
{
	if (!dpy) return 0;

	Window win = 0;
	int ret, val;
	ret = XGetInputFocus(dpy, &win, &val);

	if (val == RevertToParent)
	{
		win = get_parent_window(dpy, win);
	}

	return win;
}
