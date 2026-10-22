/* Copyright (c) 2020, TheCynicalTeam @ Github
 * main.c - main program file
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#ifdef X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <gdk/x11/gdkx.h>
#include "x_interaction.h"
#endif

#ifdef WIN32
#include <gdk/gdkwin32.h>
#include "win32_interaction.h"
#endif

#include "gridselect_window_box.h"
#include "gridselect_search_box.h"

static GtkWidget *window;
static Window myown_window;
static Window *active_window; // For displaying it first in windows list.
static Window *wins;
static gchar **in_items; // If we read from stdin.
static int wsize = 0;
static GtkWidget **boxes;
static GtkWidget *layout;
static GtkWidget *search;
static GtkWidget **filtered_boxes;
static int filtered_size;
static int width, height;
static GMainLoop *main_loop;

/* for window mask */
typedef struct {
    gint x, y, width, height;
} rect;

static rect *box_rects;
static guint boxes_drawn;

// Where the search box was placed in the window (it never moves once
// created), kept around so the XShape mask can include it when visible.
static gint search_x, search_y, search_w, search_h;

/* for screenshot mode */
static gboolean key_pressed;

static GKeyFile *color_config;
static gchar **fallback_colors;
static gsize fallback_size;

static gchar *opt_display = NULL;

static struct {
    guint box_width;
    guint box_height;
    gboolean colorize;
    gboolean show_icons;
    gboolean show_desktop;
    gboolean show_titles;
    guint icon_size;
    gchar *font;
    gboolean read_stdin;
    gboolean permissive;
    gboolean persistent;
    gboolean format;
    gboolean screenshot;
    guchar color_offset;
    gint screenshot_offset_x;
    gint screenshot_offset_y;
    gboolean vim_mode;
    gboolean at_pointer;
    gint center_x;
    gint center_y;
    gchar *color_file;
    gint selected;
    gboolean only_current;
} options;

typedef struct {
    gint desktop;
    gchar *iconpath;
    gchar *color;
    gchar *label;
    gchar *opt_name;
} Entry;

static GOptionEntry entries [] =
{
    { "persistent", 'R', 0, G_OPTION_ARG_NONE, &options.persistent,
        "Make XWinGridSelect acts like Alt-Tab switcher", NULL },
    { "read-stdin", 'r', 0, G_OPTION_ARG_NONE, &options.read_stdin,
        "Read items from stdin (and print selected item to stdout)", NULL },
    { "permissive", 'p', 0, G_OPTION_ARG_NONE, &options.permissive,
        "Lets search entry text to be used as individual item.", NULL},
    { "format", 't', 0, G_OPTION_ARG_NONE, &options.format,
        "Read items from stdin in next format: <desktop_num>, <box_color>, <icon>, <label>, <opt-name>.", NULL},
    { "vim-mode", 'V', 0, G_OPTION_ARG_NONE, &options.vim_mode,
        "Turn on vim-like navigation (hjkl, search on /)", NULL },
    { "no-colors", 'C', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &options.colorize,
        "Turn off box colorizing", NULL },
    { "no-icons", 'I', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &options.show_icons,
        "Turn off showing icons", NULL },
    { "no-desktops", 'D', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &options.show_desktop,
        "Turn off showing desktop number", NULL },
    { "no-titles", 'T', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &options.show_titles,
        "Turn off showing titles", NULL },
    { "screenshot", 'S', 0, G_OPTION_ARG_NONE, &options.screenshot,
        "Get screenshot and set it as a background (for WMs that do not support XShape)", NULL },
    { "at-pointer", 'P', 0, G_OPTION_ARG_NONE, &options.at_pointer,
        "Place center of grid at pointer position.", NULL },
    { "box-width", 'W', 0, G_OPTION_ARG_INT, &options.box_width,
        "Width of the boxes (default: 200)", "<int>" },
    { "selected", 's', 0, G_OPTION_ARG_INT, &options.selected,
        "Initially selected box", "<int>" },
    { "box-height", 'H', 0, G_OPTION_ARG_INT, &options.box_height,
        "Height of the boxes (default: 40)", "<int>" },
    { "icon-size", 'i', 0, G_OPTION_ARG_INT, &options.icon_size,
        "Size of window icons (default: 16)", "<int>" },
    { "font", 'f', 0, G_OPTION_ARG_STRING, &options.font,
        "Which font to use for displaying widgets. (default: \"Sans 10\")", "\"font [size]\"" },
    { "hue-offset", 'o', 0, G_OPTION_ARG_INT, &options.color_offset,
        "Set color hue offset (from 0 to 255)", "<int>" },
    { "color-file", 'F', 0, G_OPTION_ARG_FILENAME, &options.color_file,
        "Pick colors from file", "<file>" },
    { "only-current", 'c', 0, G_OPTION_ARG_NONE, &options.only_current,
        "Only show windows on the current workspace.", NULL},
    { "display", 0, 0, G_OPTION_ARG_STRING, &opt_display,
        "X display to use", "DISPLAY" },
    { NULL }
};

static void draw_grid (GtkFixed *where,
        GtkWidget **widgets, int rsize,
        int focus_on,
        int rwidth, int rheight);
static void on_rect_click (GtkWidget *widget, gpointer data);
static void update_box_list ();
static gboolean on_key_press (GtkEventControllerKey *controller, guint keyval, guint keycode,
        GdkModifierType state, gpointer data);
#ifdef X11
static gboolean on_xevent (GdkDisplay *display, gpointer xevent, gpointer data);
#endif
static void refilter (GridSearchBox *search_box, gpointer data);
static void apply_shape_mask (guint size);
static void read_stdin ();
static GdkPixbuf* get_screenshot ();
static void read_config ();
static void write_default_config ();
static void on_focus_change (GObject *object, GParamSpec *pspec, gpointer data);
static void read_colors ();
static gboolean parse_format (Entry *entry, gchar *data);
static GdkRectangle monitor_rect_at_point (gint x, gint y);
void tab_event (gboolean shift);

#ifdef X11
static void on_ewmh_alert_response (GObject *source, GAsyncResult *result, gpointer data)
{
    gtk_alert_dialog_choose_finish (GTK_ALERT_DIALOG (source), result, NULL);
    g_main_loop_quit ((GMainLoop *) data);
}
#endif

static void quit_main_loop (void)
{
    if (main_loop)
        g_main_loop_quit (main_loop);
}

int main (int argc, char **argv)
{
    read_config ();

    // Read options from command-line arguments.
    GError *error = NULL;
    GOptionContext *context;
    context = g_option_context_new (" - show X11 windows as color grid");
    g_option_context_add_main_entries (context, entries, NULL);
    if (!g_option_context_parse (context, &argc, &argv, &error)) {
        g_printerr ("option parsing failed: %s\n", error->message);
        exit (1);
    }
    g_option_context_free (context);

    if (opt_display)
        g_setenv ("DISPLAY", opt_display, TRUE);

    if(options.format && !options.read_stdin) {
        g_printerr("You must provide option --read-stdin!");
        exit(1);
    }

    g_set_prgname ("xwingridselect");
#ifdef X11
    // This program talks to the display server directly via Xlib/EWMH
    // (window listing, XShape cutout, window positioning), so it only
    // works on the X11 backend. GTK4 defaults to Wayland when available
    // and, unlike GTK2, can be built with both backends at once -- without
    // forcing X11 here, every gdk_x11_* call below runs against a Wayland
    // GdkDisplay/GdkSurface and crashes.
    gdk_set_allowed_backends ("x11");
#endif
    gtk_init ();

#ifdef X11
    atoms_init ();
#endif

    if (already_opened ()) {
        g_printerr ("Another instance of xwingridselect is opened.\n");
        exit (1);
    }

    if (options.read_stdin) {
        if(!options.format) {
            options.show_icons = FALSE;
            options.show_desktop = FALSE;
        }
        read_stdin ();
    } else {
#ifdef X11
        // Checks whether WM supports EWMH specifications.
        if (!wm_supports_ewmh ()) {
            GtkAlertDialog *alert = gtk_alert_dialog_new
                ("Error: your WM does not support EWMH specifications.");
            GMainLoop *alert_loop = g_main_loop_new (NULL, FALSE);
            gtk_alert_dialog_choose (alert, NULL, NULL,
                    on_ewmh_alert_response, alert_loop);
            g_main_loop_run (alert_loop);
            g_main_loop_unref (alert_loop);
            g_object_unref (alert);
            return 1;
        }

        active_window = (Window *) property (x11_root (),
                a_NET_ACTIVE_WINDOW,
                XA_WINDOW,
                NULL);
#endif
    }

    if (options.color_file)
        read_colors ();

#ifdef WIN32
    if (options.persistent) {
#ifdef DEBUG
        g_printerr ("Installing Alt-Tab hook");
#endif
        install_alt_tab_hook();
    }
#endif

    gint pointer_x = 0, pointer_y = 0;
#ifdef X11
    {
        Window root_ret, child_ret;
        int win_x, win_y;
        unsigned int mask_ret;
        XQueryPointer (x11_display (), x11_root (), &root_ret, &child_ret,
                &pointer_x, &pointer_y, &win_x, &win_y, &mask_ret);
    }
#endif

    GdkRectangle rect = monitor_rect_at_point (pointer_x, pointer_y);
    width = rect.width;
    height = rect.height;

    if (options.at_pointer) {
        options.center_x = pointer_x - rect.x;
        options.center_y = pointer_y - rect.y;

        if (options.center_x < options.box_width/2)
            options.center_x = options.box_width/2 + 1;
        else if (options.center_x > width - options.box_width/2)
            options.center_x = width - options.box_width/2 - 1;
        if (options.center_y < options.box_height/2)
            options.center_y = options.box_height/2 + 1;
        else if (options.center_y > height - options.box_height/2)
            options.center_y = height - options.box_height/2 - 1;
    } else {
        options.center_x = width/2;
        options.center_y = height/2;
    }

    window = gtk_window_new ();
    gtk_window_set_title (GTK_WINDOW (window), "XWinGridSelect");

    gtk_window_set_default_size (GTK_WINDOW (window), width, height);
    gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
    /**/
    g_signal_connect (G_OBJECT (window), "notify::is-active",
            G_CALLBACK (on_focus_change), NULL);
    /**/
    layout = gtk_fixed_new ();
    gtk_window_set_child (GTK_WINDOW (window), layout);

    if (options.screenshot) {
        gtk_window_fullscreen (GTK_WINDOW (window));

        GdkPixbuf *screenshot = get_screenshot ();
        if (screenshot) {
            GtkWidget *background = gtk_picture_new_for_pixbuf (screenshot);
            gtk_picture_set_content_fit (GTK_PICTURE (background), GTK_CONTENT_FIT_FILL);
            gtk_widget_set_size_request (background, width, height);
            gtk_fixed_put (GTK_FIXED (layout), background, 0, 0);
            g_object_unref (screenshot);
        }
    }

    search = gridselect_search_box_new ();
    gridselect_box_set_font (GRIDSELECT_BOX (search), options.font);
    gtk_widget_set_focusable (search, FALSE);
    gtk_widget_measure (search, GTK_ORIENTATION_HORIZONTAL, -1, NULL, &search_w, NULL, NULL);
    gtk_widget_measure (search, GTK_ORIENTATION_VERTICAL, -1, NULL, &search_h, NULL, NULL);
    search_x = (width - search_w)/2;
    search_y = height - search_h - options.box_height;
    gtk_fixed_put (GTK_FIXED (layout), search, search_x, search_y);
    g_signal_connect (G_OBJECT (search), "changed",
            G_CALLBACK (refilter), NULL);

    GtkEventController *key_controller = gtk_event_controller_key_new ();
    g_signal_connect (key_controller, "key-pressed", G_CALLBACK (on_key_press), NULL);
    gtk_widget_add_controller (window, key_controller);

    g_signal_connect (G_OBJECT (window), "destroy",
            G_CALLBACK (quit_main_loop), NULL);

    gtk_widget_set_visible (search, FALSE);
    gtk_window_present (GTK_WINDOW (window));

    if (options.persistent)
        gtk_widget_set_visible (window, FALSE);

#ifdef X11
    {
        GdkSurface *surface = gtk_native_get_surface (GTK_NATIVE (window));
        myown_window = gdk_x11_surface_get_xid (surface);
    }

    // GTK4 dropped the convenience wrappers for these (set_type_hint,
    // set_skip_taskbar_hint, set_skip_pager_hint, set_keep_above): talk to
    // the WM directly via EWMH instead. Trapped because any untrapped X
    // protocol error here would otherwise take down the whole process
    // (GDK's default X error handler terminates on unexpected errors).
    gdk_x11_display_error_trap_push (gdk_display_get_default ());

    Atom window_type = a_NET_WM_WINDOW_TYPE_DIALOG;
    XChangeProperty (x11_display (), myown_window, a_NET_WM_WINDOW_TYPE, XA_ATOM,
            32, PropModeReplace, (unsigned char *) &window_type, 1);

    Atom states[3] = { a_NET_WM_STATE_SKIP_TASKBAR, a_NET_WM_STATE_SKIP_PAGER, a_NET_WM_STATE_ABOVE };
    XChangeProperty (x11_display (), myown_window, a_NET_WM_STATE, XA_ATOM,
            32, PropModeReplace, (unsigned char *) states, 3);

    // GTK4 also dropped gtk_window_move/gtk_window_set_position: place the
    // window on the right monitor ourselves.
    XMoveResizeWindow (x11_display (), myown_window, rect.x, rect.y, width, height);

    XSync (x11_display (), FALSE);
    gdk_x11_display_error_trap_pop_ignored (gdk_display_get_default ());

    if (!options.read_stdin) {
        // Get PropertyNotify events from root window.
        XSelectInput (x11_display (),
                x11_root (),
                PropertyChangeMask);
        g_signal_connect (gdk_display_get_default (), "xevent", G_CALLBACK (on_xevent), NULL);
    }
#endif
#ifdef WIN32
    {
        GdkSurface *surface = gtk_native_get_surface (GTK_NATIVE (window));
        myown_window = gdk_win32_surface_get_handle (surface);
    }
#endif
    update_box_list ();

    draw_grid (GTK_FIXED (layout), boxes, wsize,
            options.selected >= wsize ? 0 : options.selected,
            options.box_width, options.box_height);

#ifdef X11
    // Window will be shown on all desktops (and so hidden in windows list)
    gdk_x11_display_error_trap_push (gdk_display_get_default ());
    unsigned int desk = 0xFFFFFFFF; // -1
    XChangeProperty(x11_display (), myown_window, a_NET_WM_DESKTOP, XA_CARDINAL,
            32, PropModeReplace, (unsigned char *)&desk, 1);
    XSync (x11_display (), FALSE);
    gdk_x11_display_error_trap_pop_ignored (gdk_display_get_default ());
#endif

    main_loop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run (main_loop);
    g_main_loop_unref (main_loop);

#ifdef X11
    if (!options.read_stdin)
        XFree (wins);
#endif

    return 0;
}

// Geometry (including its virtual-screen origin) of the monitor containing
// (x, y), or the first monitor known to the display if none matches.
static GdkRectangle monitor_rect_at_point (gint x, gint y)
{
    GdkDisplay *display = gdk_display_get_default ();
    GListModel *monitors = gdk_display_get_monitors (display);
    guint n = g_list_model_get_n_items (monitors);

    GdkRectangle result = { 0, 0, 0, 0 };
    for (guint i = 0; i < n; i++) {
        GdkMonitor *monitor = g_list_model_get_item (monitors, i);
        GdkRectangle geom;
        gdk_monitor_get_geometry (monitor, &geom);
        if (i == 0)
            result = geom;
        if (x >= geom.x && x < geom.x + geom.width &&
                y >= geom.y && y < geom.y + geom.height) {
            result = geom;
            g_object_unref (monitor);
            break;
        }
        g_object_unref (monitor);
    }

    return result;
}

static void draw_grid (GtkFixed *where,
        GtkWidget **widgets, int rsize,
        int focus_on,
        int rwidth, int rheight)
{
    boxes_drawn = 0;
    int cur_x = options.center_x - rwidth/2;
    int cur_y = options.center_y - rheight/2;
    if (rsize) {
        int i = 0;
        int offset = 0;
        int max_offset = (width*2) / rwidth + (height*2) / rheight;
        int side = 0;

        while (i < rsize) {
            int j = 0;
            do {
                if (i == rsize)
                    break;
                if (cur_x >= 0 && cur_x+rwidth <= width && cur_y >= 0 && cur_y+rheight <= height) {
                    offset = 0;
                    if (gtk_widget_get_parent (widgets[i]))
                        gtk_fixed_move (GTK_FIXED (where), widgets[i], cur_x, cur_y);
                    else
                        gtk_fixed_put (GTK_FIXED (where), widgets[i], cur_x, cur_y);
                    gtk_widget_set_size_request (widgets[i], rwidth, rheight);
                    gtk_widget_set_visible (widgets[i], TRUE);
                    if (!options.screenshot) {
                        box_rects[i].x = cur_x;
                        box_rects[i].y = cur_y;
                        boxes_drawn++;
                    }
                    i++;
                } else {
                    offset++;
                }
                if (side) {
                    if (j % (side * 4) < side || j % (side * 4) >= side * 3)
                        cur_x += rwidth;
                    else
                        cur_x -= rwidth;
                    if (j % (side * 4) < side * 2)
                        cur_y += rheight;
                    else
                        cur_y -= rheight;
                }
                j++;
            } while (j < side * 4);
            if (offset >= max_offset)
                break;
            side++;
            cur_x = options.center_x - rwidth/2;
            cur_y -= rheight;
        }
        if (focus_on >= rsize)
            // If some window was killed and focus was on the last element
            focus_on = rsize-1;
        gtk_widget_grab_focus (widgets[focus_on]);
    }
    if (!options.screenshot)
        apply_shape_mask (boxes_drawn);
}

static void on_rect_click (GtkWidget *widget, gpointer data)
{
    GridWindowBox *box = GRIDSELECT_WINDOW_BOX (widget);

    if (!options.read_stdin) {
        gtk_widget_set_visible (window, FALSE);
        switch_to_window (gridselect_window_box_get_xwindow (box));
    } else {
        puts (gridselect_window_box_get_name (box));
    }

    if (options.persistent) {
        if (strlen (gridselect_search_box_get_text (GRIDSELECT_SEARCH_BOX (search)))) {
            gtk_widget_set_visible (search, FALSE);
            gridselect_search_box_set_text (GRIDSELECT_SEARCH_BOX (search), "\0");
        }
    } else {
        g_main_loop_quit (main_loop);
    }
}

static void update_box_list ()
{
    if (!options.read_stdin) {
        if (wsize) {
            for (int i = 0; i < wsize; i++) {
                gtk_fixed_remove (GTK_FIXED (layout), boxes[i]);
            }
            free (boxes);
#ifdef X11
            XFree (wins);
#endif
        }
        wins = sorted_windows_list (&myown_window, active_window, &wsize, options.only_current);
#ifdef X11
        if (wins) {
            // Get PropertyNotify events from each relevant window.
            for (int i = 0; i < wsize; i++) {
                XSelectInput (x11_display (),
                        wins[i],
                        PropertyChangeMask);
            }
        }
#endif
    }

    if (!options.screenshot && box_rects) {
        free (box_rects);
        box_rects = NULL;
    }

    if (wsize) {
        boxes = (GtkWidget **) malloc (wsize * sizeof (GtkWidget *));

        if (!options.screenshot) {
            box_rects = (rect *) calloc (wsize, sizeof (rect));
            for (int i = 0; i < wsize; i++) {
                box_rects[i].width = options.box_width;
                box_rects[i].height = options.box_height;
            }
        }

        GError *col_error;
        Entry entry;
        for (int i = 0; i < wsize; i++) {
            if (!options.read_stdin) {
                boxes[i] = gridselect_window_box_new_with_xwindow (wins[i]);
#ifdef X11
                gridselect_window_box_set_show_desktop (GRIDSELECT_WINDOW_BOX (boxes[i]), options.show_desktop);
#endif
                gridselect_window_box_set_show_titles (GRIDSELECT_WINDOW_BOX (boxes[i]), options.show_titles);
                if (options.show_icons)
                    gridselect_window_box_setup_icon_from_wm (GRIDSELECT_WINDOW_BOX(boxes[i]), options.icon_size, options.icon_size);
            } else {
                if(!options.format)
                    boxes[i] = gridselect_window_box_new_with_name (in_items[i]);
                else {
                    if(parse_format(&entry, in_items[i])){
                        boxes[i] = gridselect_window_box_new_with_name(entry.label);
                        if((entry.desktop)>=0) {//g_printerr("Custom background digits not implemented yet\n");
                            gridselect_window_box_set_desktop(GRIDSELECT_WINDOW_BOX(boxes[i]), entry.desktop-1);
                            gridselect_window_box_set_show_desktop (GRIDSELECT_WINDOW_BOX(boxes[i]), TRUE);
                        }
                        if(options.show_icons && (entry.iconpath)[0]!='*') {
                            if(strchr(entry.iconpath, '.')) {
                                gridselect_window_box_setup_icon_from_file(GRIDSELECT_WINDOW_BOX(boxes[i]), entry.iconpath,
                                        options.icon_size, options.icon_size);
                            } else {
                                gridselect_window_box_setup_icon_from_theme(GRIDSELECT_WINDOW_BOX(boxes[i]), entry.iconpath,
                                        options.icon_size, options.icon_size);
                            }
                        }
                        if(strlen(entry.opt_name)){
                            g_printerr("%s\n", entry.opt_name);
                            gridselect_window_box_set_opt_name(GRIDSELECT_WINDOW_BOX(boxes[i]), entry.opt_name);
                        }
                    } else {
                        boxes[i] = gridselect_window_box_new_with_name("Parse error");
                    }
                }
            }
            gridselect_box_set_font (GRIDSELECT_BOX (boxes [i]), options.font);
            gridselect_window_box_set_colorize (GRIDSELECT_WINDOW_BOX (boxes[i]), options.colorize);
            gridselect_window_box_set_color_offset (GRIDSELECT_WINDOW_BOX (boxes[i]), options.color_offset);
            if (options.colorize && options.color_file) {
                gchar *color = NULL;
                if (!options.read_stdin) {
                    const gchar *wm_class = gridselect_window_box_get_opt_name (GRIDSELECT_WINDOW_BOX (boxes[i]));
                    gchar *class1 = g_strdup (wm_class);
                    gchar *class2 = g_strdup (wm_class+strlen (class1)+1);
                    if (g_key_file_has_key (color_config, "colors", class1, &col_error))
                        color = g_key_file_get_string (color_config, "colors", class1, &col_error);
                    else if (g_key_file_has_key (color_config, "colors", class2, &col_error))
                        color = g_key_file_get_string (color_config, "colors", class2, &col_error);
                    g_free (class1);
                    g_free (class2);
                }

                if (!color && fallback_size)
                    color = g_strdup (fallback_colors [i % fallback_size]);

                if (color)
                    gridselect_window_box_set_color_from_string (GRIDSELECT_WINDOW_BOX (boxes[i]), color);

                g_free (color);
            }
            if(options.format) {
                if((entry.color)[0]=='#')
                    gridselect_window_box_set_color_from_string(GRIDSELECT_WINDOW_BOX(boxes[i]), entry.color);
            }
            g_signal_connect (G_OBJECT (boxes[i]), "clicked",
                    G_CALLBACK (on_rect_click), NULL);
        }
    }
}

static gboolean on_key_press (GtkEventControllerKey *controller, guint keyval, guint keycode,
        GdkModifierType state, gpointer data)
{
    key_pressed = TRUE;
    switch (keyval) {
        case GDK_KEY_Escape:
            if (strlen (gridselect_search_box_get_text (GRIDSELECT_SEARCH_BOX (search)))
                    || gtk_widget_get_visible (search)) {
                gtk_widget_set_visible (search, FALSE);
                gridselect_search_box_set_text (GRIDSELECT_SEARCH_BOX (search), "\0");
            } else {
                if (options.persistent)
                    gtk_widget_set_visible (window, FALSE);
                else
                    g_main_loop_quit (main_loop);
            }
            break;
        case GDK_KEY_Return:
            if(strlen (gridselect_search_box_get_text (GRIDSELECT_SEARCH_BOX (search))) && !filtered_size &&
                    options.read_stdin && options.permissive) {
                puts (gridselect_search_box_get_text (GRIDSELECT_SEARCH_BOX (search)));
                g_main_loop_quit (main_loop);
            }
            break;
        case GDK_KEY_Left:
        case GDK_KEY_Up:
        case GDK_KEY_Right:
        case GDK_KEY_Down:
            break;
        case GDK_KEY_Tab:
            tab_event(FALSE);
            return TRUE;
        case GDK_KEY_ISO_Left_Tab:
            tab_event(TRUE);
            return TRUE;
            break;
        case GDK_KEY_End:
            if (options.permissive) {
                GridWindowBox* box = GRIDSELECT_WINDOW_BOX (gtk_window_get_focus (GTK_WINDOW (window)));
                gridselect_search_box_set_text (GRIDSELECT_SEARCH_BOX (search), gridselect_window_box_get_name (box));
                gtk_widget_set_visible (search, TRUE);
            }
            break;
        case GDK_KEY_BackSpace:
            gridselect_search_box_remove_symbols (GRIDSELECT_SEARCH_BOX (search), 1);
            if (!strlen (gridselect_search_box_get_text (GRIDSELECT_SEARCH_BOX (search))))
                if (!options.vim_mode)
                    gtk_widget_set_visible (search, FALSE);
            break;
        default:
            {
                // Ignore Ctrl key.
                if (state & GDK_CONTROL_MASK) {
                    if (!options.vim_mode) {
                        switch (keyval) {
                            case GDK_KEY_n:
                                gtk_widget_child_focus (layout, GTK_DIR_DOWN);
                                break;
                            case GDK_KEY_p:
                                gtk_widget_child_focus (layout, GTK_DIR_UP);
                                break;
                            case GDK_KEY_f:
                                gtk_widget_child_focus (layout, GTK_DIR_RIGHT);
                                break;
                            case GDK_KEY_b:
                                gtk_widget_child_focus (layout, GTK_DIR_LEFT);
                                break;
                            case GDK_KEY_m:
                                if(strlen (gridselect_search_box_get_text (GRIDSELECT_SEARCH_BOX (search))) && !filtered_size &&
                                        options.read_stdin && options.permissive) {
                                    puts (gridselect_search_box_get_text (GRIDSELECT_SEARCH_BOX (search)));
                                    g_main_loop_quit (main_loop);
                                } else {
                                    g_signal_emit_by_name (gtk_window_get_focus (GTK_WINDOW (window)), "clicked", NULL);
                                }
                                break;
                            case GDK_KEY_h:
                                gridselect_search_box_remove_symbols (GRIDSELECT_SEARCH_BOX (search), 1);
                                if (!strlen (gridselect_search_box_get_text (GRIDSELECT_SEARCH_BOX (search))))
                                    gtk_widget_set_visible (search, FALSE);
                                break;
                            case GDK_KEY_w:
                                gridselect_search_box_kill_word (GRIDSELECT_SEARCH_BOX (search));
                                if (!strlen (gridselect_search_box_get_text (GRIDSELECT_SEARCH_BOX (search))))
                                    gtk_widget_set_visible (search, FALSE);
                                break;
                            case GDK_KEY_g:
                                if (strlen (gridselect_search_box_get_text (GRIDSELECT_SEARCH_BOX (search)))
                                        || gtk_widget_get_visible (search)) {
                                    gtk_widget_set_visible (search, FALSE);
                                    gridselect_search_box_set_text (GRIDSELECT_SEARCH_BOX (search), "\0");
                                } else {
                                    g_main_loop_quit (main_loop);
                                }
                                break;
                        }
                    }
                    return FALSE;
                }

                if (options.vim_mode && !gtk_widget_get_visible (search)) {
                    switch (keyval) {
                        case GDK_KEY_h:
                            gtk_widget_child_focus (layout, GTK_DIR_LEFT);
                            break;
                        case GDK_KEY_j:
                            gtk_widget_child_focus (layout, GTK_DIR_DOWN);
                            break;
                        case GDK_KEY_k:
                            gtk_widget_child_focus (layout, GTK_DIR_UP);
                            break;
                        case GDK_KEY_l:
                            gtk_widget_child_focus (layout, GTK_DIR_RIGHT);
                            break;
                        case GDK_KEY_slash:
                            gtk_widget_set_visible (search, TRUE);
                            g_signal_emit_by_name (G_OBJECT (search), "changed", NULL);
                            break;
                    }
                    return TRUE;
                }

                // GTK4 key events no longer carry a composed input string
                // (GdkEventKey::string is gone); map the keyval to its
                // Unicode codepoint ourselves.
                gunichar ch = gdk_keyval_to_unicode (keyval);
                if (ch && !g_unichar_iscntrl (ch)) {
                    gchar key[8] = { 0 };
                    gint len = g_unichar_to_utf8 (ch, key);
                    key[len] = '\0';
                    gridselect_search_box_append_text (GRIDSELECT_SEARCH_BOX (search), key);
                    int text_length = strlen (gridselect_search_box_get_text (GRIDSELECT_SEARCH_BOX (search)));
                    if (text_length)
                        gtk_widget_set_visible (search, TRUE);
                    return TRUE;
                }
            }
    }
    return FALSE;
}

#ifdef X11
static gboolean on_xevent (GdkDisplay *display, gpointer xevent_ptr, gpointer data)
{
    XEvent *xevent = (XEvent *) xevent_ptr;
    if (!xevent)
        return FALSE;

    if (xevent->type == PropertyNotify) {
        Atom atom = xevent->xproperty.atom;
        Window win = xevent->xproperty.window;
        if (win == x11_root ()) {
            if (atom == a_NET_CLIENT_LIST) {
                int focus_on = 0;
                if (filtered_size && filtered_boxes) {
                    for (int i = 0; i < filtered_size; i++)
                        if (gtk_widget_is_focus (filtered_boxes [i])) {
                            focus_on = i;
                            break;
                        }
                } else {
                    for (int i = 0; i < wsize; i++)
                        if (gtk_widget_is_focus (boxes [i])) {
                            focus_on = i;
                            break;
                        }
                }
                update_box_list ();
                if (strlen (gridselect_search_box_get_text (GRIDSELECT_SEARCH_BOX (search)))) {
                    refilter (GRIDSELECT_SEARCH_BOX (search), NULL);
                    draw_grid (GTK_FIXED (layout), filtered_boxes, filtered_size, focus_on,
                            options.box_width, options.box_height);
                } else {
                    draw_grid (GTK_FIXED (layout), boxes, wsize, focus_on,
                            options.box_width, options.box_height);
                }
            }
        } else {
            if (atom == a_WM_NAME || atom == a_NET_WM_NAME || atom == a_NET_WM_VISIBLE_NAME) {
                // Search for appropriate widget to change label.
                for (int i = 0; i < wsize; i++)
                    if (wins [i] == win) {
                        gridselect_window_box_update_xwindow_name (GRIDSELECT_WINDOW_BOX (boxes[i]));
                        break;
                    }
            }
            if (atom == a_NET_WM_ICON && options.show_icons) {
                // Search for appropriate widget to update icon.
                for (int i = 0; i < wsize; i++)
                    if (wins [i] == win) {
                        gridselect_window_box_setup_icon_from_wm (GRIDSELECT_WINDOW_BOX (boxes[i]), options.icon_size, options.icon_size);
                        break;
                    }
            }
        }
    }

    return FALSE;
}
#endif

static gboolean search_by_letters (const gchar *source, gint s_len, const gchar *letters, gint l_len)
{
    gboolean found = FALSE;
    const gchar *p1 = letters;
    const gchar *p2 = source;
    while (p1 < letters + l_len) {
        gunichar c1 = g_utf8_get_char (p1);
        found = FALSE;
        while (p2 < source + s_len) {
            gunichar c2 = g_utf8_get_char (p2);
            if (c1 == c2) {
                found = TRUE;
                p2 = g_utf8_find_next_char (p2, NULL);
                break;
            }
            p2 = g_utf8_find_next_char (p2, NULL);
        }
        if (!found)
            break;
        p1 = g_utf8_find_next_char (p1, NULL);
    }

    return found;
}

static void refilter (GridSearchBox *search_box, gpointer data)
{
    if (filtered_size) {
        free (filtered_boxes);
    }
    filtered_size = 0;

    for (int i = 0; i < wsize; i++)
        gtk_widget_set_visible (boxes [i], FALSE);

    gchar *search_for = g_utf8_casefold (gridselect_search_box_get_text (search_box), -1);
    int s_size = strlen (search_for);
    if (s_size) {
        filtered_boxes = (GtkWidget **) malloc (wsize * sizeof (GtkWidget *));

        GtkWidget **priority1 = (GtkWidget **) malloc (wsize * sizeof (GtkWidget *));
        GtkWidget **priority2 = (GtkWidget **) malloc (wsize * sizeof (GtkWidget *));
        GtkWidget **priority3 = (GtkWidget **) malloc (wsize * sizeof (GtkWidget *));
        gint p1size = 0;
        gint p2size = 0;
        gint p3size = 0;

        for (int i = 0; i < wsize; i++) {
            gchar *wname_cmp = NULL;
            gchar *opt_name1_cmp = NULL;
            gchar *opt_name2_cmp = NULL;
            int wn_size = 0;
            int op1_size = 0;
            int op2_size = 0;

            wname_cmp = g_utf8_casefold (gridselect_window_box_get_name (GRIDSELECT_WINDOW_BOX (boxes[i])), -1);
            wn_size = strlen (wname_cmp);
            const gchar *opt_name = gridselect_window_box_get_opt_name (GRIDSELECT_WINDOW_BOX (boxes[i]));
            if (opt_name) {
                opt_name1_cmp = g_utf8_casefold (opt_name, -1);
                op1_size = strlen (opt_name1_cmp);
                if (!options.read_stdin) {
                    opt_name2_cmp = g_utf8_casefold (opt_name+op1_size+1, -1);
                    op2_size = strlen (opt_name2_cmp);
                }
            }
            gboolean found = FALSE;
            if (g_str_has_prefix (wname_cmp, search_for)) {
                found = TRUE;
                priority1 [p1size++] = boxes [i];
            }
            if (!found && ((g_strstr_len (wname_cmp, wn_size, search_for) != NULL) ||
                        (op1_size && g_str_has_prefix (opt_name1_cmp, search_for)) ||
                        (op2_size && g_str_has_prefix (opt_name2_cmp, search_for)))) {
                found = TRUE;
                priority2 [p2size++] = boxes [i];
            }
            if (!found && ((search_by_letters (wname_cmp, wn_size, search_for, s_size)) ||
                        (op1_size && g_strstr_len (opt_name1_cmp, op1_size, search_for) != NULL) ||
                        (op2_size && g_strstr_len (opt_name2_cmp, op2_size, search_for) != NULL))) {
                found = TRUE;
                priority3 [p3size++] = boxes [i];
            }
            g_free (wname_cmp);
            g_free (opt_name1_cmp);
            g_free (opt_name2_cmp);
            if (found)
                filtered_size++;
        }

        for (int i = 0; i < p1size; i++)
            filtered_boxes [i] = priority1 [i];
        for (int i = 0; i < p2size; i++)
            filtered_boxes [p1size+i] = priority2 [i];
        for (int i = 0; i < p3size; i++)
            filtered_boxes [p1size+p2size+i] = priority3 [i];

        free (priority1);
        free (priority2);
        free (priority3);

        draw_grid (GTK_FIXED (layout), filtered_boxes, filtered_size, 0,
                options.box_width, options.box_height);
    } else {
        draw_grid (GTK_FIXED (layout), boxes, wsize, 0,
                options.box_width, options.box_height);
    }

    if (gtk_widget_get_visible (search)) {
        // Force the search entry back above the boxes (GtkFixed stacks
        // children in insertion order; there is no equivalent of GTK2's
        // hide+show-to-restack trick, so move it explicitly instead).
        gtk_widget_insert_before (search, layout, NULL);
    }

    g_free (search_for);
}

#ifdef X11
static void apply_shape_mask (guint size)
{
    gboolean show_search = search &&
        (strlen (gridselect_search_box_get_text (GRIDSELECT_SEARCH_BOX (search))) ||
         (options.vim_mode && gtk_widget_get_visible (search)));

    guint count = size + (show_search ? 1 : 0);
    XRectangle *rects = g_new0 (XRectangle, count ? count : 1);
    guint n = 0;

    for (guint i = 0; i < size; i++) {
        rects[n].x = box_rects[i].x;
        rects[n].y = box_rects[i].y;
        rects[n].width = box_rects[i].width;
        rects[n].height = box_rects[i].height;
        n++;
    }

    if (show_search) {
        rects[n].x = search_x;
        rects[n].y = search_y;
        rects[n].width = search_w;
        rects[n].height = search_h;
        n++;
    }

    gdk_x11_display_error_trap_push (gdk_display_get_default ());
    XShapeCombineRectangles (x11_display (), myown_window, ShapeBounding, 0, 0,
            rects, n, ShapeSet, 0);
    XSync (x11_display (), FALSE);
    gdk_x11_display_error_trap_pop_ignored (gdk_display_get_default ());
    g_free (rects);
}
#else
static void apply_shape_mask (guint size)
{
}
#endif

static void read_stdin ()
{
    char buffer [BUFSIZ];
    char *p;
    int current_size = 1;
    in_items = calloc (1, sizeof (char *));
    wsize = 0;
    while (fgets (buffer, BUFSIZ, stdin)) {
        if (wsize == current_size) {
            current_size *= 2;
            in_items = realloc (in_items, current_size * sizeof (char *));
        }
        if ((p = strchr (buffer, '\n')))
            *p = '\0';
        in_items [wsize] = g_strdup (buffer);
        wsize++;
    }
}

#ifdef X11
// GTK4 removed GdkScreen/GdkDrawable (and with them gdk_pixbuf_get_from_drawable),
// so grab the root window's pixels directly through Xlib instead.
static int mask_shift (unsigned long mask)
{
    int shift = 0;
    while (mask && !(mask & 1)) {
        mask >>= 1;
        shift++;
    }
    return shift;
}

static int mask_bits (unsigned long mask)
{
    int bits = 0;
    while (mask) {
        bits += mask & 1;
        mask >>= 1;
    }
    return bits;
}

static GdkPixbuf* get_screenshot ()
{
    Display *dpy = x11_display ();
    Window root = x11_root ();

    XWindowAttributes attrs;
    XGetWindowAttributes (dpy, root, &attrs);
    gint swidth = attrs.width;
    gint sheight = attrs.height;

    guint m_offset_x = 0, m_offset_y = 0;
    GListModel *monitors = gdk_display_get_monitors (gdk_display_get_default ());
    if (g_list_model_get_n_items (monitors) > 1) {
        gint px = 0, py = 0;
        Window root_ret, child_ret;
        int win_x, win_y;
        unsigned int mask_ret;
        XQueryPointer (dpy, root, &root_ret, &child_ret, &px, &py, &win_x, &win_y, &mask_ret);
        GdkRectangle mon_rect = monitor_rect_at_point (px, py);
        m_offset_x = mon_rect.x;
        m_offset_y = mon_rect.y;
    }

    if (swidth <= m_offset_x)
        m_offset_x = 0;
    if (sheight <= m_offset_y)
        m_offset_y = 0;

    gint cap_x = options.screenshot_offset_x + m_offset_x;
    gint cap_y = options.screenshot_offset_y + m_offset_y;
    gint cap_w = swidth - cap_x;
    gint cap_h = sheight - cap_y;
    if (cap_w <= 0 || cap_h <= 0)
        return NULL;

    // XGetImage can legitimately fail here (e.g. unsupported on some
    // compositor/XWayland setups); GDK's default X error handler
    // terminates the process on any untrapped protocol error, so this
    // has to be trapped and turned into a graceful NULL instead.
    gdk_x11_display_error_trap_push (gdk_display_get_default ());
    XImage *image = XGetImage (dpy, root, cap_x, cap_y, cap_w, cap_h, AllPlanes, ZPixmap);
    XSync (dpy, FALSE);
    if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) || !image) {
        if (image)
            XDestroyImage (image);
        return NULL;
    }

    GdkPixbuf *pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, cap_w, cap_h);
    guchar *pixels = gdk_pixbuf_get_pixels (pixbuf);
    int rowstride = gdk_pixbuf_get_rowstride (pixbuf);

    int rshift = mask_shift (image->red_mask), rbits = mask_bits (image->red_mask);
    int gshift = mask_shift (image->green_mask), gbits = mask_bits (image->green_mask);
    int bshift = mask_shift (image->blue_mask), bbits = mask_bits (image->blue_mask);
    unsigned long rmax = (1UL << rbits) - 1;
    unsigned long gmax = (1UL << gbits) - 1;
    unsigned long bmax = (1UL << bbits) - 1;

    for (int yy = 0; yy < cap_h; yy++) {
        guchar *row = pixels + yy * rowstride;
        for (int xx = 0; xx < cap_w; xx++) {
            unsigned long p = XGetPixel (image, xx, yy);
            guchar *dst = row + xx * 3;
            dst[0] = (guchar) (((p & image->red_mask) >> rshift) * 255 / (rmax ? rmax : 1));
            dst[1] = (guchar) (((p & image->green_mask) >> gshift) * 255 / (gmax ? gmax : 1));
            dst[2] = (guchar) (((p & image->blue_mask) >> bshift) * 255 / (bmax ? bmax : 1));
        }
    }

    XDestroyImage (image);
    return pixbuf;
}
#else
static GdkPixbuf* get_screenshot ()
{
    return NULL;
}
#endif

static void read_config ()
{
    // Set default options.
    options.vim_mode = FALSE;
    options.box_width = 200;
    options.box_height = 40;
    options.colorize = TRUE;
    options.color_offset = 0;
    options.show_icons = TRUE;
    options.show_desktop = TRUE;
    options.show_titles = TRUE;
    options.icon_size = 16;
    options.font = g_strdup ("Sans 10");
    options.read_stdin = FALSE;
    options.screenshot = TRUE;
    options.screenshot_offset_x = 0;
    options.screenshot_offset_y = 0;

    gchar *filename = g_strjoin ("/", g_get_user_config_dir (), "xwingridselect/config", NULL);

    GError *error = NULL;
    GKeyFile *config = g_key_file_new ();

    if (!g_key_file_load_from_file (config, filename, 0, &error)) {
        write_default_config ();
        return;
    }

    const gchar *group = "default";
    if (g_key_file_has_group (config, group)) {
        if (g_key_file_has_key (config, group, "vim_mode", &error))
            options.vim_mode = g_key_file_get_boolean (config, group, "vim_mode", &error);
        if (g_key_file_has_key (config, group, "box_width", &error))
            options.box_width = g_key_file_get_integer (config, group, "box_width", &error);
        if (g_key_file_has_key (config, group, "box_height", &error))
            options.box_height = g_key_file_get_integer (config, group, "box_height", &error);
        if (g_key_file_has_key (config, group, "colorize", &error))
            options.colorize = g_key_file_get_boolean (config, group, "colorize", &error);
        if (g_key_file_has_key (config, group, "color_offset", &error))
            options.color_offset = g_key_file_get_integer (config, group, "color_offset", &error);
        if (g_key_file_has_key (config, group, "show_icons", &error))
            options.show_icons = g_key_file_get_boolean (config, group, "show_icons", &error);
        if (g_key_file_has_key (config, group, "show_desktop", &error))
            options.show_desktop = g_key_file_get_boolean (config, group, "show_desktop", &error);
        if (g_key_file_has_key (config, group, "show_titles", &error))
            options.show_titles = g_key_file_get_boolean (config, group, "show_titles", &error);
        if (g_key_file_has_key (config, group, "icon_size", &error))
            options.icon_size = g_key_file_get_integer (config, group, "icon_size", &error);
        if (g_key_file_has_key (config, group, "font", &error))
            options.font = g_key_file_get_string (config, group, "font", &error);
        if (g_key_file_has_key (config, group, "screenshot", &error))
            options.screenshot = g_key_file_get_boolean (config, group, "screenshot", &error);
        if (g_key_file_has_key (config, group, "screenshot_offset_x", &error))
            options.screenshot_offset_x = g_key_file_get_integer (config, group, "screenshot_offset_x", &error);
        if (g_key_file_has_key (config, group, "screenshot_offset_y", &error))
            options.screenshot_offset_y = g_key_file_get_integer (config, group, "screenshot_offset_y", &error);
        if (g_key_file_has_key (config, group, "at_pointer", &error))
            options.at_pointer = g_key_file_get_boolean (config, group, "at_pointer", &error);
        if (g_key_file_has_key (config, group, "color_file", &error))
            options.color_file = g_key_file_get_string (config, group, "color_file", &error);
    }

    g_key_file_free (config);
}

static void write_default_config ()
{
    gchar *confdir = g_strjoin ("/", g_get_user_config_dir (), "xwingridselect", NULL);
    gchar *filename = g_strjoin ("/", g_get_user_config_dir (), "xwingridselect/config", NULL);

    if (g_mkdir_with_parents (confdir, 0755) != -1) {
        FILE *config;
        if ((config = fopen (filename, "w")) != NULL) {
            fprintf (config, "[default]\n");
            fprintf (config, "vim_mode = %s\n", (options.vim_mode) ? "true" : "false");
            fprintf (config, "box_width = %d\n", options.box_width);
            fprintf (config, "box_height = %d\n", options.box_height);
            fprintf (config, "colorize = %s\n", (options.colorize) ? "true" : "false");
            fprintf (config, "color_offset = %d\n", options.color_offset);
            fprintf (config, "show_icons = %s\n", (options.show_icons) ? "true" : "false");
            fprintf (config, "show_desktop = %s\n", (options.show_desktop) ? "true" : "false");
            fprintf (config, "show_titles = %s\n", (options.show_titles) ? "true" : "false");
            fprintf (config, "icon_size = %d\n", options.icon_size);
            fprintf (config, "font = %s\n", options.font);
            fprintf (config, "screenshot = %s\n", (options.screenshot) ? "true" : "false");
            fprintf (config, "screenshot_offset_x = %d\n", options.screenshot_offset_x);
            fprintf (config, "screenshot_offset_y = %d\n", options.screenshot_offset_y);
            fprintf (config, "at_pointer = %s\n", (options.at_pointer) ? "true" : "false");
            fprintf (config, "# color_file = /path/to/file\n");
            fclose (config);
        }
    }

    g_printerr ("created new config in %s\n", confdir);
}

static void on_focus_change (GObject *object, GParamSpec *pspec, gpointer data)
{
    if (!gtk_window_is_active (GTK_WINDOW (window))) { /* focus out */
        /* workaround for awesome wm and its unexpected focus changes */
        if (options.screenshot && !key_pressed)
            return;
        if (options.persistent)
            gtk_widget_set_visible (window, FALSE);
        else
            g_main_loop_quit (main_loop);
    }
}

static void read_colors ()
{

    GError *error = NULL;
    color_config = g_key_file_new ();

    if (!g_key_file_load_from_file (color_config, options.color_file, 0, &error)) {
        g_printerr ("%s\n", error->message);
        return;
    }

    fallback_size = 0;

    const gchar *group = "colors";
    if (g_key_file_has_group (color_config, group)) {
        if (g_key_file_has_key (color_config, group, "fallback", &error))
            fallback_colors = g_key_file_get_string_list (color_config, group, "fallback", &fallback_size, &error);
    }
}
static gboolean parse_format (Entry* entry, char *data)
{
    gchar **opts = g_strsplit(data, ",", 5);
    if(!opts[1] + !opts[2] + !opts[3] + !opts[4]) { //What did i just write?
        g_printerr("Format error\n");
        return FALSE;
    }
    g_strchug(opts[0]);
    if(g_ascii_isdigit(opts[0][0])) {
        if(sscanf(opts[0], "%d", &(entry->desktop))==EOF) return FALSE;
    } else
        entry->desktop = -1;
    entry->color = opts[1];
    entry->iconpath = opts[2];
    entry->label = opts[3];
    entry->opt_name = opts[4];
    //g_strfreev(opts);
    g_strchug(entry->color);
    g_strchug(entry->iconpath);
    g_strchug(entry->label);
    g_strchug(entry->opt_name);
    return TRUE;
}

void tab_event (gboolean shift) //FIXME: put prototype for this function
    //in some header file
{
    gboolean is_visible = FALSE;
    g_object_get (window, "visible", &is_visible, NULL);
    if(is_visible) {
        GtkWidget **bs;
        guint bsize = 0;
        if(gtk_widget_get_visible (search)) {
            bs = filtered_boxes;
            bsize = filtered_size;
        } else {
            bs = boxes;
            bsize = wsize;
        }
        if (bsize == 0) return; // nothing to switch between
        // Calculate current box by straightforward pointer comparison
        guint current_box = 0;
        GridWindowBox* box = GRIDSELECT_WINDOW_BOX (gtk_window_get_focus (GTK_WINDOW (window)));
        for (guint i = 0; i < bsize; i++)
            if (GRIDSELECT_WINDOW_BOX(box) == GRIDSELECT_WINDOW_BOX(bs[i])) {
                current_box = i;
                break;
            }
        if(!shift) {
            current_box < bsize-1 ? current_box++ : (current_box = 0);
        } else {
            current_box > 0 ? current_box-- : (current_box = bsize-1);
        }
        gtk_widget_grab_focus (bs[current_box]);
    } else {
        update_box_list();
        draw_grid (GTK_FIXED (layout), boxes, wsize, 0,
                options.box_width, options.box_height);
        gtk_window_present (GTK_WINDOW (window));
    }
}
