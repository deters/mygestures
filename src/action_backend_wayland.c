#include "action_backend.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "logging.h"
#include "uinput_device.h"
#include <unistd.h>
#include "wayland_context.h"

static WaylandContext ctx;
static int ctx_discovered = 0;

static void ensure_ctx(void) {
    if (!ctx_discovered) {
        discover_wayland_context(&ctx);
        ctx_discovered = 1;
    }
}

static void wayland_keypress(const char *data) {
    uinput_keypress_string(data);
}

static char *get_gnome_shortcut(const char *schema, const char *key) {
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));

    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%sgsettings get %s %s 2>/dev/null", prefix, schema, key);
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;

    char line[512];
    if (fgets(line, sizeof(line), fp)) {
        pclose(fp);
        
        char *start = strchr(line, '\'');
        if (!start) start = strchr(line, '"');
        if (start) {
            start++;
            char *end = strchr(start, '\'');
            if (!end) end = strchr(start, '"');
            if (end) {
                *end = '\0';
                
                if (strcmp(start, "disabled") == 0) return NULL;

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
                
                return translated;
            }
        }
    } else {
        pclose(fp);
    }
    return NULL;
}

static void wayland_execute_desktop_shortcut(const char *gnome_key, const char *fallback_keys) {
    const char *schemas[] = {
        "org.gnome.desktop.wm.keybindings",
        "org.gnome.settings-daemon.plugins.media-keys",
        "org.gnome.shell.keybindings",
        NULL
    };

    for (int i = 0; schemas[i] != NULL; i++) {
        char *shortcut = get_gnome_shortcut(schemas[i], gnome_key);
        if (shortcut) {
            wayland_keypress(shortcut);
            free(shortcut);
            return;
        }
    }

    if (fallback_keys) {
        wayland_keypress(fallback_keys);
    }
}

// Sway actions
static void sway_iconify(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%sswaymsg move scratchpad >/dev/null 2>&1", prefix);
    system(cmd); 
}
static void sway_kill_window(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%sswaymsg kill >/dev/null 2>&1", prefix);
    system(cmd); 
}
static void sway_raise(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%sswaymsg focus >/dev/null 2>&1", prefix);
    system(cmd); 
}
static void sway_lower(void) { LOG_WARN("Lower action is not natively supported under Sway tiling layout.\n"); }
static void sway_maximize(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%sswaymsg fullscreen enable >/dev/null 2>&1", prefix);
    system(cmd); 
}
static void sway_restore(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%sswaymsg fullscreen disable >/dev/null 2>&1", prefix);
    system(cmd); 
}
static void sway_toggle_maximized(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%sswaymsg fullscreen toggle >/dev/null 2>&1", prefix);
    system(cmd); 
}
static void sway_workspace_left(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%sswaymsg workspace prev_on_output >/dev/null 2>&1", prefix);
    system(cmd); 
}
static void sway_workspace_right(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%sswaymsg workspace next_on_output >/dev/null 2>&1", prefix);
    system(cmd); 
}
static void sway_workspace_up(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%sswaymsg workspace prev >/dev/null 2>&1", prefix);
    system(cmd); 
}
static void sway_workspace_down(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%sswaymsg workspace next >/dev/null 2>&1", prefix);
    system(cmd); 
}
static void sway_show_overview(void) { wayland_keypress("Super_L"); }
static void sway_show_app_grid(void) { wayland_keypress("Super_L+d"); }

// Hyprland actions
static void hypr_iconify(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%shyprctl dispatch movetoworkspacesilent special:minimized >/dev/null 2>&1", prefix);
    system(cmd); 
}
static void hypr_kill_window(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%shyprctl dispatch killactive >/dev/null 2>&1", prefix);
    system(cmd); 
}
static void hypr_raise(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%shyprctl dispatch alterzorder top >/dev/null 2>&1", prefix);
    system(cmd); 
}
static void hypr_lower(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%shyprctl dispatch alterzorder bottom >/dev/null 2>&1", prefix);
    system(cmd); 
}
static void hypr_maximize(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%shyprctl dispatch fullscreen 1 >/dev/null 2>&1", prefix);
    system(cmd); 
}
static void hypr_restore(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%shyprctl dispatch fullscreen 1 >/dev/null 2>&1", prefix);
    system(cmd); 
}
static void hypr_toggle_maximized(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%shyprctl dispatch fullscreen 1 >/dev/null 2>&1", prefix);
    system(cmd); 
}
static void hypr_workspace_left(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%shyprctl dispatch workspace e-1 >/dev/null 2>&1", prefix);
    system(cmd); 
}
static void hypr_workspace_right(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%shyprctl dispatch workspace e+1 >/dev/null 2>&1", prefix);
    system(cmd); 
}
static void hypr_workspace_up(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%shyprctl dispatch workspace m-1 >/dev/null 2>&1", prefix);
    system(cmd); 
}
static void hypr_workspace_down(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%shyprctl dispatch workspace m+1 >/dev/null 2>&1", prefix);
    system(cmd); 
}
static void hypr_show_overview(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%shyprctl dispatch togglespecialworkspace >/dev/null 2>&1", prefix);
    system(cmd); 
}
static void hypr_show_app_grid(void) { wayland_keypress("Super_L+a"); }

// GNOME actions
static void gnome_iconify(void) { wayland_execute_desktop_shortcut("minimize", "Super_L+h"); }
static void gnome_kill_window(void) { wayland_execute_desktop_shortcut("close", "Alt_L+F4"); }
static void gnome_raise(void) {}
static void gnome_lower(void) { wayland_keypress("Alt_L+Escape"); }
static void gnome_maximize(void) { wayland_execute_desktop_shortcut("maximize", "Super_L+Up"); }
static void gnome_restore(void) { wayland_execute_desktop_shortcut("unmaximize", "Super_L+Down"); }
static void gnome_toggle_maximized(void) { wayland_execute_desktop_shortcut("toggle-maximized", "Alt_L+F10"); }
static void gnome_workspace_left(void) { wayland_execute_desktop_shortcut("switch-to-workspace-left", "Control_L+Alt_L+Left"); }
static void gnome_workspace_right(void) { wayland_execute_desktop_shortcut("switch-to-workspace-right", "Control_L+Alt_L+Right"); }
static void gnome_workspace_up(void) { wayland_execute_desktop_shortcut("switch-to-workspace-up", "Control_L+Alt_L+Up"); }
static void gnome_workspace_down(void) { wayland_execute_desktop_shortcut("switch-to-workspace-down", "Control_L+Alt_L+Down"); }
static void gnome_show_overview(void) { wayland_execute_desktop_shortcut("panel-main-menu", "Super_L"); }
static void gnome_show_app_grid(void) { wayland_execute_desktop_shortcut("toggle-application-view", "Super_L+a"); }

// KDE actions
static void kde_iconify(void) { wayland_keypress("Alt_L+F9"); }
static void kde_kill_window(void) { wayland_keypress("Alt_L+F4"); }
static void kde_raise(void) {}
static void kde_lower(void) { wayland_keypress("Alt_L+Escape"); }
static void kde_maximize(void) { wayland_keypress("Super_L+Page_Up"); }
static void kde_restore(void) { wayland_keypress("Super_L+Page_Down"); }
static void kde_toggle_maximized(void) { wayland_keypress("Super_L+Page_Up"); }
static void kde_workspace_left(void) { wayland_keypress("Control_L+F1"); }
static void kde_workspace_right(void) { wayland_keypress("Control_L+F2"); }
static void kde_workspace_up(void) { wayland_keypress("Control_L+Alt_L+Up"); }
static void kde_workspace_down(void) { wayland_keypress("Control_L+Alt_L+Down"); }
static void kde_show_overview(void) { wayland_keypress("Super_L+w"); }
static void kde_show_app_grid(void) { wayland_keypress("Alt_L+F1"); }

// Generic Wayland Fallback Actions
static void generic_iconify(void) { wayland_keypress("Super_L+h"); }
static void generic_kill_window(void) { wayland_keypress("Alt_L+F4"); }
static void generic_raise(void) {}
static void generic_lower(void) { wayland_keypress("Alt_L+Escape"); }
static void generic_maximize(void) { wayland_keypress("Super_L+Up"); }
static void generic_restore(void) { wayland_keypress("Super_L+Down"); }
static void generic_toggle_maximized(void) { wayland_keypress("Alt_L+F10"); }
static void generic_workspace_left(void) { wayland_keypress("Control_L+Alt_L+Left"); }
static void generic_workspace_right(void) { wayland_keypress("Control_L+Alt_L+Right"); }
static void generic_workspace_up(void) { wayland_keypress("Control_L+Alt_L+Up"); }
static void generic_workspace_down(void) { wayland_keypress("Control_L+Alt_L+Down"); }
static void generic_show_overview(void) { wayland_keypress("Super_L"); }
static void generic_show_app_grid(void) { wayland_keypress("Super_L+a"); }

static void wl_click(int button) {
    uinput_click(button);
}

static ActionBackend wayland_backend;

ActionBackend *action_backend_wayland_get(void) {
    ensure_ctx();

    wayland_backend.click = wl_click;

    if (ctx.is_sway) {
        wayland_backend.iconify = sway_iconify;
        wayland_backend.kill_window = sway_kill_window;
        wayland_backend.raise = sway_raise;
        wayland_backend.lower = sway_lower;
        wayland_backend.maximize = sway_maximize;
        wayland_backend.restore = sway_restore;
        wayland_backend.toggle_maximized = sway_toggle_maximized;
        wayland_backend.keypress = wayland_keypress;
        wayland_backend.workspace_left = sway_workspace_left;
        wayland_backend.workspace_right = sway_workspace_right;
        wayland_backend.workspace_up = sway_workspace_up;
        wayland_backend.workspace_down = sway_workspace_down;
        wayland_backend.show_overview = sway_show_overview;
        wayland_backend.show_app_grid = sway_show_app_grid;
        return &wayland_backend;
    }

    if (ctx.is_hypr) {
        wayland_backend.iconify = hypr_iconify;
        wayland_backend.kill_window = hypr_kill_window;
        wayland_backend.raise = hypr_raise;
        wayland_backend.lower = hypr_lower;
        wayland_backend.maximize = hypr_maximize;
        wayland_backend.restore = hypr_restore;
        wayland_backend.toggle_maximized = hypr_toggle_maximized;
        wayland_backend.keypress = wayland_keypress;
        wayland_backend.workspace_left = hypr_workspace_left;
        wayland_backend.workspace_right = hypr_workspace_right;
        wayland_backend.workspace_up = hypr_workspace_up;
        wayland_backend.workspace_down = hypr_workspace_down;
        wayland_backend.show_overview = hypr_show_overview;
        wayland_backend.show_app_grid = hypr_show_app_grid;
        return &wayland_backend;
    }

    if (ctx.is_gnome) {
        wayland_backend.iconify = gnome_iconify;
        wayland_backend.kill_window = gnome_kill_window;
        wayland_backend.raise = gnome_raise;
        wayland_backend.lower = gnome_lower;
        wayland_backend.maximize = gnome_maximize;
        wayland_backend.restore = gnome_restore;
        wayland_backend.toggle_maximized = gnome_toggle_maximized;
        wayland_backend.keypress = wayland_keypress;
        wayland_backend.workspace_left = gnome_workspace_left;
        wayland_backend.workspace_right = gnome_workspace_right;
        wayland_backend.workspace_up = gnome_workspace_up;
        wayland_backend.workspace_down = gnome_workspace_down;
        wayland_backend.show_overview = gnome_show_overview;
        wayland_backend.show_app_grid = gnome_show_app_grid;
    } else if (ctx.is_kde) {
        wayland_backend.iconify = kde_iconify;
        wayland_backend.kill_window = kde_kill_window;
        wayland_backend.raise = kde_raise;
        wayland_backend.lower = kde_lower;
        wayland_backend.maximize = kde_maximize;
        wayland_backend.restore = kde_restore;
        wayland_backend.toggle_maximized = kde_toggle_maximized;
        wayland_backend.keypress = wayland_keypress;
        wayland_backend.workspace_left = kde_workspace_left;
        wayland_backend.workspace_right = kde_workspace_right;
        wayland_backend.workspace_up = kde_workspace_up;
        wayland_backend.workspace_down = kde_workspace_down;
        wayland_backend.show_overview = kde_show_overview;
        wayland_backend.show_app_grid = kde_show_app_grid;
    } else {
        wayland_backend.iconify = generic_iconify;
        wayland_backend.kill_window = generic_kill_window;
        wayland_backend.raise = generic_raise;
        wayland_backend.lower = generic_lower;
        wayland_backend.maximize = generic_maximize;
        wayland_backend.restore = generic_restore;
        wayland_backend.toggle_maximized = generic_toggle_maximized;
        wayland_backend.keypress = wayland_keypress;
        wayland_backend.workspace_left = generic_workspace_left;
        wayland_backend.workspace_right = generic_workspace_right;
        wayland_backend.workspace_up = generic_workspace_up;
        wayland_backend.workspace_down = generic_workspace_down;
        wayland_backend.show_overview = generic_show_overview;
        wayland_backend.show_app_grid = generic_show_app_grid;
    }
    
    return &wayland_backend;
}