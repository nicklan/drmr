/* nknob.h
 * LV2 DrMr plugin
 * Copyright 2012 Nick Lanham <nick@afternight.org>
 *
 * NKnob - A simplified version of phatknob that just is a new gui
 *         over a GtkRange (i.e. it can be used exactly like a
 *         GtkRange from the outside)
 *
 * From PhatKnob code:
 *    Most of this code comes from gAlan 0.2.0, copyright (C) 1999
 *    Tony Garnock-Jones, with modifications by Sean Bolton,
 *    copyright (c) 2004.  (gtkdial.c rolls over in its grave.)
 *
 *    Phatised by Loki Davison.
 *
 * GNU Public License v3. source code is available at 
 * <http://github.com/nicklan/drmr>
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */


#if defined(GTK_DISABLE_SINGLE_INCLUDES) && !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __NKNOB_H__
#define __NKNOB_H__

#include <gtk/gtkrange.h>

G_BEGIN_DECLS

#define N_TYPE_KNOB          (n_knob_get_type ( ))
#define N_KNOB(obj)          GTK_CHECK_CAST(obj, n_knob_get_type(), NKnob)
#define N_KNOB_CLASS(klass)  GTK_CHECK_CLASS_CAST(klass, n_knob_get_type(), NKnobClass)
#define N_IS_KNOB(obj)       GTK_CHECK_TYPE(obj, n_knob_get_type())
#define N_IS_KNOB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), N_TYPE_KNOB))
#define N_KNOB_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), N_TYPE_KNOB, NKnobClass))


typedef struct _NKnob        NKnob;
typedef struct _NKnobClass   NKnobClass;

struct _NKnob {
  GtkRange range;

  /* image prefix */
  gchar *load_prefix;

  /* State of widget (to do with user interaction) */
  guint8 state;
  gint saved_x, saved_y;
  
  /* size of the widget */
  gint size;
  
  /* Pixmap for knob */
  GdkPixbuf *pixbuf;
};

struct _NKnobClass {
  GtkRangeClass parent_class;
};

GType n_knob_get_type ( ) G_GNUC_CONST;

GtkWidget* n_knob_new (GtkAdjustment* adjustment);

GtkWidget* n_knob_new_with_range (double value,
				  double lower,
				  double upper,
				  double step);

void   n_knob_set_load_prefix(NKnob* knob, gchar* prefix);
gchar* n_knob_get_load_prefix(NKnob* knob);

G_END_DECLS

#endif
