#include <gtk/gtk.h>
#include <string.h>
#include "configuration.h"
#include "configuration_parser.h"
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

static void refresh_gesture_list(GestosApp *gestos);

static void on_delete_clicked(GtkWidget *widget, gpointer user_data) {
    Gesture *g = (Gesture *)user_data;
    Context *ctx = g->context;
    
    /* Find and remove from context */
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
        /* In a real app we'd free 'g', but let's be safe here */
    }
    
    /* Hacky way to get GestosApp - in real app we'd pass it better */
    GtkWidget *list = gtk_widget_get_ancestor(widget, GTK_TYPE_LIST_BOX);
    GestosApp *gestos = g_object_get_data(G_OBJECT(list), "gestos-app");
    refresh_gesture_list(gestos);
}

static void on_editor_response(GtkDialog *dialog, int response, gpointer user_data) {
    GestureEditor *editor = (GestureEditor *)user_data;
    if (response == GTK_RESPONSE_ACCEPT) {
        const char *name = gtk_editable_get_text(GTK_EDITABLE(editor->name_entry));
        const char *move_name = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(editor->move_combo));
        const char *action = gtk_editable_get_text(GTK_EDITABLE(editor->action_entry));
        
        if (editor->gesture) {
            /* Editing existing */
            editor->gesture->name = strdup(name);
            editor->gesture->movement = configuration_find_movement_by_name(editor->app->config, (char*)move_name);
            if (editor->gesture->action_count > 0) {
                editor->gesture->action_list[0]->original_str = strdup(action);
            } else {
                configuration_add_action_from_string(editor->gesture, action);
            }
        } else {
            /* Creating new */
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
    if (g && g->action_count > 0) gtk_editable_set_text(GTK_EDITABLE(editor->action_entry), g->action_list[0]->original_str);
    gtk_grid_attach(GTK_GRID(grid), editor->action_entry, 1, 2, 1, 1);
    
    g_signal_connect(editor->dialog, "response", G_CALLBACK(on_editor_response), editor);
    gtk_window_present(GTK_WINDOW(editor->dialog));
}

static void on_edit_clicked(GtkListBox *list, GtkListBoxRow *row, gpointer user_data) {
    GestosApp *gestos = (GestosApp *)user_data;
    /* We stored the Gesture* in the row data */
    Gesture *g = g_object_get_data(G_OBJECT(row), "gesture-ptr");
    if (g) open_gesture_editor(gestos, g);
}

static void on_add_clicked(GtkWidget *widget, gpointer user_data) {
    open_gesture_editor((GestosApp *)user_data, NULL);
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

    if (gesture->action_count > 0 && gesture->action_list[0]->original_str) {
        GtkWidget *label_action = gtk_label_new(gesture->action_list[0]->original_str);
        gtk_widget_set_halign(label_action, GTK_ALIGN_START);
        gtk_widget_add_css_class(label_action, "action-label");
        gtk_box_append(GTK_BOX(vbox), label_action);
    }
    
    gtk_box_append(GTK_BOX(main_hbox), vbox);

    GtkWidget *label_move = gtk_label_new(gesture->movement ? gesture->movement->name : "Unknown");
    gtk_widget_add_css_class(label_move, "move-badge");
    gtk_box_append(GTK_BOX(main_hbox), label_move);

    GtkWidget *del_btn = gtk_button_new_from_icon_name("user-trash-symbolic");
    gtk_widget_add_css_class(del_btn, "flat");
    gtk_widget_add_css_class(del_btn, "destructive-action");
    g_signal_connect(del_btn, "clicked", G_CALLBACK(on_delete_clicked), gesture);
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

static void on_sidebar_row_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data) {
    if (!row) return;
    GestosApp *gestos = (GestosApp *)user_data;
    int index = gtk_list_box_row_get_index(row);
    if (index >= 0 && index < gestos->config->context_count) {
        gestos->current_context = gestos->config->context_list[index];
        refresh_gesture_list(gestos);
    }
}

static void on_search_changed(GtkSearchEntry *entry, gpointer user_data) {
    refresh_gesture_list((GestosApp *)user_data);
}

static void on_about_clicked(GtkWidget *widget, gpointer user_data) {
    GtkWindow *parent = GTK_WINDOW(user_data);
    gtk_show_about_dialog(parent,
        "program-name", "Gestos",
        "version", "1.2.0",
        "copyright", "Copyright © 2026",
        "comments", "Modern GNOME Gesture Editor",
        "authors", (const char *[]){"Gemini CLI", NULL},
        "logo-icon-name", "input-mouse",
        "license-type", GTK_LICENSE_GPL_3_0,
        NULL);
}

static void activate(GtkApplication *app, gpointer user_data) {
    GestosApp *gestos = (GestosApp *)user_data;

    gestos->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(gestos->window), "Gestos");
    gtk_window_set_default_size(GTK_WINDOW(gestos->window), 900, 650);

    GtkWidget *header = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(gestos->window), header);

    GtkWidget *save_btn = gtk_button_new_with_label("Save");
    gtk_widget_add_css_class(save_btn, "suggested-action");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), save_btn);
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_save_config_clicked), gestos);

    GtkWidget *add_btn = gtk_button_new_from_icon_name("list-add-symbolic");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), add_btn);
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_add_clicked), gestos);

    GtkWidget *about_btn = gtk_button_new_from_icon_name("help-about-symbolic");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), about_btn);
    g_signal_connect(about_btn, "clicked", G_CALLBACK(on_about_clicked), gestos->window);

    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_window_set_child(GTK_WINDOW(gestos->window), paned);
    gtk_paned_set_position(GTK_PANED(paned), 220);

    GtkWidget *sidebar_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(sidebar_vbox, "sidebar");
    gtk_paned_set_start_child(GTK_PANED(paned), sidebar_vbox);

    GtkWidget *sidebar_title = gtk_label_new("Applications");
    gtk_widget_set_halign(sidebar_title, GTK_ALIGN_START);
    gtk_widget_set_margin_start(sidebar_title, 16);
    gtk_widget_set_margin_top(sidebar_title, 16);
    gtk_widget_add_css_class(sidebar_title, "sidebar-header");
    gtk_box_append(GTK_BOX(sidebar_vbox), sidebar_title);

    gestos->sidebar_list = gtk_list_box_new();
    gtk_widget_add_css_class(gestos->sidebar_list, "sidebar-list");
    gtk_box_append(GTK_BOX(sidebar_vbox), gestos->sidebar_list);
    g_signal_connect(gestos->sidebar_list, "row-selected", G_CALLBACK(on_sidebar_row_selected), gestos);

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
    g_signal_connect(gestos->main_list, "row-activated", G_CALLBACK(on_edit_clicked), gestos);

    /* Load configuration */
    gestos->config = configuration_new();
    configuration_load_from_defaults(gestos->config, 0);

    for (int i = 0; i < gestos->config->context_count; i++) {
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new(gestos->config->context_list[i]->name);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_widget_set_margin_start(label, 16);
        gtk_widget_set_margin_top(label, 10);
        gtk_widget_set_margin_bottom(label, 10);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        gtk_list_box_append(GTK_LIST_BOX(gestos->sidebar_list), row);
    }

    if (gestos->config->context_count > 0) {
        GtkListBoxRow *first = gtk_list_box_get_row_at_index(GTK_LIST_BOX(gestos->sidebar_list), 0);
        gtk_list_box_select_row(GTK_LIST_BOX(gestos->sidebar_list), first);
        gestos->current_context = gestos->config->context_list[0];
        refresh_gesture_list(gestos);
    }

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider,
        ".sidebar-header { font-weight: bold; font-size: 0.75em; text-transform: uppercase; margin-bottom: 12px; }\n"
        ".sidebar-list row { border-radius: 8px; margin: 2px 12px; padding: 2px; }\n"
        ".context-title { font-size: 2.2em; font-weight: 800; }\n"
        ".boxed-list { border-radius: 12px; }\n"
        ".boxed-list row:last-child { border-bottom: none; }\n"
        ".move-badge { padding: 4px 12px; border-radius: 20px; font-weight: bold; font-size: 0.8em; }\n"
        ".action-label { font-size: 0.85em; }\n"
        ".toast { padding: 10px 20px; border-radius: 20px; position: absolute; bottom: 40px; left: 50%; transform: translateX(-50%); font-weight: bold; }\n");
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                               GTK_STYLE_PROVIDER(provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

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
