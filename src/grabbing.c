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
#include "brush.h"
#include "helpers.h"
#include "gestures.h"
#include "wm.h"
#include "brush-image.h"

#define DELTA_MIN 20
enum {LEFT=1, RIGHT, UP, DOWN};
char *gesture_names[] = {"NULL", "LEFT", "RIGHT", "UP", "DOWN"};
int shut_down = 0;
/* this is the modifier used to catch clicks...*/
int button_modifier;
char *button_modifier_str;
/* the button used */
int button;
/* the conf file */
char conf_file[4096];
/* use a brush?? */
int without_brush = 0;
/* should the pointer be wrapped after a gesture?? */
int dont_wrap_pointer=0;

/* the key masks */
enum {SHIFT=0, CTRL, ALT, WIN, SCROLL, NUM, CAPS, MOD_END};
unsigned int valid_masks[MOD_END];
char *modifiers_names[MOD_END] = {"SHIFT", "CTRL", "ALT", "WIN", "SCROLL", "NUM", "CAPS"};

/* should we daemonize??*/
int is_daemonized = 0;

int old_x=-1;
int old_y=-1;

EMPTY_STACK(gestures_stack);
XButtonEvent first_click;
struct wm_helper *wm_helper;

backing_t backing;
brush_t brush;

void empty_gestures()
{
        while (! is_empty(&gestures_stack))
                pop(&gestures_stack);
        return;
}

void push_gesture(int ges)
{
        int last_ges = (int) peek(&gestures_stack);
        if (last_ges != ges)
                push((void *) ges, &gestures_stack);
        return;
}

void start_grab(XEvent *e)
{
        XButtonEvent *ev = (XButtonEvent *)e;
        empty_gestures();
        
        old_x= ev->x_root;
        old_y= ev->y_root;


        memcpy(&first_click, ev, sizeof(XButtonEvent));
        
        if (!without_brush){
                XGrabServer(ev->display);
                backing_save(&backing,
                             ev->x_root - brush.image_width,
                             ev->y_root - brush.image_height);
                backing_save(&backing,
                             ev->x_root + brush.image_width,
                             ev->y_root + brush.image_height);
                
                brush_draw(&brush, old_x, old_y);
        }
        /* printf("starting grab\n"); */
        return;
}


void stop_grab(XEvent *e)
{
        int gest_num = stack_size(&gestures_stack);
        char *gest_str;
        int i;
        XButtonEvent *ev = (XButtonEvent *) e;

        if (gest_num <= 0){
                return;
        }

        /* focus on the starting window */
        XWarpPointer(first_click.display, None, first_click.window, 0,0,0,0, first_click.x, first_click.y);

        gest_str = (char *) malloc(sizeof(char)*(gest_num+1));
        bzero(gest_str, sizeof(char)*(gest_num+1));

        for(i=0; i<gest_num &&!is_empty(&gestures_stack) ; i++){
                int ges = (int) pop(&gestures_stack);
                gest_str[gest_num-i-1] = gesture_names[ges][0];
        }
        gest_str[gest_num] = '\0';

        if (!without_brush){
                backing_restore(&backing);
                XSync(ev->display, False);
                XUngrabServer(ev->display);
        }

        process_gestures(&first_click, gest_str);

        if (! dont_wrap_pointer){
                XWarpPointer(ev->display, None, ev->window, 0,0,0,0, ev->x, ev->y);
        }

        free(gest_str);
        return;
}

void process_move(XEvent *e)
{
        int x_delta, y_delta;
        int new_x, new_y;
        int r = 0;

        
        XMotionEvent *ev = (XMotionEvent *) e;

        new_x = ev->x_root;
        new_y = ev->y_root;
        if (old_x == -1){
                old_x = new_x;
                old_y = new_y;
                return;
        }

        x_delta = new_x - old_x;
        y_delta = new_y - old_y;

        r = x_delta*x_delta + y_delta*y_delta;
        if (r < DELTA_MIN*DELTA_MIN)
                return;

        if (2*x_delta*x_delta <= r){
                if (y_delta > 0){
                        push_gesture(DOWN);
                        /* printf("down\n"); */
                } else {
                        push_gesture(UP);
                        /* printf("up\n"); */
                }
                
        } else {
                if (x_delta > 0){
                        push_gesture(RIGHT);
                        /* printf("rigth \n"); */
                } else {
                        push_gesture(LEFT);
                        /* printf("left\n"); */
                }
                        
                
        }

        old_x = new_x;
        old_y = new_y;

        /* printf("changed to: %d,%d \n", old_x, old_y); */

        if (!without_brush){
                backing_save(&backing,
                             ev->x_root - brush.image_width,
                             ev->y_root - brush.image_height);
                backing_save(&backing,
                             ev->x_root + brush.image_width,
                             ev->y_root + brush.image_height);
                brush_line_to(&brush, ev->x_root, ev->y_root);
        }
        
        return;
}





void event_loop(Display *dpy)
{
        XEvent e;
        while (!shut_down)
        {
                XNextEvent (dpy, &e);
                /* XWindowEvent(dpy, ButtonPressMask|ButtonReleaseMask|PointerMotionMask, &e); */
                switch(e.type){
                case ButtonPress:
                        start_grab(&e);
                        break;
                case ButtonRelease:
                        stop_grab(&e);
                        break;
                case MotionNotify:
                        process_move(&e);
                        break;
                        
                /* default: */
                        /* printf("got an event of type %d\n", e.type); */

                }

        }


}

int init_wm_helper(void)
{
        wm_helper = &generic_wm_helper;
        
        return 1;
}


/* taken from ecore.. */
int x_key_mask_get(KeySym sym, Display *dpy)
{
        XModifierKeymap    *mod;
        KeySym              sym2;
        int                 i, j;
        const int           masks[8] =
                {
                        ShiftMask, LockMask, ControlMask,
                        Mod1Mask, Mod2Mask, Mod3Mask, Mod4Mask, Mod5Mask
                };
        
        mod = XGetModifierMapping(dpy);
        if ((mod) && (mod->max_keypermod > 0))
        {
                for (i = 0; i < (8 * mod->max_keypermod); i++)
                {
                        for (j = 0; j < 8; j++)
                        {
                                sym2 = XKeycodeToKeysym(dpy, mod->modifiermap[i], j);
                                if (sym2 != 0) break;
                        }
                        if (sym2 == sym)
                        {
                                int mask;
                                
                                mask = masks[i / mod->max_keypermod];
                                if (mod->modifiermap) XFree(mod->modifiermap);
                                XFree(mod);
                                return mask;
                        }
                }
        }
        if (mod)
        {
                if (mod->modifiermap) XFree(mod->modifiermap);
                XFree(mod);
        }
        return 0;
}


void init_masks(Display *dpy)
{
        valid_masks[SHIFT] = x_key_mask_get(XK_Shift_L, dpy);
        valid_masks[CTRL]  = x_key_mask_get(XK_Control_L, dpy);
        
        /* apple's xdarwin has no alt!!!! */
        valid_masks[ALT]   = x_key_mask_get(XK_Alt_L, dpy);
        if (!valid_masks[ALT])
                valid_masks[ALT] = x_key_mask_get(XK_Meta_L, dpy);
        if (!valid_masks[ALT])
                valid_masks[ALT] = x_key_mask_get(XK_Super_L, dpy);
        
        /* the windows key... a valid modifier :) */
        valid_masks[WIN]   = x_key_mask_get(XK_Super_L, dpy);
        if (!valid_masks[WIN])
                valid_masks[WIN] = x_key_mask_get(XK_Mode_switch, dpy);
        if (!valid_masks[WIN])
                valid_masks[WIN] = x_key_mask_get(XK_Meta_L, dpy);
        
        valid_masks[SCROLL]    = x_key_mask_get(XK_Scroll_Lock, dpy);
        valid_masks[NUM]       = x_key_mask_get(XK_Num_Lock, dpy);
        valid_masks[CAPS]      = x_key_mask_get(XK_Caps_Lock, dpy);

}

void print_bin(unsigned int a)
{
        char str[33];
        int i=0;
        for (; i<32; i++){             
        

                if (a & (1<<i))
                        str[i] = '1';
                else
                        str[i] = '0';
        }
        str[32] = 0;
        printf("%s\n", str);
}

void create_masks(unsigned int *arr)
{
        unsigned int i,j;

        for (i=0; i< (1<<(MOD_END)); i++){
                arr[i]=0;
                for (j=0; j<MOD_END; j++){
                        if ( (1<<j)&i){
                                arr[i] |= valid_masks[j];
                        }
                }
                /* print_bin(arr[i]); */
        }

        
        
        return;
}


int grab_pointer(Display *dpy)
{
        int err = 0, i=0;
        int screen = 0;
        unsigned int masks[(1<<(MOD_END))];
        bzero(masks, (1<<(MOD_END))*sizeof(unsigned int));

        if (button_modifier != AnyModifier)
                create_masks(masks);
        
        for (screen = 0; screen < ScreenCount (dpy); screen++)
        {
                for(i=1; i< (1<<(MOD_END)); i++)
                        err = XGrabButton(dpy, button, button_modifier |masks[i] ,
                                          RootWindow (dpy, screen),
                                          False, PointerMotionMask| ButtonReleaseMask| ButtonPressMask,
                                          GrabModeAsync, GrabModeAsync, None, None);
        }

        
        return 0;
}

unsigned int str_to_modifier(char *str)
{
        int i;

        if (str == NULL){
                fprintf(stderr, "no modifier supplied.\n");
                exit(-1);
        }

        if (strncasecmp(str, "AnyModifier", 11)==0)
                return AnyModifier;
        
        for (i=0; i< MOD_END; i++)
                if (strncasecmp(str,
                                modifiers_names[i],
                                strlen(modifiers_names[i])) ==0)
                        return valid_masks[i];
        /* no match... */
        return valid_masks[SHIFT];
}



int init(Display *dpy)
{
        int err = 0;
        int scr;

        /* set button modifier */
        button_modifier = str_to_modifier(button_modifier_str);
        XAllowEvents (dpy, AsyncBoth, CurrentTime);

        
        scr = DefaultScreen(dpy);

        if (!without_brush){
                err = backing_init(&backing, dpy, DefaultRootWindow(dpy),
                                   DisplayWidth(dpy, scr), DisplayHeight(dpy, scr),
                                   DefaultDepth(dpy, scr));
                if (err){
                        fprintf(stderr, "cannot open backing store.... \n");
                        return err;
                }
                
                err =  brush_init(&brush, &backing);
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

int end()
{
        if (!without_brush){
                brush_deinit(&brush);
                backing_deinit(&backing);
        }
        return 0;
}


void parse_brush_color(char *color){
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
	else printf("no such color, %s. using \"blue\"\n");
	return;

}

void usage()
{
        printf("\n");
        printf("xgestures %s . Author: Nir Tzachar.\n",VERSION);
        printf("\n");
        printf("-h, --help\t: print this usage info\n");
        printf("-b, --button\t: which button to use. default is 3\n");
        printf("-c, --config\t: which config file to use. default: ~/.gestures\n");
        printf("-d, --daemonize\t: laymans daemonize\n");
        printf("-r, --dont-wrap-pointer\t: dont wrap the pointer after a gesture is done \n");
        printf("-m, --modifier\t: which modifier to use. valid values are: \n");
        printf("\t\t  CTRL, SHIFT, ALT, WIN, CAPS, NUM, AnyModifier \n");
        printf("\t\t  default is SHIFT\n");
        printf("-l, --brush-color\t: choose a brush color. available colors are:\n");
	printf("\t\t\t  yellow, white, red, green, purple, blue (default)\n");
        printf("-w, --without-brush\t: don't paint the gesture on screen.\n");
        exit(0);
}

int handle_args(int argc, char**argv)
{
        char opt;
        char *home;
        static struct option opts[] = {
                {"help", 0, 0, 'h'},
                {"button", 1, 0, 'b'},
                {"modifier", 1, 0, 'm'},
                {"without-brush", 0, 0, 'w'},
                {"config", 1, 0, 'c'},
                {"daemonize",0,0,'d'},
                {"dont-wrap-pointer",0,0,'r'},
                {"brush-color",1,0,'l'},
                {0,0,0,0}};
               
        button = Button3;        
        button_modifier = valid_masks[SHIFT]; //AnyModifier;
        button_modifier_str = "Shift";
        
        home = getenv("HOME");
        sprintf(conf_file,"%s/.gestures",home);

        while(1){
                opt = getopt_long(argc, argv, "h::b:m:c:l:wdr", opts, NULL);
                if (opt == -1)
                        break;
                
                switch(opt){
                case 'h':
                        usage();
                        break;
                case 'b':
                        button = atoi(optarg);
                        break;
                case 'm':
                        button_modifier_str = strdup(optarg);
                        break;
                case 'c':
                        strncpy(conf_file, optarg, 4096);
                        break;
                case 'w':
                        without_brush = 1;
                        break;
                case 'd':
                        is_daemonized = 1;
                        break;
                case 'r':
                        dont_wrap_pointer = 1;
                        break;
		case 'l':
			parse_brush_color(optarg);
			break;
                }



        }


        return 0;
}


void sighup(int a)
{
        init_gestures(conf_file);
        return;
}

void sigchld(int a)
{
        int err;
        waitpid(-1, &err, WNOHANG);
        return;
}

void daemonize()
{
        int i;
        
        i = fork();
        if (i != 0)
                exit(0);

        chdir("/");
        return;
}

int main(int argc, char **argv)
{
        Display *dpy;
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
