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
#include <signal.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <getopt.h>
#include "drawing-brush.h"
#include "helpers.h"
#include "gestures.h"
#include "wm.h"
#include "drawing-brush-image.h"

#define DELTA_MIN	20
#define MOUSEGESTURE_MAX_VARIANCE  10

/* the movements */
enum DIRECTIONS {
	NONE, LEFT, RIGHT, UP, DOWN, ONE, THREE, SEVEN, NINE
};

/* Names of movements (will consider the initial letters on the config file) */
char *gesture_names[] = { "NONE", "LEFT", "RIGHT", "UP", "DOWN", "1", "3", "7",
		"9" };

/* close xgestures */
int shut_down = 0;

/* the modifier key (TODO: Re-use this parameter) */
int button_modifier;

/*  */
char *button_modifier_str;

/*  */
int button;

/* arquivo de configuração */
char conf_file[4096];

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

/* carregar como processo */
int is_daemonized = 0;

/* Initial position of the movement (algorithm 1) */
int old_x = -1;
int old_y = -1;

/* Initial position of the movement (algorithm 2) */
int old_x_2 = -1;
int old_y_2 = -1;

/* display */
Display *dpy;

/* movements stack (first capture algoritm) */
EMPTY_STACK(accurate_stroke_sequence);

/* movements stack (secound capture algoritm) */
EMPTY_STACK(fuzzy_stroke_sequence);

XButtonEvent first_click;
struct wm_helper *wm_helper;

/* back of the draw */
backing_t backing;
brush_t brush;

/**
 * clear the two gesture stacks
 */
void clear_stroke_sequence(struct stack *stroke_sequence) {
	while (!is_empty(stroke_sequence))
		pop(stroke_sequence);
	return;
}

/**
 * add a stroke to the stroke sequence.
 */
int push_stroke(int stroke, struct stack* stroke_sequence) {
	int last_stroke = (int) peek(stroke_sequence);
	if (last_stroke != stroke) {
		if (stroke != NONE) {
			push((void *) stroke, stroke_sequence);
		}
		return 1;
	} else {
		return 0;
	}
}

/**
 * clean variables and get a transparent background to draw the movement
 */

void start_grab(XButtonEvent *e) {

	// clear captured sequences
	clear_stroke_sequence(&accurate_stroke_sequence);
	clear_stroke_sequence(&fuzzy_stroke_sequence);

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

char * stroke_sequence_to_str(struct stack * stroke_sequence) {

	int gest_num = stack_size(stroke_sequence);

	char * gest_str = (char *) malloc(sizeof(char) * (gest_num + 1));
	bzero(gest_str, sizeof(char) * (gest_num + 1));

	int i;
	for (i = 0; i < gest_num && !is_empty(stroke_sequence); i++) {
		int stroke = (int) pop(stroke_sequence);
		gest_str[gest_num - i - 1] = gesture_names[stroke][0];
	};

	gest_str[gest_num] = '\0';

	return gest_str;

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

	char * accurate_stroke_str = stroke_sequence_to_str(
			&accurate_stroke_sequence);
	char * fuzzy_stroke_str = stroke_sequence_to_str(&fuzzy_stroke_sequence);

	if ((strcmp("", fuzzy_stroke_str) == 0)
			&& (strcmp("", accurate_stroke_str) == 0)) {

		int err = XUngrabButton(e->display, 3, button_modifier,
		RootWindow (e->display, 0));

		mouseClick(e->display, e->window, button);
		grab_pointer(e->display);

	} else {

		struct window_info * activeWindow = getWindowInfo(first_click.display);

		// sends the both strings to process.
		process_movement_sequences(first_click.display, activeWindow,
				accurate_stroke_str, fuzzy_stroke_str);

	}

	return;
}

int get_accurated_stroke(int x_delta, int y_delta) {

	if ((x_delta == 0) && (y_delta == 0)) {
		return NONE;
	}

	// check if the movement is near main axes
	if ((x_delta == 0) || (y_delta == 0) || (x_delta / y_delta > 3)
			|| (y_delta / x_delta > 3)) {

		// x axes
		if (abs(x_delta) > abs(y_delta)) {

			if (x_delta > 0) {
				return RIGHT;
			} else if (x_delta < 0) {
				return LEFT;
			}

			// y axes
		} else {

			if (y_delta > 0) {
				return DOWN;
			} else if (y_delta < 0) {
				return UP;
			}

		}

		// diagonal axes
	} else {

		if (y_delta < 0) {
			if (x_delta < 0) {
				return SEVEN;
			} else if (x_delta > 0) { // RIGHT
				return NINE;
			}
		} else if (y_delta > 0) { // DOWN
			if (x_delta < 0) { // RIGHT
				return ONE;
			} else if (x_delta > 0) {
				return THREE;
			}
		}

	}

}

int get_fuzzy_stroke(int x_delta, int y_delta) {

	if (abs(y_delta) > abs(x_delta)) {
		if (y_delta > 0) {
			return DOWN;
		} else {
			return UP;
		}

	} else {
		if (x_delta > 0) {
			return RIGHT;
		} else {
			return LEFT;
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

	int x_delta = new_x - old_x;
	int y_delta = new_y - old_y;

	int stroke = get_accurated_stroke(x_delta, y_delta);

	// check minimum distance
	int square_distance = x_delta * x_delta + y_delta * y_delta;

	if (square_distance > DELTA_MIN * DELTA_MIN) {

		// grab stroke
		push_stroke(stroke, &accurate_stroke_sequence);

		// reset start position
		old_x = new_x;
		old_y = new_y;

	}

	int x_delta_2 = new_x - old_x_2;
	int y_delta_2 = new_y - old_y_2;

	int fuzzy_stroke = get_fuzzy_stroke(x_delta_2, y_delta_2);

	int square_distance_2 = x_delta_2 * x_delta_2 + y_delta_2 * y_delta_2;

	if ( DELTA_MIN * DELTA_MIN < square_distance_2) {
		// grab stroke

		push_stroke(fuzzy_stroke, &fuzzy_stroke_sequence);

		// reset start position
		old_x_2 = new_x;
		old_y_2 = new_y;
	}

	return;
}

void event_loop(Display *dpy) {
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
	wm_helper = &generic_wm_helper;

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
				sym2 = XKeycodeToKeysym(dpy, mod->modifiermap[i], j);
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

int init(Display *dpy) {
	int err = 0;
	int scr;

	/* set button modifier */
	button_modifier = str_to_modifier(button_modifier_str);
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

	err = init_gestures(conf_file);
	if (err) {
		fprintf(stderr, "cannot init gestures.... \n");
		return err;
	}

	/* choose a wm helper */
	init_wm_helper();

	/* last, start grabbing the pointer ...*/
	grab_pointer(dpy);
	return err;
}

int end() {
	if (!without_brush) {
		brush_deinit(&brush);
		backing_deinit(&backing);
	}
	return 0;
}

void parse_brush_color(char *color) {
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
		printf("no such color, %s. using \"blue\"\n");
	return;

}

void usage() {
	printf("\n");
	printf(
			"mygestures %s. Credits: Nir Tzachar (xgestures) & Lucas Augusto Deters\n",
			VERSION);
	printf("\n");
	printf("-h, --help\t: print this usage info\n");
	printf(
			"-c, --config\t: set config file. Defaults: $HOME/.config/mygestures/mygestures.conf /etc/mygestures.conf");
	printf("-b, --button\t: which button to use. default is 3\n");
	printf("-d, --daemonize\t: laymans daemonize\n");
	printf("-m, --modifier\t: which modifier to use. valid values are: \n");
	printf("\t\t  CTRL, SHIFT, ALT, WIN, CAPS, NUM, AnyModifier \n");
	printf("\t\t  default is SHIFT\n");
	printf(
			"-l, --brush-color\t: choose a brush color. available colors are:\n");
	printf("\t\t\t  yellow, white, red, green, purple, blue (default)\n");
	printf("-w, --without-brush\t: don't paint the gesture on screen.\n");
	exit(0);
}

int handle_args(int argc, char**argv) {
	char opt;
	char *home;
	static struct option opts[] = { { "help", 0, 0, 'h' },
			{ "button", 1, 0, 'b' }, /*{ "modifier", 1, 0, 'm' },*/{
					"without-brush", 0, 0, 'w' }, { "config", 1, 0, 'c' }, {
					"daemonize", 0, 0, 'd' }, { "brush-color", 1, 0, 'l' }, { 0,
					0, 0, 0 } };

	button = Button3;
	button_modifier = valid_masks[SHIFT]; //AnyModifier;
	button_modifier_str = "AnyModifier";

	home = getenv("HOME");
	sprintf(conf_file, "%s/.config/mygestures/mygestures.conf", home);

	while (1) {
		opt = getopt_long(argc, argv, "h::b:m:c:l:wdr", opts, NULL);
		if (opt == -1)
			break;

		switch (opt) {
		case 'h':
			usage();
			break;
		case 'b':
			button = atoi(optarg);
			break;
			/*case 'm':
			 button_modifier_str = strdup(optarg);
			 break;*/
		case 'c':
			strncpy(conf_file, optarg, 4096);
			break;
		case 'w':
			without_brush = 1;
			break;
		case 'd':
			is_daemonized = 1;
			break;
		case 'l':
			parse_brush_color(optarg);
			break;
		}

	}

	return 0;
}

void sighup(int a) {
	init_gestures(conf_file);
	return;
}

void sigchld(int a) {
	int err;
	waitpid(-1, &err, WNOHANG);
	return;
}

void daemonize() {
	int i;

	i = fork();
	if (i != 0)
		exit(0);

	i = chdir("/");
	return;
}

int main(int argc, char **argv) {

	char *s;

	handle_args(argc, argv);
	if (is_daemonized)
		daemonize();

	signal(SIGHUP, sighup);
	signal(SIGCHLD, sigchld);
	s = XDisplayName(NULL);
	dpy = XOpenDisplay(s);
	if (NULL == dpy) {
		printf("%s: can't open display %s\n", argv[0], s);
		exit(0);
	}
	init_masks(dpy);

	init(dpy);

	event_loop(dpy);

	end();
	XUngrabPointer(dpy, CurrentTime);
	XCloseDisplay(dpy);
	return 0;

}
