#include <gtk/gtk.h>
#include "configuration.h"
#include "mygestures.h"

typedef struct {
    GtkApplication *app;
    Configuration *config;
    GtkWidget *window;
    GtkWidget *list_box;
} GestosApp;

static void add_gesture_row(GtkWidget *list_box, Gesture *gesture) {
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);

    GtkWidget *label_name = gtk_label_new(gesture->name);
    gtk_widget_set_halign(label_name, GTK_ALIGN_START);
    gtk_widget_set_hexpand(label_name, TRUE);
    gtk_label_set_weight(GTK_LABEL(label_name), PANGO_WEIGHT_BOLD);

    GtkWidget *label_move = gtk_label_new(gesture->movement ? gesture->movement->name : "Unknown");
    gtk_widget_set_halign(label_move, GTK_ALIGN_END);
    gtk_widget_add_css_class(label_move, "dim-label");

    gtk_box_append(GTK_BOX(box), label_name);
    gtk_box_append(GTK_BOX(box), label_move);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);

    gtk_list_box_append(GTK_LIST_BOX(list_box), row);
}

static void on_about_clicked(GtkWidget *widget, gpointer user_data) {
    GtkWindow *parent = GTK_WINDOW(user_data);
    gtk_show_about_dialog(parent,
        "program-name", "Gestos",
        "version", "1.0.0",
        "copyright", "Copyright © 2026",
        "comments", "A modern UI for MyGestures",
        "authors", (const char *[]){"Gemini CLI", NULL},
        "logo-icon-name", "input-mouse",
        NULL);
}

static void on_search_changed(GtkSearchEntry *entry, gpointer user_data) {
    GestosApp *gestos = (GestosApp *)user_data;
    const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
    
    GtkWidget *child = gtk_widget_get_first_child(gestos->list_box);
    while (child) {
        GtkWidget *row_box = gtk_list_box_row_get_child(GTK_LIST_BOX_ROW(child));
        GtkWidget *label = gtk_widget_get_first_child(row_box); // First label is name
        const char *label_text = gtk_label_get_text(GTK_LABEL(label));
        
        if (text && strlen(text) > 0) {
            if (g_strrstr(label_text, text)) {
                gtk_widget_set_visible(child, TRUE);
            } else {
                gtk_widget_set_visible(child, FALSE);
            }
        } else {
            gtk_widget_set_visible(child, TRUE);
        }
        child = gtk_widget_get_next_sibling(child);
    }
}

static void activate(GtkApplication *app, gpointer user_data) {
    GestosApp *gestos = (GestosApp *)user_data;

    gestos->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(gestos->window), "Gestos");
    gtk_window_set_default_size(GTK_WINDOW(gestos->window), 600, 450);

    GtkWidget *header = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(gestos->window), header);

    GtkWidget *about_btn = gtk_button_new_from_icon_name("help-about-symbolic");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), about_btn);
    g_signal_connect(about_btn, "clicked", G_CALLBACK(on_about_clicked), gestos->window);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(gestos->window), main_box);

    GtkWidget *search_bar_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_margin_start(search_bar_box, 20);
    gtk_widget_set_margin_end(search_bar_box, 20);
    gtk_widget_set_margin_top(search_bar_box, 10);
    gtk_box_append(GTK_BOX(main_box), search_bar_box);

    GtkWidget *search_entry = gtk_search_entry_new();
    gtk_widget_set_hexpand(search_entry, TRUE);
    gtk_box_append(GTK_BOX(search_bar_box), search_entry);
    g_signal_connect(search_entry, "search-changed", G_CALLBACK(on_search_changed), gestos);

    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_box_append(GTK_BOX(main_box), scrolled);

    gestos->list_box = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(gestos->list_box), GTK_SELECTION_NONE);
    gtk_widget_set_margin_start(gestos->list_box, 20);
    gtk_widget_set_margin_end(gestos->list_box, 20);
    gtk_widget_set_margin_top(gestos->list_box, 20);
    gtk_widget_set_margin_bottom(gestos->list_box, 20);
    gtk_widget_add_css_class(gestos->list_box, "boxed-list");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), gestos->list_box);

    /* Load configuration */
    gestos->config = configuration_new();
    configuration_load_from_defaults(gestos->config, 0);

    /* Populate list with global gestures */
    if (gestos->config->context_count > 0) {
        Context *global = gestos->config->context_list[0]; // Assuming first is global
        for (int i = 0; i < global->gesture_count; i++) {
            add_gesture_row(gestos->list_box, global->gesture_list[i]);
        }
    }

    /* CSS for that Apple-like feel */
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        ".boxed-list { border: 1px solid #ddd; border-radius: 8px; background: white; }\n"
        ".boxed-list row { border-bottom: 1px solid #eee; }\n"
        ".boxed-list row:last-child { border-bottom: none; }\n"
        ".dim-label { color: #888; font-size: 0.9em; }\n"
        "window { background-color: #f5f5f7; }\n", -1);
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
