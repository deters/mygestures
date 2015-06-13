/*
 * grabbing.h
 *
 *  Created on: Aug 31, 2013
 *      Author: deters
 */

#ifndef GRABBING_H_
#define GRABBING_H_
#endif /* GRABBING_H_ */

#include "gestures.h"
#include <X11/Xlib.h>
#include "drawing-brush-image.h"
#include "drawing-brush.h"



/*
 * 	Display * dpy = NULL;

	int button = 0;

	int without_brush = 0;

	int old_x = -1;
	int old_y = -1;

	int old_x_2 = -1;
	int old_y_2 = -1;

	char * accurate_stroke_sequence;

	char * fuzzy_stroke_sequence;

	backing_t backing;
	brush_t brush;
 *
 */

typedef struct _grabbing {
	Display * dpy;
	int button ;
	int without_brush ;
	int old_x;
	int old_y;
	int old_x_2;
	int old_y_2;
	char * accurate_stroke_sequence;
	char * fuzzy_stroke_sequence;
	backing_t * backing;
	brush_t * brush;
	struct brush_image_t * brush_image;
} grabbing;

grabbing * grabbing_new();

int grabbing_prepare(grabbing * self);
void grabbing_set_button(grabbing * self, int b);
void grabbing_set_without_brush(grabbing * self, int b);
void grabbing_set_brush_color(grabbing * self, char * color);

capture * grabbing_capture(grabbing * self);

void grabbing_free_capture(capture *free_me);

void grabbing_finalize(grabbing * self);


void grabbing_iconify(grabbing * self);
void grabbing_kill(grabbing * self);
void grabbing_raise(grabbing * self);
void grabbing_lower(grabbing * self);
void grabbing_maximize(grabbing * self);
void grabbing_root_send(grabbing * self, char * keys);

