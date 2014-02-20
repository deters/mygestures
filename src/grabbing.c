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
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XTest.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <getopt.h>
#include <math.h>
#include "drawing-brush.h"
#include "grabbing.h"
#include "gestures.h"
#include "drawing-brush-image.h"

#define DELTA_MIN	30 /*TODO*/
#define MAX_STROKE_SEQUENCE 63 /*TODO*/

Display * dpy = NULL;


/* the button to grab */
int button = 0;

/* the modifier key (TODO: REVIEW) */
unsigned int button_modifier = 0;

/* Not draw the movement on the screen */
int without_brush = 0;

/* modifier keys */
enum {
	SHIFT = 0, CTRL, ALT, WIN, SCROLL, NUM, CAPS, MOD_END
};

/* names of the modifier keys */
char *modifiers_names[MOD_END] = { "SHIFT", "CTRL", "ALT", "WIN", "SCROLL",
		"NUM", "CAPS" };

/* Filter to capture the events of the mouse */
unsigned int valid_masks[MOD_END];


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

/* close xgestures */
int shut_down = 0;





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

void create_masks(unsigned int *arr) {
	unsigned int i, j;

	for (i = 0; i < (1 << (MOD_END)); i++) {
		arr[i] = 0;
		for (j = 0; j < MOD_END; j++) {
			if ((1 << j) & i) {
				arr[i] |= valid_masks[j];
			}
		}

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
	int result = 0, i = 0;
	int screen = 0;
	unsigned int masks[(1 << (MOD_END))];
	bzero(masks, (1 << (MOD_END)) * sizeof(unsigned int));

	if (button_modifier != AnyModifier)
		create_masks(masks);

	for (screen = 0; screen < ScreenCount (dpy); screen++) {
		for (i = 1; i < (1 << (MOD_END)); i++)
			result += XGrabButton(dpy, button, /*AnyModifier */
			button_modifier | masks[i],
			RootWindow (dpy, screen),
			False,
			PointerMotionMask | ButtonReleaseMask | ButtonPressMask,
			GrabModeAsync, GrabModeAsync, None, None);
	}

	return result;
}


int ungrab_pointer(Display *dpy) {
	int result = 0, i = 0;
	int screen = 0;
	unsigned int masks[(1 << (MOD_END))];
	bzero(masks, (1 << (MOD_END)) * sizeof(unsigned int));

	if (button_modifier != AnyModifier)
		create_masks(masks);

	for (screen = 0; screen < ScreenCount (dpy); screen++) {
		for (i = 1; i < (1 << (MOD_END)); i++)

			result += XUngrabButton(dpy, button, button_modifier | masks[i],
					RootWindow (dpy, screen));
	}

	return result;
}


/*
 * Emulate a mouse click at the given display.
 *
 * PRIVATE
 */
void mouse_click(Display *display, int button) {
	XTestFakeButtonEvent(display, button, True, CurrentTime);
	sleep(0.001);
	XTestFakeButtonEvent(display, button, False, CurrentTime);
}

/**
 * Obtém o resultado dos dois algoritmos de captura de movimentos, e envia para serem processadas.
 */
void end_movement(XButtonEvent *e) {

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

		int sequences_count = 2;
		char ** sequences = malloc(sizeof(char *)*sequences_count);

		sequences[0] = accurate_stroke_sequence;
		sequences[1] = fuzzy_stroke_sequence;

		// sends the both strings to process.
		gesture_process_movement(dpy,
				 sequences, sequences_count);

	}



	return;
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

void grabbing_event_loop() {
	XEvent e;

	while ((!shut_down)) {

		XNextEvent(dpy, &e);

		switch (e.type) {

		case MotionNotify:
			update_movement((XMotionEvent *) &e);
			break;

		case ButtonPress:
			start_movement((XButtonEvent *) &e);
			break;

		case ButtonRelease:
			end_movement((XButtonEvent *) &e);
			break;

		}

	}

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

	init_masks(dpy);


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
	grab_pointer(dpy);

	return err;
}

void grabbing_finalize() {
	if (!without_brush) {
		brush_deinit(&brush);
		backing_deinit(&backing);
	}

	XCloseDisplay(dpy);
	return;
}

