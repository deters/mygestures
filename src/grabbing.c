/*
 Copyright 2005 Nir Tzachar

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.  */
/**
 *  This class grabs mouse events and try to translate them into stroke sequences.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <X11/Xlib.h>
//#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XTest.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <getopt.h>
#include <math.h>
#include <assert.h>
#include "drawing-brush.h"
#include "grabbing.h"
#include "wm.h"
#include "gestures.h"
#include "drawing-brush-image.h"

#define DELTA_MIN	30 /*TODO*/
#define MAX_STROKE_SEQUENCE 63 /*TODO*/

Display * dpy = NULL;

/* the button to grab */
int button = 0;

/* Not draw the movement on the screen */
int without_brush = 0;

/* Initial position of the movement (algorithm 1) */
int old_x = -1;
int old_y = -1;

/* Initial position of the movement (algorithm 2) */
int old_x_2 = -1;
int old_y_2 = -1;

/* valid strokes */
char stroke_names[] = { 'N', 'L', 'R', 'U', 'D', '1', '3', '7', '9' };

/* movements stack (first capture algoritm) */
char * accurate_stroke_sequence;

/* movements stack (secound capture algoritm) */
char * fuzzy_stroke_sequence;

/* back of the draw */
backing_t backing;
brush_t brush;

void free_captured_movements(struct captured_movements *free_me) {

	assert(free_me);

	free(free_me->advanced_movements);
	free(free_me->basic_movements);
	free(free_me->window_class);
	free(free_me->window_title);
	free(free_me);
}

/**
 * Clear previous movement data.
 */
void start_movement(XButtonEvent *e) {

	// clear captured sequences
	accurate_stroke_sequence[0] = '\0';
	fuzzy_stroke_sequence[0] = '\0';

	// guarda a localização do início do movimento
	old_x = e->x_root;
	old_y = e->y_root;

	old_x_2 = e->x_root;
	old_y_2 = e->y_root;

	if (!without_brush) {
		backing_save(&backing, e->x_root - brush.image_width,
				e->y_root - brush.image_height);
		backing_save(&backing, e->x_root + brush.image_width,
				e->y_root + brush.image_height);

		brush_draw(&brush, old_x, old_y);
	}
	return;
}

void grabbing_set_button(int b) {
	button = b;
}

void grabbing_set_without_brush(int b) {
	without_brush = b;
}

void grabbing_set_brush_color(char * color) {

	if (strcmp(color, "red") == 0)
		brush_image = &brush_image_red;
	else if (strcmp(color, "green") == 0)
		brush_image = &brush_image_green;
	else if (strcmp(color, "yellow") == 0)
		brush_image = &brush_image_yellow;
	else if (strcmp(color, "white") == 0)
		brush_image = &brush_image_white;
	else if (strcmp(color, "purple") == 0)
		brush_image = &brush_image_purple;
	else if (strcmp(color, "blue") == 0)
		brush_image = &brush_image_blue;
	else
		printf("no such color, %s. using \"blue\"\n", color);
	return;

}

int grab_pointer(Display *dpy) {
	int result = 0, i = 0;
	int screen = 0;

	for (screen = 0; screen < ScreenCount(dpy); screen++) {

		result += XGrabButton(dpy, button, AnyModifier, RootWindow(dpy, screen),
		False,
		PointerMotionMask | ButtonReleaseMask | ButtonPressMask,
		GrabModeAsync, GrabModeAsync, None, None);
	}

	return result;
}

int ungrab_pointer(Display *dpy) {
	int result = 0, i = 0;
	int screen = 0;

	for (screen = 0; screen < ScreenCount(dpy); screen++) {

		result += XUngrabButton(dpy, button, AnyModifier,
				RootWindow(dpy, screen));
	}

	return result;
}

/*
 * Get the parent window.
 *
 * PRIVATE
 */
Window get_parent_window(Display *dpy, Window w) {
	Window root_return, parent_return, *child_return = NULL;
	unsigned int nchildren_return;
	int ret;
	ret = XQueryTree(dpy, w, &root_return, &parent_return, &child_return,
			&nchildren_return);

	if (ret) {
		XFree(child_return);
	}

	return parent_return;
}

/*
 * Return the focused window at the given display.
 *
 * PRIVATE
 */
Window get_focused_window(Display *dpy) {

	Window win = 0;
	int val = 0;

	XGetInputFocus(dpy, &win, &val);

	if (val == RevertToParent) {
		win = get_parent_window(dpy, win);
	}

	return win;

}

/*
 * Emulate a mouse click at the given display.
 *
 * PRIVATE
 */
void mouse_click(Display *dpy, int button) {
	XTestFakeButtonEvent(dpy, button, True, CurrentTime);
	usleep(1);
	XTestFakeButtonEvent(dpy, button, False, CurrentTime);
}

/*
 * Get the title of a given window at out_window_title.
 *
 * PRIVATE
 */
Status fetch_window_title(Display *dpy, Window w, char **out_window_title) {
	int status;

	// TODO: investigate memory leak here... see XTextProperty

	XTextProperty text_prop;
	char **list;
	int num;

	status = XGetWMName(dpy, w, &text_prop);
	if (!status || !text_prop.value || !text_prop.nitems) {
		*out_window_title = "";
	}
	status = Xutf8TextPropertyToTextList(dpy, &text_prop, &list, &num);

	if (status < Success || !num || !*list) {
		*out_window_title = "";
	} else {
		*out_window_title = (char *) strdup(*list);
	}
	XFree(text_prop.value);
	XFreeStringList(list);

	return 1;
}

/*
 * Return a window_info struct for the focused window at a given Display.
 *
 * PRIVATE
 */
void get_window_info(Display* dpy, Window win, char ** out_window_title,
		char ** out_window_class) {

	int val;

	char *win_title;
	fetch_window_title(dpy, win, &win_title);

	char *win_class = NULL;

	XClassHint class_hints;

	int result = XGetClassHint(dpy, win, &class_hints);

	if (result) {

		if (class_hints.res_class != NULL)
			win_class = strdup(class_hints.res_class);

		if (win_class == NULL) {
			win_class = "";

		}
	}

	XFree(class_hints.res_name);
	XFree(class_hints.res_class);

	if (win_class) {
		*out_window_class = win_class;
	} else {
		*out_window_class = "";
	}

	if (win_title) {
		*out_window_title = win_title;
	} else {
		*out_window_title = "";
	}

}

/* alloc a key_press struct ???? */
struct key_press * alloc_key_press(void) {
	struct key_press *ans = malloc(sizeof(struct key_press));
	bzero(ans, sizeof(struct key_press));
	return ans;
}

/* alloc a key_press struct ???? */
void free_key_press(struct key_press *free_me) {

	assert(free_me);

	if (free_me->next) {
		free_key_press(free_me->next);
	}

	free(free_me);

}

/**
 * Creates a Keysym from a char sequence
 *
 * PRIVATE
 */
struct key_press *string_to_keypress(char *string) {

	char * copy = strdup(string);
	char * copy2 = copy;

	char ** string_ptr = &copy2;

	struct key_press * ans = NULL;
	struct key_press * pointer = NULL;

	KeySym k;
	char *token = NULL;

	token = strsep(string_ptr, "+\n ");

	while (token != NULL) {

		k = XStringToKeysym(token);

		if (k == NoSymbol) {
			fprintf(stderr, "Warning: error converting %s to keysym\n", token);
			free_key_press(ans);
			ans = NULL;
			break;
		}

		if (!pointer) {
			pointer = alloc_key_press();
			ans = pointer;
		} else {
			pointer->next = alloc_key_press();
			pointer = pointer->next;
		}

		pointer->key = k;

		token = strsep(string_ptr, "+\n ");
	}

	free(copy);
	return ans;

}

/**
 * Fake key event
 */
void press_key(Display *dpy, KeySym key, Bool is_press) {

	assert(key);

	XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, key), is_press, CurrentTime);
	return;
}

/**
 * Fake sequence key events
 */
void generic_root_send(Display *dpy, char *keys) {

	assert(keys);

	struct key_press * keys_compiled = string_to_keypress(keys);

	if (keys_compiled) {

		struct key_press *first_key;
		struct key_press *tmp;

		first_key = (struct key_press *) keys_compiled;

		for (tmp = first_key; tmp != NULL; tmp = tmp->next) {
			press_key(dpy, (KeySym) tmp->key, True);
		}

		for (tmp = first_key; tmp != NULL; tmp = tmp->next) {
			press_key(dpy, (KeySym) tmp->key, False);
		}

		free_key_press(keys_compiled);

	}

	return;
}

/**
 * Obtém o resultado dos dois algoritmos de captura de movimentos, e envia para serem processadas.
 */
struct captured_movements * end_movement(XButtonEvent *e) {

	struct captured_movements * captured = NULL;

	// if is drawing
	if (!without_brush) {
		backing_restore(&backing);
		XSync(e->display, False);
	};

	if ((strlen(fuzzy_stroke_sequence) == 0)
			&& (strlen(accurate_stroke_sequence) == 0)) {

		// temporary ungrab button
		ungrab_pointer(e->display);

		// emulate the click
		mouse_click(e->display, button);

		// restart grabbing
		grab_pointer(e->display);

	} else {

		captured = malloc(sizeof(struct captured_movements));

		captured->advanced_movements = strdup(accurate_stroke_sequence);
		captured->basic_movements = strdup(fuzzy_stroke_sequence);

		char * window_title = "";
		char * window_class = "";

		get_window_info(dpy, get_focused_window(dpy), &window_title,
				&window_class);

		captured->window_class = window_class;
		captured->window_title = window_title;

	}

	return captured;

}

char stroke_sequence_complex_detect_stroke(int x_delta, int y_delta) {

	if ((x_delta == 0) && (y_delta == 0)) {
		return stroke_names[NONE];
	}

	// check if the movement is near main axes
	if ((x_delta == 0) || (y_delta == 0)
			|| (fabs((float) x_delta / (float) y_delta) > 3)
			|| (fabs((float) y_delta / (float) x_delta) > 3)) {

		// x axe
		if (abs(x_delta) > abs(y_delta)) {

			if (x_delta > 0) {
				return stroke_names[RIGHT];
			} else {
				return stroke_names[LEFT];
			}

			// y axe
		} else {

			if (y_delta > 0) {
				return stroke_names[DOWN];
			} else {
				return stroke_names[UP];
			}

		}

		// diagonal axes
	} else {

		if (y_delta < 0) {
			if (x_delta < 0) {
				return stroke_names[SEVEN];
			} else if (x_delta > 0) { // RIGHT
				return stroke_names[NINE];
			}
		} else if (y_delta > 0) { // DOWN
			if (x_delta < 0) { // RIGHT
				return stroke_names[ONE];
			} else if (x_delta > 0) {
				return stroke_names[THREE];
			}
		}

	}

	return stroke_names[NONE];

}

char get_fuzzy_stroke(int x_delta, int y_delta) {

	if (abs(y_delta) > abs(x_delta)) {
		if (y_delta > 0) {
			return stroke_names[DOWN];
		} else {
			return stroke_names[UP];
		}

	} else {
		if (x_delta > 0) {
			return stroke_names[RIGHT];
		} else {
			return stroke_names[LEFT];
		}

	}

}

void stroke_sequence_push_stroke(char* stroke_sequence, char stroke) {
	// grab stroke
	int len = strlen(stroke_sequence);
	if ((len == 0) || (stroke_sequence[len - 1] != stroke)) {

		if (len < MAX_STROKE_SEQUENCE) {

			stroke_sequence[len] = stroke;
			stroke_sequence[len + 1] = '\0';

		}

	}
}

void update_movement(XMotionEvent *e) {

	// se for o caso, desenha o movimento na tela
	if (!without_brush) {
		backing_save(&backing, e->x_root - brush.image_width,
				e->y_root - brush.image_height);
		backing_save(&backing, e->x_root + brush.image_width,
				e->y_root + brush.image_height);
		brush_line_to(&brush, e->x_root, e->y_root);
	}

	int new_x = e->x_root;
	int new_y = e->y_root;

	int x_delta = (new_x - old_x);
	int y_delta = (new_y - old_y);

	if ((abs(x_delta) > DELTA_MIN) || (abs(y_delta) > DELTA_MIN)) {

		char stroke = stroke_sequence_complex_detect_stroke(x_delta, y_delta);

		stroke_sequence_push_stroke(accurate_stroke_sequence, stroke);

		// reset start position
		old_x = new_x;
		old_y = new_y;

	}

	int x_delta_2 = new_x - old_x_2;
	int y_delta_2 = new_y - old_y_2;

	char fuzzy_stroke = get_fuzzy_stroke(x_delta_2, y_delta_2);

	int square_distance_2 = x_delta_2 * x_delta_2 + y_delta_2 * y_delta_2;

	if ( DELTA_MIN * DELTA_MIN < square_distance_2) {
		// grab stroke

		stroke_sequence_push_stroke(fuzzy_stroke_sequence, fuzzy_stroke);

		// reset start position
		old_x_2 = new_x;
		old_y_2 = new_y;
	}

	return;
}

struct captured_movements * grabbing_capture_movements() {

	XEvent e;

	struct captured_movements * captured = NULL;

	XNextEvent(dpy, &e);

	switch (e.type) {

	case MotionNotify:
		update_movement((XMotionEvent *) &e);
		break;

	case ButtonPress:
		start_movement((XButtonEvent *) &e);
		break;

	case ButtonRelease:

		captured = end_movement((XButtonEvent *) &e);
		break;

	}

	return captured;

}

/* taken from ecore.. */
int x_key_mask_get(KeySym sym, Display *dpy) {
	XModifierKeymap *mod;
	KeySym sym2;
	int i, j;
	const int masks[8] = {
	ShiftMask, LockMask, ControlMask,
	Mod1Mask, Mod2Mask, Mod3Mask, Mod4Mask, Mod5Mask };

	mod = XGetModifierMapping(dpy);
	if ((mod) && (mod->max_keypermod > 0)) {
		for (i = 0; i < (8 * mod->max_keypermod); i++) {
			for (j = 0; j < 8; j++) {

				sym2 = XkbKeycodeToKeysym(dpy, mod->modifiermap[i], j, 0);

				if (sym2 != 0)
					break;
			}
			if (sym2 == sym) {
				int mask;

				mask = masks[i / mod->max_keypermod];
				if (mod->modifiermap)
					XFree(mod->modifiermap);
				XFree(mod);
				return mask;
			}
		}
	}
	if (mod) {
		if (mod->modifiermap)
			XFree(mod->modifiermap);
		XFree(mod);
	}
	return 0;
}

int grabbing_init() {

	char *s = NULL;
	s = XDisplayName(NULL);

	dpy = XOpenDisplay(s);
	if (!dpy) {
		fprintf(stderr, "Can't open display %s\n", s);
		return 1;
	}

	if (!button) {
		button = 3;
	}

	accurate_stroke_sequence = (char *) malloc(
			sizeof(char) * (MAX_STROKE_SEQUENCE + 1));
	accurate_stroke_sequence[0] = '\0';

	fuzzy_stroke_sequence = (char *) malloc(
			sizeof(char) * (MAX_STROKE_SEQUENCE + 1));
	fuzzy_stroke_sequence[0] = '\0';

	int err = 0;
	int scr = DefaultScreen(dpy);

	XAllowEvents(dpy, AsyncBoth, CurrentTime);

	if (!without_brush) {
		err = backing_init(&backing, dpy, DefaultRootWindow(dpy),
				DisplayWidth(dpy, scr), DisplayHeight(dpy, scr),
				DefaultDepth(dpy, scr));
		if (err) {
			fprintf(stderr, "cannot open backing store.... \n");
			return err;
		}

		err = brush_init(&brush, &backing);
		if (err) {
			fprintf(stderr, "cannot init brush.... \n");
			return err;
		}
	}

	/* last, start grabbing the pointer ...*/

	int res = grab_pointer(dpy);
	if (res == 0) {
		err = -1;
	}

	return err;
}

/**
 * Execute an action
 */
void execute_action(struct action *action) {

	assert(action);

	int pid = -1;

	switch (action->type) {
	case ACTION_EXECUTE:

		if ((pid = vfork()) == 0) {
			system(action->data);
			//execl(action->original_str, NULL); /* after a successful execl the parent should be resumed */
			_exit(127); /* terminate the child in case execl fails */
		}

		break;
	case ACTION_ICONIFY:
		grabbing_iconify();
		break;
	case ACTION_KILL:
		grabbing_kill();
		break;
	case ACTION_RAISE:
		grabbing_raise();
		break;
	case ACTION_LOWER:
		grabbing_lower();
		break;
	case ACTION_MAXIMIZE:
		grabbing_maximize();
		break;
	case ACTION_ROOT_SEND:
		grabbing_root_send(action->data);
		break;
	default:
		fprintf(stderr, "found an unknown gesture \n");
	}

	return;
}

void grabbing_run() {

	struct captured_movements *grabbed = NULL;

	grabbed = grabbing_capture_movements();

	if (grabbed) {

		printf("\n");
		printf("Window Title = \"%s\"\n", grabbed->window_title);
		printf("Window Class = \"%s\"\n", grabbed->window_class);

		char * sequence = grabbed->advanced_movements;
		struct gesture * gesture = gesture_match(sequence,
				grabbed->window_class, grabbed->window_title);

		if (!gesture) {
			char * sequence = grabbed->basic_movements;
			gesture = gesture_match(sequence, grabbed->window_class,
					grabbed->window_title);
		}

		if (!gesture) {
			printf("Captured sequences %s or %s --> not found\n",
					grabbed->advanced_movements, grabbed->basic_movements);
		} else {
			printf(
					"Captured sequence: '%s' --> Movement '%s' --> Gesture '%s'\n",
					sequence, gesture->movement->name, gesture->name);

			int j = 0;

			for (j = 0; j < gesture->actions_count; ++j) {
				struct action * a = gesture->actions[j];
				printf(" (%s)\n", a->data);
				execute_action(a);
			}

		}

		free_captured_movements(grabbed);
	}

}

void grabbing_iconify() {
	generic_iconify(dpy, get_focused_window(dpy));
}

void grabbing_kill() {
	generic_kill(dpy, get_focused_window(dpy));
}

void grabbing_raise() {
	generic_raise(dpy, get_focused_window(dpy));
}

void grabbing_lower() {
	generic_lower(dpy, get_focused_window(dpy));
}

void grabbing_maximize() {
	generic_maximize(dpy, get_focused_window(dpy));
}

void grabbing_root_send(char *keys) {
	generic_root_send(dpy, keys);
}

void grabbing_finalize() {
	if (!without_brush) {
		brush_deinit(&brush);
		backing_deinit(&backing);
	}

	XCloseDisplay(dpy);
	return;
}

