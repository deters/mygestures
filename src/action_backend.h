#ifndef ACTION_BACKEND_H_
#define ACTION_BACKEND_H_

#include "configuration.h"

typedef struct ActionBackend {
    void (*iconify)(void);
    void (*kill_window)(void);
    void (*raise)(void);
    void (*lower)(void);
    void (*maximize)(void);
    void (*restore)(void);
    void (*toggle_maximized)(void);
    void (*keypress)(const char *keys);
    void (*workspace_left)(void);
    void (*workspace_right)(void);
    void (*workspace_up)(void);
    void (*workspace_down)(void);
    void (*show_overview)(void);
    void (*show_app_grid)(void);
    void (*click)(int button);
} ActionBackend;

/* Initialize the appropriate backend based on context (e.g. Display pointer for X11) */
void action_backend_init(void *context);

/* Execute an action using the initialized backend */
void execute_action_agnostic(Action *action);

/* Get instances of specific backends */
ActionBackend *action_backend_x11_get(void *context);
ActionBackend *action_backend_wayland_get(void);

#endif /* ACTION_BACKEND_H_ */