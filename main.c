#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

typedef struct {
    GtkEntry *input_entry;
    GtkRadioButton *compress_button;
    GtkRadioButton *convert_button;
    GtkRadioButton *download_button;
    GtkProgressBar *progress_bar;
} AppWidgets;

// Function to decode base64 icon and set as application icon
void decode_base64_icon(const char *base64_data, GdkPixbuf **pixbuf) {
    gsize length = 0;
    guchar *data = g_base64_decode(base64_data, &length);
    if (data == NULL) {
        g_printerr("Failed to decode base64 data.\n");
        return;
    }
    
    GInputStream *stream = g_memory_input_stream_new_from_data(data, length, g_free);
    if (stream == NULL) {
        g_printerr("Failed to create input stream from data.\n");
        g_free(data);
        return;
    }
    
    *pixbuf = gdk_pixbuf_new_from_stream(stream, NULL, NULL);
    if (*pixbuf == NULL) {
        g_printerr("Failed to create pixbuf from stream.\n");
    }
    
    g_object_unref(stream);
    g_free(data);
}

// Function to browse files
void on_browse_button_clicked(GtkWidget *widget, gpointer data) {
    GtkEntry *input_entry = GTK_ENTRY(data);
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Open File", NULL, GTK_FILE_CHOOSER_ACTION_OPEN,
                                                    "_Cancel", GTK_RESPONSE_CANCEL,
                                                    "_Open", GTK_RESPONSE_ACCEPT,
                                                    NULL);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        gtk_entry_set_text(input_entry, filename);
        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

// Function to update progress bar
gboolean update_progress_bar(gpointer data) {
    AppWidgets *app_widgets = (AppWidgets *)data;
    gdouble current_value = gtk_progress_bar_get_fraction(app_widgets->progress_bar);
    if (current_value < 1.0) {
        current_value += 0.01;
        gtk_progress_bar_set_fraction(app_widgets->progress_bar, current_value);
        return TRUE;
    }
    return FALSE;
}

// Function to execute the selected operation
void on_execute_button_clicked(GtkWidget *widget, gpointer data) {
    AppWidgets *app_widgets = (AppWidgets *)data;
    const char *input_file = gtk_entry_get_text(app_widgets->input_entry);

    if (strlen(input_file) == 0) {
        GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
                                                   "Please provide a valid input file or URL.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    struct passwd *pw = getpwuid(getuid());
    if (pw == NULL) {
        g_printerr("Failed to get user information.\n");
        return;
    }
    
    const char *homedir = pw->pw_dir;
    char command[1024];

    gtk_progress_bar_set_fraction(app_widgets->progress_bar, 0.0);
    g_timeout_add(100, update_progress_bar, app_widgets);

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app_widgets->compress_button))) {
        snprintf(command, sizeof(command), "ffmpeg -y -i \"%s\" -vcodec libx265 -crf 28 \"%s/output.mp4\"", input_file, homedir);
    } else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app_widgets->convert_button))) {
        snprintf(command, sizeof(command), "ffmpeg -y -i \"%s\" -vn \"%s/output.mp3\"", input_file, homedir);
    } else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app_widgets->download_button))) {
        snprintf(command, sizeof(command), "youtube-dl -o \"%s/%%(title)s.%%(ext)s\" \"%s\"", homedir, input_file);
    } else {
        GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
                                                   "No operation selected.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    int ret = system(command);
    GtkWidget *dialog;
    if (ret == 0) {
        gtk_progress_bar_set_fraction(app_widgets->progress_bar, 1.0);
        dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
                                       "Operation completed successfully.");
    } else {
        dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                                       "Operation failed.");
    }
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

// Function to close the application
void on_close_button_clicked(GtkWidget *widget, gpointer data) {
    gtk_main_quit();
}

int main(int argc, char *argv[]) {
    GtkWidget *window, *grid, *input_label, *input_entry;
    GtkWidget *browse_button, *compress_button, *convert_button, *download_button;
    GtkWidget *execute_button, *close_button, *progress_bar;
    GdkPixbuf *icon_pixbuf = NULL;
    AppWidgets *app_widgets = g_malloc0(sizeof(AppWidgets)); // Use g_malloc0 to zero-initialize

    gtk_init(&argc, &argv);

    const char *base64_icon = "BASE64_ICON_DATA_HERE"; // Replace with your base64 encoded PNG data
    decode_base64_icon(base64_icon, &icon_pixbuf);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Video Tool by @bdul");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 300);
    gtk_container_set_border_width(GTK_CONTAINER(window), 15);

    if (icon_pixbuf != NULL) {
        gtk_window_set_icon(GTK_WINDOW(window), icon_pixbuf);
        g_object_unref(icon_pixbuf);
    }

    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_add(GTK_CONTAINER(window), grid);

    input_label = gtk_label_new("File Path or YouTube URL:");
    gtk_grid_attach(GTK_GRID(grid), input_label, 0, 0, 1, 1);

    input_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), input_entry, 1, 0, 2, 1);
    app_widgets->input_entry = GTK_ENTRY(input_entry);

    browse_button = gtk_button_new_with_label("Browse");
    g_signal_connect(browse_button, "clicked", G_CALLBACK(on_browse_button_clicked), input_entry);
    gtk_grid_attach(GTK_GRID(grid), browse_button, 3, 0, 1, 1);

    compress_button = gtk_radio_button_new_with_label(NULL, "Compress Video");
    gtk_grid_attach(GTK_GRID(grid), compress_button, 1, 1, 1, 1);
    app_widgets->compress_button = GTK_RADIO_BUTTON(compress_button);

    convert_button = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(compress_button), "Convert to Audio");
    gtk_grid_attach(GTK_GRID(grid), convert_button, 1, 2, 1, 1);
    app_widgets->convert_button = GTK_RADIO_BUTTON(convert_button);

    download_button = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(compress_button), "Download YouTube Video");
    gtk_grid_attach(GTK_GRID(grid), download_button, 1, 3, 1, 1);
    app_widgets->download_button = GTK_RADIO_BUTTON(download_button);

    progress_bar = gtk_progress_bar_new();
    gtk_grid_attach(GTK_GRID(grid), progress_bar, 1, 4, 3, 1);
    app_widgets->progress_bar = GTK_PROGRESS_BAR(progress_bar);

    execute_button = gtk_button_new_with_label("Execute");
    g_signal_connect(execute_button, "clicked", G_CALLBACK(on_execute_button_clicked), app_widgets);
    gtk_grid_attach(GTK_GRID(grid), execute_button, 1, 5, 1, 1);

    close_button = gtk_button_new_with_label("Close");
    g_signal_connect(close_button, "clicked", G_CALLBACK(on_close_button_clicked), NULL);
    gtk_grid_attach(GTK_GRID(grid), close_button, 2, 5, 1, 1);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    gtk_widget_show_all(window);
    gtk_main();

    g_free(app_widgets);
    return 0;
}
