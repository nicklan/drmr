/* drmr.h
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

#ifndef DRMR_H
#define DRMR_H

#include <sndfile.h>
#include <pthread.h>

#include "lv2/lv2plug.in/ns/ext/atom/forge.h"
// util includes atom.h
#include "lv2/lv2plug.in/ns/ext/atom/util.h"
#include "lv2/lv2plug.in/ns/lv2core/lv2.h"
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"
#include "lv2/lv2plug.in/ns/ext/state/state.h"

// drumkit scanned from a hydrogen xml file
typedef struct {
  char* name;
  char* desc;
  char* path;
  char** sample_names;
  int samples;
} scanned_kit;

typedef struct {
  int num_kits;
  scanned_kit* kits;
} kits;

// libsndfile stuff

typedef struct {
  float min;
  float max;

  SF_INFO *info;
  uint32_t limit;
  float* data;
} drmr_layer;

typedef struct {
  SF_INFO *info;
  char active;
  uint32_t offset;
  uint32_t limit;
  uint32_t layer_count;
  float velocity;
  drmr_layer *layers;
  float* data;
  int dataoffset;
} drmr_sample;

// lv2 stuff

#define DRMR_URI "http://github.com/nicklan/drmr"
#define GAIN_MIN -60.0f
#define GAIN_MAX 6.0f

typedef enum {
  DRMR_CONTROL = 0,
  DRMR_LEFT,
  DRMR_RIGHT,
  DRMR_BASENOTE,
  DRMR_GAIN_ONE,
  DRMR_GAIN_TWO,
  DRMR_GAIN_THREE,
  DRMR_GAIN_FOUR,
  DRMR_GAIN_FIVE,
  DRMR_GAIN_SIX,
  DRMR_GAIN_SEVEN,
  DRMR_GAIN_EIGHT,
  DRMR_GAIN_NINE,
  DRMR_GAIN_TEN,
  DRMR_GAIN_ELEVEN,
  DRMR_GAIN_TWELVE,
  DRMR_GAIN_THIRTEEN,
  DRMR_GAIN_FOURTEEN,
  DRMR_GAIN_FIFTEEN,
  DRMR_GAIN_SIXTEEN,
  DRMR_GAIN_SEVENTEEN,
  DRMR_GAIN_EIGHTEEN,
  DRMR_GAIN_NINETEEN,
  DRMR_GAIN_TWENTY,
  DRMR_GAIN_TWENTYONE,
  DRMR_GAIN_TWENTYTWO,
  DRMR_GAIN_TWENTYTHREE,
  DRMR_GAIN_TWENTYFOUR,
  DRMR_GAIN_TWENTYFIVE,
  DRMR_GAIN_TWENTYSIX,
  DRMR_GAIN_TWENTYSEVEN,
  DRMR_GAIN_TWENTYEIGHT,
  DRMR_GAIN_TWENTYNINE,
  DRMR_GAIN_THIRTY,
  DRMR_GAIN_THIRTYONE,
  DRMR_GAIN_THIRTYTWO,
  DRMR_PAN_ONE,
  DRMR_PAN_TWO,
  DRMR_PAN_THREE,
  DRMR_PAN_FOUR,
  DRMR_PAN_FIVE,
  DRMR_PAN_SIX,
  DRMR_PAN_SEVEN,
  DRMR_PAN_EIGHT,
  DRMR_PAN_NINE,
  DRMR_PAN_TEN,
  DRMR_PAN_ELEVEN,
  DRMR_PAN_TWELVE,
  DRMR_PAN_THIRTEEN,
  DRMR_PAN_FOURTEEN,
  DRMR_PAN_FIFTEEN,
  DRMR_PAN_SIXTEEN,
  DRMR_PAN_SEVENTEEN,
  DRMR_PAN_EIGHTEEN,
  DRMR_PAN_NINETEEN,
  DRMR_PAN_TWENTY,
  DRMR_PAN_TWENTYONE,
  DRMR_PAN_TWENTYTWO,
  DRMR_PAN_TWENTYTHREE,
  DRMR_PAN_TWENTYFOUR,
  DRMR_PAN_TWENTYFIVE,
  DRMR_PAN_TWENTYSIX,
  DRMR_PAN_TWENTYSEVEN,
  DRMR_PAN_TWENTYEIGHT,
  DRMR_PAN_TWENTYNINE,
  DRMR_PAN_THIRTY,
  DRMR_PAN_THIRTYONE,
  DRMR_PAN_THIRTYTWO,
  DRMR_CORE_EVENT,
  DRMR_NUM_PORTS
} DrMrPortIndex;

typedef struct {
  LV2_URID midi_event;
  LV2_URID ui_msg;
  LV2_URID kit_path;
  LV2_URID atom_eventTransfer;
  LV2_URID atom_resource;
  LV2_URID string_urid;
  LV2_URID bool_urid;
  LV2_URID int_urid;
  LV2_URID get_state;
  LV2_URID midi_info;
  LV2_URID sample_trigger;
  LV2_URID velocity_toggle;
  LV2_URID note_off_toggle;
  LV2_URID zero_position;
} drmr_uris;

typedef struct {
  // Ports
  float* left;
  float* right;
  LV2_Atom_Sequence *control_port;
  LV2_Atom_Sequence *core_event_port;

  LV2_Atom_Forge forge;

  // params
  bool ignore_velocity;
  bool ignore_note_off;
  int zero_position;
  float** gains;
  float** pans;
  float* baseNote;
  double rate;

  // URIs
  LV2_URID_Map* map;
  drmr_uris uris;

  // Kit info
  char* current_path;
  char** request_buf;
  int curReq;

  // Samples
  drmr_sample* samples;
  uint8_t num_samples;

  // loading thread stuff
  pthread_mutex_t load_mutex;
  pthread_cond_t  load_cond;
  pthread_t load_thread;

} DrMr;

static inline
void map_drmr_uris(LV2_URID_Map *map,
		   drmr_uris *uris) {
  uris->midi_event =
    map->map(map->handle,
	     "http://lv2plug.in/ns/ext/midi#MidiEvent");
  uris->string_urid =
    map->map(map->handle, LV2_ATOM__String);
  uris->bool_urid =
    map->map(map->handle, LV2_ATOM__Bool);
  uris->int_urid =
    map->map(map->handle, LV2_ATOM__Int);
  uris->ui_msg =
    map->map(map->handle,
	     DRMR_URI "#uimsg");
  uris->kit_path =
    map->map(map->handle,
	     DRMR_URI "#kitpath");
  uris->get_state =
    map->map(map->handle,
	     DRMR_URI "#getstate");
  uris->midi_info =
    map->map(map->handle,
	     DRMR_URI "#midiinfo");
  uris->sample_trigger =
    map->map(map->handle,
	     DRMR_URI "#sampletrigger");
  uris->velocity_toggle =
    map->map(map->handle,
	     DRMR_URI "#velocitytoggle");
  uris->note_off_toggle =
    map->map(map->handle,
	     DRMR_URI "#noteofftoggle");
  uris->zero_position =
    map->map(map->handle,
	     DRMR_URI "#zeroposition");
  uris->atom_eventTransfer = 
    map->map(map->handle, LV2_ATOM__eventTransfer);
  uris->atom_resource = 
    map->map(map->handle, LV2_ATOM__Resource);
}


#endif // DRMR_H
