/*
 Copyright 2008-2016 Lucas Augusto Deters
 Copyright 2005 Nir Tzachar

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 one line to give the program's name and an idea of what it does.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "actions.h"
#include "action_backend.h"
#include "logging.h"

/* Actions */
const char *action_name[ACTION_COUNT + 1] = {
		"ERROR", "EXIT_GEST", "EXECUTE", "ICONIFY", "KILL", "RECONF", "RAISE", "LOWER", "MAXIMIZE",
		"RESTORE", "TOGGLE_MAXIMIZED", "KEYPRESS", "ABORT", 
		"WORKSPACE_LEFT", "WORKSPACE_RIGHT", "WORKSPACE_UP", "WORKSPACE_DOWN", 
		"SHOW_OVERVIEW", "SHOW_APP_GRID", "CLICK", "LAST" };

const char *get_action_name(int action) {
	return action_name[action];
}

static ActionBackend *current_backend = NULL;

void action_backend_init(void *context) {
    if (context) {
        current_backend = action_backend_x11_get(context);
    } else {
        current_backend = action_backend_wayland_get();
    }
}

void execute_action_agnostic(Action *action) {
    int id;

    assert(action);

    if (action->type == ACTION_EXECUTE) {
        id = fork();
        if (id == 0) {
            int i = system(action->original_str);
            exit(i);
        }
        if (id < 0) {
            LOG_ERROR("Error forking.\n");
        }
        return;
    }

    if (!current_backend) {
        LOG_ERROR("Action backend not initialized. Cannot execute %s\n", get_action_name(action->type));
        return;
    }

    switch (action->type) {
        case ACTION_ICONIFY:
            if (current_backend->iconify) current_backend->iconify();
            break;
        case ACTION_KILL:
            if (current_backend->kill_window) current_backend->kill_window();
            break;
        case ACTION_RAISE:
            if (current_backend->raise) current_backend->raise();
            break;
        case ACTION_LOWER:
            if (current_backend->lower) current_backend->lower();
            break;
        case ACTION_MAXIMIZE:
            if (current_backend->maximize) current_backend->maximize();
            break;
        case ACTION_RESTORE:
            if (current_backend->restore) current_backend->restore();
            break;
        case ACTION_TOGGLE_MAXIMIZED:
            if (current_backend->toggle_maximized) current_backend->toggle_maximized();
            break;
        case ACTION_KEYPRESS:
            if (current_backend->keypress) current_backend->keypress(action->original_str);
            break;
        case ACTION_WORKSPACE_LEFT:
            if (current_backend->workspace_left) current_backend->workspace_left();
            break;
        case ACTION_WORKSPACE_RIGHT:
            if (current_backend->workspace_right) current_backend->workspace_right();
            break;
        case ACTION_WORKSPACE_UP:
            if (current_backend->workspace_up) current_backend->workspace_up();
            break;
        case ACTION_WORKSPACE_DOWN:
            if (current_backend->workspace_down) current_backend->workspace_down();
            break;
        case ACTION_SHOW_OVERVIEW:
            if (current_backend->show_overview) current_backend->show_overview();
            break;
        case ACTION_SHOW_APP_GRID:
            if (current_backend->show_app_grid) current_backend->show_app_grid();
            break;
        case ACTION_CLICK:
            if (current_backend->click) {
                int btn = action->original_str ? atoi(action->original_str) : 3;
                current_backend->click(btn);
            }
            break;
        default:
            LOG_ERROR("found an unknown gesture \n");
            break;
    }
}

void execute_click_agnostic(int button) {
    if (current_backend && current_backend->click) {
        current_backend->click(button);
    }
}
