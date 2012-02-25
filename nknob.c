/* nknob.c
 * LV2 DrMr plugin
 * Copyright 2012 Nick Lanham <nick@afternight.org>
 *
 * NKnob - A simplified version of phatknob that just is a new gui
 *         over a GtkRange (i.e. it can be used exactly like a
 *         GtkRange from the outside)
 *
 * In addition, this knob makes the drmr_ui.so module memory resident
 * so it can avoid attempting to re-load itself when shown/hidden in
 * a ui.
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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include "nknob.h"

#define SCROLL_DELAY_LENGTH     100
#define KNOB_SIZE               50

enum {
  STATE_IDLE,         
  STATE_PRESSED,              
  STATE_DRAGGING,
  STATE_SCROLL,
};


/* properties */
enum
{
  PROP_0, /* oops, no props any more */
};

static void n_knob_class_init         (NKnobClass *klass);
static void n_knob_init               (NKnob *knob);
static void n_knob_destroy            (GtkObject *object);
static void n_knob_realize            (GtkWidget *widget);
static void n_knob_size_request       (GtkWidget *widget,
				       GtkRequisition *requisition);
static gint n_knob_expose             (GtkWidget *widget, 
				       GdkEventExpose *event);
static gint n_knob_button_press       (GtkWidget *widget, 
				       GdkEventButton *event);
static gint n_knob_button_release     (GtkWidget *widget,
				       GdkEventButton *event);
static gint n_knob_motion_notify      (GtkWidget *widget, 
				       GdkEventMotion *event);
static gint n_knob_scroll             (GtkWidget *widget, 
				       GdkEventScroll *event);
static void n_knob_update_mouse       (NKnob *knob, 
				       gint x,
				       gint y,
				       gboolean absolute);


static void n_knob_set_property      (GObject *object, 
				      guint prop_id, 
				      const GValue *value, 
				      GParamSpec   *pspec);
static void n_knob_get_property      (GObject *object, 
				      guint prop_id, 
				      GValue *value, 
				      GParamSpec *pspec);

GError *gerror;

/* global pixbufs for less mem usage */
static GdkPixbuf **pixbuf = NULL;

// Can't use G_DEFINE_TYPE because ardour is at gtk 2.22
static NKnobClass *parent_class;
GType
n_knob_get_type (void) {
  static GType nknob_type;
  if (!nknob_type) {
    static const GTypeInfo object_info = {
      sizeof (NKnobClass),
      (GBaseInitFunc) NULL,
      (GBaseFinalizeFunc) NULL,
      (GClassInitFunc) n_knob_class_init,
      (GClassFinalizeFunc) NULL,
      NULL, // class data
      sizeof (NKnob),
      0, // preallocs
      (GInstanceInitFunc) n_knob_init,
      NULL  // value table
    };
    nknob_type = g_type_register_static(GTK_TYPE_RANGE,"NKnob",&object_info,0);
  }
  return nknob_type;
}

static void n_knob_class_init (NKnobClass *klass) {
  GtkObjectClass   *object_class;
  GtkWidgetClass   *widget_class;
  GObjectClass     *g_object_class;
  
  object_class =   GTK_OBJECT_CLASS(klass);
  widget_class =   GTK_WIDGET_CLASS(klass);
  g_object_class = G_OBJECT_CLASS(klass);

  parent_class = g_type_class_peek_parent (klass);
  
  g_object_class->set_property = n_knob_set_property;
  g_object_class->get_property = n_knob_get_property;
  
  object_class->destroy =        n_knob_destroy;
  
  widget_class->realize =        n_knob_realize;
  widget_class->expose_event =   n_knob_expose;
  widget_class->size_request =   n_knob_size_request;
  widget_class->button_press_event = n_knob_button_press;
  widget_class->button_release_event = n_knob_button_release;
  widget_class->motion_notify_event = n_knob_motion_notify;
  widget_class->scroll_event =   n_knob_scroll;
}

static gboolean tooltip_callback(GtkWidget  *widget,
				 gint        x,
				 gint        y,
				 gboolean    keyboard_mode,
				 GtkTooltip *tooltip,
				 gpointer    user_data) {
  if (gtk_widget_get_has_tooltip(widget)) {
    gchar buf[16];
    snprintf(buf,16,"%.2f",gtk_range_get_value(GTK_RANGE(widget)));
    gtk_tooltip_set_text(tooltip,buf);
    return TRUE;
  }
  return FALSE;
}

static void n_knob_init (NKnob *knob) {
  knob->state = STATE_IDLE;
  knob->saved_x = knob->saved_y = 0;
  knob->size = KNOB_SIZE;
  knob->pixbuf = NULL;
  knob->load_prefix = NULL;
  g_signal_connect(G_OBJECT(knob),"query-tooltip",G_CALLBACK(tooltip_callback),NULL);
}


/**
 * n_knob_new:
 * @adjustment: a #GtkAdjustment or NULL
 *
 * Creates a new #NKnob with the supplied
 * #GtkAdjustment. if adjustment is NULL or
 * invalid, a new #GtkAdjustment will be created
 * with the default values of:
 * 0.0, 0.0, 10.0, 0.1, 0.1, 0.2
 * 
 * Returns: a newly created #NKnob
 * 
 */
GtkWidget *n_knob_new(GtkAdjustment *adjustment) {
  return g_object_new (N_TYPE_KNOB,
		       "adjustment",
		       adjustment,
		       NULL);
}

/**
 * n_knob_new_with_range:
 * @value: the initial value the new knob should have
 * @lower: the lowest value the new knob will allow
 * @upper: the highest value the new knob will allow
 * @step: increment added or subtracted when mouse scrolling
 *
 * Creates a new #NKnob.  The knob will create a new
 * #GtkAdjustment from @value, @lower, @upper, and @step.  If these
 * parameters represent a bogus configuration, the program will
 * terminate.
 *
 * Returns: a newly created #NKnob
 * 
 */
GtkWidget* n_knob_new_with_range (gdouble value, gdouble lower,
				  gdouble upper, gdouble step) {
  GtkAdjustment* adj;
  adj = (GtkAdjustment*) gtk_adjustment_new (value, lower, upper, step, step, 0);
  return n_knob_new (adj);
}

void   n_knob_set_load_prefix(NKnob* knob, gchar* prefix) {
  if (knob->load_prefix) g_free(knob->load_prefix);
  knob->load_prefix = g_strdup(prefix);
}
gchar* n_knob_get_load_prefix(NKnob* knob) {
  return knob->load_prefix;
}


static void n_knob_destroy(GtkObject *object) {
  NKnob *knob;
  
  g_return_if_fail(object != NULL);
  g_return_if_fail(N_IS_KNOB(object));

  knob = N_KNOB(object);
  knob->pixbuf = NULL;

  if (knob->load_prefix) g_free(knob->load_prefix);
  knob->load_prefix = NULL;

  if (GTK_OBJECT_CLASS(parent_class)->destroy)
    (*GTK_OBJECT_CLASS(parent_class)->destroy)(object);
}


static void n_knob_realize(GtkWidget *widget) {
  NKnob *knob;
  extern GdkPixbuf **pixbuf;
  int i=0;
  
  g_return_if_fail(widget != NULL);
  g_return_if_fail(N_IS_KNOB(widget));
  
  if (GTK_WIDGET_CLASS(parent_class)->realize)
    (*GTK_WIDGET_CLASS(parent_class)->realize)(widget);

  knob = N_KNOB(widget);
    
  /* init first pixbuf */
  if(pixbuf == NULL){
    pixbuf = g_malloc0(sizeof(GdkPixbuf *));
  }
  /* check for fitting pixbuf or NULL */
  while(pixbuf[i] != NULL && gdk_pixbuf_get_height(pixbuf[i]) != knob->size){
    i++;
  }
  /* if NULL realloc pixbuf pointer array one bigger
   * malloc new pixbuf with new size
   * set local pixbuf pointer to global
   * set last pixbuf pointer to NULL */
  if(pixbuf[i] == NULL){
    gchar* path;
    if (knob->load_prefix)
      path = g_build_path("/",knob->load_prefix,"knob.png",NULL);
    else {
      g_warning("Trying to show knob with no load prefix, looking only in cwd\n");
      path = "knob.png";
    }
    pixbuf[i] = gdk_pixbuf_new_from_file_at_size(path,52*knob->size,knob->size,&gerror);
    if (knob->load_prefix)
      g_free(path);
    knob->pixbuf = pixbuf[i];
    pixbuf=g_realloc(pixbuf,sizeof(GdkPixbuf *) * (i+2));
    pixbuf[i+1] = NULL;                                                 
  } else { /* if not NULL set fitting pixbuf */
    knob->pixbuf = pixbuf[i];
  }
}

static void n_knob_size_request (GtkWidget *widget, GtkRequisition *requisition) {
  NKnob *knob;
  
  knob = N_KNOB(widget);
  requisition->width = knob->size;
  requisition->height = knob->size;
}

static inline gdouble get_zero_one_value(NKnob *knob) {
  GtkAdjustment* adj = gtk_range_get_adjustment(GTK_RANGE(knob));
  return (adj->value - adj->lower)/(adj->upper - adj->lower);
}

static inline gdouble get_adj_value(NKnob *knob, gdouble val) {
  GtkAdjustment* adj = gtk_range_get_adjustment(GTK_RANGE(knob));
  return val*(adj->upper - adj->lower) + adj->lower;
}

static gint n_knob_expose(GtkWidget *widget, GdkEventExpose *event)
{
  NKnob *knob;
  gint dx,xoff;

  g_return_val_if_fail(widget != NULL, FALSE);
  g_return_val_if_fail(N_IS_KNOB(widget), FALSE);
  g_return_val_if_fail(event != NULL, FALSE);

  if (event->count > 0)
    return FALSE;

  knob = N_KNOB(widget);

  xoff = widget->allocation.width/2 - knob->size/2;
  dx = (gint)(51 * get_zero_one_value(knob)) * knob->size;
  
  gdk_pixbuf_render_to_drawable_alpha( knob->pixbuf, widget->window,
				       dx, 0, widget->allocation.x+xoff, widget->allocation.y,
				       knob->size, knob->size, GDK_PIXBUF_ALPHA_FULL, 0, 0,0,0 );

  return FALSE;
}

static gint n_knob_button_press(GtkWidget *widget, GdkEventButton *event) {
  NKnob *knob;

  g_return_val_if_fail(widget != NULL, FALSE);
  g_return_val_if_fail(N_IS_KNOB(widget), FALSE);
  g_return_val_if_fail(event != NULL, FALSE);

  knob = N_KNOB(widget);

  switch (knob->state) {
  case STATE_IDLE:
    switch (event->button) {
    case 1:
    case 3:
      gtk_grab_add(widget);
      knob->state = STATE_PRESSED;
      knob->saved_x = event->x;
      knob->saved_y = event->y;
      break;
      
    default:
      break;
    }
    break;
  default:
    break;
  }

  return FALSE;
}

static gint n_knob_button_release(GtkWidget *widget, GdkEventButton *event) {
  NKnob *knob;

  g_return_val_if_fail(widget != NULL, FALSE);
  g_return_val_if_fail(N_IS_KNOB(widget), FALSE);
  g_return_val_if_fail(event != NULL, FALSE);
  
  knob = N_KNOB(widget);

  switch (event->button) {
  case 1:
  case 3:
    switch (knob->state) {
    case STATE_PRESSED:
      gtk_grab_remove(widget);
      knob->state = STATE_IDLE;
    case STATE_DRAGGING:
      gtk_grab_remove(widget);
      knob->state = STATE_IDLE;
      
      break;
    default:
      break;
    }
    break;
  case 2: {
    gtk_range_set_value(GTK_RANGE(knob),0.0);
  }
  }
  
  return FALSE;
}

static gint n_knob_motion_notify(GtkWidget *widget, GdkEventMotion *event) {
  NKnob *knob;
  GdkModifierType mods;
  gint x, y, xoff;

  g_return_val_if_fail(widget != NULL, FALSE);
  g_return_val_if_fail(N_IS_KNOB(widget), FALSE);
  g_return_val_if_fail(event != NULL, FALSE);

  knob = N_KNOB(widget);

  x = event->x;
  y = event->y;
    

  if (event->is_hint || (event->window != widget->window))
    gdk_window_get_pointer(widget->window, &x, &y, &mods);
  
  xoff = widget->allocation.width/2 - knob->size/2;
  x-=xoff;

  switch (knob->state) {
  case STATE_PRESSED:
    knob->state = STATE_DRAGGING;
    /* fall through */
  case STATE_DRAGGING:
    if (mods & GDK_BUTTON1_MASK) {
      n_knob_update_mouse(knob, x-widget->allocation.x, y-widget->allocation.y , TRUE);
      return TRUE;
    } else if (mods & GDK_BUTTON3_MASK) {
      n_knob_update_mouse(knob, x-widget->allocation.x, y-widget->allocation.y ,  FALSE);
      return TRUE;
    }
    break;
    
  default:
    break;
  }
  
  return FALSE;
}

static gint n_knob_scroll (GtkWidget *widget, GdkEventScroll *event) {
  NKnob *knob;
  GtkRange *range;
  GtkAdjustment *adj;

  gdouble oldval,newval;
  gboolean handled;
  GtkScrollType type;

  knob = N_KNOB(widget);
  range = GTK_RANGE(widget);
  adj = gtk_range_get_adjustment(range);

  gtk_widget_grab_focus (widget);
  
  knob->state = STATE_SCROLL;

  oldval = gtk_range_get_value(range);

  switch (event->direction) {
  case GDK_SCROLL_UP:
    newval = oldval +
      gtk_adjustment_get_step_increment(adj);
    type = GTK_SCROLL_STEP_UP;
    break;
  case GDK_SCROLL_DOWN:
    newval = oldval -
      gtk_adjustment_get_step_increment(adj);
    type = GTK_SCROLL_STEP_DOWN;
  default: // only handle those for now
    break;
  }

  gtk_range_set_value(range,newval);
  newval = gtk_range_get_value(range);
  if (newval != oldval)
    g_signal_emit_by_name (range, "change-value", type, gtk_range_get_value(range), &handled);

  knob->state = STATE_IDLE;
  
  return TRUE;
}



static void n_knob_update_mouse(NKnob *knob, gint x, gint y,
				gboolean absolute) {
  gdouble old_adj_val,old_value, new_value, dv, dh;
  gdouble angle,handled;

  g_return_if_fail(knob != NULL);
  g_return_if_fail(N_IS_KNOB(knob));

  old_adj_val = gtk_range_get_value(GTK_RANGE(knob));
  old_value = get_zero_one_value(knob);

  angle = atan2(-y + (knob->size>>1), x - (knob->size>>1));

  if (absolute) {
    angle /= G_PI;
    if (angle < -0.5)
      angle += 2;

    new_value = -(2.0/3.0) * (angle - 1.25);   /* map [1.25pi, -0.25pi] onto [0, 1] */
    
  } else {
    dv = knob->saved_y - y; /* inverted cartesian graphics coordinate system */
    dh = x - knob->saved_x;
    knob->saved_x = x;
    knob->saved_y = y;

    if (x >= 0 && x <= knob->size)
      dh = 0;  /* dead zone */
    else {
      angle = cos(angle);
      dh *= angle * angle;
    }

    new_value = old_value +
      dv * 0.1 +          /* "step" == 0.1 */
      dh / 200.0f;
  }
  new_value = get_adj_value(knob,new_value);
  gtk_range_set_value(GTK_RANGE(knob),new_value);
  new_value = gtk_range_get_value(GTK_RANGE(knob));
  if (new_value != old_adj_val) {
    g_signal_emit_by_name (knob, "change-value", GTK_SCROLL_JUMP, 
			   new_value, &handled);
  }
}



static void
n_knob_set_property (GObject      *object, 
		     guint         prop_id, 
		     const GValue *value, 
		     GParamSpec   *pspec)
{
  switch (prop_id) {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
n_knob_get_property (GObject    *object, 
		     guint       prop_id, 
		     GValue     *value, 
		     GParamSpec *pspec)
{
  switch (prop_id)  {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

#ifdef _TEST_N_KNOB
static gboolean timeout_func(gpointer data) {
  GtkRange *range = GTK_RANGE(data);
  GtkAdjustment *adj = gtk_range_get_adjustment(range);
  gdouble val = gtk_range_get_value(range);

  if (val == adj->upper)
    val = adj->lower;
  else
    val += gtk_adjustment_get_step_increment(adj);
  gtk_range_set_value(range,val);
  return TRUE;
}

static gboolean changed_callback(GtkRange* range, GtkScrollType type, gdouble value, gpointer data) {
  printf("Changed to: %f\n",value);
  return FALSE;
}

int main(int argc, char* argv[]) {
  GtkWidget* window;
  GtkWidget* knob;
  GtkWidget* vbox;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "NKnob Test");
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
  g_signal_connect (G_OBJECT (window), "delete-event",
		    G_CALLBACK (gtk_main_quit), NULL);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);
     
  /*  knob */
  knob = n_knob_new_with_range (-90, -90, 0, 1);
  n_knob_set_load_prefix(N_KNOB(knob),"../");
  gtk_box_pack_start (GTK_BOX (vbox), knob, TRUE, FALSE, 0);
  g_signal_connect (G_OBJECT (knob), "change-value",
                    G_CALLBACK (changed_callback), NULL);


  gtk_widget_show_all (window);

  if (0)
    g_timeout_add(5,timeout_func,knob);

  gtk_main ( );

  return 0;
}

#endif
