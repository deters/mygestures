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
    GtkWidget *action_entry;
} GestureEditor;

typedef struct {
    GestosApp *app;
    Context *context;
    GtkWidget *dialog;
    GtkWidget *name_entry;
    GtkWidget *title_entry;
    GtkWidget *class_entry;
} AppEditor;

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
    gtk_widget_set_margin_top(content, 12);
    gtk_widget_set_margin_bottom(content, 12);
    gtk_widget_set_margin_start(content, 12);
    gtk_widget_set_margin_end(content, 12);
    
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_box_append(GTK_BOX(content), grid);
    
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Name:"), 0, 0, 1, 1);
    editor->name_entry = gtk_entry_new();
    if (ctx) gtk_editable_set_text(GTK_EDITABLE(editor->name_entry), ctx->name);
    gtk_grid_attach(GTK_GRID(grid), editor->name_entry, 1, 0, 1, 1);
    
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Window Title (Regex):"), 0, 1, 1, 1);
    editor->title_entry = gtk_entry_new();
    if (ctx) gtk_editable_set_text(GTK_EDITABLE(editor->title_entry), ctx->title ? ctx->title : "");
    gtk_grid_attach(GTK_GRID(grid), editor->title_entry, 1, 1, 1, 1);
    
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Window Class (Regex):"), 0, 2, 1, 1);
    editor->class_entry = gtk_entry_new();
    if (ctx) gtk_editable_set_text(GTK_EDITABLE(editor->class_entry), ctx->class ? ctx->class : "");
    gtk_grid_attach(GTK_GRID(grid), editor->class_entry, 1, 2, 1, 1);
    
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

/* --- GESTURE EDITOR --- */

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
    const char *type_str = "";
    switch(a->type) {
        case ACTION_EXECUTE: type_str = "exec"; break;
        case ACTION_KEYPRESS: type_str = "keypress"; break;
        case ACTION_KILL: type_str = "kill"; break;
        case ACTION_ICONIFY: type_str = "iconify"; break;
        case ACTION_RESTORE: type_str = "restore"; break;
        case ACTION_MAXIMIZE: type_str = "maximize"; break;
        case ACTION_LOWER: type_str = "lower"; break;
        case ACTION_RAISE: type_str = "raise"; break;
        case ACTION_TOGGLE_MAXIMIZED: type_str = "toggle-maximized"; break;
        default: type_str = "unknown"; break;
    }
    char *res = NULL;
    if (a->original_str && strlen(a->original_str) > 0) {
        if (asprintf(&res, "%s %s", type_str, a->original_str) == -1) res = NULL;
    } else {
        res = strdup(type_str);
    }
    return res;
}

static void on_gesture_editor_response(GtkDialog *dialog, int response, gpointer user_data) {
    GestureEditor *editor = (GestureEditor *)user_data;
    if (response == GTK_RESPONSE_ACCEPT) {
        const char *name = gtk_editable_get_text(GTK_EDITABLE(editor->name_entry));
        const char *move_name = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(editor->move_combo));
        const char *action = gtk_editable_get_text(GTK_EDITABLE(editor->action_entry));
        
        if (editor->gesture) {
            editor->gesture->name = strdup(name);
            editor->gesture->movement = configuration_find_movement_by_name(editor->app->config, (char*)move_name);
            /* Reset actions and re-add from string to correctly set type */
            editor->gesture->action_count = 0;
            configuration_add_action_from_string(editor->gesture, action);
        } else {
            Gesture *new_g = configuration_create_gesture(editor->app->current_context, (char*)name, (char*)move_name);
            configuration_add_action_from_string(new_g, action);
        }
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
    gtk_widget_set_margin_top(content, 12);
    gtk_widget_set_margin_bottom(content, 12);
    gtk_widget_set_margin_start(content, 12);
    gtk_widget_set_margin_end(content, 12);
    
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_box_append(GTK_BOX(content), grid);
    
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Name:"), 0, 0, 1, 1);
    editor->name_entry = gtk_entry_new();
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
    
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Action:"), 0, 2, 1, 1);
    editor->action_entry = gtk_entry_new();
    if (g && g->action_count > 0) {
        char *full_action = get_full_action_str(g->action_list[0]);
        gtk_editable_set_text(GTK_EDITABLE(editor->action_entry), full_action);
        free(full_action);
    }
    gtk_grid_attach(GTK_GRID(grid), editor->action_entry, 1, 2, 1, 1);
    
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

/* --- MAIN UI LOGIC --- */

static void add_gesture_row(GestosApp *gestos, Gesture *gesture) {
    GtkWidget *row = gtk_list_box_row_new();
    g_object_set_data(G_OBJECT(row), "gesture-ptr", gesture);
    
    GtkWidget *main_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start(main_hbox, 16);
    gtk_widget_set_margin_end(main_hbox, 16);
    gtk_widget_set_margin_top(main_hbox, 8);
    gtk_widget_set_margin_bottom(main_hbox, 8);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_hexpand(vbox, TRUE);

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
    gtk_box_append(GTK_BOX(main_hbox), label_move);

    GtkWidget *del_btn = gtk_button_new_from_icon_name("user-trash-symbolic");
    gtk_widget_add_css_class(del_btn, "flat");
    gtk_widget_add_css_class(del_btn, "destructive-action");
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
        ".move-badge { padding: 4px 12px; border-radius: 20px; font-weight: bold; font-size: 0.8em; }\n"
        ".action-label { font-size: 0.85em; }\n"
        ".toast { padding: 10px 20px; border-radius: 20px; position: absolute; bottom: 40px; left: 50%; transform: translateX(-50%); font-weight: bold; }\n");
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
