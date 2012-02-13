/* drmr.c
 * LV2 DrMr plugin
 * Copyright 2012 Nick Lanham <nick@afternight.org>
 *
 * Public License v3. source code is available at 
 * <http://github.com/nicklan/drmr>

 * THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>

#include <gtk/gtk.h>

#include "drmr.h"
#include "drmr_hydrogen.h"
#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"

#define DRMR_UI_URI "http://github.com/nicklan/drmr#ui"

typedef struct {
  LV2UI_Write_Function write;
  LV2UI_Controller     controller;

  GtkWidget *drmr_widget;
  GtkTable *sample_table;
  GtkComboBox *kit_combo;
  GtkListStore *kit_store;
  GtkWidget** gain_sliders;
  GtkWidget** pan_sliders;
  int cols;

  int samples;

  GQuark gain_quark, pan_quark;

  int curKit;
  kits* kits;
} DrMrUi;

static void gain_callback(GtkRange* range, gpointer data) {
  DrMrUi* ui = (DrMrUi*)data;
  int gidx = GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(range),ui->gain_quark));
  float gain = gtk_range_get_value(range);
  ui->write(ui->controller,gidx+DRMR_GAIN_ONE,4,0,&gain);
}

static void pan_callback(GtkRange* range, gpointer data) {
  DrMrUi* ui = (DrMrUi*)data;
  int pidx = GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(range),ui->pan_quark));
  float pan = gtk_range_get_value(range);
  ui->write(ui->controller,pidx+DRMR_PAN_ONE,4,0,&pan);
}

static void fill_sample_table(DrMrUi* ui, int samples, GtkWidget** gain_sliders, GtkWidget** pan_sliders) {
  int row = 0;
  int col = 0;
  int si;
  gchar buf[32];;
  int rows = (samples/ui->cols);
  gtk_table_resize(ui->sample_table,rows,ui->cols);
  for(si = 0;si<samples;si++) {
    GtkWidget *frame,*hbox,*gain_vbox,*pan_vbox;
    GtkWidget* gain_slider;
    GtkWidget* pan_slider;
    GtkWidget* gain_label;
    GtkWidget* pan_label;
    sprintf(buf,"<b>Sample %i</b>",(si+1));

    frame = gtk_frame_new(buf);
    gtk_label_set_use_markup(GTK_LABEL(gtk_frame_get_label_widget(GTK_FRAME(frame))),true);
    gtk_frame_set_shadow_type(GTK_FRAME(frame),GTK_SHADOW_OUT);
    hbox = gtk_hbox_new(false,0);
 
    gain_slider = gtk_vscale_new_with_range(GAIN_MIN,6.0,1.0);
    g_object_set_qdata (G_OBJECT(gain_slider),ui->gain_quark,GINT_TO_POINTER(si));
    g_signal_connect(G_OBJECT(gain_slider),"value-changed",G_CALLBACK(gain_callback),ui);
    if (gain_sliders) gain_sliders[si] = gain_slider;
    gtk_range_set_inverted(GTK_RANGE(gain_slider),true);
    gtk_scale_set_value_pos(GTK_SCALE(gain_slider),GTK_POS_BOTTOM);
    gtk_range_set_value(GTK_RANGE(gain_slider),0.0);
    gtk_scale_set_digits(GTK_SCALE(gain_slider),1);
    gtk_scale_add_mark(GTK_SCALE(gain_slider),0.0,GTK_POS_RIGHT,"0 dB");
    // Hrmm, -inf label is at top in ardour for some reason
    //gtk_scale_add_mark(GTK_SCALE(gain_slider),GAIN_MIN,GTK_POS_RIGHT,"-inf");
    gain_label = gtk_label_new("Gain");
    gain_vbox = gtk_vbox_new(false,0);

    pan_slider = gtk_hscale_new_with_range(-1.0,1.0,0.1);
    if (pan_sliders) pan_sliders[si] = pan_slider;
    gtk_range_set_value(GTK_RANGE(pan_slider),0);
    g_object_set_qdata (G_OBJECT(pan_slider),ui->pan_quark,GINT_TO_POINTER(si));
    gtk_scale_add_mark(GTK_SCALE(pan_slider),0.0,GTK_POS_TOP,NULL);
    g_signal_connect(G_OBJECT(pan_slider),"value-changed",G_CALLBACK(pan_callback),ui);
    pan_label = gtk_label_new("Pan");
    pan_vbox = gtk_vbox_new(false,0);
    
    gtk_box_pack_start(GTK_BOX(gain_vbox),gain_slider,true,true,0);
    gtk_box_pack_start(GTK_BOX(gain_vbox),gain_label,false,false,0);

    gtk_box_pack_start(GTK_BOX(pan_vbox),pan_slider,true,true,0);
    gtk_box_pack_start(GTK_BOX(pan_vbox),pan_label,false,false,0);

    gtk_box_pack_start(GTK_BOX(hbox),gain_vbox,true,true,0);
    gtk_box_pack_start(GTK_BOX(hbox),pan_vbox,true,true,0);

    gtk_container_add(GTK_CONTAINER(frame),hbox);

    gtk_table_attach_defaults(ui->sample_table,frame,col,col+1,row,row+1);

    col++;
    if (col >= ui->cols) {
      col = 0;
      row++;
    }
  }
}

void kit_combobox_changed(GtkComboBox* box, gpointer data) {
  DrMrUi* ui = (DrMrUi*)data;
  gint new_kit = gtk_combo_box_get_active (GTK_COMBO_BOX(box));
  float fkit = (float)new_kit;
  if (ui->curKit != new_kit)
    ui->write(ui->controller,DRMR_KITNUM,4,0,&fkit);
}

static void fill_kit_combo(GtkComboBox* combo, kits* kits) {
  int i;
  GtkTreeIter iter;
  GtkListStore *store = GTK_LIST_STORE(gtk_combo_box_get_model(combo));
  for (i=0;i<kits->num_kits;i++) {
    gtk_list_store_append (store, &iter);
    gtk_list_store_set(store, &iter, 0, kits->kits[i].name, -1);
  }
}

static void build_drmr_ui(DrMrUi* ui) {
  GtkWidget *drmr_ui_widget;
  GtkWidget *kit_hbox, *kit_combo_box, *kit_label;
  GtkCellRenderer *cell_rend;
  
  drmr_ui_widget = gtk_vbox_new(false,0);
  g_object_set(drmr_ui_widget,"border-width",6,NULL);

  ui->kit_store = gtk_list_store_new(1,G_TYPE_STRING);

  kit_hbox = gtk_hbox_new(false,0);
  kit_combo_box = gtk_combo_box_new_with_model(GTK_TREE_MODEL(ui->kit_store));
  kit_label = gtk_label_new("Kit:");

  cell_rend = gtk_cell_renderer_text_new();
  gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(kit_combo_box), cell_rend, true);
  gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(kit_combo_box), cell_rend,"text",0,NULL);

  gtk_box_pack_start(GTK_BOX(kit_hbox),kit_label,
		     false,false,15);
  gtk_box_pack_start(GTK_BOX(kit_hbox),kit_combo_box,
		     true,true,0);
  gtk_box_pack_start(GTK_BOX(drmr_ui_widget),kit_hbox,
		     false,false,5);

  ui->drmr_widget = drmr_ui_widget;
  ui->sample_table = NULL;
  ui->kit_combo = GTK_COMBO_BOX(kit_combo_box);


  g_signal_connect(G_OBJECT(kit_combo_box),"changed",G_CALLBACK(kit_combobox_changed),ui);

  gtk_widget_show_all(drmr_ui_widget);
}

static LV2UI_Handle
instantiate(const LV2UI_Descriptor*   descriptor,
            const char*               plugin_uri,
            const char*               bundle_path,
            LV2UI_Write_Function      write_function,
            LV2UI_Controller          controller,
            LV2UI_Widget*             widget,
            const LV2_Feature* const* features) {
  DrMrUi     *ui = (DrMrUi*)malloc(sizeof(DrMrUi));
  int i;

  ui->write      = write_function;
  ui->controller = controller;
  ui->drmr_widget = NULL;
  ui->curKit = -1;
  ui->samples = 0;
  *widget = NULL;

  build_drmr_ui(ui);

  ui->kits = scan_kits();
  ui->gain_quark = g_quark_from_string("drmr_gain_quark");
  ui->pan_quark = g_quark_from_string("drmr_pan_quark");
  ui->gain_sliders = NULL;
  ui->pan_sliders = NULL;
  ui->cols = 4;
  fill_kit_combo(ui->kit_combo, ui->kits);

  *widget = ui->drmr_widget;

  return ui;
}


static void cleanup(LV2UI_Handle handle) {
  DrMrUi* ui = (DrMrUi*)handle;
  gtk_widget_destroy(ui->drmr_widget);
  if (ui->gain_sliders) free(ui->gain_sliders);
  if (ui->pan_sliders) free(ui->pan_sliders);
  free_kits(ui->kits);
  free(ui);
}

static void
port_event(LV2UI_Handle handle,
           uint32_t     port_index,
           uint32_t     buffer_size,
           uint32_t     format,
           const void*  buffer) {
  DrMrPortIndex index = (DrMrPortIndex)port_index;
  DrMrUi* ui = (DrMrUi*)handle;

  if (index == DRMR_KITNUM) {
    if (format != 0) 
      fprintf(stderr,"Invalid format for kitnum: %i\n",format);
    else {
      int kit = (int)(*((float*)buffer));
      int samples = ui->kits->kits[kit].samples;
      GtkWidget** gain_sliders;
      GtkWidget** pan_sliders;
      if (ui->sample_table) {
	ui->gain_sliders = NULL;
	ui->pan_sliders = NULL;
	gtk_widget_destroy(GTK_WIDGET(ui->sample_table));
      }
      ui->sample_table = GTK_TABLE(gtk_table_new(1,1,true));
      gtk_table_set_col_spacings(ui->sample_table,5);
      gtk_table_set_row_spacings(ui->sample_table,5);
      ui->curKit = kit;
      if (ui->gain_sliders) free(ui->gain_sliders);
      if (ui->pan_sliders) free(ui->pan_sliders);
      gain_sliders = malloc(samples*sizeof(GtkWidget*));
      pan_sliders = malloc(samples*sizeof(GtkWidget*));
      fill_sample_table(ui,samples,gain_sliders,pan_sliders);
      gtk_box_pack_start(GTK_BOX(ui->drmr_widget),GTK_WIDGET(ui->sample_table),
			 true,true,5);
      gtk_box_reorder_child(GTK_BOX(ui->drmr_widget),GTK_WIDGET(ui->sample_table),0);
      gtk_widget_show_all(GTK_WIDGET(ui->sample_table));
      gtk_combo_box_set_active(ui->kit_combo,kit);
      ui->samples = samples;
      ui->gain_sliders = gain_sliders;
      ui->pan_sliders = pan_sliders;
    }
  }
  else if (index >= DRMR_GAIN_ONE &&
	   index <= DRMR_GAIN_THIRTYTWO) {
    if (ui->gain_sliders) {
      float gain = *(float*)buffer;
      int idx = index-DRMR_GAIN_ONE;
      if (idx < ui->samples) {
	GtkRange* range = GTK_RANGE(ui->gain_sliders[idx]);
	gtk_range_set_value(range,gain);
      }
    }
  }
  else if (index >= DRMR_PAN_ONE &&
	   index <= DRMR_PAN_THIRTYTWO) {
    if (ui->pan_sliders) {
      float pan = *(float*)buffer;
      int idx = index-DRMR_PAN_ONE;
      if (idx < ui->samples) {
	GtkRange* range = GTK_RANGE(ui->pan_sliders[idx]);
	gtk_range_set_value(range,pan);
      }
    }
  }
}

static const void*
extension_data(const char* uri) {
  return NULL;
}

static const LV2UI_Descriptor descriptor = {
  DRMR_UI_URI,
  instantiate,
  cleanup,
  port_event,
  extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor*
lv2ui_descriptor(uint32_t index) {
  switch (index) {
  case 0:
    return &descriptor;
  default:
    return NULL;
  }
}

