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

int grabbing_init();
void grabbing_set_button(int b);
void grabbing_set_without_brush(int b);
void grabbing_set_brush_color(char * color);

capture * grabbing_capture();

void grabbing_free_grabbed_information(capture *free_me);

void grabbing_finalize();


void grabbing_iconify();
void grabbing_kill();
void grabbing_raise();
void grabbing_lower();
void grabbing_maximize();
void grabbing_root_send(char * keys);

