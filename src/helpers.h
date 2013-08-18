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

#ifndef __HELPERS
#define __HELPERS

#include <X11/Xlib.h>

struct link {
        void *data;
        struct link *next;
};

struct stack{
        struct link *head;
        int size;
};

#define EMPTY_STACK(name) struct stack name = { \
  .head = NULL, \
 .size = 0, \
  }


struct link *alloc_link(void *data);
void free_link(struct link *free_me);
void push(void *data, struct stack *stack);
void *pop(struct stack *stack);
void *peek(struct stack *stack);
int is_empty(struct stack *stack);
struct stack *init_stack(struct stack *);
int stack_size(struct stack *);
Window find_deepest_window(Display *dpy, Window grandfather, Window parent,
                           int x, int y);
Window get_window(XButtonEvent *ev, int get_frame);

#endif
