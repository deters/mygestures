#define _GNU_SOURCE
#include <gtk/gtk.h>
#include <string.h>
#include "configuration.h"
#include "configuration_parser.h"
#include "actions.h"
#include "mygestures.h"

typedef struct {
    GtkApplication *app;
    Configuration *config;
    GtkWidget *window;
    GtkWidget *main_list;
    GtkWidget *search_entry;
} GestosApp;

typedef struct {
    GestosApp *app;
    Gesture *gesture;
    GtkWidget *dialog;
    GtkWidget *name_entry;
    GtkWidget *move_combo;
    GtkWidget *action_type_combo;
    GtkWidget *action_val_entry;
    GtkWidget *action_val_label;
    GtkWidget *action_val_box;
    GtkWidget *record_btn;
    gboolean recording;
    GtkWidget *browser_dialog;
} GestureEditor;

static void refresh_gesture_list(GestosApp *gestos);

typedef struct {
    int id;
    const char *name;
    const char *prefix;
    const char *icon;
} ActionType;

static ActionType action_types[] = {
    { ACTION_KEYPRESS, "Key Combination", "keypress", "input-keyboard-symbolic" },
    { ACTION_EXECUTE, "Execute Command", "exec", "system-run-symbolic" },
    { ACTION_KILL, "Close Window", "kill", "window-close-symbolic" },
    { ACTION_TOGGLE_MAXIMIZED, "Toggle Maximized", "toggle-maximized", "window-maximize-symbolic" },
    { ACTION_MAXIMIZE, "Maximize Window", "maximize", "window-maximize-symbolic" },
    { ACTION_RESTORE, "Restore Window", "restore", "window-restore-symbolic" },
    { ACTION_ICONIFY, "Minimize Window", "iconify", "window-minimize-symbolic" },
    { ACTION_RAISE, "Raise Window", "raise", "go-up-symbolic" },
    { ACTION_LOWER, "Lower Window", "lower", "go-down-symbolic" },
    { ACTION_WORKSPACE_LEFT, "Workspace Left", "workspace-left", "go-previous-symbolic" },
    { ACTION_WORKSPACE_RIGHT, "Workspace Right", "workspace-right", "go-next-symbolic" },
    { ACTION_WORKSPACE_UP, "Workspace Up", "workspace-up", "go-up-symbolic" },
    { ACTION_WORKSPACE_DOWN, "Workspace Down", "workspace-down", "go-down-symbolic" },
    { ACTION_SHOW_OVERVIEW, "Show Overview", "show-overview", "view-fullscreen-symbolic" },
    { ACTION_SHOW_APP_GRID, "Show App Grid", "show-app-grid", "view-grid-symbolic" },
    { ACTION_CLICK, "Mouse Click", "click", "input-mouse-symbolic" },
    { ACTION_TOGGLE_FULLSCREEN, "Toggle Fullscreen", "toggle-fullscreen", "view-fullscreen-symbolic" },
    { ACTION_SHOW_DESKTOP, "Show Desktop", "show-desktop", "user-desktop-symbolic" },
    { ACTION_LOCK_SCREEN, "Lock Screen", "lock-screen", "system-lock-screen-symbolic" },
    { ACTION_TERMINAL, "Open Terminal", "terminal", "utilities-terminal-symbolic" },
    { ACTION_VOLUME_UP, "Volume Up", "volume-up", "audio-volume-high-symbolic" },
    { ACTION_VOLUME_DOWN, "Volume Down", "volume-down", "audio-volume-low-symbolic" },
    { ACTION_VOLUME_MUTE, "Volume Mute", "volume-mute", "audio-volume-muted-symbolic" },
    { ACTION_MEDIA_PLAY, "Play/Pause Media", "media-play", "media-playback-start-symbolic" },
    { ACTION_MEDIA_NEXT, "Next Track", "media-next", "media-skip-forward-symbolic" },
    { ACTION_MEDIA_PREV, "Previous Track", "media-prev", "media-skip-backward-symbolic" },
    { ACTION_WWW, "Web Browser", "www", "web-browser-symbolic" },
    { ACTION_HOME, "Home Folder", "home", "folder-home-symbolic" },
    { ACTION_EMAIL, "Email Client", "email", "mail-unread-symbolic" },
    { ACTION_SEARCH, "Search", "search", "system-search-symbolic" },
    { ACTION_CALCULATOR, "Calculator", "calculator", "accessories-calculator-symbolic" },
    { ACTION_CONTROL_CENTER, "System Settings", "control-center", "preferences-system-symbolic" },
    { ACTION_LOGOUT, "Log Out", "logout", "system-log-out-symbolic" },
    { ACTION_SCREENSHOT, "Take Screenshot", "screenshot", "camera-photo-symbolic" },
    { ACTION_SCREENSHOT_WINDOW, "Screenshot Window", "screenshot-window", "camera-photo-symbolic" },
    { ACTION_SCREENSHOT_AREA, "Screenshot Area", "screenshot-area", "camera-photo-symbolic" },
    { ACTION_EXIT_GEST, "Exit MyGestures", "exit-gest", "application-exit-symbolic" },
    { ACTION_RECONF, "Reload Configuration", "reconf", "view-refresh-symbolic" },
    { ACTION_ABORT, "Abort Gesture", "abort", "dialog-cancel-symbolic" },
    { 0, NULL, NULL, NULL }
};

/* --- GNOME ACTION BROWSER --- */

typedef struct {
    char *name;
    char *accelerator;
    char *description;
    char *command;
} GnomeAction;

typedef struct {
    GestureEditor *editor;
    GtkWidget *dialog;
    GtkWidget *list;
    GList *actions;
} GnomeActionBrowser;

static char* translate_gnome_accel(const char *accel) {
    if (!accel || strlen(accel) == 0) return strdup("");

    GString *res = g_string_new("");
    char **parts = g_strsplit(accel, ">", -1);

    for (int i = 0; parts[i] != NULL; i++) {
        char *p = parts[i];
        if (p[0] == '<') p++;

        if (strlen(p) == 0) continue;

        if (parts[i+1] != NULL) {
            /* Modifier */
            if (g_ascii_strcasecmp(p, "Control") == 0 || g_ascii_strcasecmp(p, "Primary") == 0) g_string_append(res, "Control_L+");
            else if (g_ascii_strcasecmp(p, "Alt") == 0) g_string_append(res, "Alt_L+");
            else if (g_ascii_strcasecmp(p, "Shift") == 0) g_string_append(res, "Shift_L+");
            else if (g_ascii_strcasecmp(p, "Super") == 0) g_string_append(res, "Super_L+");
        } else {
            /* Final key */
            g_string_append(res, p);
        }
    }

    g_strfreev(parts);
    return g_string_free(res, FALSE);
}

static void fetch_gnome_shortcuts(GnomeActionBrowser *browser) {
    const char *schemas[] = {
        "org.gnome.desktop.wm.keybindings",
        "org.gnome.settings-daemon.plugins.media-keys",
        "org.gnome.shell.keybindings",
        NULL
    };

    GSettingsSchemaSource *source = g_settings_schema_source_get_default();

    for (int i = 0; schemas[i]; i++) {
        GSettingsSchema *schema = g_settings_schema_source_lookup(source, schemas[i], TRUE);
        if (!schema) continue;

        GSettings *settings = g_settings_new(schemas[i]);
        char **keys = g_settings_schema_list_keys(schema);

        for (int j = 0; keys[j]; j++) {
            GSettingsSchemaKey *skey = g_settings_schema_get_key(schema, keys[j]);
            const char *summary = g_settings_schema_key_get_summary(skey);

            GVariant *val = g_settings_get_value(settings, keys[j]);
            char *accel = NULL;

            if (g_variant_is_of_type(val, G_VARIANT_TYPE_STRING)) {
                accel = g_variant_dup_string(val, NULL);
            } else if (g_variant_is_of_type(val, G_VARIANT_TYPE_STRING_ARRAY)) {
                const char **arr = g_variant_get_strv(val, NULL);
                if (arr && arr[0]) accel = g_strdup(arr[0]);
                g_free(arr);
            }

            if (accel && strlen(accel) > 0 && strcmp(accel, "disabled") != 0) {
                GnomeAction *action = g_new0(GnomeAction, 1);
                action->name = g_strdup(keys[j]);
                action->description = g_strdup(summary ? summary : keys[j]);
                action->accelerator = accel;
                browser->actions = g_list_append(browser->actions, action);
            } else {
                g_free(accel);
            }

            g_variant_unref(val);
            g_settings_schema_key_unref(skey);
        }

        /* Fetch custom shortcuts from this schema if it's media-keys */
        if (strcmp(schemas[i], "org.gnome.settings-daemon.plugins.media-keys") == 0) {
            char **paths = g_settings_get_strv(settings, "custom-keybindings");
            if (paths) {
                for (int k = 0; paths[k]; k++) {
                    GSettings *custom = g_settings_new_with_path("org.gnome.settings-daemon.plugins.media-keys.custom-keybinding", paths[k]);
                    char *c_name = g_settings_get_string(custom, "name");
                    char *c_cmd = g_settings_get_string(custom, "command");
                    char *c_bind = g_settings_get_string(custom, "binding");
                    
                    if (c_bind && strlen(c_bind) > 0) {
                        GnomeAction *action = g_new0(GnomeAction, 1);
                        action->name = g_strdup(c_name);
                        action->description = g_strdup(c_name);
                        action->accelerator = c_bind;
                        action->command = c_cmd;
                        browser->actions = g_list_append(browser->actions, action);
                    } else {
                        g_free(c_cmd);
                    }
                    g_free(c_name);
                    g_free(c_bind);
                    g_object_unref(custom);
                }
                g_strfreev(paths);
            }
        }

        g_strfreev(keys);
        g_settings_schema_unref(schema);
        g_object_unref(settings);
    }
}

static void on_gnome_action_row_activated(GtkListBox *list, GtkListBoxRow *row, gpointer user_data) {
    GnomeActionBrowser *browser = (GnomeActionBrowser *)user_data;
    GnomeAction *action = g_object_get_data(G_OBJECT(row), "gnome-action");

    if (action) {
        if (action->command) {
            /* If it's a custom command, use EXECUTE instead of faking keys */
            gtk_drop_down_set_selected(GTK_DROP_DOWN(browser->editor->action_type_combo), 1); /* Execute */
            gtk_editable_set_text(GTK_EDITABLE(browser->editor->action_val_entry), action->command);
        } else {
            /* Try to map common actions to native types first */
            int native_id = -1;
            if (strcmp(action->name, "close") == 0) native_id = ACTION_KILL;
            else if (strcmp(action->name, "maximize") == 0) native_id = ACTION_MAXIMIZE;
            else if (strcmp(action->name, "unmaximize") == 0) native_id = ACTION_RESTORE;
            else if (strcmp(action->name, "minimize") == 0) native_id = ACTION_ICONIFY;
            else if (strcmp(action->name, "toggle-fullscreen") == 0) native_id = ACTION_TOGGLE_FULLSCREEN;
            else if (strcmp(action->name, "show-desktop") == 0) native_id = ACTION_SHOW_DESKTOP;
            else if (strcmp(action->name, "screensaver") == 0) native_id = ACTION_LOCK_SCREEN;
            else if (strcmp(action->name, "terminal") == 0) native_id = ACTION_TERMINAL;
            else if (strcmp(action->name, "volume-up") == 0) native_id = ACTION_VOLUME_UP;
            else if (strcmp(action->name, "volume-down") == 0) native_id = ACTION_VOLUME_DOWN;
            else if (strcmp(action->name, "volume-mute") == 0) native_id = ACTION_VOLUME_MUTE;
            else if (strcmp(action->name, "play") == 0) native_id = ACTION_MEDIA_PLAY;
            else if (strcmp(action->name, "next") == 0) native_id = ACTION_MEDIA_NEXT;
            else if (strcmp(action->name, "previous") == 0) native_id = ACTION_MEDIA_PREV;
            else if (strcmp(action->name, "switch-to-workspace-left") == 0) native_id = ACTION_WORKSPACE_LEFT;
            else if (strcmp(action->name, "switch-to-workspace-right") == 0) native_id = ACTION_WORKSPACE_RIGHT;
            else if (strcmp(action->name, "switch-to-workspace-up") == 0) native_id = ACTION_WORKSPACE_UP;
            else if (strcmp(action->name, "switch-to-workspace-down") == 0) native_id = ACTION_WORKSPACE_DOWN;
            else if (strcmp(action->name, "panel-main-menu") == 0) native_id = ACTION_SHOW_OVERVIEW;
            else if (strcmp(action->name, "toggle-application-view") == 0) native_id = ACTION_SHOW_APP_GRID;
            else if (strcmp(action->name, "www") == 0) native_id = ACTION_WWW;
            else if (strcmp(action->name, "home") == 0) native_id = ACTION_HOME;
            else if (strcmp(action->name, "email") == 0) native_id = ACTION_EMAIL;
            else if (strcmp(action->name, "search") == 0) native_id = ACTION_SEARCH;
            else if (strcmp(action->name, "calculator") == 0) native_id = ACTION_CALCULATOR;
            else if (strcmp(action->name, "control-center") == 0) native_id = ACTION_CONTROL_CENTER;
            else if (strcmp(action->name, "logout") == 0) native_id = ACTION_LOGOUT;
            else if (strcmp(action->name, "screenshot") == 0 || strcmp(action->name, "screenshot-clip") == 0) native_id = ACTION_SCREENSHOT;
            else if (strcmp(action->name, "window-screenshot") == 0 || strcmp(action->name, "window-screenshot-clip") == 0) native_id = ACTION_SCREENSHOT_WINDOW;
            else if (strcmp(action->name, "area-screenshot") == 0 || strcmp(action->name, "area-screenshot-clip") == 0) native_id = ACTION_SCREENSHOT_AREA;
            
            if (native_id != -1) {
                for (int i = 0; action_types[i].name; i++) {
                    if (action_types[i].id == native_id) {
                        gtk_drop_down_set_selected(GTK_DROP_DOWN(browser->editor->action_type_combo), i);
                        break;
                    }
                }
            } else {
                gtk_drop_down_set_selected(GTK_DROP_DOWN(browser->editor->action_type_combo), 0); /* Keypress */
                char *translated = translate_gnome_accel(action->accelerator);
                gtk_editable_set_text(GTK_EDITABLE(browser->editor->action_val_entry), translated);
                free(translated);
            }
        }
    }

    gtk_window_destroy(GTK_WINDOW(browser->dialog));
}

static gboolean gnome_action_filter_func(GtkListBoxRow *row, gpointer user_data) {
    const char *search_text = (const char *)user_data;
    if (!search_text || strlen(search_text) == 0) return TRUE;
    
    GnomeAction *action = g_object_get_data(G_OBJECT(row), "gnome-action");
    if (!action) return FALSE;
    
    return (g_strrstr(action->name, search_text) || 
            g_strrstr(action->description, search_text) ||
            (action->command && g_strrstr(action->command, search_text)));
}

static void on_gnome_browser_search_changed(GtkSearchEntry *entry, gpointer user_data) {
    GnomeActionBrowser *browser = (GnomeActionBrowser *)user_data;
    const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
    gtk_list_box_set_filter_func(GTK_LIST_BOX(browser->list), gnome_action_filter_func, (gpointer)text, NULL);
}

static void on_browser_dialog_destroy(GtkWidget *widget, gpointer user_data) {
    GnomeActionBrowser *browser = (GnomeActionBrowser *)user_data;
    if (browser) {
        if (browser->editor) {
            browser->editor->browser_dialog = NULL;
        }
        for (GList *l = browser->actions; l; l = l->next) {
            GnomeAction *a = (GnomeAction *)l->data;
            if (a) {
                g_free(a->name);
                g_free(a->accelerator);
                g_free(a->description);
                g_free(a->command);
                g_free(a);
            }
        }
        g_list_free(browser->actions);
        g_free(browser);
    }
}

static void open_gnome_action_browser(GtkWidget *btn, gpointer user_data) {
    GestureEditor *editor = (GestureEditor *)user_data;
    if (editor->browser_dialog) {
        gtk_window_present(GTK_WINDOW(editor->browser_dialog));
        return;
    }

    GnomeActionBrowser *browser = g_new0(GnomeActionBrowser, 1);
    browser->editor = editor;

    browser->dialog = gtk_window_new();
    editor->browser_dialog = browser->dialog;
    g_signal_connect(browser->dialog, "destroy", G_CALLBACK(on_browser_dialog_destroy), browser);

    gtk_window_set_title(GTK_WINDOW(browser->dialog), "Browse GNOME Actions");
    gtk_window_set_transient_for(GTK_WINDOW(browser->dialog), GTK_WINDOW(editor->dialog));
    gtk_window_set_modal(GTK_WINDOW(browser->dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(browser->dialog), 550, 650);

    GtkWidget *header = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(browser->dialog), header);

    GtkWidget *close_btn = gtk_button_new_with_label("Close");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), close_btn);
    g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(gtk_window_destroy), browser->dialog);

    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(browser->dialog), content);
    
    GtkWidget *search_entry = gtk_search_entry_new();
    gtk_widget_set_margin_top(search_entry, 12);
    gtk_widget_set_margin_bottom(search_entry, 12);
    gtk_widget_set_margin_start(search_entry, 12);
    gtk_widget_set_margin_end(search_entry, 12);
    gtk_box_append(GTK_BOX(content), search_entry);
    g_signal_connect(search_entry, "search-changed", G_CALLBACK(on_gnome_browser_search_changed), browser);

    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_box_append(GTK_BOX(content), scrolled);

    browser->list = gtk_list_box_new();
    gtk_widget_add_css_class(browser->list, "boxed-list");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), browser->list);

    fetch_gnome_shortcuts(browser);

    for (GList *l = browser->actions; l; l = l->next) {
        GnomeAction *a = (GnomeAction *)l->data;
        GtkWidget *row = gtk_list_box_row_new();
        g_object_set_data(G_OBJECT(row), "gnome-action", a);

        GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_set_margin_top(hbox, 10);
        gtk_widget_set_margin_bottom(hbox, 10);
        gtk_widget_set_margin_start(hbox, 10);
        gtk_widget_set_margin_end(hbox, 10);

        GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_widget_set_hexpand(vbox, TRUE);

        GtkWidget *l_desc = gtk_label_new(a->description);
        gtk_widget_set_halign(l_desc, GTK_ALIGN_START);
        gtk_label_set_wrap(GTK_LABEL(l_desc), TRUE);
        gtk_box_append(GTK_BOX(vbox), l_desc);

        GString *subtext = g_string_new(a->name);
        if (a->command) g_string_append_printf(subtext, " — %s", a->command);
        
        GtkWidget *l_name = gtk_label_new(subtext->str);
        gtk_widget_set_halign(l_name, GTK_ALIGN_START);
        gtk_widget_add_css_class(l_name, "dim-label");
        gtk_box_append(GTK_BOX(vbox), l_name);
        g_string_free(subtext, TRUE);

        gtk_box_append(GTK_BOX(hbox), vbox);

        GtkWidget *l_accel = gtk_label_new(a->accelerator);
        gtk_widget_add_css_class(l_accel, "accel-badge");
        gtk_box_append(GTK_BOX(hbox), l_accel);

        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), hbox);
        gtk_list_box_append(GTK_LIST_BOX(browser->list), row);
    }

    g_signal_connect(browser->list, "row-activated", G_CALLBACK(on_gnome_action_row_activated), browser);

    gtk_window_present(GTK_WINDOW(browser->dialog));
}

/* --- GESTURE EDITOR --- */

static const char* get_action_icon(int id) {
    for (int i = 0; action_types[i].name; i++) {
        if (action_types[i].id == id) return action_types[i].icon;
    }
    return "system-run-symbolic";
}

static const char* get_action_class(int id) {
    switch (id) {
        case ACTION_KEYPRESS:
        case ACTION_EXECUTE:
            return "icon-bg-purple";
        case ACTION_KILL:
        case ACTION_MAXIMIZE:
        case ACTION_RESTORE:
        case ACTION_TOGGLE_MAXIMIZED:
        case ACTION_ICONIFY:
        case ACTION_RAISE:
        case ACTION_LOWER:
        case ACTION_TOGGLE_FULLSCREEN:
            return "icon-bg-orange";
        case ACTION_WORKSPACE_LEFT:
        case ACTION_WORKSPACE_RIGHT:
        case ACTION_WORKSPACE_UP:
        case ACTION_WORKSPACE_DOWN:
        case ACTION_SHOW_OVERVIEW:
        case ACTION_SHOW_APP_GRID:
            return "icon-bg-blue";
        case ACTION_EXIT_GEST:
        case ACTION_RECONF:
        case ACTION_ABORT:
        case ACTION_WWW:
        case ACTION_HOME:
        case ACTION_EMAIL:
        case ACTION_SEARCH:
        case ACTION_CALCULATOR:
        case ACTION_CONTROL_CENTER:
        case ACTION_LOGOUT:
        case ACTION_SCREENSHOT:
        case ACTION_SCREENSHOT_WINDOW:
        case ACTION_SCREENSHOT_AREA:
        default:
            return "icon-bg-green";
    }
}

static void on_action_type_changed(GObject *gobject, GParamSpec *pspec, gpointer user_data) {
    GtkDropDown *combo = GTK_DROP_DOWN(gobject);
    GestureEditor *editor = (GestureEditor *)user_data;
    guint index = gtk_drop_down_get_selected(combo);
    if (index == GTK_INVALID_LIST_POSITION) return;
    
    int id = action_types[index].id;
    
    if (id == ACTION_EXECUTE || id == ACTION_KEYPRESS) {
        gtk_widget_set_visible(editor->action_val_box, TRUE);
        gtk_label_set_text(GTK_LABEL(editor->action_val_label), 
            (id == ACTION_EXECUTE) ? "Command:" : "Keys:");
        gtk_widget_set_visible(editor->record_btn, (id == ACTION_KEYPRESS));
    } else {
        gtk_widget_set_visible(editor->action_val_box, FALSE);
    }
}

static gboolean on_record_key_pressed(GtkEventControllerKey *controller,
                                     guint keyval,
                                     guint keycode,
                                     GdkModifierType state,
                                     gpointer user_data) {
    GestureEditor *editor = (GestureEditor *)user_data;
    if (!editor->recording) return FALSE;

    GString *s = g_string_new("");
    if (state & GDK_CONTROL_MASK) g_string_append(s, "Control_L+");
    if (state & GDK_ALT_MASK) g_string_append(s, "Alt_L+");
    if (state & GDK_SHIFT_MASK) g_string_append(s, "Shift_L+");
    if (state & GDK_SUPER_MASK) g_string_append(s, "Super_L+");

    const char *name = gdk_keyval_name(keyval);
    if (name) {
        /* Basic mapping for common keys */
        if (strcmp(name, "Control_L") != 0 && strcmp(name, "Control_R") != 0 &&
            strcmp(name, "Alt_L") != 0 && strcmp(name, "Alt_R") != 0 &&
            strcmp(name, "Shift_L") != 0 && strcmp(name, "Shift_R") != 0 &&
            strcmp(name, "Super_L") != 0 && strcmp(name, "Super_R") != 0) {
            
            g_string_append(s, name);
            gtk_editable_set_text(GTK_EDITABLE(editor->action_val_entry), s->str);
            
            editor->recording = FALSE;
            gtk_button_set_label(GTK_BUTTON(editor->record_btn), "Record");
            gtk_widget_remove_css_class(editor->record_btn, "recording-active");
        }
    }
    
    g_string_free(s, TRUE);
    return TRUE;
}

static void on_record_clicked(GtkWidget *btn, gpointer user_data) {
    GestureEditor *editor = (GestureEditor *)user_data;
    editor->recording = !editor->recording;
    if (editor->recording) {
        gtk_button_set_label(GTK_BUTTON(btn), "Recording...");
        gtk_widget_add_css_class(btn, "recording-active");
    } else {
        gtk_button_set_label(GTK_BUTTON(btn), "Record");
        gtk_widget_remove_css_class(btn, "recording-active");
    }
}

static void on_gesture_save_clicked(GtkWidget *btn, gpointer user_data) {
    GestureEditor *editor = (GestureEditor *)user_data;
    const char *name = gtk_editable_get_text(GTK_EDITABLE(editor->name_entry));
    
    GtkStringList *move_list = GTK_STRING_LIST(gtk_drop_down_get_model(GTK_DROP_DOWN(editor->move_combo)));
    guint move_idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(editor->move_combo));
    const char *move_name = (move_idx != GTK_INVALID_LIST_POSITION) ? 
        gtk_string_list_get_string(move_list, move_idx) : NULL;
    
    if (!move_name) {
        if (editor->app->config->movement_count > 0) {
            move_name = editor->app->config->movement_list[0]->name;
        } else {
            move_name = "anonymous";
        }
    }
    
    guint type_idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(editor->action_type_combo));
    if (type_idx == GTK_INVALID_LIST_POSITION) type_idx = 0;
    ActionType *type = &action_types[type_idx];
    const char *val = gtk_editable_get_text(GTK_EDITABLE(editor->action_val_entry));
    
    char *full_action;
    if (type->id == ACTION_EXECUTE || type->id == ACTION_KEYPRESS) {
        if (asprintf(&full_action, "%s %s", type->prefix, val) == -1) full_action = NULL;
    } else {
        full_action = strdup(type->prefix);
    }

    if (editor->gesture) {
        editor->gesture->name = strdup(name);
        editor->gesture->movement = configuration_find_movement_by_name(editor->app->config, (char*)move_name);
        editor->gesture->action_count = 0;
        configuration_add_action_from_string(editor->gesture, full_action);
    } else {
        Gesture *new_g = configuration_create_gesture(editor->app->config, (char*)name, (char*)move_name);
        configuration_add_action_from_string(new_g, full_action);
    }
    free(full_action);
    refresh_gesture_list(editor->app);

    gtk_window_destroy(GTK_WINDOW(editor->dialog));
}

static void on_dropdown_setup(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *icon = gtk_image_new();
    gtk_widget_add_css_class(icon, "drop-icon");
    gtk_box_append(GTK_BOX(box), icon);
    GtkWidget *label = gtk_label_new(NULL);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), label);
    gtk_list_item_set_child(list_item, box);
}

static void on_dropdown_bind(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
    GtkWidget *box = gtk_list_item_get_child(list_item);
    GtkWidget *icon = gtk_widget_get_first_child(box);
    GtkWidget *label = gtk_widget_get_next_sibling(icon);
    GtkStringObject *string_obj = GTK_STRING_OBJECT(gtk_list_item_get_item(list_item));
    const char *text = gtk_string_object_get_string(string_obj);
    gtk_label_set_text(GTK_LABEL(label), text);
    const char *icon_name = "system-run-symbolic";
    for (int i = 0; action_types[i].name; i++) {
        if (strcmp(action_types[i].name, text) == 0) {
            icon_name = action_types[i].icon;
            break;
        }
    }
    gtk_image_set_from_icon_name(GTK_IMAGE(icon), icon_name);
}

static void on_editor_dialog_destroy(GtkWidget *widget, gpointer user_data) {
    GestureEditor *editor = (GestureEditor *)user_data;
    if (editor->browser_dialog) {
        gtk_window_destroy(GTK_WINDOW(editor->browser_dialog));
    }
    g_free(editor);
}

static void open_gesture_editor(GestosApp *gestos, Gesture *g) {
    GestureEditor *editor = g_new0(GestureEditor, 1);
    editor->app = gestos;
    editor->gesture = g;
    
    editor->dialog = gtk_window_new();
    g_signal_connect(editor->dialog, "destroy", G_CALLBACK(on_editor_dialog_destroy), editor);

    gtk_window_set_title(GTK_WINDOW(editor->dialog), g ? "Edit Gesture" : "New Gesture");
    gtk_window_set_transient_for(GTK_WINDOW(editor->dialog), GTK_WINDOW(gestos->window));
    gtk_window_set_modal(GTK_WINDOW(editor->dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(editor->dialog), 450, -1);

    GtkWidget *header = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(editor->dialog), header);

    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), cancel_btn);
    g_signal_connect_swapped(cancel_btn, "clicked", G_CALLBACK(gtk_window_destroy), editor->dialog);

    GtkWidget *save_btn = gtk_button_new_with_label("Save");
    gtk_widget_add_css_class(save_btn, "suggested-action");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), save_btn);
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_gesture_save_clicked), editor);

    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(editor->dialog), content);
    gtk_widget_set_margin_top(content, 18);
    gtk_widget_set_margin_bottom(content, 18);
    gtk_widget_set_margin_start(content, 18);
    gtk_widget_set_margin_end(content, 18);
    
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_box_append(GTK_BOX(content), grid);
    
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Name:"), 0, 0, 1, 1);
    editor->name_entry = gtk_entry_new();
    gtk_widget_set_hexpand(editor->name_entry, TRUE);
    if (g) gtk_editable_set_text(GTK_EDITABLE(editor->name_entry), g->name);
    gtk_grid_attach(GTK_GRID(grid), editor->name_entry, 1, 0, 1, 1);
    
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Movement:"), 0, 1, 1, 1);
    GtkStringList *move_sl = gtk_string_list_new(NULL);
    for (int i = 0; i < gestos->config->movement_count; i++) {
        gtk_string_list_append(move_sl, gestos->config->movement_list[i]->name);
    }
    editor->move_combo = gtk_drop_down_new(G_LIST_MODEL(move_sl), NULL);
    for (int i = 0; i < gestos->config->movement_count; i++) {
        if (g && g->movement && strcmp(g->movement->name, gestos->config->movement_list[i]->name) == 0) {
            gtk_drop_down_set_selected(GTK_DROP_DOWN(editor->move_combo), i);
        }
    }
    if (!g) gtk_drop_down_set_selected(GTK_DROP_DOWN(editor->move_combo), 0);
    gtk_grid_attach(GTK_GRID(grid), editor->move_combo, 1, 1, 1, 1);
    
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Action Type:"), 0, 2, 1, 1);
    GtkStringList *type_sl = gtk_string_list_new(NULL);
    int active_type = 0;
    for (int i = 0; action_types[i].name; i++) {
        gtk_string_list_append(type_sl, action_types[i].name);
        if (g && g->action_count > 0 && g->action_list[0]->type == action_types[i].id) {
            active_type = i;
        }
    }
    editor->action_type_combo = gtk_drop_down_new(G_LIST_MODEL(type_sl), NULL);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(editor->action_type_combo), active_type);
    gtk_grid_attach(GTK_GRID(grid), editor->action_type_combo, 1, 2, 1, 1);
    
    editor->action_val_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    editor->action_val_label = gtk_label_new("Command:");
    gtk_grid_attach(GTK_GRID(grid), editor->action_val_label, 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), editor->action_val_box, 1, 3, 1, 1);
    
    editor->action_val_entry = gtk_entry_new();
    gtk_widget_set_hexpand(editor->action_val_entry, TRUE);
    gtk_box_append(GTK_BOX(editor->action_val_box), editor->action_val_entry);
    
    editor->record_btn = gtk_button_new_with_label("Record");
    gtk_widget_add_css_class(editor->record_btn, "record-btn");
    gtk_box_append(GTK_BOX(editor->action_val_box), editor->record_btn);
    g_signal_connect(editor->record_btn, "clicked", G_CALLBACK(on_record_clicked), editor);

    GtkWidget *browse_btn = gtk_button_new_with_label("Browse GNOME Actions");
    gtk_widget_add_css_class(browse_btn, "browse-btn");
    gtk_grid_attach(GTK_GRID(grid), browse_btn, 1, 4, 1, 1);
    g_signal_connect(browse_btn, "clicked", G_CALLBACK(open_gnome_action_browser), editor);

    if (g && g->action_count > 0) {
        if (g->action_list[0]->original_str)
            gtk_editable_set_text(GTK_EDITABLE(editor->action_val_entry), g->action_list[0]->original_str);
    }
    
    GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(on_dropdown_setup), NULL);
    g_signal_connect(factory, "bind", G_CALLBACK(on_dropdown_bind), NULL);
    gtk_drop_down_set_factory(GTK_DROP_DOWN(editor->action_type_combo), factory);

    GtkListItemFactory *list_factory = gtk_signal_list_item_factory_new();
    g_signal_connect(list_factory, "setup", G_CALLBACK(on_dropdown_setup), NULL);
    g_signal_connect(list_factory, "bind", G_CALLBACK(on_dropdown_bind), NULL);
    gtk_drop_down_set_list_factory(GTK_DROP_DOWN(editor->action_type_combo), list_factory);

    g_signal_connect(editor->action_type_combo, "notify::selected", G_CALLBACK(on_action_type_changed), editor);
    on_action_type_changed(G_OBJECT(editor->action_type_combo), NULL, editor);

    GtkEventController *key_controller = gtk_event_controller_key_new();
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(on_record_key_pressed), editor);
    gtk_widget_add_controller(editor->dialog, key_controller);
    
    gtk_window_present(GTK_WINDOW(editor->dialog));
}

static void on_gesture_row_activated(GtkListBox *list, GtkListBoxRow *row, gpointer user_data) {
    GestosApp *gestos = (GestosApp *)user_data;
    Gesture *g = g_object_get_data(G_OBJECT(row), "gesture-ptr");
    if (g) open_gesture_editor(gestos, g);
}

static void on_add_gesture_clicked(GtkWidget *widget, gpointer user_data) {
    open_gesture_editor((GestosApp *)user_data, NULL);
}

static void on_gesture_delete_clicked(GtkWidget *widget, gpointer user_data) {
    Gesture *g = (Gesture *)user_data;
    GtkWidget *list = gtk_widget_get_ancestor(widget, GTK_TYPE_LIST_BOX);
    GestosApp *gestos = g_object_get_data(G_OBJECT(list), "gestos-app");
    Configuration *conf = gestos->config;

    int found = -1;
    for (int i = 0; i < conf->gesture_count; i++) {
        if (conf->gesture_list[i] == g) {
            found = i;
            break;
        }
    }
    if (found != -1) {
        for (int i = found; i < conf->gesture_count - 1; i++) {
            conf->gesture_list[i] = conf->gesture_list[i+1];
        }
        conf->gesture_count--;
    }
    refresh_gesture_list(gestos);
}

static char* get_full_action_str(Action *a) {
    for (int i = 0; action_types[i].name; i++) {
        if (action_types[i].id == a->type) {
            char *res = NULL;
            if (a->original_str && strlen(a->original_str) > 0) {
                if (asprintf(&res, "%s %s", action_types[i].prefix, a->original_str) == -1) res = NULL;
            } else {
                res = strdup(action_types[i].prefix);
            }
            return res;
        }
    }
    return strdup("unknown");
}

/* --- MAIN UI LOGIC --- */

static void add_gesture_row(GestosApp *gestos, Gesture *gesture) {
    GtkWidget *row = gtk_list_box_row_new();
    gtk_widget_add_css_class(row, "gesture-row");
    g_object_set_data(G_OBJECT(row), "gesture-ptr", gesture);
    
    GtkWidget *main_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_margin_start(main_hbox, 16);
    gtk_widget_set_margin_end(main_hbox, 16);
    gtk_widget_set_margin_top(main_hbox, 12);
    gtk_widget_set_margin_bottom(main_hbox, 12);

    int action_id = ACTION_NULL;
    if (gesture->action_count > 0) action_id = gesture->action_list[0]->type;
    
    GtkWidget *icon_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(icon_box, "icon-holder");
    gtk_widget_add_css_class(icon_box, get_action_class(action_id));
    gtk_widget_set_valign(icon_box, GTK_ALIGN_CENTER);
    
    GtkWidget *icon = gtk_image_new_from_icon_name(get_action_icon(action_id));
    gtk_widget_set_size_request(icon, 20, 20);
    gtk_box_append(GTK_BOX(icon_box), icon);
    gtk_box_append(GTK_BOX(main_hbox), icon_box);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(vbox, TRUE);
    gtk_widget_set_valign(vbox, GTK_ALIGN_CENTER);

    GtkWidget *label_name = gtk_label_new(gesture->name);
    gtk_widget_set_halign(label_name, GTK_ALIGN_START);
    PangoAttrList *attrs = pango_attr_list_new();
    pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    gtk_label_set_attributes(GTK_LABEL(label_name), attrs);
    pango_attr_list_unref(attrs);
    gtk_box_append(GTK_BOX(vbox), label_name);

    if (gesture->action_count > 0) {
        char *full_action = get_full_action_str(gesture->action_list[0]);
        GtkWidget *label_action = gtk_label_new(full_action);
        gtk_widget_set_halign(label_action, GTK_ALIGN_START);
        gtk_widget_add_css_class(label_action, "action-label");
        gtk_box_append(GTK_BOX(vbox), label_action);
        free(full_action);
    }
    
    gtk_box_append(GTK_BOX(main_hbox), vbox);

    GtkWidget *label_move = gtk_label_new(gesture->movement ? gesture->movement->name : "Unknown");
    gtk_widget_add_css_class(label_move, "move-badge");
    gtk_widget_set_valign(label_move, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(main_hbox), label_move);

    GtkWidget *del_btn = gtk_button_new_from_icon_name("user-trash-symbolic");
    gtk_widget_add_css_class(del_btn, "flat");
    gtk_widget_add_css_class(del_btn, "destructive-action");
    gtk_widget_set_valign(del_btn, GTK_ALIGN_CENTER);
    g_signal_connect(del_btn, "clicked", G_CALLBACK(on_gesture_delete_clicked), gesture);
    gtk_box_append(GTK_BOX(main_hbox), del_btn);

    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), main_hbox);
    gtk_list_box_append(GTK_LIST_BOX(gestos->main_list), row);
}

static void refresh_gesture_list(GestosApp *gestos) {
    GtkWidget *child = gtk_widget_get_first_child(gestos->main_list);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_list_box_remove(GTK_LIST_BOX(gestos->main_list), child);
        child = next;
    }

    const char *search_text = gtk_editable_get_text(GTK_EDITABLE(gestos->search_entry));

    for (int i = 0; i < gestos->config->gesture_count; i++) {
        Gesture *g = gestos->config->gesture_list[i];
        if (search_text && strlen(search_text) > 0) {
            if (!g_strrstr(g->name, search_text)) continue;
        }
        add_gesture_row(gestos, g);
    }
}

static void on_search_changed(GtkSearchEntry *entry, gpointer user_data) {
    refresh_gesture_list((GestosApp *)user_data);
}

static void on_save_config_clicked(GtkWidget *widget, gpointer user_data) {
    GestosApp *gestos = (GestosApp *)user_data;
    char *filename = configuration_get_default_filename();
    configuration_save_to_file(gestos->config, filename);
    
    GtkWidget *toast = gtk_label_new("Configuration saved!");
    gtk_widget_add_css_class(toast, "toast");
    gtk_box_append(GTK_BOX(gtk_window_get_child(GTK_WINDOW(gestos->window))), toast);
    g_timeout_add_seconds(2, (GSourceFunc)gtk_widget_unparent, toast);
    free(filename);
}

static void on_about_clicked(GtkWidget *widget, gpointer user_data) {
    GtkWindow *parent = GTK_WINDOW(user_data);
    gtk_show_about_dialog(parent, "program-name", "Gestos", "version", "1.3.0", "logo-icon-name", "input-mouse", NULL);
}

static void activate(GtkApplication *app, gpointer user_data) {
    GestosApp *gestos = (GestosApp *)user_data;

    gestos->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(gestos->window), "Gestos");
    gtk_window_set_default_size(GTK_WINDOW(gestos->window), 650, 700);

    GtkWidget *header = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(gestos->window), header);

    GtkWidget *save_btn = gtk_button_new_with_label("Save");
    gtk_widget_add_css_class(save_btn, "suggested-action");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), save_btn);
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_save_config_clicked), gestos);

    GtkWidget *add_gest_btn = gtk_button_new_from_icon_name("list-add-symbolic");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), add_gest_btn);
    gtk_widget_set_tooltip_text(add_gest_btn, "Add Gesture");
    g_signal_connect(add_gest_btn, "clicked", G_CALLBACK(on_add_gesture_clicked), gestos);

    GtkWidget *about_btn = gtk_button_new_from_icon_name("help-about-symbolic");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), about_btn);
    g_signal_connect(about_btn, "clicked", G_CALLBACK(on_about_clicked), gestos->window);

    /* CONTENT */
    GtkWidget *content_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(gestos->window), content_vbox);

    GtkWidget *content_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_top(content_header, 24);
    gtk_widget_set_margin_bottom(content_header, 24);
    gtk_widget_set_margin_start(content_header, 24);
    gtk_widget_set_margin_end(content_header, 24);
    gtk_box_append(GTK_BOX(content_vbox), content_header);

    GtkWidget *gestos_title = gtk_label_new("Gestures");
    gtk_widget_add_css_class(gestos_title, "context-title");
    gtk_widget_set_hexpand(gestos_title, TRUE);
    gtk_widget_set_halign(gestos_title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(content_header), gestos_title);

    gestos->search_entry = gtk_search_entry_new();
    gtk_box_append(GTK_BOX(content_header), gestos->search_entry);
    g_signal_connect(gestos->search_entry, "search-changed", G_CALLBACK(on_search_changed), gestos);

    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_box_append(GTK_BOX(content_vbox), scrolled);

    gestos->main_list = gtk_list_box_new();
    gtk_widget_set_margin_start(gestos->main_list, 24);
    gtk_widget_set_margin_end(gestos->main_list, 24);
    gtk_widget_add_css_class(gestos->main_list, "boxed-list");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), gestos->main_list);
    g_object_set_data(G_OBJECT(gestos->main_list), "gestos-app", gestos);
    g_signal_connect(gestos->main_list, "row-activated", G_CALLBACK(on_gesture_row_activated), gestos);

    /* LOAD */
    gestos->config = configuration_new();
    configuration_load_from_defaults(gestos->config, 0);
    refresh_gesture_list(gestos);

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider,
        "window { background-color: @theme_bg_color; }\n"
        ".context-title { font-size: 2.2em; font-weight: 800; letter-spacing: -0.5px; margin-bottom: 4px; }\n"
        "entry { border-radius: 10px; padding: 8px 12px; border: 1px solid alpha(currentColor, 0.15); background: @view_bg_color; transition: all 0.2s ease; }\n"
        "entry:focus { border-color: #6366f1; box-shadow: 0 0 0 3px rgba(99, 102, 241, 0.2); }\n"
        "dropdown { border-radius: 10px; padding: 6px 12px; border: 1px solid alpha(currentColor, 0.15); background: @view_bg_color; transition: all 0.2s ease; }\n"
        "dropdown:focus { border-color: #6366f1; }\n"
        "scrolledwindow { border: none; }\n"
        ".boxed-list { background: @view_bg_color; border: 1px solid alpha(currentColor, 0.08); border-radius: 16px; box-shadow: 0 4px 24px rgba(0, 0, 0, 0.02); }\n"
        ".gesture-row { background: transparent; border-bottom: 1px solid alpha(currentColor, 0.06); transition: all 0.2s ease; }\n"
        ".gesture-row:last-child { border-bottom: none; }\n"
        ".gesture-row:hover { background: alpha(currentColor, 0.03); }\n"
        ".icon-holder { border-radius: 10px; padding: 6px; transition: transform 0.2s ease; }\n"
        ".icon-holder:hover { transform: scale(1.05); }\n"
        ".icon-bg-purple { background: rgba(139, 92, 246, 0.12); color: #8b5cf6; }\n"
        ".icon-bg-orange { background: rgba(249, 115, 22, 0.12); color: #f97316; }\n"
        ".icon-bg-blue { background: rgba(59, 130, 246, 0.12); color: #3b82f6; }\n"
        ".icon-bg-green { background: rgba(16, 185, 129, 0.12); color: #10b981; }\n"
        ".move-badge { background: rgba(99, 102, 241, 0.08); color: #6366f1; padding: 4px 12px; border-radius: 12px; font-weight: 700; font-size: 0.8em; border: 1px solid rgba(99, 102, 241, 0.15); }\n"
        ".accel-badge { background: alpha(currentColor, 0.05); color: alpha(currentColor, 0.8); padding: 4px 8px; border-radius: 8px; font-family: monospace, Courier, monospace; font-weight: 600; font-size: 0.8em; border: 1px solid alpha(currentColor, 0.1); }\n"
        ".action-label { font-size: 0.82em; opacity: 0.6; }\n"
        ".dim-label { font-size: 0.82em; opacity: 0.5; font-style: italic; }\n"
        "headerbar { background: @theme_bg_color; border-bottom: 1px solid alpha(currentColor, 0.06); padding: 8px 12px; }\n"
        "button { border-radius: 8px; padding: 6px 12px; transition: all 0.2s ease; }\n"
        ".suggested-action { background: linear-gradient(135deg, #6366f1, #4f46e5) !important; color: white !important; border: none !important; font-weight: 600 !important; box-shadow: 0 4px 12px rgba(99, 102, 241, 0.25) !important; }\n"
        ".suggested-action:hover { background: linear-gradient(135deg, #4f46e5, #4338ca) !important; box-shadow: 0 6px 16px rgba(99, 102, 241, 0.35) !important; }\n"
        ".destructive-action { color: alpha(currentColor, 0.4); }\n"
        ".destructive-action:hover { color: #ef4444 !important; background: rgba(239, 68, 68, 0.08) !important; }\n"
        ".recording-active { background: linear-gradient(135deg, #ef4444, #dc2626) !important; color: white !important; box-shadow: 0 4px 12px rgba(239, 68, 68, 0.3) !important; border: none !important; }\n"
        ".browse-btn { background: rgba(99, 102, 241, 0.06); color: #6366f1; border: 1px dashed rgba(99, 102, 241, 0.25); border-radius: 10px; padding: 10px; font-weight: 600; }\n"
        ".browse-btn:hover { background: rgba(99, 102, 241, 0.1); border-color: #6366f1; }\n"
        "searchentry { border-radius: 18px; padding-left: 6px; padding-right: 6px; }\n"
        ".toast { background: alpha(@theme_text_color, 0.95); color: @theme_bg_color; padding: 8px 18px; border-radius: 20px; position: absolute; bottom: 30px; left: 50%; transform: translateX(-50%); font-weight: bold; box-shadow: 0 10px 30px rgba(0, 0, 0, 0.15); }\n");
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    gtk_window_present(GTK_WINDOW(gestos->window));
}

int main(int argc, char **argv) {
    GestosApp gestos = {0};
    int status;
    gestos.app = gtk_application_new("org.mygestures.gestos", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(gestos.app, "activate", G_CALLBACK(activate), &gestos);
    status = g_application_run(G_APPLICATION(gestos.app), argc, argv);
    g_object_unref(gestos.app);
    return status;
}
