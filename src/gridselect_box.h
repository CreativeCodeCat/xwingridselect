/* Copyright (c) 2020, TheCynicalTeam @ Github
 * gridselect_box.h - headers for gridselect_box class.
 */

#ifndef GRIDSELECT_BOX_H
#define GRIDSELECT_BOX_H

#include <gtk/gtk.h>
#include <cairo.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>

G_BEGIN_DECLS

#define GRIDSELECT_TYPE_BOX            (gridselect_box_get_type ())
#define GRIDSELECT_BOX(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GRIDSELECT_TYPE_BOX, GridBox))
#define GRIDSELECT_BOX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GRIDSELECT_TYPE_BOX, GridBoxClass))
#define GRIDSELECT_IS_BOX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GRIDSELECT_TYPE_BOX))
#define GRIDSELECT_IS_BOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GRIDSELECT_TYPE_BOX))
#define GRIDSELECT_BOX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GRIDSELECT_TYPE_BOX, GridBoxClass))

typedef struct _GridBox      GridBox;
typedef struct _GridBoxClass GridBoxClass;

struct _GridBox
{
  GtkWidget parent;

  /*< private >*/
  gchar *name;

  gboolean on_box;
  gboolean box_down;

  gchar *font;
};

struct _GridBoxClass
{
  GtkWidgetClass parent_class;

  void (* clicked)  (GridBox *box);
};

GType gridselect_box_get_type (void);
GtkWidget* gridselect_box_new (void);
void gridselect_box_set_name (GridBox *box, const gchar *name);
const gchar *gridselect_box_get_name (GridBox *box);

void gridselect_box_paint (GridBox *box, cairo_t *cr, gint width, gint height);
void gridselect_box_set_font (GridBox *box, const gchar *font);

G_END_DECLS

#endif /* GRIDSELECT_BOX_H */
