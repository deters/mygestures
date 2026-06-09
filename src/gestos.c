#define _GNU_SOURCE
#include <gtk/gtk.h>
#include <string.h>
#include <unistd.h>
#include "configuration.h"
#include "configuration_parser.h"
#include "actions.h"
#include "mygestures.h"
#include "grabbing.h"

typedef struct {
    GtkApplication *app;
    Configuration *config;
    GtkWidget *window;
    GtkWidget *main_list;
    GtkWidget *search_entry;

    GtkWidget *status_dot;
    GtkWidget *status_label;
    GtkWidget *daemon_switch;
    gulong switch_handler_id;
} GestosApp;

typedef struct {
    GestosApp *app;
    Gesture *gesture;
    GtkWidget *dialog;
    GtkWidget *name_entry;
    GtkWidget *move_combo;
    GtkWidget *category_combo;
    GtkWidget *action_combo;
    GtkWidget *action_val_entry;
    GtkWidget *action_val_label;
    GtkWidget *action_val_box;
    GtkWidget *record_btn;
    gboolean recording;
    GtkWidget *browser_dialog;

    GtkWidget *canvas;
    Point2D *drawn_points;
    int drawn_count;
    int drawn_capacity;
    gboolean is_drawing;
    char *custom_expression;

    GList *all_options;
    GList *current_options;
    gboolean initializing;
} GestureEditor;

static void refresh_gesture_list(GestosApp *gestos);
static GtkWidget *create_gesture_row_content(GestosApp *gestos, Gesture *gesture);
static void add_gesture_row(GestosApp *gestos, Gesture *gesture);
static gboolean is_daemon_running(void);
static void start_daemon(void);
static void stop_daemon(void);
static gboolean check_daemon_status_timer(gpointer user_data);
static gboolean on_daemon_switch_state_set(GtkSwitch *sw, gboolean state, gpointer user_data);
static void reload_daemon_if_running(void);

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
    { ACTION_GNOME, "GNOME Action", "gnome", "preferences-system-symbolic" },
    { 0, NULL, NULL, NULL }
};

/* --- EDITOR ACTION CATEGORIES & OPTIONS --- */

enum {
    CAT_INPUT,
    CAT_WINDOW,
    CAT_WORKSPACE,
    CAT_MEDIA,
    CAT_SYSTEM,
    CAT_APP,
    CAT_GNOME,
    CAT_OTHER,
    CAT_COUNT
};

static const char *category_names[] = {
    "Input Emulation",
    "Window Management",
    "Workspaces & Overview",
    "Media & Audio",
    "System & Settings",
    "Applications",
    "GNOME Actions (Native)",
    "Other/Internal"
};

typedef struct {
    int category;
    int action_id;
    char *name;
    char *gnome_key;
    char *icon;
    char *tooltip;
    char *custom_cmd;
} EditorActionOption;

static void fetch_gnome_action_options(GList **list) {
    const char *schemas[] = {
        "org.gnome.desktop.wm.keybindings",
        "org.gnome.settings-daemon.plugins.media-keys",
        "org.gnome.shell.keybindings",
        NULL
    };

    GSettingsSchemaSource *source = g_settings_schema_source_get_default();
    if (!source) return;

    for (int i = 0; schemas[i]; i++) {
        GSettingsSchema *schema = g_settings_schema_source_lookup(source, schemas[i], TRUE);
        if (!schema) continue;

        GSettings *settings = g_settings_new(schemas[i]);
        if (!settings) {
            g_settings_schema_unref(schema);
            continue;
        }
        char **keys = g_settings_schema_list_keys(schema);
        if (keys) {
            for (int j = 0; keys[j]; j++) {
                GSettingsSchemaKey *skey = g_settings_schema_get_key(schema, keys[j]);
                if (!skey) continue;
                const char *summary = g_settings_schema_key_get_summary(skey);

                GVariant *val = g_settings_get_value(settings, keys[j]);
                if (!val) {
                    g_settings_schema_key_unref(skey);
                    continue;
                }
                char *accel = NULL;

                if (g_variant_is_of_type(val, G_VARIANT_TYPE_STRING)) {
                    accel = g_variant_dup_string(val, NULL);
                } else if (g_variant_is_of_type(val, G_VARIANT_TYPE_STRING_ARRAY)) {
                    const char **arr = g_variant_get_strv(val, NULL);
                    if (arr && arr[0]) accel = g_strdup(arr[0]);
                    g_free(arr);
                }

                if (summary && strlen(summary) > 0) {
                    EditorActionOption *opt = g_new0(EditorActionOption, 1);
                    opt->category = CAT_GNOME;
                    opt->action_id = ACTION_GNOME;
                    opt->name = g_strdup(summary);
                    opt->gnome_key = g_strdup(keys[j]);
                    opt->icon = g_strdup("preferences-system-symbolic");
                    if (accel && strlen(accel) > 0 && strcmp(accel, "disabled") != 0) {
                        opt->tooltip = g_strdup_printf("Schema key: %s (Shortcut: %s)", keys[j], accel);
                    } else {
                        opt->tooltip = g_strdup_printf("Schema key: %s (No shortcut configured)", keys[j]);
                    }
                    *list = g_list_append(*list, opt);
                }

                g_free(accel);
                g_variant_unref(val);
                g_settings_schema_key_unref(skey);
            }
            g_strfreev(keys);
        }

        if (strcmp(schemas[i], "org.gnome.settings-daemon.plugins.media-keys") == 0) {
            char **paths = g_settings_get_strv(settings, "custom-keybindings");
            if (paths) {
                for (int k = 0; paths[k]; k++) {
                    GSettings *custom = g_settings_new_with_path("org.gnome.settings-daemon.plugins.media-keys.custom-keybinding", paths[k]);
                    if (!custom) continue;
                    char *c_name = g_settings_get_string(custom, "name");
                    char *c_cmd = g_settings_get_string(custom, "command");
                    char *c_bind = g_settings_get_string(custom, "binding");

                    if (c_name && strlen(c_name) > 0) {
                        EditorActionOption *opt = g_new0(EditorActionOption, 1);
                        opt->category = CAT_GNOME;
                        opt->action_id = ACTION_EXECUTE;
                        opt->name = g_strdup(c_name);
                        opt->custom_cmd = g_strdup(c_cmd);
                        opt->icon = g_strdup("system-run-symbolic");
                        opt->tooltip = g_strdup_printf("Custom GNOME shortcut: %s", c_cmd ? c_cmd : "");
                        *list = g_list_append(*list, opt);
                    }
                    g_free(c_name);
                    g_free(c_cmd);
                    g_free(c_bind);
                    g_object_unref(custom);
                }
                g_strfreev(paths);
            }
        }

        g_settings_schema_unref(schema);
        g_object_unref(settings);
    }
}

static GList *build_editor_action_options(void) {
    GList *list = NULL;

    for (int i = 0; action_types[i].name; i++) {
        ActionType *at = &action_types[i];
        if (at->id == ACTION_GNOME) continue;

        EditorActionOption *opt = g_new0(EditorActionOption, 1);
        opt->action_id = at->id;
        opt->name = g_strdup(at->name);
        opt->icon = g_strdup(at->icon);

        switch (at->id) {
            case ACTION_KEYPRESS:
            case ACTION_CLICK:
                opt->category = CAT_INPUT;
                break;
            case ACTION_KILL:
            case ACTION_TOGGLE_MAXIMIZED:
            case ACTION_MAXIMIZE:
            case ACTION_RESTORE:
            case ACTION_ICONIFY:
            case ACTION_RAISE:
            case ACTION_LOWER:
            case ACTION_TOGGLE_FULLSCREEN:
            case ACTION_SHOW_DESKTOP:
                opt->category = CAT_WINDOW;
                break;
            case ACTION_WORKSPACE_LEFT:
            case ACTION_WORKSPACE_RIGHT:
            case ACTION_WORKSPACE_UP:
            case ACTION_WORKSPACE_DOWN:
            case ACTION_SHOW_OVERVIEW:
            case ACTION_SHOW_APP_GRID:
                opt->category = CAT_WORKSPACE;
                break;
            case ACTION_VOLUME_UP:
            case ACTION_VOLUME_DOWN:
            case ACTION_VOLUME_MUTE:
            case ACTION_MEDIA_PLAY:
            case ACTION_MEDIA_NEXT:
            case ACTION_MEDIA_PREV:
                opt->category = CAT_MEDIA;
                break;
            case ACTION_LOCK_SCREEN:
            case ACTION_TERMINAL:
            case ACTION_CONTROL_CENTER:
            case ACTION_LOGOUT:
            case ACTION_SCREENSHOT:
            case ACTION_SCREENSHOT_WINDOW:
            case ACTION_SCREENSHOT_AREA:
                opt->category = CAT_SYSTEM;
                break;
            case ACTION_WWW:
            case ACTION_HOME:
            case ACTION_EMAIL:
            case ACTION_SEARCH:
            case ACTION_CALCULATOR:
                opt->category = CAT_APP;
                break;
            case ACTION_EXECUTE:
            case ACTION_RECONF:
            case ACTION_EXIT_GEST:
            case ACTION_ABORT:
                opt->category = CAT_OTHER;
                break;
            default:
                opt->category = CAT_OTHER;
                break;
        }
        list = g_list_append(list, opt);
    }

    fetch_gnome_action_options(&list);
    return list;
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

static void on_action_changed(GObject *gobject, GParamSpec *pspec, gpointer user_data) {
    GestureEditor *editor = (GestureEditor *)user_data;
    if (!editor || !editor->action_combo || !editor->action_val_box || !editor->action_val_label || !editor->record_btn) return;

    guint opt_idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(editor->action_combo));
    if (opt_idx == GTK_INVALID_LIST_POSITION) {
        gtk_widget_set_visible(editor->action_val_box, FALSE);
        return;
    }

    if (!editor->current_options) {
        gtk_widget_set_visible(editor->action_val_box, FALSE);
        return;
    }

    EditorActionOption *opt = (EditorActionOption *)g_list_nth_data(editor->current_options, opt_idx);
    if (!opt) {
        gtk_widget_set_visible(editor->action_val_box, FALSE);
        return;
    }

    int id = opt->action_id;
    if (id == ACTION_EXECUTE || id == ACTION_KEYPRESS) {
        gtk_widget_set_visible(editor->action_val_box, TRUE);
        const char *label_text = "Command:";
        if (id == ACTION_KEYPRESS) label_text = "Keys:";
        gtk_label_set_text(GTK_LABEL(editor->action_val_label), label_text);
        gtk_widget_set_visible(editor->record_btn, (id == ACTION_KEYPRESS));

        if (!editor->initializing && opt->custom_cmd && editor->action_val_entry) {
            gtk_editable_set_text(GTK_EDITABLE(editor->action_val_entry), opt->custom_cmd);
        }
    } else {
        gtk_widget_set_visible(editor->action_val_box, FALSE);
    }
}

static void on_category_changed(GObject *gobject, GParamSpec *pspec, gpointer user_data) {
    GestureEditor *editor = (GestureEditor *)user_data;
    if (!editor || !editor->category_combo || !editor->action_combo) return;

    guint cat_idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(editor->category_combo));
    if (cat_idx == GTK_INVALID_LIST_POSITION) return;

    g_signal_handlers_block_by_func(editor->action_combo, G_CALLBACK(on_action_changed), editor);

    if (editor->current_options) {
        g_list_free(editor->current_options);
        editor->current_options = NULL;
    }

    GtkStringList *action_sl = gtk_string_list_new(NULL);

    if (editor->all_options) {
        for (GList *l = editor->all_options; l; l = l->next) {
            EditorActionOption *opt = (EditorActionOption *)l->data;
            if (opt && opt->category == (int)cat_idx) {
                gtk_string_list_append(action_sl, opt->name ? opt->name : "");
                editor->current_options = g_list_append(editor->current_options, opt);
            }
        }
    }

    gtk_drop_down_set_model(GTK_DROP_DOWN(editor->action_combo), G_LIST_MODEL(action_sl));
    g_object_unref(action_sl);

    if (!editor->initializing) {
        gtk_drop_down_set_selected(GTK_DROP_DOWN(editor->action_combo), 0);
    }

    g_signal_handlers_unblock_by_func(editor->action_combo, G_CALLBACK(on_action_changed), editor);
    on_action_changed(G_OBJECT(editor->action_combo), NULL, editor);
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

    guint opt_idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(editor->action_combo));
    if (opt_idx == GTK_INVALID_LIST_POSITION) return;

    EditorActionOption *opt = (EditorActionOption *)g_list_nth_data(editor->current_options, opt_idx);
    if (!opt) return;

    const char *val = gtk_editable_get_text(GTK_EDITABLE(editor->action_val_entry));

    char *full_action = NULL;
    if (opt->action_id == ACTION_EXECUTE) {
        if (asprintf(&full_action, "exec %s", val) == -1) full_action = NULL;
    } else if (opt->action_id == ACTION_KEYPRESS) {
        if (asprintf(&full_action, "keypress %s", val) == -1) full_action = NULL;
    } else if (opt->action_id == ACTION_GNOME) {
        if (asprintf(&full_action, "gnome %s", opt->gnome_key) == -1) full_action = NULL;
    } else {
        const char *prefix = NULL;
        for (int i = 0; action_types[i].name; i++) {
            if (action_types[i].id == opt->action_id) {
                prefix = action_types[i].prefix;
                break;
            }
        }
        if (prefix) {
            full_action = strdup(prefix);
        } else {
            full_action = strdup("abort");
        }
    }

    if (editor->gesture) {
        Movement *final_move = malloc(sizeof(Movement));
        bzero(final_move, sizeof(Movement));
        final_move->name = strdup("custom");
        final_move->points = NULL;
        final_move->point_count = 0;
        movement_set_expression(final_move, strdup(editor->custom_expression ? editor->custom_expression : "0,0 0,0"));

        if (editor->gesture->name) free(editor->gesture->name);
        editor->gesture->name = strdup(name);

        if (editor->gesture->movement) {
            if (editor->gesture->movement->name) free(editor->gesture->movement->name);
            if (editor->gesture->movement->expression) free(editor->gesture->movement->expression);
            if (editor->gesture->movement->points) free(editor->gesture->movement->points);
            free(editor->gesture->movement);
        }

        editor->gesture->movement = final_move;

        for (int i = 0; i < editor->gesture->action_count; i++) {
            if (editor->gesture->action_list[i]->original_str) {
                free(editor->gesture->action_list[i]->original_str);
            }
            free(editor->gesture->action_list[i]);
        }
        editor->gesture->action_count = 0;
        configuration_add_action_from_string(editor->gesture, full_action);
        if (!editor->gesture->is_custom) {
            editor->gesture->is_modified = 1;
            editor->gesture->is_deleted = 0;
        }
        GtkListBoxRow *found_row = NULL;
        int idx = 0;
        while (1) {
            GtkListBoxRow *row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(editor->app->main_list), idx);
            if (!row) break;
            Gesture *row_g = g_object_get_data(G_OBJECT(row), "gesture-ptr");
            if (row_g == editor->gesture) {
                found_row = row;
                break;
            }
            idx++;
        }
        if (found_row) {
            gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(found_row), NULL);
            GtkWidget *new_content = create_gesture_row_content(editor->app, editor->gesture);
            gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(found_row), new_content);
        }
    } else {
        Gesture *new_g = configuration_create_gesture(editor->app->config, (char*)name, editor->custom_expression ? editor->custom_expression : "0,0 0,0");
        new_g->is_custom = 1;
        configuration_add_action_from_string(new_g, full_action);

        const char *search_text = gtk_editable_get_text(GTK_EDITABLE(editor->app->search_entry));
        if (search_text && strlen(search_text) > 0) {
            refresh_gesture_list(editor->app);
        } else {
            add_gesture_row(editor->app, new_g);
        }
    }
    free(full_action);

    char *filename = configuration_get_default_filename();
    if (filename) {
        configuration_save_to_file(editor->app->config, filename);
        free(filename);
    }
    reload_daemon_if_running();

    gtk_window_close(GTK_WINDOW(editor->dialog));
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
    if (!box) return;
    GtkWidget *icon = gtk_widget_get_first_child(box);
    if (!icon) return;
    GtkWidget *label = gtk_widget_get_next_sibling(icon);
    if (!label) return;

    gpointer item = gtk_list_item_get_item(list_item);
    if (!item || !GTK_IS_STRING_OBJECT(item)) return;

    GtkStringObject *string_obj = GTK_STRING_OBJECT(item);
    const char *text = gtk_string_object_get_string(string_obj);
    if (!text) return;

    gtk_label_set_text(GTK_LABEL(label), text);

    const char *icon_name = "system-run-symbolic";
    GestureEditor *editor = (GestureEditor *)user_data;
    if (editor && editor->current_options) {
        for (GList *l = editor->current_options; l; l = l->next) {
            EditorActionOption *opt = (EditorActionOption *)l->data;
            if (opt && opt->name && strcmp(opt->name, text) == 0) {
                icon_name = opt->icon ? opt->icon : "system-run-symbolic";
                break;
            }
        }
    }
    gtk_image_set_from_icon_name(GTK_IMAGE(icon), icon_name);
}

static void gesture_editor_cleanup(GestureEditor *editor) {
    if (!editor || !editor->dialog) return;

    if (editor->category_combo) {
        g_signal_handlers_disconnect_by_data(editor->category_combo, editor);
        gtk_drop_down_set_model(GTK_DROP_DOWN(editor->category_combo), NULL);
        editor->category_combo = NULL;
    }

    if (editor->action_combo) {
        GtkListItemFactory *factory = gtk_drop_down_get_factory(GTK_DROP_DOWN(editor->action_combo));
        if (factory) {
            g_signal_handlers_disconnect_by_data(factory, editor);
        }
        GtkListItemFactory *list_factory = gtk_drop_down_get_list_factory(GTK_DROP_DOWN(editor->action_combo));
        if (list_factory) {
            g_signal_handlers_disconnect_by_data(list_factory, editor);
        }
        g_signal_handlers_disconnect_by_data(editor->action_combo, editor);
        gtk_drop_down_set_factory(GTK_DROP_DOWN(editor->action_combo), NULL);
        gtk_drop_down_set_list_factory(GTK_DROP_DOWN(editor->action_combo), NULL);
        gtk_drop_down_set_model(GTK_DROP_DOWN(editor->action_combo), NULL);
        editor->action_combo = NULL;
    }

    if (editor->record_btn) {
        g_signal_handlers_disconnect_by_data(editor->record_btn, editor);
        editor->record_btn = NULL;
    }

    if (editor->canvas) {
        gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(editor->canvas), NULL, NULL, NULL);
        editor->canvas = NULL;
    }

    if (editor->dialog) {
        g_signal_handlers_disconnect_by_data(editor->dialog, editor);
        editor->dialog = NULL;
    }
}

static void free_editor_data(gpointer data) {
    GestureEditor *editor = (GestureEditor *)data;
    if (!editor) return;

    if (editor->browser_dialog) {
        gtk_window_destroy(GTK_WINDOW(editor->browser_dialog));

    }
    if (editor->drawn_points) {
        free(editor->drawn_points);
    }
    if (editor->custom_expression) {
        free(editor->custom_expression);
    }
    if (editor->all_options) {
        for (GList *l = editor->all_options; l; l = l->next) {
            EditorActionOption *opt = (EditorActionOption *)l->data;
            g_free(opt->name);
            g_free(opt->gnome_key);
            g_free(opt->icon);
            g_free(opt->tooltip);
            g_free(opt->custom_cmd);
            g_free(opt);
        }
        g_list_free(editor->all_options);
    }
    if (editor->current_options) {
        g_list_free(editor->current_options);
    }
    g_free(editor);
}


static void on_canvas_draw(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data) {
    GestureEditor *editor = (GestureEditor *)user_data;

    // 1. Radial background vignette
    cairo_pattern_t *bg_pat = cairo_pattern_create_radial(width / 2.0, height / 2.0, width / 10.0,
                                                          width / 2.0, height / 2.0, width * 0.8);
    cairo_pattern_add_color_stop_rgb(bg_pat, 0.0, 0.98, 0.99, 1.0);
    cairo_pattern_add_color_stop_rgb(bg_pat, 1.0, 0.92, 0.93, 0.95);
    cairo_set_source(cr, bg_pat);
    cairo_paint(cr);
    cairo_pattern_destroy(bg_pat);

    // 2. Subtle dashed grid
    cairo_set_source_rgba(cr, 0.82, 0.85, 0.88, 0.35);
    cairo_set_line_width(cr, 1.0);
    double dash[] = {4.0, 4.0};
    cairo_set_dash(cr, dash, 2, 0);
    for (int i = 25; i < width; i += 25) {
        cairo_move_to(cr, i, 0);
        cairo_line_to(cr, i, height);
    }
    for (int j = 25; j < height; j += 25) {
        cairo_move_to(cr, 0, j);
        cairo_line_to(cr, width, j);
    }
    cairo_stroke(cr);
    cairo_set_dash(cr, NULL, 0, 0); // Clear dash

    if (editor->is_drawing && editor->drawn_count > 1) {
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

        int M = 4; // Sub-segment division for active drawing
        double start_r = 0.98, start_g = 0.72, start_b = 0.80; // Faded sunset pink
        double end_r = 0.49, end_g = 0.27, end_b = 0.90;       // Sunset violet

        // Glow layer
        for (int i = 1; i < editor->drawn_count; i++) {
            double f_start = (double)(i - 1) / (editor->drawn_count - 1);
            double f_end = (double)i / (editor->drawn_count - 1);

            for (int s = 0; s < M; s++) {
                double t1 = (double)s / M;
                double t2 = (double)(s + 1) / M;
                double f_global = f_start + (f_end - f_start) * t1;

                double w = 6.0 + 6.0 * f_global;
                double r = start_r * (1.0 - f_global) + end_r * f_global;
                double g = start_g * (1.0 - f_global) + end_g * f_global;
                double b = start_b * (1.0 - f_global) + end_b * f_global;

                double x1 = editor->drawn_points[i-1].x + (editor->drawn_points[i].x - editor->drawn_points[i-1].x) * t1;
                double y1 = editor->drawn_points[i-1].y + (editor->drawn_points[i].y - editor->drawn_points[i-1].y) * t1;
                double x2 = editor->drawn_points[i-1].x + (editor->drawn_points[i].x - editor->drawn_points[i-1].x) * t2;
                double y2 = editor->drawn_points[i-1].y + (editor->drawn_points[i].y - editor->drawn_points[i-1].y) * t2;

                cairo_set_source_rgba(cr, r, g, b, 0.2);
                cairo_set_line_width(cr, w);
                cairo_move_to(cr, x1, y1);
                cairo_line_to(cr, x2, y2);
                cairo_stroke(cr);
            }
        }

        // Solid core layer
        for (int i = 1; i < editor->drawn_count; i++) {
            double f_start = (double)(i - 1) / (editor->drawn_count - 1);
            double f_end = (double)i / (editor->drawn_count - 1);

            for (int s = 0; s < M; s++) {
                double t1 = (double)s / M;
                double t2 = (double)(s + 1) / M;
                double f_global = f_start + (f_end - f_start) * t1;

                double w = 1.5 + 2.5 * f_global;
                double r = start_r * (1.0 - f_global) + end_r * f_global;
                double g = start_g * (1.0 - f_global) + end_g * f_global;
                double b = start_b * (1.0 - f_global) + end_b * f_global;

                double x1 = editor->drawn_points[i-1].x + (editor->drawn_points[i].x - editor->drawn_points[i-1].x) * t1;
                double y1 = editor->drawn_points[i-1].y + (editor->drawn_points[i].y - editor->drawn_points[i-1].y) * t1;
                double x2 = editor->drawn_points[i-1].x + (editor->drawn_points[i].x - editor->drawn_points[i-1].x) * t2;
                double y2 = editor->drawn_points[i-1].y + (editor->drawn_points[i].y - editor->drawn_points[i-1].y) * t2;

                cairo_set_source_rgb(cr, r, g, b);
                cairo_set_line_width(cr, w);
                cairo_move_to(cr, x1, y1);
                cairo_line_to(cr, x2, y2);
                cairo_stroke(cr);
            }
        }
    } else if (editor->custom_expression) {
        Point2D *pts = NULL;
        int pt_count = 0;
        gboolean free_pts = FALSE;

        int count = 0;
        char *expr_copy = strdup(editor->custom_expression);
        char *token = strtok(expr_copy, " ");
        while (token) {
            count++;
            token = strtok(NULL, " ");
        }
        free(expr_copy);

        if (count > 0) {
            pts = malloc(sizeof(Point2D) * count);
            expr_copy = strdup(editor->custom_expression);
            token = strtok(expr_copy, " ");
            int idx = 0;
            while (token) {
                double x = 0, y = 0;
                if (sscanf(token, "%lf,%lf", &x, &y) == 2) {
                    pts[idx].x = x;
                    pts[idx].y = y;
                    idx++;
                }
                token = strtok(NULL, " ");
            }
            free(expr_copy);
            pt_count = idx;
            free_pts = TRUE;
        }

        if (pts && pt_count > 0) {
            // Find bounding box to center the path
            double min_x = pts[0].x, max_x = pts[0].x;
            double min_y = pts[0].y, max_y = pts[0].y;
            for (int i = 1; i < pt_count; i++) {
                if (pts[i].x < min_x) min_x = pts[i].x;
                if (pts[i].x > max_x) max_x = pts[i].x;
                if (pts[i].y < min_y) min_y = pts[i].y;
                if (pts[i].y > max_y) max_y = pts[i].y;
            }
            double path_center_x = (min_x + max_x) / 2.0;
            double path_center_y = (min_y + max_y) / 2.0;

            double offset_x = (width / 2.0) - path_center_x;
            double offset_y = (height / 2.0) - path_center_y;

            cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

            // Color Gradient Definition: Faded Sunset Pink to Sunset Violet
            double start_r = 0.98, start_g = 0.72, start_b = 0.80; // Faded sunset pink
            double end_r = 0.49, end_g = 0.27, end_b = 0.90;       // Sunset violet

            if (pt_count == 1) {
                cairo_set_source_rgb(cr, start_r, start_g, start_b);
                cairo_arc(cr, pts[0].x + offset_x, pts[0].y + offset_y, 8.0, 0, 2 * G_PI);
                cairo_fill(cr);
            } else {
                int M = 16;

                // Draw outer glowing shadow
                for (int i = 1; i < pt_count; i++) {
                    double f_start = (double)(i - 1) / (pt_count - 1);
                    double f_end = (double)i / (pt_count - 1);

                    for (int s = 0; s < M; s++) {
                        double t1 = (double)s / M;
                        double t2 = (double)(s + 1) / M;
                        double f_global = f_start + (f_end - f_start) * t1;

                        double w = 18.0 * f_global + 4.0;
                        double r = start_r * (1.0 - f_global) + end_r * f_global;
                        double g = start_g * (1.0 - f_global) + end_g * f_global;
                        double b = start_b * (1.0 - f_global) + end_b * f_global;

                        double x1 = pts[i-1].x + (pts[i].x - pts[i-1].x) * t1;
                        double y1 = pts[i-1].y + (pts[i].y - pts[i-1].y) * t1;
                        double x2 = pts[i-1].x + (pts[i].x - pts[i-1].x) * t2;
                        double y2 = pts[i-1].y + (pts[i].y - pts[i-1].y) * t2;

                        cairo_set_source_rgba(cr, r, g, b, 0.15);
                        cairo_set_line_width(cr, w);
                        cairo_move_to(cr, x1 + offset_x, y1 + offset_y);
                        cairo_line_to(cr, x2 + offset_x, y2 + offset_y);
                        cairo_stroke(cr);
                    }
                }

                // Draw solid core
                for (int i = 1; i < pt_count; i++) {
                    double f_start = (double)(i - 1) / (pt_count - 1);
                    double f_end = (double)i / (pt_count - 1);

                    for (int s = 0; s < M; s++) {
                        double t1 = (double)s / M;
                        double t2 = (double)(s + 1) / M;
                        double f_global = f_start + (f_end - f_start) * t1;

                        double w = 10.0 * f_global + 2.0;
                        double r = start_r * (1.0 - f_global) + end_r * f_global;
                        double g = start_g * (1.0 - f_global) + end_g * f_global;
                        double b = start_b * (1.0 - f_global) + end_b * f_global;

                        double x1 = pts[i-1].x + (pts[i].x - pts[i-1].x) * t1;
                        double y1 = pts[i-1].y + (pts[i].y - pts[i-1].y) * t1;
                        double x2 = pts[i-1].x + (pts[i].x - pts[i-1].x) * t2;
                        double y2 = pts[i-1].y + (pts[i].y - pts[i-1].y) * t2;

                        cairo_set_source_rgb(cr, r, g, b);
                        cairo_set_line_width(cr, w);
                        cairo_move_to(cr, x1 + offset_x, y1 + offset_y);
                        cairo_line_to(cr, x2 + offset_x, y2 + offset_y);
                        cairo_stroke(cr);
                    }
                }
            }

            // Draw glowing dots at key vertices
            for (int i = 0; i < pt_count; i++) {
                double fraction = (double)i / (pt_count > 1 ? (pt_count - 1) : 1);
                double dot_radius = 5.0 * fraction + 2.0;

                double r = start_r * (1.0 - fraction) + end_r * fraction;
                double g = start_g * (1.0 - fraction) + end_g * fraction;
                double b = start_b * (1.0 - fraction) + end_b * fraction;

                cairo_set_source_rgba(cr, r, g, b, 0.4);
                cairo_arc(cr, pts[i].x + offset_x, pts[i].y + offset_y, dot_radius + 4.0, 0, 2 * G_PI);
                cairo_fill(cr);

                cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
                cairo_arc(cr, pts[i].x + offset_x, pts[i].y + offset_y, fmax(2.0, dot_radius - 1.5), 0, 2 * G_PI);
                cairo_fill(cr);
            }
        }

        if (free_pts && pts) {
            free(pts);
        }
    }
}

static void on_preview_draw(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data) {
    Movement *m = (Movement *)user_data;

    // 1. Radial background vignette
    cairo_pattern_t *bg_pat = cairo_pattern_create_radial(width / 2.0, height / 2.0, width / 10.0,
                                                          width / 2.0, height / 2.0, width * 0.8);
    cairo_pattern_add_color_stop_rgb(bg_pat, 0.0, 0.98, 0.99, 1.0);
    cairo_pattern_add_color_stop_rgb(bg_pat, 1.0, 0.92, 0.93, 0.95);
    cairo_set_source(cr, bg_pat);
    cairo_paint(cr);
    cairo_pattern_destroy(bg_pat);

    if (!m || m->point_count == 0 || !m->points) {
        return;
    }

    // 2. Subtle dashed crosshair grid
    cairo_set_source_rgba(cr, 0.82, 0.85, 0.88, 0.3);
    cairo_set_line_width(cr, 0.7);
    double dash[] = {2.0, 2.0};
    cairo_set_dash(cr, dash, 2, 0);
    cairo_move_to(cr, width / 2.0, 0);
    cairo_line_to(cr, width / 2.0, height);
    cairo_move_to(cr, 0, height / 2.0);
    cairo_line_to(cr, width, height / 2.0);
    cairo_stroke(cr);
    cairo_set_dash(cr, NULL, 0, 0);

    int pt_count = m->point_count;
    Point2D *pts = m->points;

    // Find bounding box to scale and center the path
    double min_x = pts[0].x, max_x = pts[0].x;
    double min_y = pts[0].y, max_y = pts[0].y;
    for (int i = 1; i < pt_count; i++) {
        if (pts[i].x < min_x) min_x = pts[i].x;
        if (pts[i].x > max_x) max_x = pts[i].x;
        if (pts[i].y < min_y) min_y = pts[i].y;
        if (pts[i].y > max_y) max_y = pts[i].y;
    }

    double path_w = max_x - min_x;
    double path_h = max_y - min_y;
    double path_center_x = (min_x + max_x) / 2.0;
    double path_center_y = (min_y + max_y) / 2.0;

    double margin = 6.0;
    double target_w = width - 2.0 * margin;
    double target_h = height - 2.0 * margin;

    double scale = 1.0;
    if (path_w > 0 || path_h > 0) {
        double scale_x = (path_w > 0) ? (target_w / path_w) : 1e9;
        double scale_y = (path_h > 0) ? (target_h / path_h) : 1e9;
        scale = fmin(scale_x, scale_y);
        if (scale > 1.5) scale = 1.5;
    }

    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

    double start_r = 0.98, start_g = 0.72, start_b = 0.80; // Faded sunset pink
    double end_r = 0.49, end_g = 0.27, end_b = 0.90;       // Sunset violet

    if (pt_count == 1) {
        cairo_set_source_rgb(cr, start_r, start_g, start_b);
        cairo_arc(cr, width / 2.0, height / 2.0, 4.0, 0, 2 * G_PI);
        cairo_fill(cr);
    } else {
        int M = 8;

        // Draw shadow/glow
        for (int i = 1; i < pt_count; i++) {
            double f_start = (double)(i - 1) / (pt_count - 1);
            double f_end = (double)i / (pt_count - 1);

            for (int s = 0; s < M; s++) {
                double t1 = (double)s / M;
                double t2 = (double)(s + 1) / M;
                double f_global = f_start + (f_end - f_start) * t1;

                double w = 8.0 * f_global + 2.0;
                double r = start_r * (1.0 - f_global) + end_r * f_global;
                double g = start_g * (1.0 - f_global) + end_g * f_global;
                double b = start_b * (1.0 - f_global) + end_b * f_global;

                double px1 = width / 2.0 + (pts[i-1].x + (pts[i].x - pts[i-1].x) * t1 - path_center_x) * scale;
                double py1 = height / 2.0 + (pts[i-1].y + (pts[i].y - pts[i-1].y) * t1 - path_center_y) * scale;
                double px2 = width / 2.0 + (pts[i-1].x + (pts[i].x - pts[i-1].x) * t2 - path_center_x) * scale;
                double py2 = height / 2.0 + (pts[i-1].y + (pts[i].y - pts[i-1].y) * t2 - path_center_y) * scale;

                cairo_set_source_rgba(cr, r, g, b, 0.15);
                cairo_set_line_width(cr, w);
                cairo_move_to(cr, px1, py1);
                cairo_line_to(cr, px2, py2);
                cairo_stroke(cr);
            }
        }

        // Draw solid core
        for (int i = 1; i < pt_count; i++) {
            double f_start = (double)(i - 1) / (pt_count - 1);
            double f_end = (double)i / (pt_count - 1);

            for (int s = 0; s < M; s++) {
                double t1 = (double)s / M;
                double t2 = (double)(s + 1) / M;
                double f_global = f_start + (f_end - f_start) * t1;

                double w = 4.0 * f_global + 1.0;
                double r = start_r * (1.0 - f_global) + end_r * f_global;
                double g = start_g * (1.0 - f_global) + end_g * f_global;
                double b = start_b * (1.0 - f_global) + end_b * f_global;

                double px1 = width / 2.0 + (pts[i-1].x + (pts[i].x - pts[i-1].x) * t1 - path_center_x) * scale;
                double py1 = height / 2.0 + (pts[i-1].y + (pts[i].y - pts[i-1].y) * t1 - path_center_y) * scale;
                double px2 = width / 2.0 + (pts[i-1].x + (pts[i].x - pts[i-1].x) * t2 - path_center_x) * scale;
                double py2 = height / 2.0 + (pts[i-1].y + (pts[i].y - pts[i-1].y) * t2 - path_center_y) * scale;

                cairo_set_source_rgb(cr, r, g, b);
                cairo_set_line_width(cr, w);
                cairo_move_to(cr, px1, py1);
                cairo_line_to(cr, px2, py2);
                cairo_stroke(cr);
            }
        }
    }

    // Draw glowing dots at key vertices
    for (int i = 0; i < pt_count; i++) {
        double fraction = (double)i / (pt_count > 1 ? (pt_count - 1) : 1);
        double dot_radius = 2.5 * fraction + 1.0;

        double px = width / 2.0 + (pts[i].x - path_center_x) * scale;
        double py = height / 2.0 + (pts[i].y - path_center_y) * scale;

        double r = start_r * (1.0 - fraction) + end_r * fraction;
        double g = start_g * (1.0 - fraction) + end_g * fraction;
        double b = start_b * (1.0 - fraction) + end_b * fraction;

        cairo_set_source_rgba(cr, r, g, b, 0.4);
        cairo_arc(cr, px, py, dot_radius + 2.0, 0, 2 * G_PI);
        cairo_fill(cr);

        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_arc(cr, px, py, fmax(1.0, dot_radius - 0.5), 0, 2 * G_PI);
        cairo_fill(cr);
    }
}

static void on_canvas_drag_begin(GtkGestureDrag *gesture, double start_x, double start_y, gpointer user_data) {
    GestureEditor *editor = (GestureEditor *)user_data;
    editor->is_drawing = TRUE;
    editor->drawn_count = 0;

    if (editor->drawn_capacity < 128) {
        editor->drawn_capacity = 1024;
        editor->drawn_points = malloc(sizeof(Point2D) * editor->drawn_capacity);
    }

    editor->drawn_points[0].x = start_x;
    editor->drawn_points[0].y = start_y;
    editor->drawn_count = 1;

    gtk_widget_queue_draw(editor->canvas);
}

static void on_canvas_drag_update(GtkGestureDrag *gesture, double offset_x, double offset_y, gpointer user_data) {
    GestureEditor *editor = (GestureEditor *)user_data;
    if (!editor->is_drawing) return;

    double start_x, start_y;
    if (gtk_gesture_drag_get_start_point(gesture, &start_x, &start_y)) {
        double cur_x = start_x + offset_x;
        double cur_y = start_y + offset_y;

        if (editor->drawn_count > 0) {
            double dx = cur_x - editor->drawn_points[editor->drawn_count - 1].x;
            double dy = cur_y - editor->drawn_points[editor->drawn_count - 1].y;
            if (dx*dx + dy*dy < 4.0) {
                return;
            }
        }

        if (editor->drawn_count >= editor->drawn_capacity) {
            editor->drawn_capacity *= 2;
            editor->drawn_points = realloc(editor->drawn_points, sizeof(Point2D) * editor->drawn_capacity);
        }

        editor->drawn_points[editor->drawn_count].x = cur_x;
        editor->drawn_points[editor->drawn_count].y = cur_y;
        editor->drawn_count++;

        gtk_widget_queue_draw(editor->canvas);
    }
}

static void on_canvas_drag_end(GtkGestureDrag *gesture, double offset_x, double offset_y, gpointer user_data) {
    GestureEditor *editor = (GestureEditor *)user_data;
    if (!editor->is_drawing) return;
    editor->is_drawing = FALSE;

    double start_x, start_y;
    if (gtk_gesture_drag_get_start_point(gesture, &start_x, &start_y)) {
        double cur_x = start_x + offset_x;
        double cur_y = start_y + offset_y;
        if (editor->drawn_count > 0) {
            double dx = cur_x - editor->drawn_points[editor->drawn_count - 1].x;
            double dy = cur_y - editor->drawn_points[editor->drawn_count - 1].y;
            if (dx*dx + dy*dy >= 4.0) {
                if (editor->drawn_count >= editor->drawn_capacity) {
                    editor->drawn_capacity *= 2;
                    editor->drawn_points = realloc(editor->drawn_points, sizeof(Point2D) * editor->drawn_capacity);
                }
                editor->drawn_points[editor->drawn_count].x = cur_x;
                editor->drawn_points[editor->drawn_count].y = cur_y;
                editor->drawn_count++;
            }
        }
    }

    if (editor->drawn_count >= 2) {
        int simplified_count = 0;
        Point2D *simplified = grabbing_simplify_points(editor->drawn_points, editor->drawn_count, 6.0, &simplified_count);

        int buf_size = simplified_count * 40 + 1;
        char *buf = malloc(buf_size);
        buf[0] = '\0';
        for (int i = 0; i < simplified_count; i++) {
            char pt_buf[40];
            snprintf(pt_buf, sizeof(pt_buf), "%.1f,%.1f", simplified[i].x, simplified[i].y);
            strcat(buf, pt_buf);
            if (i < simplified_count - 1) {
                strcat(buf, " ");
            }
        }

        if (editor->custom_expression) {
            free(editor->custom_expression);
        }
        editor->custom_expression = buf;

        free(simplified);
    }

    gtk_widget_queue_draw(editor->canvas);
}

static gboolean on_dialog_close_request(GtkWindow *window, gpointer user_data) {
    GestureEditor *editor = (GestureEditor *)user_data;
    gesture_editor_cleanup(editor);
    return FALSE;
}

static void open_gesture_editor(GestosApp *gestos, Gesture *g) {
    GestureEditor *editor = g_new0(GestureEditor, 1);
    editor->app = gestos;
    editor->gesture = g;

    editor->dialog = gtk_window_new();
    g_object_set_data_full(G_OBJECT(editor->dialog), "editor-data", editor, free_editor_data);
    g_signal_connect(editor->dialog, "close-request", G_CALLBACK(on_dialog_close_request), editor);

    gtk_window_set_title(GTK_WINDOW(editor->dialog), g ? "Edit Gesture" : "New Gesture");
    gtk_window_set_transient_for(GTK_WINDOW(editor->dialog), GTK_WINDOW(gestos->window));
    gtk_window_set_modal(GTK_WINDOW(editor->dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(editor->dialog), 450, -1);

    GtkWidget *header = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(editor->dialog), header);

    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), cancel_btn);
    g_signal_connect_swapped(cancel_btn, "clicked", G_CALLBACK(gtk_window_close), editor->dialog);

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
    if (g && g->movement) {
        editor->custom_expression = strdup(g->movement->expression);
    }

    editor->initializing = TRUE;
    editor->all_options = build_editor_action_options();

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Category:"), 0, 1, 1, 1);
    GtkStringList *cat_sl = gtk_string_list_new(NULL);
    for (int i = 0; i < CAT_COUNT; i++) {
        gtk_string_list_append(cat_sl, category_names[i]);
    }
    editor->category_combo = gtk_drop_down_new(G_LIST_MODEL(cat_sl), NULL);
    g_object_unref(cat_sl);
    gtk_grid_attach(GTK_GRID(grid), editor->category_combo, 1, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Action:"), 0, 2, 1, 1);
    editor->action_combo = gtk_drop_down_new(NULL, NULL);
    gtk_grid_attach(GTK_GRID(grid), editor->action_combo, 1, 2, 1, 1);

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

    int active_cat_idx = CAT_OTHER;
    if (g && g->action_count > 0) {
        Action *a = g->action_list[0];
        int found = 0;
        for (GList *l = editor->all_options; l; l = l->next) {
            EditorActionOption *opt = (EditorActionOption *)l->data;
            if (a->type == ACTION_GNOME && opt->action_id == ACTION_GNOME && opt->gnome_key && a->original_str && strcmp(opt->gnome_key, a->original_str) == 0) {
                active_cat_idx = opt->category;
                found = 1;
                break;
            }
            else if (a->type == ACTION_EXECUTE && opt->category == CAT_GNOME && opt->custom_cmd && a->original_str && strcmp(opt->custom_cmd, a->original_str) == 0) {
                active_cat_idx = opt->category;
                found = 1;
                break;
            }
            else if (a->type == opt->action_id && a->type != ACTION_GNOME && opt->category != CAT_GNOME) {
                active_cat_idx = opt->category;
                found = 1;
                break;
            }
        }
        if (!found) {
            for (GList *l = editor->all_options; l; l = l->next) {
                EditorActionOption *opt = (EditorActionOption *)l->data;
                if (a->type == opt->action_id && opt->category != CAT_GNOME) {
                    active_cat_idx = opt->category;
                    break;
                }
            }
        }
    }

    g_signal_connect(editor->category_combo, "notify::selected", G_CALLBACK(on_category_changed), editor);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(editor->category_combo), active_cat_idx);

    // Manually trigger category change to build action list
    on_category_changed(G_OBJECT(editor->category_combo), NULL, editor);

    int select_opt_idx = 0;
    if (g && g->action_count > 0) {
        Action *a = g->action_list[0];
        int idx = 0;
        for (GList *l = editor->current_options; l; l = l->next) {
            EditorActionOption *opt = (EditorActionOption *)l->data;
            if (a->type == ACTION_GNOME && opt->action_id == ACTION_GNOME && opt->gnome_key && a->original_str && strcmp(opt->gnome_key, a->original_str) == 0) {
                select_opt_idx = idx;
                break;
            }
            else if (a->type == ACTION_EXECUTE && opt->custom_cmd && a->original_str && strcmp(opt->custom_cmd, a->original_str) == 0) {
                select_opt_idx = idx;
                break;
            }
            else if (a->type == opt->action_id && a->type != ACTION_GNOME && opt->category != CAT_GNOME) {
                select_opt_idx = idx;
                break;
            }
            idx++;
        }
    }
    gtk_drop_down_set_selected(GTK_DROP_DOWN(editor->action_combo), select_opt_idx);
    editor->initializing = FALSE;

    if (g && g->action_count > 0) {
        if (g->action_list[0]->original_str)
            gtk_editable_set_text(GTK_EDITABLE(editor->action_val_entry), g->action_list[0]->original_str);
    }

    GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(on_dropdown_setup), NULL);
    g_signal_connect(factory, "bind", G_CALLBACK(on_dropdown_bind), editor);
    gtk_drop_down_set_factory(GTK_DROP_DOWN(editor->action_combo), factory);
    g_object_unref(factory);

    GtkListItemFactory *list_factory = gtk_signal_list_item_factory_new();
    g_signal_connect(list_factory, "setup", G_CALLBACK(on_dropdown_setup), NULL);
    g_signal_connect(list_factory, "bind", G_CALLBACK(on_dropdown_bind), editor);
    gtk_drop_down_set_list_factory(GTK_DROP_DOWN(editor->action_combo), list_factory);
    g_object_unref(list_factory);

    g_signal_connect(editor->action_combo, "notify::selected", G_CALLBACK(on_action_changed), editor);
    on_action_changed(G_OBJECT(editor->action_combo), NULL, editor);

    GtkEventController *key_controller = gtk_event_controller_key_new();
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(on_record_key_pressed), editor);
    gtk_widget_add_controller(editor->dialog, key_controller);

    GtkWidget *canvas_label = gtk_label_new("Draw Movement (drag to customize):");
    gtk_widget_set_halign(canvas_label, GTK_ALIGN_START);
    gtk_widget_set_margin_top(canvas_label, 16);
    gtk_widget_set_margin_bottom(canvas_label, 6);
    gtk_box_append(GTK_BOX(content), canvas_label);

    GtkWidget *canvas_frame = gtk_frame_new(NULL);
    gtk_widget_set_size_request(canvas_frame, 220, 220);
    gtk_widget_set_halign(canvas_frame, GTK_ALIGN_CENTER);

    editor->canvas = gtk_drawing_area_new();
    gtk_widget_set_size_request(editor->canvas, 200, 200);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(editor->canvas), on_canvas_draw, editor, NULL);
    gtk_frame_set_child(GTK_FRAME(canvas_frame), editor->canvas);
    gtk_box_append(GTK_BOX(content), canvas_frame);

    GtkGesture *drag = gtk_gesture_drag_new();
    gtk_widget_add_controller(editor->canvas, GTK_EVENT_CONTROLLER(drag));
    g_signal_connect(drag, "drag-begin", G_CALLBACK(on_canvas_drag_begin), editor);
    g_signal_connect(drag, "drag-update", G_CALLBACK(on_canvas_drag_update), editor);
    g_signal_connect(drag, "drag-end", G_CALLBACK(on_canvas_drag_end), editor);

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
        Gesture *del_g = conf->gesture_list[found];
        if (!del_g->is_custom) {
            del_g->is_deleted = 1;
            for (int j = 0; j < del_g->action_count; j++) {
                if (del_g->action_list[j]->original_str) free(del_g->action_list[j]->original_str);
                free(del_g->action_list[j]);
            }
            del_g->action_count = 0;
            configuration_create_action(del_g, ACTION_ABORT, strdup(""));
        } else {
            if (del_g->name) free(del_g->name);
            for (int j = 0; j < del_g->action_count; j++) {
                if (del_g->action_list[j]->original_str) free(del_g->action_list[j]->original_str);
                free(del_g->action_list[j]);
            }
            free(del_g->action_list);
            if (del_g->movement) {
                if (del_g->movement->name) free(del_g->movement->name);
                if (del_g->movement->expression) free(del_g->movement->expression);
                if (del_g->movement->points) free(del_g->movement->points);
                free(del_g->movement);
            }
            free(del_g);

            for (int i = found; i < conf->gesture_count - 1; i++) {
                conf->gesture_list[i] = conf->gesture_list[i+1];
            }
            conf->gesture_count--;
        }
    }
    GtkWidget *row = gtk_widget_get_ancestor(widget, GTK_TYPE_LIST_BOX_ROW);
    if (row) {
        int idx = gtk_list_box_row_get_index(GTK_LIST_BOX_ROW(row));
        GtkListBoxRow *sibling = NULL;
        if (idx > 0) {
            sibling = gtk_list_box_get_row_at_index(GTK_LIST_BOX(gestos->main_list), idx - 1);
        } else {
            sibling = gtk_list_box_get_row_at_index(GTK_LIST_BOX(gestos->main_list), idx + 1);
        }
        if (sibling) {
            gtk_widget_grab_focus(GTK_WIDGET(sibling));
        } else {
            gtk_widget_grab_focus(GTK_WIDGET(gestos->main_list));
        }
        gtk_list_box_remove(GTK_LIST_BOX(gestos->main_list), row);
    }

    char *filename = configuration_get_default_filename();
    if (filename) {
        configuration_save_to_file(gestos->config, filename);
        free(filename);
    }
    reload_daemon_if_running();
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

static GtkWidget *create_gesture_row_content(GestosApp *gestos, Gesture *gesture) {
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

    GtkWidget *preview_frame = gtk_frame_new(NULL);
    gtk_widget_set_size_request(preview_frame, 46, 46);
    gtk_widget_set_valign(preview_frame, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(preview_frame, "gesture-preview-frame");

    GtkWidget *preview_canvas = gtk_drawing_area_new();
    gtk_widget_set_size_request(preview_canvas, 44, 44);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(preview_canvas), on_preview_draw, gesture->movement, NULL);
    gtk_frame_set_child(GTK_FRAME(preview_frame), preview_canvas);

    gtk_widget_set_tooltip_text(preview_frame, gesture->name);

    gtk_box_append(GTK_BOX(main_hbox), preview_frame);

    GtkWidget *del_btn = gtk_button_new_from_icon_name("user-trash-symbolic");
    gtk_widget_add_css_class(del_btn, "flat");
    gtk_widget_add_css_class(del_btn, "destructive-action");
    gtk_widget_set_valign(del_btn, GTK_ALIGN_CENTER);
    g_signal_connect(del_btn, "clicked", G_CALLBACK(on_gesture_delete_clicked), gesture);
    gtk_box_append(GTK_BOX(main_hbox), del_btn);

    return main_hbox;
}

static void add_gesture_row(GestosApp *gestos, Gesture *gesture) {
    GtkWidget *row = gtk_list_box_row_new();
    gtk_widget_add_css_class(row, "gesture-row");
    g_object_set_data(G_OBJECT(row), "gesture-ptr", gesture);

    GtkWidget *main_hbox = create_gesture_row_content(gestos, gesture);
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
        if (g->is_deleted) continue;
        if (search_text && strlen(search_text) > 0) {
            if (!g_strrstr(g->name, search_text)) continue;
        }
        add_gesture_row(gestos, g);
    }
}

static void on_search_changed(GtkSearchEntry *entry, gpointer user_data) {
    refresh_gesture_list((GestosApp *)user_data);
}
static void on_about_clicked(GtkWidget *widget, gpointer user_data) {
    GtkWindow *parent = GTK_WINDOW(user_data);
    gtk_show_about_dialog(parent, "program-name", "Gestos", "version", "1.3.0", "logo-icon-name", "input-mouse", NULL);
}

static gboolean is_daemon_running() {
    char buf[64];
    uid_t uid = getuid();
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "pgrep -u %d -x mygestures", uid);
    FILE *f = popen(cmd, "r");
    if (!f) return FALSE;
    gboolean running = FALSE;
    if (fgets(buf, sizeof(buf), f) != NULL) {
        running = TRUE;
    }
    pclose(f);
    return running;
}

static void start_daemon() {
    if (is_daemon_running()) return;
    const char *cmd = (access("./mygestures", X_OK) == 0) ? "./mygestures &" : "mygestures &";
    int ret = system(cmd);
    (void)ret;
}

static void stop_daemon() {
    uid_t uid = getuid();
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "pkill -u %d -x mygestures", uid);
    int ret = system(cmd);
    (void)ret;
}

static gboolean check_daemon_status_timer(gpointer user_data) {
    GestosApp *gestos = (GestosApp *)user_data;
    gboolean running = is_daemon_running();

    g_signal_handler_block(gestos->daemon_switch, gestos->switch_handler_id);
    gtk_switch_set_active(GTK_SWITCH(gestos->daemon_switch), running);
    g_signal_handler_unblock(gestos->daemon_switch, gestos->switch_handler_id);

    if (running) {
        gtk_label_set_text(GTK_LABEL(gestos->status_label), "Daemon Active");
        gtk_widget_remove_css_class(gestos->status_dot, "status-dot-stopped");
        gtk_widget_add_css_class(gestos->status_dot, "status-dot-running");
    } else {
        gtk_label_set_text(GTK_LABEL(gestos->status_label), "Daemon Off");
        gtk_widget_remove_css_class(gestos->status_dot, "status-dot-running");
        gtk_widget_add_css_class(gestos->status_dot, "status-dot-stopped");
    }

    return TRUE;
}

static gboolean on_daemon_switch_state_set(GtkSwitch *sw, gboolean state, gpointer user_data) {
    GestosApp *gestos = (GestosApp *)user_data;
    if (state) {
        start_daemon();
    } else {
        stop_daemon();
    }
    check_daemon_status_timer(gestos);
    return FALSE;
}

static void reload_daemon_if_running() {
    if (is_daemon_running()) {
        stop_daemon();
        usleep(150 * 1000);
        start_daemon();
    }
}

static void activate(GtkApplication *app, gpointer user_data) {
    GestosApp *gestos = (GestosApp *)user_data;

    gestos->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(gestos->window), "Gestos");
    gtk_window_set_default_size(GTK_WINDOW(gestos->window), 650, 700);

    GtkWidget *header = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(gestos->window), header);
    GtkWidget *add_gest_btn = gtk_button_new_from_icon_name("list-add-symbolic");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), add_gest_btn);
    gtk_widget_set_tooltip_text(add_gest_btn, "Add Gesture");
    g_signal_connect(add_gest_btn, "clicked", G_CALLBACK(on_add_gesture_clicked), gestos);

    GtkWidget *ctrl_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_valign(ctrl_box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start(ctrl_box, 12);
    gtk_widget_set_margin_end(ctrl_box, 12);

    gestos->status_dot = gtk_image_new_from_icon_name("media-record-symbolic");
    gtk_widget_add_css_class(gestos->status_dot, "status-dot-stopped");
    gtk_box_append(GTK_BOX(ctrl_box), gestos->status_dot);

    gestos->status_label = gtk_label_new("Daemon Off");
    gtk_widget_add_css_class(gestos->status_label, "status-label");
    gtk_box_append(GTK_BOX(ctrl_box), gestos->status_label);

    gestos->daemon_switch = gtk_switch_new();
    gtk_box_append(GTK_BOX(ctrl_box), gestos->daemon_switch);
    gestos->switch_handler_id = g_signal_connect(gestos->daemon_switch, "state-set", G_CALLBACK(on_daemon_switch_state_set), gestos);

    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), ctrl_box);

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
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(gestos->main_list), GTK_SELECTION_NONE);
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
        ".gesture-preview-frame { border: 1px solid alpha(currentColor, 0.08); border-radius: 8px; overflow: hidden; background: transparent; }\n"
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
        ".toast { background: alpha(@theme_text_color, 0.95); color: @theme_bg_color; padding: 8px 18px; border-radius: 20px; position: absolute; bottom: 30px; left: 50%; transform: translateX(-50%); font-weight: bold; box-shadow: 0 10px 30px rgba(0, 0, 0, 0.15); }\n"
        ".status-dot-running { color: #10b981 !important; }\n"
        ".status-dot-stopped { color: #6b7280 !important; opacity: 0.6; }\n"
        ".status-label { font-size: 0.85em; font-weight: 600; }\n");
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    check_daemon_status_timer(gestos);
    g_timeout_add_seconds(1, check_daemon_status_timer, gestos);

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
