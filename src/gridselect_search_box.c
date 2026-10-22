/* Copyright (c) 2020, TheCynicalTeam @ Github
 * gridselect_seach_box.c - search box widget.
 */

#include "gridselect_search_box.h"

#define BOX_DEFAULT_WIDTH 200

enum {
  CHANGED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_TEXT,
  N_PROPERTIES
};

static GObject*	gridselect_search_box_constructor (GType gtype,
					       guint n_properties,
					       GObjectConstructParam *properties);
static void gridselect_search_box_dispose (GObject *gobject);

static void gridselect_search_box_set_property (GObject *gobject,
					    guint prop_id,
					    const GValue *value,
					    GParamSpec *pspec);
static void gridselect_search_box_get_property (GObject *gobject,
					    guint prop_id,
					    GValue *value,
					    GParamSpec *pspec);

static void gridselect_search_box_snapshot (GtkWidget *widget, GtkSnapshot *snapshot);
static void gridselect_search_box_paint (GridSearchBox *box, cairo_t *cr, gint width, gint height);
static void gridselect_search_box_measure (GtkWidget *widget, GtkOrientation orientation, int for_size,
					    int *minimum, int *natural, int *minimum_baseline, int *natural_baseline);

static guint search_box_signals[LAST_SIGNAL] = { 0 };
static GParamSpec *obj_properties[N_PROPERTIES] = { NULL };

G_DEFINE_TYPE (GridSearchBox, gridselect_search_box, GRIDSELECT_TYPE_BOX);

static void
gridselect_search_box_class_init (GridSearchBoxClass *klass)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (klass);
  widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->constructor = gridselect_search_box_constructor;
  gobject_class->dispose = gridselect_search_box_dispose;
  gobject_class->set_property = gridselect_search_box_set_property;
  gobject_class->get_property = gridselect_search_box_get_property;

  widget_class->snapshot = gridselect_search_box_snapshot;
  widget_class->measure = gridselect_search_box_measure;

  obj_properties[PROP_TEXT] =
    g_param_spec_string ("text",
			 "Text in the search entry",
			 "Text, displayed in search entry",
			 NULL,
			 G_PARAM_CONSTRUCT | G_PARAM_READWRITE);

  g_object_class_install_property (gobject_class, PROP_TEXT, obj_properties [PROP_TEXT]);

  search_box_signals [CHANGED] =
    g_signal_new ("changed",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GridSearchBoxClass, changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

}

static void
gridselect_search_box_init (GridSearchBox *box)
{
  gtk_widget_set_focusable (GTK_WIDGET (box), FALSE);
  gtk_widget_set_receives_default (GTK_WIDGET (box), FALSE);
}

static GObject*	gridselect_search_box_constructor (GType gtype,
					guint n_properties,
					GObjectConstructParam *properties)
{
  GObject *obj;
  GridSearchBox *box;

  obj = G_OBJECT_CLASS (gridselect_search_box_parent_class)->constructor (gtype, n_properties, properties);

  box = GRIDSELECT_SEARCH_BOX (obj);

  GRIDSELECT_BOX (box)->name = g_strdup("\0");
  box->cursor = g_strdup ("|");

  return obj;
}

GtkWidget* gridselect_search_box_new (void)
{
  return g_object_new (GRIDSELECT_TYPE_SEARCH_BOX, NULL);
}

static void
gridselect_search_box_dispose (GObject *gobject)
{
  GridSearchBox *box = GRIDSELECT_SEARCH_BOX (gobject);

  if (box->cursor)
    g_free (box->cursor);
  box->cursor = NULL;

  G_OBJECT_CLASS (gridselect_search_box_parent_class)->dispose (gobject);
}

static void gridselect_search_box_set_property (GObject *gobject,
					    guint prop_id,
					    const GValue *value,
					    GParamSpec *pspec)
{
  GridSearchBox *box = GRIDSELECT_SEARCH_BOX (gobject);

  switch (prop_id) {
  case PROP_TEXT:
    gridselect_search_box_set_text (box, g_value_get_string (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    break;
  }
}

static void gridselect_search_box_get_property (GObject *gobject,
					    guint prop_id,
					    GValue *value,
					    GParamSpec *pspec)
{
  GridSearchBox *box = GRIDSELECT_SEARCH_BOX (gobject);

  switch (prop_id) {
  case PROP_TEXT:
    g_value_set_string (value, GRIDSELECT_BOX(box)->name);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    break;
  }
}

static void
gridselect_search_box_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
  g_return_if_fail (GRIDSELECT_IS_SEARCH_BOX (widget));

  int width = gtk_widget_get_width (widget);
  int height = gtk_widget_get_height (widget);

  cairo_t *cr = gtk_snapshot_append_cairo (snapshot, &GRAPHENE_RECT_INIT (0, 0, width, height));
  gridselect_search_box_paint (GRIDSELECT_SEARCH_BOX (widget), cr, width, height);
  cairo_destroy (cr);
}

static void
gridselect_search_box_paint (GridSearchBox *box, cairo_t *cr, gint width, gint height)
{
  cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
  cairo_rectangle (cr, 0, 0, width, height);
  cairo_fill (cr);

  PangoLayout *pl;
  PangoFontDescription *pfd;
  pl = pango_cairo_create_layout (cr);
  pango_layout_set_text (pl, GRIDSELECT_BOX (box)->name, -1);
  pfd = pango_font_description_from_string (GRIDSELECT_BOX (box)->font);
  pango_layout_set_font_description (pl, pfd);
  pango_font_description_free (pfd);

  int pwidth, pheight;
  pango_layout_get_pixel_size (pl, &pwidth, &pheight);

  cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 1.0);

  if ((width-pwidth) < 10)
    cairo_move_to (cr, width-pwidth-5, (height-pheight)/2);
  else
    cairo_move_to (cr, 5, (height-pheight)/2);

  pango_cairo_show_layout (cr, pl);
  g_object_unref (pl);

  if ((width-pwidth) < 10)
    cairo_rectangle (cr, width-4, 5, 2, height-10);
  else
    cairo_rectangle (cr, pwidth+6, 5, 2, height-10);
  cairo_fill (cr);

  gridselect_box_paint (GRIDSELECT_BOX (box), cr, width, height);
}

static void gridselect_search_box_measure (GtkWidget *widget, GtkOrientation orientation, int for_size,
					    int *minimum, int *natural, int *minimum_baseline, int *natural_baseline)
{
  if (minimum_baseline) *minimum_baseline = -1;
  if (natural_baseline) *natural_baseline = -1;

  if (orientation == GTK_ORIENTATION_HORIZONTAL) {
    if (minimum) *minimum = BOX_DEFAULT_WIDTH;
    if (natural) *natural = BOX_DEFAULT_WIDTH;
    return;
  }

  PangoLayout *pl = gtk_widget_create_pango_layout (widget, "|");
  PangoFontDescription *pfd = pango_font_description_from_string (GRIDSELECT_BOX (widget)->font);
  pango_layout_set_font_description (pl, pfd);
  pango_font_description_free (pfd);

  int pwidth, pheight;
  pango_layout_get_pixel_size (pl, &pwidth, &pheight);
  g_object_unref (pl);

  if (minimum) *minimum = pheight + 10;
  if (natural) *natural = pheight + 10;
}

void
gridselect_search_box_set_text (GridSearchBox *box, const gchar *text)
{
  gridselect_box_set_name (GRIDSELECT_BOX (box), text);
  g_signal_emit (box, search_box_signals [CHANGED], 0);
}

const gchar *
gridselect_search_box_get_text (GridSearchBox *box)
{
  g_return_val_if_fail (GRIDSELECT_IS_SEARCH_BOX (box), NULL);

  return GRIDSELECT_BOX (box)->name;
}

void gridselect_search_box_append_text (GridSearchBox *box, const gchar *text)
{
  gchar *new_text = g_strjoin (NULL, gridselect_search_box_get_text (box), text, NULL);
  gridselect_search_box_set_text (box, new_text);
  g_free (new_text);
}

void gridselect_search_box_remove_symbols (GridSearchBox *box, guint size)
{
  gchar *text = g_strdup (gridselect_search_box_get_text (box));
  if (strlen (text)) {
    gchar *p = text + strlen (text);
    for (int i = 0; i < size; i++) {
      p = g_utf8_find_prev_char (text, p);
      *p = '\0';
    }
    gridselect_search_box_set_text (box, text);
  }
  g_free (text);
}

void gridselect_search_box_kill_word (GridSearchBox *box)
{
  const gchar *text = gridselect_search_box_get_text (box);
  gint size = strlen (text);
  if (size) {
    const gchar *p = text + size;
    int len = 0;
    while (p > text) {
      p = g_utf8_find_prev_char (text, p);
      gunichar c = g_utf8_get_char (p);
      if (g_unichar_isspace (c) && len > 0)
	break;

      len++;
    }
    gridselect_search_box_remove_symbols (box, len);
  }
}
