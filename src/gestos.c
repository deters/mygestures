#include <gtk/gtk.h>
#include <string.h>
#include "configuration.h"
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

static void add_gesture_row(GestosApp *gestos, Gesture *gesture) {
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(main_vbox, 16);
    gtk_widget_set_margin_end(main_vbox, 16);
    gtk_widget_set_margin_top(main_vbox, 12);
    gtk_widget_set_margin_bottom(main_vbox, 12);

    GtkWidget *top_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    
    GtkWidget *label_name = gtk_label_new(gesture->name);
    gtk_widget_set_halign(label_name, GTK_ALIGN_START);
    gtk_widget_set_hexpand(label_name, TRUE);
    
    PangoAttrList *attrs = pango_attr_list_new();
    pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    gtk_label_set_attributes(GTK_LABEL(label_name), attrs);
    pango_attr_list_unref(attrs);

    GtkWidget *label_move = gtk_label_new(gesture->movement ? gesture->movement->name : "Unknown");
    gtk_widget_set_halign(label_move, GTK_ALIGN_END);
    gtk_widget_add_css_class(label_move, "move-badge");

    gtk_box_append(GTK_BOX(top_hbox), label_name);
    gtk_box_append(GTK_BOX(top_hbox), label_move);
    gtk_box_append(GTK_BOX(main_vbox), top_hbox);

    if (gesture->action_count > 0 && gesture->action_list[0]->original_str) {
        GtkWidget *label_action = gtk_label_new(gesture->action_list[0]->original_str);
        gtk_widget_set_halign(label_action, GTK_ALIGN_START);
        gtk_widget_add_css_class(label_action, "action-label");
        gtk_label_set_ellipsize(GTK_LABEL(label_action), PANGO_ELLIPSIZE_END);
        gtk_box_append(GTK_BOX(main_vbox), label_action);
    }

    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), main_vbox);
    gtk_list_box_append(GTK_LIST_BOX(gestos->main_list), row);
}

static void refresh_gesture_list(GestosApp *gestos) {
    /* Clear existing */
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
            if (!g_strrstr(g->name, search_text) && 
                !(g->movement && g_strrstr(g->movement->name, search_text))) {
                continue;
            }
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
        "version", "1.1.0",
        "copyright", "Copyright © 2026",
        "comments", "A modern, complete UI for MyGestures",
        "authors", (const char *[]){"Gemini CLI", NULL},
        "logo-icon-name", "input-mouse",
        "license-type", GTK_LICENSE_GPL_3_0,
        NULL);
}

static void activate(GtkApplication *app, gpointer user_data) {
    GestosApp *gestos = (GestosApp *)user_data;

    gestos->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(gestos->window), "Gestos");
    gtk_window_set_default_size(GTK_WINDOW(gestos->window), 850, 600);

    GtkWidget *header = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(gestos->window), header);

    GtkWidget *about_btn = gtk_button_new_from_icon_name("help-about-symbolic");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), about_btn);
    g_signal_connect(about_btn, "clicked", G_CALLBACK(on_about_clicked), gestos->window);

    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_window_set_child(GTK_WINDOW(gestos->window), paned);
    gtk_paned_set_wide_handle(GTK_PANED(paned), FALSE);
    gtk_paned_set_position(GTK_PANED(paned), 200);

    /* Sidebar */
    GtkWidget *sidebar_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(sidebar_vbox, "sidebar");
    gtk_paned_set_start_child(GTK_PANED(paned), sidebar_vbox);

    GtkWidget *sidebar_title = gtk_label_new("Applications");
    gtk_widget_set_halign(sidebar_title, GTK_ALIGN_START);
    gtk_widget_set_margin_start(sidebar_title, 16);
    gtk_widget_set_margin_top(sidebar_title, 16);
    gtk_widget_set_margin_bottom(sidebar_title, 8);
    gtk_widget_add_css_class(sidebar_title, "sidebar-header");
    gtk_box_append(GTK_BOX(sidebar_vbox), sidebar_title);

    gestos->sidebar_list = gtk_list_box_new();
    gtk_widget_add_css_class(gestos->sidebar_list, "sidebar-list");
    gtk_box_append(GTK_BOX(sidebar_vbox), gestos->sidebar_list);
    g_signal_connect(gestos->sidebar_list, "row-selected", G_CALLBACK(on_sidebar_row_selected), gestos);

    /* Main Content */
    GtkWidget *content_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(content_vbox, TRUE);
    gtk_paned_set_end_child(GTK_PANED(paned), content_vbox);

    GtkWidget *content_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start(content_header, 30);
    gtk_widget_set_margin_end(content_header, 30);
    gtk_widget_set_margin_top(content_header, 20);
    gtk_widget_set_margin_bottom(content_header, 10);
    gtk_box_append(GTK_BOX(content_vbox), content_header);

    gestos->context_title = gtk_label_new("Global");
    gtk_widget_add_css_class(gestos->context_title, "context-title");
    gtk_widget_set_hexpand(gestos->context_title, TRUE);
    gtk_widget_set_halign(gestos->context_title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(content_header), gestos->context_title);

    gestos->search_entry = gtk_search_entry_new();
    gtk_widget_set_size_request(gestos->search_entry, 200, -1);
    gtk_box_append(GTK_BOX(content_header), gestos->search_entry);
    g_signal_connect(gestos->search_entry, "search-changed", G_CALLBACK(on_search_changed), gestos);

    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_box_append(GTK_BOX(content_vbox), scrolled);

    gestos->main_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(gestos->main_list), GTK_SELECTION_NONE);
    gtk_widget_set_margin_start(gestos->main_list, 30);
    gtk_widget_set_margin_end(gestos->main_list, 30);
    gtk_widget_set_margin_top(gestos->main_list, 10);
    gtk_widget_set_margin_bottom(gestos->main_list, 30);
    gtk_widget_add_css_class(gestos->main_list, "boxed-list");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), gestos->main_list);

    /* Load configuration */
    gestos->config = configuration_new();
    configuration_load_from_defaults(gestos->config, 0);

    /* Populate sidebar */
    for (int i = 0; i < gestos->config->context_count; i++) {
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new(gestos->config->context_list[i]->name);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_widget_set_margin_start(label, 16);
        gtk_widget_set_margin_top(label, 8);
        gtk_widget_set_margin_bottom(label, 8);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        gtk_list_box_append(GTK_LIST_BOX(gestos->sidebar_list), row);
    }

    /* Initial selection */
    if (gestos->config->context_count > 0) {
        GtkListBoxRow *first = gtk_list_box_get_row_at_index(GTK_LIST_BOX(gestos->sidebar_list), 0);
        gtk_list_box_select_row(GTK_LIST_BOX(gestos->sidebar_list), first);
        gestos->current_context = gestos->config->context_list[0];
        refresh_gesture_list(gestos);
    }

    /* Professional CSS Styling */
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider,
        "window { background-color: #f5f5f7; }\n"
        ".sidebar { background-color: #ebebeb; border-right: 1px solid #d1d1d1; }\n"
        ".sidebar-header { color: #6e6e73; font-weight: bold; font-size: 0.8em; text-transform: uppercase; }\n"
        ".sidebar-list { background: transparent; }\n"
        ".sidebar-list row { border-radius: 6px; margin: 2px 8px; }\n"
        ".sidebar-list row:selected { background-color: #007aff; color: white; }\n"
        ".context-title { font-size: 1.8em; font-weight: 800; color: #1d1d1f; }\n"
        ".boxed-list { border: 1px solid #d2d2d7; border-radius: 12px; background: white; }\n"
        ".boxed-list row { border-bottom: 1px solid #f5f5f7; }\n"
        ".boxed-list row:last-child { border-bottom: none; }\n"
        ".move-badge { background-color: #f2f2f7; color: #007aff; padding: 4px 10px; border-radius: 10px; font-weight: bold; font-size: 0.85em; }\n"
        ".action-label { color: #86868b; font-size: 0.9em; }\n"
        "scrollbar { background: transparent; }\n");
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
