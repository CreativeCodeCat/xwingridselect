/* Copyright (c) 2020, TheCynicalTeam @ Github
 * gridselect_box.c - common stuff for window_box and search_box.
 */

#include "gridselect_box.h"

#define BOX_DEFAULT_WIDTH 200
#define BOX_DEFAULT_HEIGHT 40

enum {
  CLICKED,
  LAST_SIGNAL
};

static GObject *gridselect_box_constructor (GType gtype,
					guint n_properties,
					GObjectConstructParam *properties);
static void gridselect_box_dispose (GObject *gobject);
static void gridselect_box_measure (GtkWidget *widget, GtkOrientation orientation, int for_size,
				     int *minimum, int *natural, int *minimum_baseline, int *natural_baseline);

static void gridselect_box_clicked (GridBox *box);

static guint box_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GridBox, gridselect_box, GTK_TYPE_WIDGET);

static void on_box_pressed (GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y, gpointer data)
{
  GridBox *box = GRIDSELECT_BOX (data);

  if (gtk_widget_get_focusable (GTK_WIDGET (box))) {
    box->box_down = TRUE;
    gtk_widget_grab_focus (GTK_WIDGET (box));
  }
}

static void on_box_released (GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y, gpointer data)
{
  GridBox *box = GRIDSELECT_BOX (data);

  if (gtk_widget_get_focusable (GTK_WIDGET (box)) && box->on_box && box->box_down)
    gridselect_box_clicked (box);
  box->box_down = FALSE;
}

static gboolean on_box_key_pressed (GtkEventControllerKey *controller, guint keyval, guint keycode,
				     GdkModifierType state, gpointer data)
{
  GridBox *box = GRIDSELECT_BOX (data);

  if ((keyval == GDK_KEY_Return ||
       keyval == GDK_KEY_KP_Enter ||
       keyval == GDK_KEY_ISO_Enter) &&
      gtk_widget_get_focusable (GTK_WIDGET (box)))
    box->box_down = TRUE;

  return FALSE;
}

static gboolean on_box_key_released (GtkEventControllerKey *controller, guint keyval, guint keycode,
				      GdkModifierType state, gpointer data)
{
  GridBox *box = GRIDSELECT_BOX (data);

  if ((keyval == GDK_KEY_Return ||
       keyval == GDK_KEY_KP_Enter ||
       keyval == GDK_KEY_ISO_Enter) &&
      box->box_down &&
      gtk_widget_get_focusable (GTK_WIDGET (box))) {
    gridselect_box_clicked (box);
    box->box_down = FALSE;
  }

  return FALSE;
}

static void on_box_enter (GtkEventControllerMotion *controller, gdouble x, gdouble y, gpointer data)
{
  GridBox *box = GRIDSELECT_BOX (data);

  box->on_box = TRUE;
  gtk_widget_queue_draw (GTK_WIDGET (box));
}

static void on_box_leave (GtkEventControllerMotion *controller, gpointer data)
{
  GridBox *box = GRIDSELECT_BOX (data);

  box->on_box = FALSE;
  gtk_widget_queue_draw (GTK_WIDGET (box));
}

static void
gridselect_box_class_init (GridBoxClass *klass)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (klass);
  widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->constructor = gridselect_box_constructor;
  gobject_class->dispose = gridselect_box_dispose;

  widget_class->measure = gridselect_box_measure;

  klass->clicked = NULL;

  box_signals [CLICKED] =
    g_signal_new ("clicked",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GridBoxClass, clicked),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
}

static void
gridselect_box_init (GridBox *box)
{
  gtk_widget_set_focusable (GTK_WIDGET (box), TRUE);
  gtk_widget_set_receives_default (GTK_WIDGET (box), TRUE);

  box->name = NULL;

  GtkGesture *click = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click), GDK_BUTTON_PRIMARY);
  g_signal_connect (click, "pressed", G_CALLBACK (on_box_pressed), box);
  g_signal_connect (click, "released", G_CALLBACK (on_box_released), box);
  gtk_widget_add_controller (GTK_WIDGET (box), GTK_EVENT_CONTROLLER (click));

  GtkEventController *keys = gtk_event_controller_key_new ();
  g_signal_connect (keys, "key-pressed", G_CALLBACK (on_box_key_pressed), box);
  g_signal_connect (keys, "key-released", G_CALLBACK (on_box_key_released), box);
  gtk_widget_add_controller (GTK_WIDGET (box), keys);

  GtkEventController *motion = gtk_event_controller_motion_new ();
  g_signal_connect (motion, "enter", G_CALLBACK (on_box_enter), box);
  g_signal_connect (motion, "leave", G_CALLBACK (on_box_leave), box);
  gtk_widget_add_controller (GTK_WIDGET (box), motion);
}

static GObject*	gridselect_box_constructor (GType gtype,
					guint n_properties,
					GObjectConstructParam *properties)
{
  GObject *obj;
  GridBox *box;

  obj = G_OBJECT_CLASS (gridselect_box_parent_class)->constructor (gtype, n_properties, properties);

  box = GRIDSELECT_BOX (obj);

  box->font = g_strdup ("Sans 10");
  box->on_box = FALSE;
  box->box_down = FALSE;

  return obj;
}

GtkWidget* gridselect_box_new (void)
{
  return g_object_new (GRIDSELECT_TYPE_BOX, NULL);
}

GtkWidget* gridselect_box_new_with_name (gchar *name)
{
  return g_object_new (GRIDSELECT_TYPE_BOX, "name", name, NULL);
}

static void
gridselect_box_dispose (GObject *gobject)
{
  GridBox *box = GRIDSELECT_BOX (gobject);
  if (box->name)
    g_free (box->name);
  box->name = NULL;

  if (box->font)
    g_free (box->font);
  box->font = NULL;

  G_OBJECT_CLASS (gridselect_box_parent_class)->dispose (gobject);
}

static void gridselect_box_measure (GtkWidget *widget, GtkOrientation orientation, int for_size,
				     int *minimum, int *natural, int *minimum_baseline, int *natural_baseline)
{
  int size = (orientation == GTK_ORIENTATION_HORIZONTAL) ? BOX_DEFAULT_WIDTH : BOX_DEFAULT_HEIGHT;

  if (minimum) *minimum = size;
  if (natural) *natural = size;
  if (minimum_baseline) *minimum_baseline = -1;
  if (natural_baseline) *natural_baseline = -1;
}

void gridselect_box_clicked (GridBox *box)
{
  g_signal_emit (box, box_signals [CLICKED], 0);
}

void
gridselect_box_set_name (GridBox *box, const gchar *name)
{
  gchar *new_name;
  g_return_if_fail (GRIDSELECT_IS_BOX (box));

  new_name = g_strdup (name);
  g_free (box->name);
  box->name = new_name;

  gtk_widget_queue_draw (GTK_WIDGET (box));
}

const gchar *
gridselect_box_get_name (GridBox *box)
{
  g_return_val_if_fail (GRIDSELECT_IS_BOX (box), NULL);

  return box->name;
}

void gridselect_box_paint (GridBox *box, cairo_t *cr, gint width, gint height)
{
  gboolean has_focus = gtk_widget_has_focus (GTK_WIDGET (box));

  // Draw border
  cairo_rectangle (cr, 0, 0, width, height);
  if (has_focus) {
    cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
    cairo_set_line_width (cr, 4);
    cairo_stroke_preserve (cr);
  }
  cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
  cairo_set_line_width (cr, 1);
  cairo_stroke (cr);
}

void gridselect_box_set_font (GridBox *box, const gchar *font)
{
  g_return_if_fail (GRIDSELECT_IS_BOX (box));

  if (box->font)
    g_free (box->font);

  box->font = g_strdup (font);
}
