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
    LOG_INFO(1, "  [Action] Emulating keypress: %s\n", data);
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
        
        /* Modern gsettings returns ['<Alt>F10'] or @as [] */
        if (strstr(line, "@as []") || strstr(line, "[]")) return NULL;

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
                
                LOG_INFO(1, "  [GNOME] Found shortcut for %s: %s (translated to %s)\n", key, start, translated);
                return translated;
            }
        }
    } else {
        pclose(fp);
    }
    return NULL;
}

static void wayland_execute_desktop_shortcut(const char *gnome_key, const char *fallback_keys) {
    LOG_INFO(1, "  [Action] Executing desktop shortcut: %s\n", gnome_key);
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
        LOG_INFO(1, "  [Action] No GSettings found for %s, using fallback: %s\n", gnome_key, fallback_keys);
        wayland_keypress(fallback_keys);
    }
}

static void run_command(const char *cmd) {
    LOG_INFO(1, "  [Action] Running command: %s\n", cmd);
    system(cmd);
}

// Sway actions
static void sway_iconify(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%sswaymsg move scratchpad >/dev/null 2>&1", prefix);
    run_command(cmd); 
}
static void sway_kill_window(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%sswaymsg kill >/dev/null 2>&1", prefix);
    run_command(cmd); 
}
static void sway_raise(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%sswaymsg focus >/dev/null 2>&1", prefix);
    run_command(cmd); 
}
static void sway_lower(void) { LOG_WARN("Lower action is not natively supported under Sway tiling layout.\n"); }
static void sway_maximize(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%sswaymsg fullscreen enable >/dev/null 2>&1", prefix);
    run_command(cmd); 
}
static void sway_restore(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%sswaymsg fullscreen disable >/dev/null 2>&1", prefix);
    run_command(cmd); 
}
static void sway_toggle_maximized(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%sswaymsg fullscreen toggle >/dev/null 2>&1", prefix);
    run_command(cmd); 
}
static void sway_workspace_left(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%sswaymsg workspace prev_on_output >/dev/null 2>&1", prefix);
    run_command(cmd); 
}
static void sway_workspace_right(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%sswaymsg workspace next_on_output >/dev/null 2>&1", prefix);
    run_command(cmd); 
}
static void sway_workspace_up(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%sswaymsg workspace prev >/dev/null 2>&1", prefix);
    run_command(cmd); 
}
static void sway_workspace_down(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%sswaymsg workspace next >/dev/null 2>&1", prefix);
    run_command(cmd); 
}
static void sway_show_overview(void) { wayland_keypress("Super_L"); }
static void sway_show_app_grid(void) { wayland_keypress("Super_L+d"); }
static void sway_toggle_fullscreen(void) {
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%sswaymsg fullscreen toggle >/dev/null 2>&1", prefix);
    run_command(cmd); 
}
static void sway_show_desktop(void) { wayland_keypress("Super_L+d"); }
static void sway_lock_screen(void) {
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%sswaylock >/dev/null 2>&1 &", prefix);
    run_command(cmd); 
}
static void sway_terminal(void) { wayland_keypress("Control_L+Alt_L+t"); }
static void sway_volume_up(void) { wayland_keypress("XF86AudioRaiseVolume"); }
static void sway_volume_down(void) { wayland_keypress("XF86AudioLowerVolume"); }
static void sway_volume_mute(void) { wayland_keypress("XF86AudioMute"); }
static void sway_media_play(void) { wayland_keypress("XF86AudioPlay"); }
static void sway_media_next(void) { wayland_keypress("XF86AudioNext"); }
static void sway_media_prev(void) { wayland_keypress("XF86AudioPrev"); }
static void sway_www(void) { wayland_keypress("XF86WWW"); }
static void sway_home(void) { wayland_keypress("XF86Explorer"); }
static void sway_email(void) { wayland_keypress("XF86Mail"); }
static void sway_search(void) { wayland_keypress("XF86Search"); }
static void sway_calculator(void) { wayland_keypress("XF86Calculator"); }
static void sway_control_center(void) { wayland_keypress("XF86ControlPanel"); }
static void sway_logout(void) { wayland_keypress("Control_L+Alt_L+Delete"); }
static void sway_screenshot(void) { wayland_keypress("Print"); }
static void sway_screenshot_window(void) { wayland_keypress("Alt_L+Print"); }
static void sway_screenshot_area(void) { wayland_keypress("Shift_L+Print"); }

// Hyprland actions
static void hypr_iconify(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%shyprctl dispatch movetoworkspacesilent special:minimized >/dev/null 2>&1", prefix);
    run_command(cmd); 
}
static void hypr_kill_window(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%shyprctl dispatch killactive >/dev/null 2>&1", prefix);
    run_command(cmd); 
}
static void hypr_raise(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%shyprctl dispatch alterzorder top >/dev/null 2>&1", prefix);
    run_command(cmd); 
}
static void hypr_lower(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%shyprctl dispatch alterzorder bottom >/dev/null 2>&1", prefix);
    run_command(cmd); 
}
static void hypr_maximize(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%shyprctl dispatch fullscreen 1 >/dev/null 2>&1", prefix);
    run_command(cmd); 
}
static void hypr_restore(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%shyprctl dispatch fullscreen 1 >/dev/null 2>&1", prefix);
    run_command(cmd); 
}
static void hypr_toggle_maximized(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%shyprctl dispatch fullscreen 1 >/dev/null 2>&1", prefix);
    run_command(cmd); 
}
static void hypr_workspace_left(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%shyprctl dispatch workspace e-1 >/dev/null 2>&1", prefix);
    run_command(cmd); 
}
static void hypr_workspace_right(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%shyprctl dispatch workspace e+1 >/dev/null 2>&1", prefix);
    run_command(cmd); 
}
static void hypr_workspace_up(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%shyprctl dispatch workspace m-1 >/dev/null 2>&1", prefix);
    run_command(cmd); 
}
static void hypr_workspace_down(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%shyprctl dispatch workspace m+1 >/dev/null 2>&1", prefix);
    run_command(cmd); 
}
static void hypr_show_overview(void) { 
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%shyprctl dispatch togglespecialworkspace >/dev/null 2>&1", prefix);
    run_command(cmd); 
}
static void hypr_show_app_grid(void) { wayland_keypress("Super_L+a"); }
static void hypr_toggle_fullscreen(void) {
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%shyprctl dispatch fullscreen 0 >/dev/null 2>&1", prefix);
    run_command(cmd); 
}
static void hypr_show_desktop(void) { wayland_keypress("Super_L+d"); }
static void hypr_lock_screen(void) {
    ensure_ctx();
    char prefix[1024];
    get_user_command_prefix(&ctx, prefix, sizeof(prefix));
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%shyprlock >/dev/null 2>&1 &", prefix);
    run_command(cmd); 
}
static void hypr_terminal(void) { wayland_keypress("Control_L+Alt_L+t"); }
static void hypr_volume_up(void) { wayland_keypress("XF86AudioRaiseVolume"); }
static void hypr_volume_down(void) { wayland_keypress("XF86AudioLowerVolume"); }
static void hypr_volume_mute(void) { wayland_keypress("XF86AudioMute"); }
static void hypr_media_play(void) { wayland_keypress("XF86AudioPlay"); }
static void hypr_media_next(void) { wayland_keypress("XF86AudioNext"); }
static void hypr_media_prev(void) { wayland_keypress("XF86AudioPrev"); }
static void hypr_www(void) { wayland_keypress("XF86WWW"); }
static void hypr_home(void) { wayland_keypress("XF86Explorer"); }
static void hypr_email(void) { wayland_keypress("XF86Mail"); }
static void hypr_search(void) { wayland_keypress("XF86Search"); }
static void hypr_calculator(void) { wayland_keypress("XF86Calculator"); }
static void hypr_control_center(void) { wayland_keypress("XF86ControlPanel"); }
static void hypr_logout(void) { wayland_keypress("Control_L+Alt_L+Delete"); }
static void hypr_screenshot(void) { wayland_keypress("Print"); }
static void hypr_screenshot_window(void) { wayland_keypress("Alt_L+Print"); }
static void hypr_screenshot_area(void) { wayland_keypress("Shift_L+Print"); }

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
static void gnome_toggle_fullscreen(void) { wayland_execute_desktop_shortcut("toggle-fullscreen", "F11"); }
static void gnome_show_desktop(void) { wayland_execute_desktop_shortcut("show-desktop", "Super_L+d"); }
static void gnome_lock_screen(void) { wayland_execute_desktop_shortcut("screensaver", "Super_L+l"); }
static void gnome_terminal(void) { wayland_execute_desktop_shortcut("terminal", "Control_L+Alt_L+t"); }
static void gnome_volume_up(void) { wayland_execute_desktop_shortcut("volume-up", "XF86AudioRaiseVolume"); }
static void gnome_volume_down(void) { wayland_execute_desktop_shortcut("volume-down", "XF86AudioLowerVolume"); }
static void gnome_volume_mute(void) { wayland_execute_desktop_shortcut("volume-mute", "XF86AudioMute"); }
static void gnome_media_play(void) { wayland_execute_desktop_shortcut("play", "XF86AudioPlay"); }
static void gnome_media_next(void) { wayland_execute_desktop_shortcut("next", "XF86AudioNext"); }
static void gnome_media_prev(void) { wayland_execute_desktop_shortcut("previous", "XF86AudioPrev"); }
static void gnome_www(void) { wayland_execute_desktop_shortcut("www", "XF86WWW"); }
static void gnome_home(void) { wayland_execute_desktop_shortcut("home", "XF86Explorer"); }
static void gnome_email(void) { wayland_execute_desktop_shortcut("email", "XF86Mail"); }
static void gnome_search(void) { wayland_execute_desktop_shortcut("search", "XF86Search"); }
static void gnome_calculator(void) { wayland_execute_desktop_shortcut("calculator", "XF86Calculator"); }
static void gnome_control_center(void) { wayland_execute_desktop_shortcut("control-center", "XF86ControlPanel"); }
static void gnome_logout(void) { wayland_execute_desktop_shortcut("logout", "Control_L+Alt_L+Delete"); }
static void gnome_screenshot(void) { wayland_execute_desktop_shortcut("screenshot", "Print"); }
static void gnome_screenshot_window(void) { wayland_execute_desktop_shortcut("window-screenshot", "Alt_L+Print"); }
static void gnome_screenshot_area(void) { wayland_execute_desktop_shortcut("area-screenshot", "Shift_L+Print"); }

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
static void kde_toggle_fullscreen(void) { wayland_keypress("F11"); }
static void kde_show_desktop(void) { wayland_keypress("Super_L+d"); }
static void kde_lock_screen(void) { wayland_keypress("Super_L+l"); }
static void kde_terminal(void) { wayland_keypress("Control_L+Alt_L+t"); }
static void kde_volume_up(void) { wayland_keypress("XF86AudioRaiseVolume"); }
static void kde_volume_down(void) { wayland_keypress("XF86AudioLowerVolume"); }
static void kde_volume_mute(void) { wayland_keypress("XF86AudioMute"); }
static void kde_media_play(void) { wayland_keypress("XF86AudioPlay"); }
static void kde_media_next(void) { wayland_keypress("XF86AudioNext"); }
static void kde_media_prev(void) { wayland_keypress("XF86AudioPrev"); }
static void kde_www(void) { wayland_keypress("XF86WWW"); }
static void kde_home(void) { wayland_keypress("XF86Explorer"); }
static void kde_email(void) { wayland_keypress("XF86Mail"); }
static void kde_search(void) { wayland_keypress("XF86Search"); }
static void kde_calculator(void) { wayland_keypress("XF86Calculator"); }
static void kde_control_center(void) { wayland_keypress("XF86ControlPanel"); }
static void kde_logout(void) { wayland_keypress("Control_L+Alt_L+Delete"); }
static void kde_screenshot(void) { wayland_keypress("Print"); }
static void kde_screenshot_window(void) { wayland_keypress("Alt_L+Print"); }
static void kde_screenshot_area(void) { wayland_keypress("Shift_L+Print"); }

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
static void generic_toggle_fullscreen(void) { wayland_keypress("F11"); }
static void generic_show_desktop(void) { wayland_keypress("Super_L+d"); }
static void generic_lock_screen(void) { wayland_keypress("Super_L+l"); }
static void generic_terminal(void) { wayland_keypress("Control_L+Alt_L+t"); }
static void generic_volume_up(void) { wayland_keypress("XF86AudioRaiseVolume"); }
static void generic_volume_down(void) { wayland_keypress("XF86AudioLowerVolume"); }
static void generic_volume_mute(void) { wayland_keypress("XF86AudioMute"); }
static void generic_media_play(void) { wayland_keypress("XF86AudioPlay"); }
static void generic_media_next(void) { wayland_keypress("XF86AudioNext"); }
static void generic_media_prev(void) { wayland_keypress("XF86AudioPrev"); }
static void generic_www(void) { wayland_keypress("XF86WWW"); }
static void generic_home(void) { wayland_keypress("XF86Explorer"); }
static void generic_email(void) { wayland_keypress("XF86Mail"); }
static void generic_search(void) { wayland_keypress("XF86Search"); }
static void generic_calculator(void) { wayland_keypress("XF86Calculator"); }
static void generic_control_center(void) { wayland_keypress("XF86ControlPanel"); }
static void generic_logout(void) { wayland_keypress("Control_L+Alt_L+Delete"); }
static void generic_screenshot(void) { wayland_keypress("Print"); }
static void generic_screenshot_window(void) { wayland_keypress("Alt_L+Print"); }
static void generic_screenshot_area(void) { wayland_keypress("Shift_L+Print"); }

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
        wayland_backend.toggle_fullscreen = sway_toggle_fullscreen;
        wayland_backend.show_desktop = sway_show_desktop;
        wayland_backend.lock_screen = sway_lock_screen;
        wayland_backend.terminal = sway_terminal;
        wayland_backend.volume_up = sway_volume_up;
        wayland_backend.volume_down = sway_volume_down;
        wayland_backend.volume_mute = sway_volume_mute;
        wayland_backend.media_play = sway_media_play;
        wayland_backend.media_next = sway_media_next;
        wayland_backend.media_prev = sway_media_prev;
        wayland_backend.www = sway_www;
        wayland_backend.home = sway_home;
        wayland_backend.email = sway_email;
        wayland_backend.search = sway_search;
        wayland_backend.calculator = sway_calculator;
        wayland_backend.control_center = sway_control_center;
        wayland_backend.logout = sway_logout;
        wayland_backend.screenshot = sway_screenshot;
        wayland_backend.screenshot_window = sway_screenshot_window;
        wayland_backend.screenshot_area = sway_screenshot_area;
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
        wayland_backend.toggle_fullscreen = hypr_toggle_fullscreen;
        wayland_backend.show_desktop = hypr_show_desktop;
        wayland_backend.lock_screen = hypr_lock_screen;
        wayland_backend.terminal = hypr_terminal;
        wayland_backend.volume_up = hypr_volume_up;
        wayland_backend.volume_down = hypr_volume_down;
        wayland_backend.volume_mute = hypr_volume_mute;
        wayland_backend.media_play = hypr_media_play;
        wayland_backend.media_next = hypr_media_next;
        wayland_backend.media_prev = hypr_media_prev;
        wayland_backend.www = hypr_www;
        wayland_backend.home = hypr_home;
        wayland_backend.email = hypr_email;
        wayland_backend.search = hypr_search;
        wayland_backend.calculator = hypr_calculator;
        wayland_backend.control_center = hypr_control_center;
        wayland_backend.logout = hypr_logout;
        wayland_backend.screenshot = hypr_screenshot;
        wayland_backend.screenshot_window = hypr_screenshot_window;
        wayland_backend.screenshot_area = hypr_screenshot_area;
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
        wayland_backend.toggle_fullscreen = gnome_toggle_fullscreen;
        wayland_backend.show_desktop = gnome_show_desktop;
        wayland_backend.lock_screen = gnome_lock_screen;
        wayland_backend.terminal = gnome_terminal;
        wayland_backend.volume_up = gnome_volume_up;
        wayland_backend.volume_down = gnome_volume_down;
        wayland_backend.volume_mute = gnome_volume_mute;
        wayland_backend.media_play = gnome_media_play;
        wayland_backend.media_next = gnome_media_next;
        wayland_backend.media_prev = gnome_media_prev;
        wayland_backend.www = gnome_www;
        wayland_backend.home = gnome_home;
        wayland_backend.email = gnome_email;
        wayland_backend.search = gnome_search;
        wayland_backend.calculator = gnome_calculator;
        wayland_backend.control_center = gnome_control_center;
        wayland_backend.logout = gnome_logout;
        wayland_backend.screenshot = gnome_screenshot;
        wayland_backend.screenshot_window = gnome_screenshot_window;
        wayland_backend.screenshot_area = gnome_screenshot_area;
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
        wayland_backend.toggle_fullscreen = kde_toggle_fullscreen;
        wayland_backend.show_desktop = kde_show_desktop;
        wayland_backend.lock_screen = kde_lock_screen;
        wayland_backend.terminal = kde_terminal;
        wayland_backend.volume_up = kde_volume_up;
        wayland_backend.volume_down = kde_volume_down;
        wayland_backend.volume_mute = kde_volume_mute;
        wayland_backend.media_play = kde_media_play;
        wayland_backend.media_next = kde_media_next;
        wayland_backend.media_prev = kde_media_prev;
        wayland_backend.www = kde_www;
        wayland_backend.home = kde_home;
        wayland_backend.email = kde_email;
        wayland_backend.search = kde_search;
        wayland_backend.calculator = kde_calculator;
        wayland_backend.control_center = kde_control_center;
        wayland_backend.logout = kde_logout;
        wayland_backend.screenshot = kde_screenshot;
        wayland_backend.screenshot_window = kde_screenshot_window;
        wayland_backend.screenshot_area = kde_screenshot_area;
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
        wayland_backend.toggle_fullscreen = generic_toggle_fullscreen;
        wayland_backend.show_desktop = generic_show_desktop;
        wayland_backend.lock_screen = generic_lock_screen;
        wayland_backend.terminal = generic_terminal;
        wayland_backend.volume_up = generic_volume_up;
        wayland_backend.volume_down = generic_volume_down;
        wayland_backend.volume_mute = generic_volume_mute;
        wayland_backend.media_play = generic_media_play;
        wayland_backend.media_next = generic_media_next;
        wayland_backend.media_prev = generic_media_prev;
        wayland_backend.www = generic_www;
        wayland_backend.home = generic_home;
        wayland_backend.email = generic_email;
        wayland_backend.search = generic_search;
        wayland_backend.calculator = generic_calculator;
        wayland_backend.control_center = generic_control_center;
        wayland_backend.logout = generic_logout;
        wayland_backend.screenshot = generic_screenshot;
        wayland_backend.screenshot_window = generic_screenshot_window;
        wayland_backend.screenshot_area = generic_screenshot_area;
    }
    
    return &wayland_backend;
}