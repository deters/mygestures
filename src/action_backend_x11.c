#include "action_backend.h"
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "x11_window.h"

static Display *x11_dpy = NULL;

enum {
    _NET_WM_STATE_REMOVE = 0, _NET_WM_STATE_ADD = 1, _NET_WM_STATE_TOGGLE = 2
};

static int client_msg(Display *disp, Window win, char *msg, unsigned long data0,
                      unsigned long data1, unsigned long data2, unsigned long data3,
                      unsigned long data4) {
    XEvent event;
    long mask = SubstructureRedirectMask | SubstructureNotifyMask;

    event.xclient.type = ClientMessage;
    event.xclient.serial = 0;
    event.xclient.send_event = True;
    event.xclient.message_type = XInternAtom(disp, msg, False);
    event.xclient.window = win;
    event.xclient.format = 32;
    event.xclient.data.l[0] = data0;
    event.xclient.data.l[1] = data1;
    event.xclient.data.l[2] = data2;
    event.xclient.data.l[3] = data3;
    event.xclient.data.l[4] = data4;

    if (XSendEvent(disp, DefaultRootWindow(disp), False, mask, &event)) {
        XFlush(disp);
        return EXIT_SUCCESS;
    } else {
        fprintf(stderr, "Cannot send %s event.\n", msg);
        return EXIT_FAILURE;
    }
}

static void x11_iconify(void) {
    if (!x11_dpy) return;
    Window w = get_focused_window(x11_dpy);
    if (w != None) XIconifyWindow(x11_dpy, w, 0);
}

static void x11_kill_window(void) {
    if (!x11_dpy) return;
    Window w = get_focused_window(x11_dpy);
    if (w == None || w == RootWindow(x11_dpy, DefaultScreen(x11_dpy))) return;
    XSync(x11_dpy, 0);
    XKillClient(x11_dpy, w);
    XSync(x11_dpy, 0);
}

static void x11_raise(void) {
    if (!x11_dpy) return;
    Window w = get_focused_window(x11_dpy);
    if (w != None) XRaiseWindow(x11_dpy, w);
}

static void x11_lower(void) {
    if (!x11_dpy) return;
    Window w = get_focused_window(x11_dpy);
    if (w != None) XLowerWindow(x11_dpy, w);
}

static void x11_maximize(void) {
    if (!x11_dpy) return;
    Window w = get_focused_window(x11_dpy);
    if (w != None) {
        Atom prop1 = XInternAtom(x11_dpy, "_NET_WM_STATE_MAXIMIZED_VERT", False);
        Atom prop2 = XInternAtom(x11_dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
        client_msg(x11_dpy, w, "_NET_WM_STATE", _NET_WM_STATE_ADD, (unsigned long) prop1, (unsigned long) prop2, 0, 0);
    }
}

static void x11_restore(void) {
    if (!x11_dpy) return;
    Window w = get_focused_window(x11_dpy);
    if (w != None) {
        Atom prop1 = XInternAtom(x11_dpy, "_NET_WM_STATE_MAXIMIZED_VERT", False);
        Atom prop2 = XInternAtom(x11_dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
        client_msg(x11_dpy, w, "_NET_WM_STATE", _NET_WM_STATE_REMOVE, (unsigned long) prop1, (unsigned long) prop2, 0, 0);
    }
}

static void x11_toggle_maximized(void) {
    if (!x11_dpy) return;
    Window w = get_focused_window(x11_dpy);
    if (w != None) {
        Atom prop1 = XInternAtom(x11_dpy, "_NET_WM_STATE_MAXIMIZED_VERT", False);
        Atom prop2 = XInternAtom(x11_dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
        client_msg(x11_dpy, w, "_NET_WM_STATE", _NET_WM_STATE_TOGGLE, (unsigned long) prop1, (unsigned long) prop2, 0, 0);
    }
}

struct key_press {
    KeySym key;
    struct key_press *next;
};

static void x11_press_key(KeySym key, Bool is_press) {
    if (x11_dpy) {
        XTestFakeKeyEvent(x11_dpy, XKeysymToKeycode(x11_dpy, key), is_press, CurrentTime);
    }
}

static struct key_press* x11_string_to_keypress(const char *str_ptr) {
    if (!str_ptr) return NULL;
    char *copy = strdup(str_ptr);
    struct key_press base;
    base.next = NULL;
    struct key_press *key = &base;
    KeySym k;
    char *token;
    char *str_iter = copy;

    while ((token = strsep(&str_iter, "+\n ")) != NULL) {
        if (*token == '\0') continue;
        k = XStringToKeysym(token);
        if (k == NoSymbol) {
            fprintf(stderr, "error converting %s to keysym\n", token);
            continue;
        }
        key->next = malloc(sizeof(struct key_press));
        key = key->next;
        key->key = k;
        key->next = NULL;
    }
    free(copy);
    return base.next;
}

static void x11_release_keys_reverse(struct key_press *key) {
    if (key == NULL) return;
    x11_release_keys_reverse(key->next);
    x11_press_key(key->key, False);
}

static void x11_keypress(const char *data) {
    if (!x11_dpy || !data) return;
    struct key_press *keys = x11_string_to_keypress(data);
    if (!keys) return;

    for (struct key_press *tmp = keys; tmp != NULL; tmp = tmp->next) {
        x11_press_key(tmp->key, True);
        usleep(10000);
    }

    usleep(50000);

    x11_release_keys_reverse(keys);

    while (keys != NULL) {
        struct key_press *next = keys->next;
        free(keys);
        keys = next;
    }
}

static void x11_execute_desktop_shortcut(const char *gnome_key, const char *fallback_keys) {
    const char *schemas[] = {
        "org.gnome.desktop.wm.keybindings",
        "org.gnome.settings-daemon.plugins.media-keys",
        "org.gnome.shell.keybindings",
        NULL
    };
    char cmd[512];
    for (int i = 0; schemas[i] != NULL; i++) {
        snprintf(cmd, sizeof(cmd), "gsettings get %s %s 2>/dev/null", schemas[i], gnome_key);
        FILE *fp = popen(cmd, "r");
        if (!fp) continue;

        char line[512];
        if (fgets(line, sizeof(line), fp)) {
            pclose(fp);
            
            if (strstr(line, "@as []") || strstr(line, "''") || strstr(line, "No such key")) {
                continue;
            }

            char *start = strchr(line, '\'');
            if (!start) start = strchr(line, '"');
            if (start) {
                start++;
                char *end = strchr(start, '\'');
                if (!end) end = strchr(start, '"');
                if (end) {
                    *end = '\0';
                    
                    char *translated = malloc(strlen(start) * 2 + 1);
                    translated[0] = '\0';
                    char *p = start;
                    while (*p) {
                        if (strncmp(p, "<Alt>", 5) == 0) { strcat(translated, "Alt_L+"); p += 5; }
                        else if (strncmp(p, "<Super>", 7) == 0) { strcat(translated, "Super_L+"); p += 7; }
                        else if (strncmp(p, "<Shift>", 7) == 0) { strcat(translated, "Shift_L+"); p += 7; }
                        else if (strncmp(p, "<Control>", 9) == 0) { strcat(translated, "Control_L+"); p += 9; }
                        else if (strncmp(p, "<Ctrl>", 6) == 0) { strcat(translated, "Control_L+"); p += 6; }
                        else if (*p == '>') { p++; }
                        else {
                            size_t len = strlen(translated);
                            translated[len] = *p;
                            translated[len+1] = '\0';
                            p++;
                        }
                    }
                    size_t final_len = strlen(translated);
                    if (final_len > 0 && translated[final_len - 1] == '+') {
                        translated[final_len - 1] = '\0';
                    }
                    
                    if (strlen(translated) > 0) {
                        x11_keypress(translated);
                    }
                    free(translated);
                    return;
                }
            }
        } else {
            pclose(fp);
        }
    }
    if (fallback_keys) {
        x11_keypress(fallback_keys);
    }
}

static void x11_workspace_left(void) { x11_execute_desktop_shortcut("switch-to-workspace-left", "Control_L+Alt_L+Left"); }
static void x11_workspace_right(void) { x11_execute_desktop_shortcut("switch-to-workspace-right", "Control_L+Alt_L+Right"); }
static void x11_workspace_up(void) { x11_execute_desktop_shortcut("switch-to-workspace-up", "Control_L+Alt_L+Up"); }
static void x11_workspace_down(void) { x11_execute_desktop_shortcut("switch-to-workspace-down", "Control_L+Alt_L+Down"); }
static void x11_show_overview(void) { x11_execute_desktop_shortcut("panel-main-menu", "Super_L"); }
static void x11_show_app_grid(void) { x11_execute_desktop_shortcut("toggle-application-view", "Super_L+a"); }

static ActionBackend x11_backend = {
    .iconify = x11_iconify,
    .kill_window = x11_kill_window,
    .raise = x11_raise,
    .lower = x11_lower,
    .maximize = x11_maximize,
    .restore = x11_restore,
    .toggle_maximized = x11_toggle_maximized,
    .keypress = x11_keypress,
    .workspace_left = x11_workspace_left,
    .workspace_right = x11_workspace_right,
    .workspace_up = x11_workspace_up,
    .workspace_down = x11_workspace_down,
    .show_overview = x11_show_overview,
    .show_app_grid = x11_show_app_grid
};

ActionBackend *action_backend_x11_get(void *context) {
    x11_dpy = (Display *)context;
    return &x11_backend;
}
