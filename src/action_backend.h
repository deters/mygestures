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
    void (*toggle_fullscreen)(void);
    void (*show_desktop)(void);
    void (*lock_screen)(void);
    void (*terminal)(void);
    void (*volume_up)(void);
    void (*volume_down)(void);
    void (*volume_mute)(void);
    void (*media_play)(void);
    void (*media_next)(void);
    void (*media_prev)(void);
    void (*www)(void);
    void (*home)(void);
    void (*email)(void);
    void (*search)(void);
    void (*calculator)(void);
    void (*control_center)(void);
    void (*logout)(void);
    void (*screenshot)(void);
    void (*screenshot_window)(void);
    void (*screenshot_area)(void);
} ActionBackend;

/* Initialize the appropriate backend */
void action_backend_init(void);

/* Execute an action using the initialized backend */
void execute_action_agnostic(Action *action);

/* Get instances of specific backends */
ActionBackend *action_backend_wayland_get(void);

#endif /* ACTION_BACKEND_H_ */