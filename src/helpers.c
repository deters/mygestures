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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xmu/WinUtil.h>
#include "helpers.h"

struct link *alloc_link(void *data)
{
        struct link *ans = malloc(sizeof (struct link));
        bzero(ans, sizeof (struct link));
        ans->data = data;
        return ans;
}

void free_link(struct link *free_me)
{
        free(free_me);
        return;
}

struct stack *init_stack(struct stack *stack)
{
        stack->head = NULL;
        stack->size = 0;
        return stack;
}

void push(void *data, struct stack *stack)
{
        if (stack == NULL){
                printf("ERROR: passing a null stack pointer to %s\n", __func__);
                return;
        }
        struct link *link = alloc_link(data);

        /* this catches both cases of initialized an not initialized stacks..
           assuming stack is initialized with  null!!!*/
        link->next = stack->head;
        stack->head = link;
        stack->size++;
        return;
}
void *pop(struct stack *stack)
{
        struct link *head = stack->head;
        void *ans;
        if (head == NULL){
                printf("ERROR: poping an empty stack\n");
                return NULL;
        }

        stack->head = head->next;
        ans = head->data;
        free_link(head);
        stack->size--;
        return ans;
}

void *peek(struct stack *stack)
{
        struct link *head = stack->head;
        void *ans;
        if (head == NULL){
                return NULL;
        }

        ans = head->data;
        return ans;  

}

int is_empty(struct stack *stack)
{
        return (stack == NULL) || (stack->head == NULL);
}

int stack_size(struct stack *stack)
{
        return stack->size;
}

Window get_window(XButtonEvent *ev, int get_frame)
{
        Window target_win = ev->subwindow;
        if (get_frame)
                return target_win;
        
        if (target_win != None){
                Window root;
                int dummyi;
                unsigned int dummy;
                
                if (XGetGeometry (ev->display, target_win, &root, &dummyi, &dummyi,
                                  &dummy, &dummy, &dummy, &dummy)
                    && target_win != root)
                        target_win = XmuClientWindow (ev->display, target_win);
        }
        
        
        return target_win;
}


