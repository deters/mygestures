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
		"SHOW_OVERVIEW", "SHOW_APP_GRID", "CLICK", 
		"TOGGLE_FULLSCREEN", "SHOW_DESKTOP", "LOCK_SCREEN", "TERMINAL",
		"VOLUME_UP", "VOLUME_DOWN", "VOLUME_MUTE",
		"MEDIA_PLAY", "MEDIA_NEXT", "MEDIA_PREV",
		"WWW", "HOME", "EMAIL", "SEARCH", "CALCULATOR", "CONTROL_CENTER", "LOGOUT",
		"SCREENSHOT", "SCREENSHOT_WINDOW", "SCREENSHOT_AREA", "GNOME", "LAST" };

const char *get_action_name(int action) {
	return action_name[action];
}

static ActionBackend *current_backend = NULL;

void action_backend_init(void) {
    current_backend = action_backend_wayland_get();
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
        case ACTION_TOGGLE_FULLSCREEN:
            if (current_backend->toggle_fullscreen) current_backend->toggle_fullscreen();
            break;
        case ACTION_SHOW_DESKTOP:
            if (current_backend->show_desktop) current_backend->show_desktop();
            break;
        case ACTION_LOCK_SCREEN:
            if (current_backend->lock_screen) current_backend->lock_screen();
            break;
        case ACTION_TERMINAL:
            if (current_backend->terminal) current_backend->terminal();
            break;
        case ACTION_VOLUME_UP:
            if (current_backend->volume_up) current_backend->volume_up();
            break;
        case ACTION_VOLUME_DOWN:
            if (current_backend->volume_down) current_backend->volume_down();
            break;
        case ACTION_VOLUME_MUTE:
            if (current_backend->volume_mute) current_backend->volume_mute();
            break;
        case ACTION_MEDIA_PLAY:
            if (current_backend->media_play) current_backend->media_play();
            break;
        case ACTION_MEDIA_NEXT:
            if (current_backend->media_next) current_backend->media_next();
            break;
        case ACTION_MEDIA_PREV:
            if (current_backend->media_prev) current_backend->media_prev();
            break;
        case ACTION_WWW:
            if (current_backend->www) current_backend->www();
            break;
        case ACTION_HOME:
            if (current_backend->home) current_backend->home();
            break;
        case ACTION_EMAIL:
            if (current_backend->email) current_backend->email();
            break;
        case ACTION_SEARCH:
            if (current_backend->search) current_backend->search();
            break;
        case ACTION_CALCULATOR:
            if (current_backend->calculator) current_backend->calculator();
            break;
        case ACTION_CONTROL_CENTER:
            if (current_backend->control_center) current_backend->control_center();
            break;
        case ACTION_LOGOUT:
            if (current_backend->logout) current_backend->logout();
            break;
        case ACTION_SCREENSHOT:
            if (current_backend->screenshot) current_backend->screenshot();
            break;
        case ACTION_SCREENSHOT_WINDOW:
            if (current_backend->screenshot_window) current_backend->screenshot_window();
            break;
        case ACTION_SCREENSHOT_AREA:
            if (current_backend->screenshot_area) current_backend->screenshot_area();
            break;
        case ACTION_GNOME:
            if (current_backend->gnome_action) current_backend->gnome_action(action->original_str);
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