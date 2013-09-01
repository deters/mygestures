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
/*
 - recognize movements without a modifier key                                            - 13 JAN 2008   OK
 - gestures customized for each application                                              - 02 MAR 2008   OK
 - emule a click on Java applications                                                    - 02 MAR 2008   (not complete)
 - custom moviment definition on .gestures                                              - 14 MAR 2008   OK
 TODO:
 - disable gesture recognition on some apps
 - translate and review the source code
 - create a GUI
 - quick icon on the taskbar (with options: inactivate xgestures, automatic start, open configure gui)
 - Translate the GUI
 - Store the configurations on XML
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <getopt.h>
#include <math.h>
#include "drawing-brush.h"
#include "helpers.h"
#include "grabbing.h"
#include "gestures.h"
#include "wm.h"
#include "drawing-brush-image.h"

#define DELTA_MIN	30
#define MAX_STROKE_SEQUENCE 63

//struct grabbing {

int button = 0;

unsigned int button_modifier = 0;

//};

/* the movements */
enum DIRECTION {
	NONE, LEFT, RIGHT, UP, DOWN, ONE, THREE, SEVEN, NINE
};

/* Names of movements (will consider the initial letters on the config file) */
char gesture_names[] = { 'N', 'L', 'R', 'U', 'D', '1', '3', '7', '9' };

/* the modifier key (TODO: Re-use this parameter) */

/* Not draw the movement on the screen */
int without_brush = 0;

/* modifier keys */
enum {
	SHIFT = 0, CTRL, ALT, WIN, SCROLL, NUM, CAPS, MOD_END
};

/* Filter to capture the events of the mouse */
unsigned int valid_masks[MOD_END];

/* names of the modifier keys */
char *modifiers_names[MOD_END] = { "SHIFT", "CTRL", "ALT", "WIN", "SCROLL",
		"NUM", "CAPS" };

/* Initial position of the movement (algorithm 1) */
int old_x = -1;
int old_y = -1;

/* Initial position of the movement (algorithm 2) */
int old_x_2 = -1;
int old_y_2 = -1;


/* movements stack (first capture algoritm) */
char * accurate_stroke_sequence;

/* movements stack (secound capture algoritm) */
char * fuzzy_stroke_sequence;

XButtonEvent first_click;
struct action_helper *action_helper;

/* back of the draw */
backing_t backing;
brush_t brush;

/* close xgestures */
int shut_down = 0;

/**
 * clean variables and get a transparent background to draw the movement
 */

void start_grab(XButtonEvent *e) {

	// clear captured sequences
	accurate_stroke_sequence[0] = '\0';
	fuzzy_stroke_sequence[0] = '\0';

	// guarda o evento inicial
	memcpy(&first_click, e, sizeof(XButtonEvent));

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

void create_masks(unsigned int *arr) {
	unsigned int i, j;

	for (i = 0; i < (1 << (MOD_END)); i++) {
		arr[i] = 0;
		for (j = 0; j < MOD_END; j++) {
			if ((1 << j) & i) {
				arr[i] |= valid_masks[j];
			}
		}
		/* print_bin(arr[i]); */
	}

	return;
}

void grabbing_set_button(int b) {
	button = b;
}


unsigned int str_to_modifier(char *str) {
	int i;

	if (str == NULL) {
		fprintf(stderr, "no modifier supplied.\n");
		exit(-1);
	}

	if (strncasecmp(str, "AnyModifier", 11) == 0)
		return AnyModifier;

	for (i = 0; i < MOD_END; i++)
		if (strncasecmp(str, modifiers_names[i], strlen(modifiers_names[i]))
				== 0)
			return valid_masks[i];
	/* no match... */
	return valid_masks[SHIFT];
}


void grabbing_set_button_modifier(char *button_modifier_str) {

	button_modifier = str_to_modifier(button_modifier_str);

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
	int err = 0, i = 0;
	int screen = 0;
	unsigned int masks[(1 << (MOD_END))];
	bzero(masks, (1 << (MOD_END)) * sizeof(unsigned int));

	if (button_modifier != AnyModifier)
		create_masks(masks);

// em todas as telas ativas
	for (screen = 0; screen < ScreenCount (dpy); screen++) {
		for (i = 1; i < (1 << (MOD_END)); i++)
			// aguarda que o botão direito seja clicado em alguma janela...
			err = XGrabButton(dpy, button, /*AnyModifier */
			button_modifier | masks[i],
			RootWindow (dpy, screen),
			False,
			PointerMotionMask | ButtonReleaseMask | ButtonPressMask,
			GrabModeAsync, GrabModeAsync, None, None);
	}

	return 0;
}

/**
 * Obtém o resultado dos dois algoritmos de captura de movimentos, e envia para serem processadas.
 */
void stop_grab(XButtonEvent *e) {

	// if is drawing
	if (!without_brush) {
		backing_restore(&backing);
		XSync(e->display, False);
	};

	if ((strlen(fuzzy_stroke_sequence) == 0)
			&& (strlen(accurate_stroke_sequence) == 0)) {

		// temporary ungrab button
		XUngrabButton(e->display, 3, button_modifier,
		RootWindow (e->display, 0));

		// emulate the click
		mouse_click(e->display, button);

		// restart grabbing
		grab_pointer(e->display);

	} else {

		struct window_info * activeWindow = generic_get_window_context(
				first_click.display);

		// sends the both strings to process.
		struct gesture * gest = process_movement_sequences(first_click.display,
				activeWindow, accurate_stroke_sequence, fuzzy_stroke_sequence);

		if (gest != NULL) {

			if (gest->action->type == ACTION_EXIT_GEST) {
				shut_down = 1;
			}

			execute_action(first_click.display, gest->action);
		}

	}

	return;
}

char get_accurated_stroke(int x_delta, int y_delta) {

	if ((x_delta == 0) && (y_delta == 0)) {
		return gesture_names[NONE];
	}

	// check if the movement is near main axes
	if ((x_delta == 0) || (y_delta == 0)
			|| (fabs((float) x_delta / (float) y_delta) > 3)
			|| (fabs((float) y_delta / (float) x_delta) > 3)) {

		// x axe
		if (abs(x_delta) > abs(y_delta)) {

			if (x_delta > 0) {
				return gesture_names[RIGHT];
			} else {
				return gesture_names[LEFT];
			}

			// y axe
		} else {

			if (y_delta > 0) {
				return gesture_names[DOWN];
			} else {
				return gesture_names[UP];
			}

		}

		// diagonal axes
	} else {

		if (y_delta < 0) {
			if (x_delta < 0) {
				return gesture_names[SEVEN];
			} else if (x_delta > 0) { // RIGHT
				return gesture_names[NINE];
			}
		} else if (y_delta > 0) { // DOWN
			if (x_delta < 0) { // RIGHT
				return gesture_names[ONE];
			} else if (x_delta > 0) {
				return gesture_names[THREE];
			}
		}

	}

	return gesture_names[NONE];

}

char get_fuzzy_stroke(int x_delta, int y_delta) {

	if (abs(y_delta) > abs(x_delta)) {
		if (y_delta > 0) {
			return gesture_names[DOWN];
		} else {
			return gesture_names[UP];
		}

	} else {
		if (x_delta > 0) {
			return gesture_names[RIGHT];
		} else {
			return gesture_names[LEFT];
		}

	}

}

void push_stroke(char stroke, char* stroke_sequence) {
	// grab stroke
	int len = strlen(stroke_sequence);
	if ((len == 0) || (stroke_sequence[len - 1] != stroke)) {

		if (len < MAX_STROKE_SEQUENCE) {

			stroke_sequence[len] = stroke;
			stroke_sequence[len + 1] = '\0';

		}

	}
}

void process_move(XMotionEvent *e) {

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

		char stroke = get_accurated_stroke(x_delta, y_delta);

		push_stroke(stroke, accurate_stroke_sequence);

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

		push_stroke(fuzzy_stroke, fuzzy_stroke_sequence);

		// reset start position
		old_x_2 = new_x;
		old_y_2 = new_y;
	}

	return;
}

void grabbing_event_loop(Display * dpy) {
	XEvent e;

	while ((!shut_down)) {

		XNextEvent(dpy, &e);

		switch (e.type) {

		case MotionNotify:
			process_move((XMotionEvent *) &e);
			break;

		case ButtonPress:
			start_grab((XButtonEvent *) &e);
			break;

		case ButtonRelease:
			stop_grab((XButtonEvent *) &e);
			break;

		}

	}

}

int init_wm_helper(void) {
	action_helper = &generic_action_helper;

	return 1;
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

				//sym2 = XKeycodeToKeysym(dpy, mod->modifiermap[i], j);

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

void init_masks(Display *dpy) {
	valid_masks[SHIFT] = x_key_mask_get(XK_Shift_L, dpy);
	valid_masks[CTRL] = x_key_mask_get(XK_Control_L, dpy);

	/* apple's xdarwin has no alt!!!! */
	valid_masks[ALT] = x_key_mask_get(XK_Alt_L, dpy);
	if (!valid_masks[ALT])
		valid_masks[ALT] = x_key_mask_get(XK_Meta_L, dpy);
	if (!valid_masks[ALT])
		valid_masks[ALT] = x_key_mask_get(XK_Super_L, dpy);

	/* the windows key... a valid modifier :) */
	valid_masks[WIN] = x_key_mask_get(XK_Super_L, dpy);
	if (!valid_masks[WIN])
		valid_masks[WIN] = x_key_mask_get(XK_Mode_switch, dpy);
	if (!valid_masks[WIN])
		valid_masks[WIN] = x_key_mask_get(XK_Meta_L, dpy);

	valid_masks[SCROLL] = x_key_mask_get(XK_Scroll_Lock, dpy);
	valid_masks[NUM] = x_key_mask_get(XK_Num_Lock, dpy);
	valid_masks[CAPS] = x_key_mask_get(XK_Caps_Lock, dpy);

}

void print_bin(unsigned int a) {
	char str[33];
	int i = 0;
	for (; i < 32; i++) {

		if (a & (1 << i))
			str[i] = '1';
		else
			str[i] = '0';
	}
	str[32] = 0;
	printf("%s\n", str);
}


int grabbing_init(Display *dpy) {

	if (!button) {
		button = 3;
	}

	init_masks(dpy);

	accurate_stroke_sequence = (char *) malloc(
			sizeof(char) * (MAX_STROKE_SEQUENCE + 1));
	accurate_stroke_sequence[0] = '\0';

	fuzzy_stroke_sequence = (char *) malloc(
			sizeof(char) * (MAX_STROKE_SEQUENCE + 1));
	fuzzy_stroke_sequence[0] = '\0';

	int err = 0;
	int scr;

	XAllowEvents(dpy, AsyncBoth, CurrentTime);

	scr = DefaultScreen(dpy);

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


	/* choose a wm helper */
	init_wm_helper();

	/* last, start grabbing the pointer ...*/
	grab_pointer(dpy);

	return err;
}

void grabbing_finalize() {
	if (!without_brush) {
		brush_deinit(&brush);
		backing_deinit(&backing);
	}
	return;
}

