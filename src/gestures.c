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

#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include "gestures.h"
#include "helpers.h"
#include "wm.h"

enum {ACTION_EXIT_GEST=1, ACTION_EXECUTE, ACTION_ICONIFY,
      ACTION_KILL, ACTION_DISPLAY_GESTS, ACTION_RECONF,
      ACTION_RAISE, ACTION_LOWER, ACTION_MAXIMIZE,
      ACTION_ROOT_SEND, ACTION_LAST};
char *gestures_names[] = {"NONE", "exit", "exec", "minimize",
                          "kill", "display_gests", "reconf", "raise",
                          "lower", "maximize", "root_send"};

/* this holds all known gestures */
struct gesture **known_gestures = NULL;
int known_gestures_num = 0;
EMPTY_STACK(temp_stack);

/* conf file: */
extern char conf_file[];


extern int shut_down;

struct gesture *alloc_gesture(char *gest_str, struct action *action)
{
        struct gesture *ans = malloc(sizeof (struct gesture));
        bzero(ans, sizeof (struct gesture));
        ans->gest_str = gest_str;
        ans->action = action;
        return ans;
}

void free_gesture(struct gesture *free_me)
{
        free(free_me->action);
        free(free_me);
        
        return;
}

struct action *alloc_action(int type, void *data)
{
        struct action *ans = malloc(sizeof (struct action));
        bzero(ans, sizeof (struct action));
        ans->type = type;
        ans->data = data;

        return ans;
}

void free_action(struct action *free_me)
{
        free(free_me);
        return;
}

struct key_press *alloc_key_press(void)
{
        struct key_press *ans = malloc(sizeof (struct key_press));
        bzero(ans, sizeof (struct key_press));
        return ans;
}

void free_key_press(struct key_press *free_me)
{
        free(free_me);
        return;
}

int compare_gestures(const void *a, const void *b)
{
        const struct gesture **gest_a = a;
        const struct gesture **gest_b = b;
        
        return strcmp((*gest_a)->gest_str, (*gest_b)->gest_str);
}

void display_all_gestures()
{
        int i;
        int id = fork();
        if (id == 0){
                char *msg;
                int index = 0;
                char *command;
                int msg_len = (GEST_SEQUENCE_MAX+2)*sizeof(char)*known_gestures_num;
                msg = malloc(msg_len);
                bzero(msg, msg_len);
                command = malloc(msg_len + sizeof(char)*256);
                                 
                for (i=0; i< known_gestures_num; i++){
                        struct gesture *gesture = known_gestures[i];
                        index = strlen(msg);
                        if (gesture->action->type == ACTION_EXECUTE)
                                sprintf(&msg[index], "%s %s %s\n",
                                        gesture->gest_str, gestures_names[gesture->action->type],
                                        gesture->action->data);
                        else if (gesture->action->type == ACTION_ROOT_SEND){
                                struct key_press *key = gesture->action->data;
                                if (key == NULL)
                                        continue;
                                sprintf(&msg[index], "%s %s %s\n",
                                        gesture->gest_str, gestures_names[gesture->action->type],
                                        key->original_str);
                        } else sprintf(&msg[index], "%s %s\n",
                                       gesture->gest_str, gestures_names[gesture->action->type]);

                }
                sprintf(command, "echo -e \"%s\" | xmessage -center -file -", msg);
                system(command);
                exit(0);
        }

}


void press_key(Display *dpy, KeySym key, Bool is_press)
{

        XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, key),
                          is_press, CurrentTime);
        return;
}

void root_send(XButtonEvent *ev, struct action *action)
{
        struct key_press *first_key;
        struct key_press *tmp;

        if (action == NULL){
                fprintf(stderr," internal error in %s\n",
                        __func__);
                return;
                
        }
        first_key = (struct key_press *)action->data;

        if (first_key == NULL){
                fprintf(stderr," internal error in %s, key is null\n",
                        __func__);
                return;
        }
        
        for( tmp = first_key; tmp != NULL; tmp = tmp->next)
                press_key(ev->display, tmp->key, True);
        
        for( tmp = first_key; tmp != NULL; tmp = tmp->next)
                press_key(ev->display, tmp->key, False);
        
        return;
}


/* get the action, and the button event which started it all/...
   Window pointed_window points to the window on which the action started
*/
void execute_action(XButtonEvent *ev, struct action *action)
{
        int id;
        
        switch (action->type){
        case ACTION_EXIT_GEST:
                shut_down = True;
                break;
        case ACTION_EXECUTE:
                id = fork();
                if (id == 0){
                        system(action->data);
                        exit(0);
                }
                break;
        case ACTION_ICONIFY:
                wm_helper->iconify(ev);
                break;
        case ACTION_KILL:
                wm_helper->kill(ev);
                break;
        case ACTION_DISPLAY_GESTS:
                display_all_gestures();
                break;
        case ACTION_RECONF:
                init_gestures(conf_file);
                break;
        case ACTION_RAISE:
                wm_helper->raise(ev);
                break;
        case ACTION_LOWER:
                wm_helper->lower(ev);
                break;
        case ACTION_MAXIMIZE:
                wm_helper->maximize(ev);
                break;
        case ACTION_ROOT_SEND:
                root_send(ev, action);
                break;
        default:
                fprintf(stderr, "found an unknown gesture \n");
        }

        return;
}


void process_gestures(XButtonEvent *e, char *gest_str)
{
        struct gesture **gest;
        struct gesture key;
        struct gesture *key_ptr = &key;

        key.gest_str = gest_str;
        
        /* printf("gestures are: %s\n", gest_str); */

        /* search this gesture in the gest array.. */
        gest = bsearch(&key_ptr, known_gestures,
                       known_gestures_num, sizeof(struct gesture *),
                       compare_gestures);

        if (gest == NULL){
                /* printf("found no match to %s\n", gest_str); */
        } else {
                /* printf("found match to %s\n", gest_str); */
                execute_action(e, (*gest)->action);
        }
        
}

void *compile_key_action(char *str_ptr)
{
        struct key_press base;
        struct key_press *key;
        KeySym k;
        char *str = str_ptr;
        char *token = str ;
        char *str_dup;
        int i;
        
        if (str == NULL)
                return NULL;

        /* do this before strsep.. */
        str_dup = strdup(str);
        
        key = &base;
        token = strsep(&str_ptr, "+\n ");
        while(token != NULL){
                /* printf("found : %s\n", token); */
                k = XStringToKeysym(token);
                if (k == NoSymbol){
                        fprintf(stderr, "error converting %s to keysym\n", token);
                        exit(-1);
                }
                key->next = alloc_key_press();
                key = key->next;
                key->key = k;
                token = strsep(&str_ptr, "+\n ");
        }

        base.next->original_str = str_dup;;
        return base.next;
}

char *remove_new_line(char *str)
{
        int len =0;
        int i;
        if (str == NULL)
                return NULL;
        len = strlen(str);

        for (i=0; i< len; i++)
                if (str[i] == '\n')
                        str[i] = '\0';
        return str;

}

/* this reads the config files and fill the gest stack */
int read_config(char *conf_file)
{

        FILE *conf = fopen(conf_file, "r");
        struct action *action;
        struct gesture *gest;
        char buff[4096];
        char *buff_ptr = buff;
        char **buff_ptr_ptr;
        char *gest_name;
        char *gest_sequence;
        char *gest_extra;
        void *data;
        char *sequence;
        int i;
        char *token;
        
        if (conf == NULL){
                fprintf(stderr, "error opening configuration: %s\n", conf_file);
                return -1;
        }

        while( fgets(buff, 4096, conf) != NULL){
                if (buff[0] == '#')
                        continue;
                remove_new_line(buff);
                buff_ptr = buff;
                buff_ptr_ptr = &buff_ptr;
                token = strsep(buff_ptr_ptr, " \t");
                if (token == NULL)
                        continue;
                /* first token is the sequence */
                gest_sequence = token;
                /* second token is the action */
                token = strsep(buff_ptr_ptr, " \t");
                if (token == NULL)
                        continue;
                gest_name = token;
                gest_extra = *buff_ptr_ptr;
                
                for (i=1; i< ACTION_LAST; i++){
                        if (strncasecmp(gestures_names[i],
                                        token,
                                        strlen(gestures_names[i])) != 0)
                                continue;
                        if (i == ACTION_EXECUTE ){
                                if (strlen(gest_extra) <= 0){
                                        fprintf(stderr, "error in exec action\n");
                                        continue;
                                }
                                /* printf("will exec %s\n", gest_extra); */
                                data = strdup(gest_extra);
                        } else if (i == ACTION_ROOT_SEND ){
                                if (strlen(gest_extra) <= 0){
                                        fprintf(stderr, "error in exec action\n");
                                        continue;
                                }
                                data = compile_key_action(gest_extra);
                                if (data == NULL){
                                        fprintf(stderr, "error reading config file: root_send\n");
                                        exit(-1);
                                }
                        } else
                                data = NULL;

                        sequence =  strdup(gest_sequence);
                        
                        action = alloc_action(i, data);
                        gest = alloc_gesture(sequence, action);
                        push(gest, &temp_stack);
                }
                
        }

        fclose(conf);
        return 0;

}


int init_gestures(char *config_file)
{
        int err = 0;
        int i;
        
        err = read_config(config_file);
        if (err){
                fprintf(stderr, "cannot open config file: %s\n", config_file);
                return err;
        }

        /* now, fill the gesture array */
        if (known_gestures_num != 0) /* reconfiguration.. */
                free(known_gestures);
        known_gestures_num = stack_size(&temp_stack);
        known_gestures = malloc(sizeof(struct gesture *)*known_gestures_num);

        /* printf("got %d gests\n", known_gestures_num); */
        
        for (i=0; i<known_gestures_num; i++){
                known_gestures[i] = (struct gesture *) pop(&temp_stack);    
        }

        /* sort the array... */
        qsort(known_gestures, known_gestures_num,
              sizeof(struct gesture *), compare_gestures);
              
        
        return err;
}
