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
    GtkWidget *sidebar_list;
    GtkWidget *main_list;
    GtkWidget *search_entry;
    GtkWidget *context_title;
    Context *current_context;
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
} GestureEditor;

typedef struct {
    GestosApp *app;
    Context *context;
    GtkWidget *dialog;
    GtkWidget *name_entry;
    GtkWidget *title_entry;
    GtkWidget *class_entry;
} AppEditor;

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
    { ACTION_GNOME, "GNOME Shortcut", "gnome", "preferences-desktop-keyboard-shortcuts-symbolic" },
    { 0, NULL, NULL, NULL }
};

static void refresh_gesture_list(GestosApp *gestos);
static void refresh_sidebar(GestosApp *gestos);

/* --- APP (CONTEXT) EDITOR --- */

static void on_app_editor_response(GtkDialog *dialog, int response, gpointer user_data) {
    AppEditor *editor = (AppEditor *)user_data;
    if (response == GTK_RESPONSE_ACCEPT) {
        const char *name = gtk_editable_get_text(GTK_EDITABLE(editor->name_entry));
        const char *title = gtk_editable_get_text(GTK_EDITABLE(editor->title_entry));
        const char *class = gtk_editable_get_text(GTK_EDITABLE(editor->class_entry));
        
        if (editor->context) {
            /* Editing existing */
            editor->context->name = strdup(name);
            editor->context->title = strdup(title);
            editor->context->class = strdup(class);
        } else {
            /* Creating new */
            configuration_create_context(editor->app->config, strdup(name), strdup(title), strdup(class));
        }
        refresh_sidebar(editor->app);
    }
    gtk_window_destroy(GTK_WINDOW(dialog));
    g_free(editor);
}

static void open_app_editor(GestosApp *gestos, Context *ctx) {
    AppEditor *editor = g_new0(AppEditor, 1);
    editor->app = gestos;
    editor->context = ctx;
    
    editor->dialog = gtk_dialog_new_with_buttons(ctx ? "Edit Application" : "New Application",
                                               GTK_WINDOW(gestos->window),
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               "_Cancel", GTK_RESPONSE_CANCEL,
                                               "_Save", GTK_RESPONSE_ACCEPT,
                                               NULL);
    
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(editor->dialog));
    gtk_widget_set_margin_top(content, 18);
    gtk_widget_set_margin_bottom(content, 18);
    gtk_widget_set_margin_start(content, 18);
    gtk_widget_set_margin_end(content, 18);
    
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_box_append(GTK_BOX(content), grid);
    
    GtkWidget *helper = gtk_label_new("Use Regular Expressions (Regex) to match Title and Class.");
    gtk_widget_add_css_class(helper, "dim-label");
    gtk_grid_attach(GTK_GRID(grid), helper, 0, 0, 2, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Name:"), 0, 1, 1, 1);
    editor->name_entry = gtk_entry_new();
    gtk_widget_set_hexpand(editor->name_entry, TRUE);
    if (ctx) gtk_editable_set_text(GTK_EDITABLE(editor->name_entry), ctx->name);
    gtk_grid_attach(GTK_GRID(grid), editor->name_entry, 1, 1, 1, 1);
    
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Window Title:"), 0, 2, 1, 1);
    editor->title_entry = gtk_entry_new();
    gtk_widget_set_hexpand(editor->title_entry, TRUE);
    if (ctx) gtk_editable_set_text(GTK_EDITABLE(editor->title_entry), ctx->title ? ctx->title : "");
    gtk_grid_attach(GTK_GRID(grid), editor->title_entry, 1, 2, 1, 1);
    
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Window Class:"), 0, 3, 1, 1);
    editor->class_entry = gtk_entry_new();
    gtk_widget_set_hexpand(editor->class_entry, TRUE);
    if (ctx) gtk_editable_set_text(GTK_EDITABLE(editor->class_entry), ctx->class ? ctx->class : "");
    gtk_grid_attach(GTK_GRID(grid), editor->class_entry, 1, 3, 1, 1);
    
    g_signal_connect(editor->dialog, "response", G_CALLBACK(on_app_editor_response), editor);
    gtk_window_present(GTK_WINDOW(editor->dialog));
}

static void on_add_app_clicked(GtkWidget *widget, gpointer user_data) {
    open_app_editor((GestosApp *)user_data, NULL);
}

static void on_delete_app_clicked(GtkWidget *widget, gpointer user_data) {
    GestosApp *gestos = (GestosApp *)user_data;
    if (!gestos->current_context || strcmp(gestos->current_context->name, "global") == 0) return;

    /* Find and remove from config */
    int found = -1;
    for (int i = 0; i < gestos->config->context_count; i++) {
        if (gestos->config->context_list[i] == gestos->current_context) {
            found = i;
            break;
        }
    }
    
    if (found != -1) {
        for (int i = found; i < gestos->config->context_count - 1; i++) {
            gestos->config->context_list[i] = gestos->config->context_list[i+1];
        }
        gestos->config->context_count--;
        gestos->current_context = (gestos->config->context_count > 0) ? gestos->config->context_list[0] : NULL;
        refresh_sidebar(gestos);
    }
}

static void on_sidebar_row_activated(GtkListBox *list, GtkListBoxRow *row, gpointer user_data) {
    GestosApp *gestos = (GestosApp *)user_data;
    Context *ctx = g_object_get_data(G_OBJECT(row), "context-ptr");
    if (ctx && strcmp(ctx->name, "global") != 0) {
        open_app_editor(gestos, ctx);
    }
}

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
            gtk_combo_box_set_active(GTK_COMBO_BOX(browser->editor->action_type_combo), 1); /* Execute */
            gtk_editable_set_text(GTK_EDITABLE(browser->editor->action_val_entry), action->command);
        } else {
            /* Try to map common actions to native types first */
            int native_id = -1;
            if (strcmp(action->name, "close") == 0) native_id = ACTION_KILL;
            else if (strcmp(action->name, "maximize") == 0) native_id = ACTION_MAXIMIZE;
            else if (strcmp(action->name, "unmaximize") == 0) native_id = ACTION_RESTORE;
            else if (strcmp(action->name, "minimize") == 0) native_id = ACTION_ICONIFY;
            
            if (native_id != -1) {
                for (int i = 0; action_types[i].name; i++) {
                    if (action_types[i].id == native_id) {
                        gtk_combo_box_set_active(GTK_COMBO_BOX(browser->editor->action_type_combo), i);
                        break;
                    }
                }
            } else {
                gtk_combo_box_set_active(GTK_COMBO_BOX(browser->editor->action_type_combo), 0); /* Keypress */
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

static void open_gnome_action_browser(GtkWidget *btn, gpointer user_data) {
    GestureEditor *editor = (GestureEditor *)user_data;
    GnomeActionBrowser *browser = g_new0(GnomeActionBrowser, 1);
    browser->editor = editor;

    browser->dialog = gtk_dialog_new_with_buttons("Browse GNOME Actions",
                                                GTK_WINDOW(editor->dialog),
                                                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                "_Close", GTK_RESPONSE_CLOSE,
                                                NULL);
    gtk_window_set_default_size(GTK_WINDOW(browser->dialog), 550, 650);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(browser->dialog));
    
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
        gtk_widget_add_css_class(l_accel, "move-badge");
        gtk_box_append(GTK_BOX(hbox), l_accel);

        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), hbox);
        gtk_list_box_append(GTK_LIST_BOX(browser->list), row);
    }

    g_signal_connect(browser->list, "row-activated", G_CALLBACK(on_gnome_action_row_activated), browser);
    g_signal_connect(browser->dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);

    gtk_window_present(GTK_WINDOW(browser->dialog));
}

/* --- GESTURE EDITOR --- */

static const char* get_action_icon(int id) {
    for (int i = 0; action_types[i].name; i++) {
        if (action_types[i].id == id) return action_types[i].icon;
    }
    return "system-run-symbolic";
}

static void on_action_type_changed(GtkComboBox *combo, gpointer user_data) {
    GestureEditor *editor = (GestureEditor *)user_data;
    int index = gtk_combo_box_get_active(combo);
    if (index < 0) return;
    
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
            gtk_widget_remove_css_class(editor->record_btn, "suggested-action");
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
        gtk_widget_add_css_class(btn, "suggested-action");
    } else {
        gtk_button_set_label(GTK_BUTTON(btn), "Record");
        gtk_widget_remove_css_class(btn, "suggested-action");
    }
}

static void on_gesture_editor_response(GtkDialog *dialog, int response, gpointer user_data) {
    GestureEditor *editor = (GestureEditor *)user_data;
    if (response == GTK_RESPONSE_ACCEPT) {
        const char *name = gtk_editable_get_text(GTK_EDITABLE(editor->name_entry));
        const char *move_name = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(editor->move_combo));
        
        int type_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(editor->action_type_combo));
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
            Gesture *new_g = configuration_create_gesture(editor->app->current_context, (char*)name, (char*)move_name);
            configuration_add_action_from_string(new_g, full_action);
        }
        free(full_action);
        refresh_gesture_list(editor->app);
    }
    gtk_window_destroy(GTK_WINDOW(dialog));
    g_free(editor);
}

static void open_gesture_editor(GestosApp *gestos, Gesture *g) {
    GestureEditor *editor = g_new0(GestureEditor, 1);
    editor->app = gestos;
    editor->gesture = g;
    
    editor->dialog = gtk_dialog_new_with_buttons(g ? "Edit Gesture" : "New Gesture",
                                               GTK_WINDOW(gestos->window),
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               "_Cancel", GTK_RESPONSE_CANCEL,
                                               "_Save", GTK_RESPONSE_ACCEPT,
                                               NULL);
    
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(editor->dialog));
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
    editor->move_combo = gtk_combo_box_text_new();
    for (int i = 0; i < gestos->config->movement_count; i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(editor->move_combo), gestos->config->movement_list[i]->name);
        if (g && g->movement && strcmp(g->movement->name, gestos->config->movement_list[i]->name) == 0) {
            gtk_combo_box_set_active(GTK_COMBO_BOX(editor->move_combo), i);
        }
    }
    if (!g) gtk_combo_box_set_active(GTK_COMBO_BOX(editor->move_combo), 0);
    gtk_grid_attach(GTK_GRID(grid), editor->move_combo, 1, 1, 1, 1);
    
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Action Type:"), 0, 2, 1, 1);
    editor->action_type_combo = gtk_combo_box_text_new();
    int active_type = 0;
    for (int i = 0; action_types[i].name; i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(editor->action_type_combo), action_types[i].name);
        if (g && g->action_count > 0 && g->action_list[0]->type == action_types[i].id) {
            active_type = i;
        }
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(editor->action_type_combo), active_type);
    gtk_grid_attach(GTK_GRID(grid), editor->action_type_combo, 1, 2, 1, 1);
    
    editor->action_val_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    editor->action_val_label = gtk_label_new("Command:");
    gtk_grid_attach(GTK_GRID(grid), editor->action_val_label, 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), editor->action_val_box, 1, 3, 1, 1);
    
    editor->action_val_entry = gtk_entry_new();
    gtk_widget_set_hexpand(editor->action_val_entry, TRUE);
    gtk_box_append(GTK_BOX(editor->action_val_box), editor->action_val_entry);
    
    editor->record_btn = gtk_button_new_with_label("Record");
    gtk_box_append(GTK_BOX(editor->action_val_box), editor->record_btn);
    g_signal_connect(editor->record_btn, "clicked", G_CALLBACK(on_record_clicked), editor);

    GtkWidget *browse_btn = gtk_button_new_with_label("Browse GNOME Actions");
    gtk_widget_add_css_class(browse_btn, "flat");
    gtk_grid_attach(GTK_GRID(grid), browse_btn, 1, 4, 1, 1);
    g_signal_connect(browse_btn, "clicked", G_CALLBACK(open_gnome_action_browser), editor);

    if (g && g->action_count > 0) {
        if (g->action_list[0]->original_str)
            gtk_editable_set_text(GTK_EDITABLE(editor->action_val_entry), g->action_list[0]->original_str);
    }
    
    g_signal_connect(editor->action_type_combo, "changed", G_CALLBACK(on_action_type_changed), editor);
    on_action_type_changed(GTK_COMBO_BOX(editor->action_type_combo), editor);

    GtkEventController *key_controller = gtk_event_controller_key_new();
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(on_record_key_pressed), editor);
    gtk_widget_add_controller(editor->dialog, key_controller);
    
    g_signal_connect(editor->dialog, "response", G_CALLBACK(on_gesture_editor_response), editor);
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
    Context *ctx = g->context;
    int found = -1;
    for (int i = 0; i < ctx->gesture_count; i++) {
        if (ctx->gesture_list[i] == g) {
            found = i;
            break;
        }
    }
    if (found != -1) {
        for (int i = found; i < ctx->gesture_count - 1; i++) {
            ctx->gesture_list[i] = ctx->gesture_list[i+1];
        }
        ctx->gesture_count--;
    }
    GtkWidget *list = gtk_widget_get_ancestor(widget, GTK_TYPE_LIST_BOX);
    GestosApp *gestos = g_object_get_data(G_OBJECT(list), "gestos-app");
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
    g_object_set_data(G_OBJECT(row), "gesture-ptr", gesture);
    
    GtkWidget *main_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_margin_start(main_hbox, 16);
    gtk_widget_set_margin_end(main_hbox, 16);
    gtk_widget_set_margin_top(main_hbox, 12);
    gtk_widget_set_margin_bottom(main_hbox, 12);

    int action_id = ACTION_NULL;
    if (gesture->action_count > 0) action_id = gesture->action_list[0]->type;
    
    GtkWidget *icon = gtk_image_new_from_icon_name(get_action_icon(action_id));
    gtk_widget_set_valign(icon, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(icon, 32, 32);
    gtk_box_append(GTK_BOX(main_hbox), icon);

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

    if (!gestos->current_context) return;

    gtk_label_set_text(GTK_LABEL(gestos->context_title), gestos->current_context->name);
    const char *search_text = gtk_editable_get_text(GTK_EDITABLE(gestos->search_entry));

    for (int i = 0; i < gestos->current_context->gesture_count; i++) {
        Gesture *g = gestos->current_context->gesture_list[i];
        if (search_text && strlen(search_text) > 0) {
            if (!g_strrstr(g->name, search_text)) continue;
        }
        add_gesture_row(gestos, g);
    }
}

static void refresh_sidebar(GestosApp *gestos) {
    GtkWidget *child = gtk_widget_get_first_child(gestos->sidebar_list);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_list_box_remove(GTK_LIST_BOX(gestos->sidebar_list), child);
        child = next;
    }

    for (int i = 0; i < gestos->config->context_count; i++) {
        Context *ctx = gestos->config->context_list[i];
        GtkWidget *row = gtk_list_box_row_new();
        g_object_set_data(G_OBJECT(row), "context-ptr", ctx);
        GtkWidget *label = gtk_label_new(ctx->name);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_widget_set_margin_start(label, 16);
        gtk_widget_set_margin_top(label, 10);
        gtk_widget_set_margin_bottom(label, 10);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        gtk_list_box_append(GTK_LIST_BOX(gestos->sidebar_list), row);
        
        if (ctx == gestos->current_context) {
            gtk_list_box_select_row(GTK_LIST_BOX(gestos->sidebar_list), GTK_LIST_BOX_ROW(row));
        }
    }
}

static void on_sidebar_row_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data) {
    if (!row) return;
    GestosApp *gestos = (GestosApp *)user_data;
    Context *ctx = g_object_get_data(G_OBJECT(row), "context-ptr");
    gestos->current_context = ctx;
    refresh_gesture_list(gestos);
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
    gtk_window_set_default_size(GTK_WINDOW(gestos->window), 950, 700);

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

    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_window_set_child(GTK_WINDOW(gestos->window), paned);
    gtk_paned_set_position(GTK_PANED(paned), 240);

    /* SIDEBAR */
    GtkWidget *sidebar_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(sidebar_box, "sidebar");
    gtk_paned_set_start_child(GTK_PANED(paned), sidebar_box);

    GtkWidget *sidebar_top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_margin_top(sidebar_top, 16);
    gtk_widget_set_margin_bottom(sidebar_top, 16);
    gtk_widget_set_margin_start(sidebar_top, 16);
    gtk_widget_set_margin_end(sidebar_top, 16);
    gtk_box_append(GTK_BOX(sidebar_box), sidebar_top);

    GtkWidget *sidebar_title = gtk_label_new("Applications");
    gtk_widget_set_halign(sidebar_title, GTK_ALIGN_START);
    gtk_widget_set_hexpand(sidebar_title, TRUE);
    gtk_widget_add_css_class(sidebar_title, "sidebar-header");
    gtk_box_append(GTK_BOX(sidebar_top), sidebar_title);

    GtkWidget *add_app_btn = gtk_button_new_from_icon_name("list-add-symbolic");
    gtk_widget_add_css_class(add_app_btn, "flat");
    gtk_box_append(GTK_BOX(sidebar_top), add_app_btn);
    g_signal_connect(add_app_btn, "clicked", G_CALLBACK(on_add_app_clicked), gestos);

    gestos->sidebar_list = gtk_list_box_new();
    gtk_widget_add_css_class(gestos->sidebar_list, "sidebar-list");
    gtk_box_append(GTK_BOX(sidebar_box), gestos->sidebar_list);
    g_signal_connect(gestos->sidebar_list, "row-selected", G_CALLBACK(on_sidebar_row_selected), gestos);
    g_signal_connect(gestos->sidebar_list, "row-activated", G_CALLBACK(on_sidebar_row_activated), gestos);

    GtkWidget *sidebar_bottom = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_margin_top(sidebar_bottom, 8);
    gtk_widget_set_margin_bottom(sidebar_bottom, 8);
    gtk_widget_set_margin_start(sidebar_bottom, 8);
    gtk_widget_set_margin_end(sidebar_bottom, 8);
    gtk_box_append(GTK_BOX(sidebar_box), sidebar_bottom);

    GtkWidget *del_app_btn = gtk_button_new_from_icon_name("user-trash-symbolic");
    gtk_widget_add_css_class(del_app_btn, "flat");
    gtk_widget_add_css_class(del_app_btn, "destructive-action");
    gtk_box_append(GTK_BOX(sidebar_bottom), del_app_btn);
    g_signal_connect(del_app_btn, "clicked", G_CALLBACK(on_delete_app_clicked), gestos);

    /* CONTENT */
    GtkWidget *content_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_paned_set_end_child(GTK_PANED(paned), content_vbox);

    GtkWidget *content_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_top(content_header, 24);
    gtk_widget_set_margin_bottom(content_header, 24);
    gtk_widget_set_margin_start(content_header, 24);
    gtk_widget_set_margin_end(content_header, 24);
    gtk_box_append(GTK_BOX(content_vbox), content_header);

    gestos->context_title = gtk_label_new("Global");
    gtk_widget_add_css_class(gestos->context_title, "context-title");
    gtk_widget_set_hexpand(gestos->context_title, TRUE);
    gtk_widget_set_halign(gestos->context_title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(content_header), gestos->context_title);

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
    refresh_sidebar(gestos);
    if (gestos->config->context_count > 0) {
        gestos->current_context = gestos->config->context_list[0];
        refresh_gesture_list(gestos);
    }

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider,
        ".sidebar-header { font-weight: bold; font-size: 0.75em; text-transform: uppercase; }\n"
        ".sidebar-list row { border-radius: 8px; margin: 2px 12px; padding: 2px; }\n"
        ".context-title { font-size: 2.2em; font-weight: 800; }\n"
        ".boxed-list { border-radius: 12px; }\n"
        ".move-badge { background: alpha(currentColor, 0.1); padding: 4px 12px; border-radius: 20px; font-weight: bold; font-size: 0.8em; }\n"
        ".action-label { font-size: 0.85em; opacity: 0.7; }\n"
        ".dim-label { font-size: 0.85em; opacity: 0.7; font-style: italic; }\n"
        ".toast { background: alpha(@theme_text_color, 0.9); color: @theme_bg_color; padding: 10px 20px; border-radius: 20px; position: absolute; bottom: 40px; left: 50%; transform: translateX(-50%); font-weight: bold; }\n");
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
