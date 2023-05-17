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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "drmr.h"
#include "drmr_hydrogen.h"

#define REQ_BUF_SIZE 10
#define VELOCITY_MAX 127

static int current_kit_changed = 0;

static void* load_thread(void* arg) {
  DrMr* drmr = (DrMr*)arg;
  drmr_sample *loaded_samples,*old_samples;
  int loaded_count, old_scount;
  char *request, *request_orig;
  for(;;) {
    pthread_mutex_lock(&drmr->load_mutex);
    pthread_cond_wait(&drmr->load_cond,
		      &drmr->load_mutex);
    pthread_mutex_unlock(&drmr->load_mutex); 
    old_samples = drmr->samples;
    old_scount = drmr->num_samples;
    request_orig = request = drmr->request_buf[drmr->curReq];
    if (!strncmp(request, "file://", 7))
      request += 7;
    loaded_samples = load_hydrogen_kit(request,drmr->rate,&loaded_count);
    if (!loaded_samples) {
      fprintf(stderr,"Failed to load kit at: %s\n",request);
      pthread_mutex_lock(&drmr->load_mutex);
      drmr->num_samples = 0;
      drmr->samples = NULL;
      pthread_mutex_unlock(&drmr->load_mutex); 
    }
    else {
      // just lock for the critical moment when we swap in the new kit
      printf("loaded kit at: %s\n",request);
      pthread_mutex_lock(&drmr->load_mutex);
      drmr->samples = loaded_samples;
      drmr->num_samples = loaded_count;
      pthread_mutex_unlock(&drmr->load_mutex); 
    }
    if (old_scount > 0) free_samples(old_samples,old_scount);
    drmr->current_path = request_orig;
    current_kit_changed = 1;
  }
  return 0;
}

static LV2_Handle
instantiate(const LV2_Descriptor*     descriptor,
            double                    rate,
            const char*               bundle_path,
            const LV2_Feature* const* features) {
  int i;
  DrMr* drmr = malloc(sizeof(DrMr));
  drmr->map = NULL;
  drmr->samples = NULL;
  drmr->num_samples = 0;
  drmr->current_path = NULL;
  drmr->curReq = -1;
  drmr->rate = rate;
  drmr->ignore_velocity = false;
  drmr->ignore_note_off = true;

#ifdef DRMR_UI_ZERO_SAMP
  drmr->zero_position = DRMR_UI_ZERO_SAMP;
#else
  drmr->zero_position = 0;
#endif

  if (pthread_mutex_init(&drmr->load_mutex, 0)) {
    fprintf(stderr, "Could not initialize load_mutex.\n");
    free(drmr);
    return 0;
  }
  if (pthread_cond_init(&drmr->load_cond, 0)) {
    fprintf(stderr, "Could not initialize load_cond.\n");
    free(drmr);
    return 0;
  }

  while(*features) {
    if (!strcmp((*features)->URI, LV2_URID_URI "#map"))
      drmr->map = (LV2_URID_Map *)((*features)->data);
    features++;
  }
  if (!drmr->map) {
    fprintf(stderr, "LV2 host does not support urid#map.\n");
    free(drmr);
    return 0;
  } 
  map_drmr_uris(drmr->map,&(drmr->uris));
  
  lv2_atom_forge_init(&drmr->forge, drmr->map);

  if (pthread_create(&drmr->load_thread, 0, load_thread, drmr)) {
    fprintf(stderr, "Could not initialize loading thread.\n");
    free(drmr);
    return 0;
  }

  drmr->request_buf = malloc(REQ_BUF_SIZE*sizeof(char*));
  memset(drmr->request_buf,0,REQ_BUF_SIZE*sizeof(char*));

  drmr->gains = malloc(32*sizeof(float*));
  drmr->pans = malloc(32*sizeof(float*));
  for(i = 0;i<32;i++) {
    drmr->gains[i] = NULL;
    drmr->pans[i] = NULL;
  }

  return (LV2_Handle)drmr;
}

static void
connect_port(LV2_Handle instance,
             uint32_t   port,
             void*      data) {
  DrMr* drmr = (DrMr*)instance;
  DrMrPortIndex port_index = (DrMrPortIndex)port;
  switch (port_index) {
  case DRMR_CONTROL:
    drmr->control_port = (LV2_Atom_Sequence*)data;
    break;
  case DRMR_CORE_EVENT:
    drmr->core_event_port = (LV2_Atom_Sequence*)data;
    break;
  case DRMR_LEFT:
    drmr->left = (float*)data;
    break;
  case DRMR_RIGHT:
    drmr->right = (float*)data;
    break;
  case DRMR_BASENOTE:
    if (data) drmr->baseNote = (float*)data;
  default:
    break;
  }

  if (port_index >= DRMR_GAIN_ONE && port_index <= DRMR_GAIN_THIRTYTWO) {
    int goff = port_index - DRMR_GAIN_ONE;
    drmr->gains[goff] = (float*)data;
  }

  if (port_index >= DRMR_PAN_ONE && port_index <= DRMR_PAN_THIRTYTWO) {
    int poff = port_index - DRMR_PAN_ONE;
    drmr->pans[poff] = (float*)data;
  }
}

static inline LV2_Atom *build_update_message(DrMr *drmr) {
  LV2_Atom_Forge_Frame set_frame;
  LV2_Atom* msg = (LV2_Atom*)lv2_atom_forge_resource
    (&drmr->forge, &set_frame, 1, drmr->uris.ui_msg);
  if (drmr->current_path) {
    lv2_atom_forge_property_head(&drmr->forge, drmr->uris.kit_path,0);
    lv2_atom_forge_string(&drmr->forge, drmr->current_path, strlen(drmr->current_path));
  }
  lv2_atom_forge_pop(&drmr->forge,&set_frame);
  return msg;
}

static inline LV2_Atom *build_state_message(DrMr *drmr) {
  LV2_Atom_Forge_Frame set_frame;
  LV2_Atom* msg = (LV2_Atom*)lv2_atom_forge_resource
    (&drmr->forge, &set_frame, 1, drmr->uris.get_state);
  if (drmr->current_path) {
    lv2_atom_forge_property_head(&drmr->forge, drmr->uris.kit_path,0);
    lv2_atom_forge_string(&drmr->forge, drmr->current_path, strlen(drmr->current_path));
  }
  lv2_atom_forge_property_head(&drmr->forge, drmr->uris.velocity_toggle,0);
  lv2_atom_forge_bool(&drmr->forge, drmr->ignore_velocity?true:false);
  lv2_atom_forge_property_head(&drmr->forge, drmr->uris.note_off_toggle,0);
  lv2_atom_forge_bool(&drmr->forge, drmr->ignore_note_off?true:false);
  lv2_atom_forge_property_head(&drmr->forge, drmr->uris.zero_position,0);
  lv2_atom_forge_int(&drmr->forge, drmr->zero_position);
  lv2_atom_forge_pop(&drmr->forge,&set_frame);
  return msg;
}

static inline LV2_Atom *build_midi_info_message(DrMr *drmr, uint8_t *data) {
  LV2_Atom_Forge_Frame set_frame;
  LV2_Atom* msg = (LV2_Atom*)lv2_atom_forge_resource
    (&drmr->forge, &set_frame, 1, drmr->uris.midi_info);
  lv2_atom_forge_property_head(&drmr->forge, drmr->uris.midi_event,0);
  lv2_atom_forge_write(&drmr->forge, data, 3);
  lv2_atom_forge_pop(&drmr->forge,&set_frame);
  return msg;
}

static inline void layer_to_sample(drmr_sample *sample, float gain) {
  int i;
  float mapped_gain = (1-(gain/GAIN_MIN));
  if (mapped_gain > 1.0f) mapped_gain = 1.0f;
  for(i = 0;i < sample->layer_count;i++) {
    if (sample->layers[i].min <= mapped_gain &&
	(sample->layers[i].max > mapped_gain ||
	 (sample->layers[i].max == 1 && mapped_gain == 1))) {
      sample->limit = sample->layers[i].limit;
      sample->info = sample->layers[i].info;
      sample->data = sample->layers[i].data;
      return;
    }
  }
  fprintf(stderr,"Couldn't find layer for gain %f in sample\n\n",gain);
  /* to avoid not playing something, and to deal with kits like the 
     k-27_trash_kit, let's just use the first layer */ 
  sample->limit = sample->layers[0].limit;
  sample->info = sample->layers[0].info;
  sample->data = sample->layers[0].data;
}

static inline void trigger_sample(DrMr *drmr, int nn, uint8_t* const data, uint32_t offset) {
  // need to mutex this to avoid getting the samples array
  // changed after the check that the midi-note is valid
  pthread_mutex_lock(&drmr->load_mutex);
  if (nn >= 0 && nn < drmr->num_samples) {
    if (drmr->samples[nn].layer_count > 0) {
      layer_to_sample(drmr->samples+nn,*(drmr->gains[nn]));
      if (drmr->samples[nn].limit == 0)
	fprintf(stderr,"Failed to find layer at: %i for %f\n",nn,*drmr->gains[nn]);
    }
    if (data) {
      lv2_atom_forge_frame_time(&drmr->forge, 0);
      build_midi_info_message(drmr,data);
    }
    drmr->samples[nn].active = 1;
    drmr->samples[nn].offset = 0;
    drmr->samples[nn].velocity = drmr->ignore_velocity?1.0f:((float)data[2])/VELOCITY_MAX;
    drmr->samples[nn].dataoffset = offset;
  }
  pthread_mutex_unlock(&drmr->load_mutex);
}

static inline void untrigger_sample(DrMr *drmr, int nn, uint32_t offset) {
  pthread_mutex_lock(&drmr->load_mutex);
  if (nn >= 0 && nn < drmr->num_samples) {
    if (drmr->samples[nn].layer_count > 0) {
      layer_to_sample(drmr->samples+nn,*(drmr->gains[nn]));
      if (drmr->samples[nn].limit == 0)
	fprintf(stderr,"Failed to find layer at: %i for %f\n",nn,*drmr->gains[nn]);
    }
    drmr->samples[nn].active = 0;
    drmr->samples[nn].dataoffset = offset;
  }
  pthread_mutex_unlock(&drmr->load_mutex);
}

#define DB3SCALE -0.8317830986718104f
#define DB3SCALEPO 1.8317830986718104f
// taken from lv2 example amp plugin
#define DB_CO(g) ((g) > GAIN_MIN ? powf(10.0f, (g) * 0.05f) : 0.0f)

static void run(LV2_Handle instance, uint32_t n_samples) {
  int i,baseNote;
  DrMr* drmr = (DrMr*)instance;

  baseNote = (int)floorf(*(drmr->baseNote));

  const uint32_t event_capacity = drmr->core_event_port->atom.size;
  lv2_atom_forge_set_buffer(&drmr->forge,
			    (uint8_t*)drmr->core_event_port,
			    event_capacity);
  LV2_Atom_Forge_Frame seq_frame;
  lv2_atom_forge_sequence_head(&drmr->forge, &seq_frame, 0);

  LV2_ATOM_SEQUENCE_FOREACH(drmr->control_port, ev) {
    if (ev->body.type == drmr->uris.midi_event) {
      uint8_t nn;
      uint8_t* const data = (uint8_t* const)(ev + 1);
      uint32_t offset = (ev->time.frames > 0 && ev->time.frames < n_samples) ? ev->time.frames : 0;
      //int channel = *data & 15;
      switch ((*data) >> 4) {
      case 8:
	if (!drmr->ignore_note_off) {
	  nn = data[1];
	  nn-=baseNote;
	  untrigger_sample(drmr,nn,offset);
	}
	break;
      case 9: {
	nn = data[1];
	nn-=baseNote;
	trigger_sample(drmr,nn,data,offset);
	break;
      }
      default:
	//printf("Unhandeled status: %i\n",(*data)>>4);
	break;
      }
    } 
    else if (ev->body.type == drmr->uris.atom_resource) {
      const LV2_Atom_Object *obj = (LV2_Atom_Object*)&ev->body;
      if (obj->body.otype == drmr->uris.ui_msg) {
	const LV2_Atom* path = NULL;
	const LV2_Atom* trigger = NULL;
	const LV2_Atom* ignvel = NULL;
	const LV2_Atom* ignno = NULL;
	const LV2_Atom* zerop = NULL;
	lv2_atom_object_get(obj,
			    drmr->uris.kit_path, &path,
			    drmr->uris.sample_trigger, &trigger,
			    drmr->uris.velocity_toggle, &ignvel,
			    drmr->uris.note_off_toggle, &ignno,
			    drmr->uris.zero_position, &zerop,
			    0);
	if (path) {
	  int reqPos = (drmr->curReq+1)%REQ_BUF_SIZE;
	  char *tmp = NULL;
	  if (reqPos >= 0 &&
	      drmr->request_buf[reqPos])
	    tmp = drmr->request_buf[reqPos];
	  drmr->request_buf[reqPos] = strdup(LV2_ATOM_BODY(path));
	  drmr->curReq = reqPos;
	  if (tmp) free(tmp);
	}
	if (trigger) {
	  int32_t si = ((const LV2_Atom_Int*)trigger)->body;
	  uint8_t mdata[3];
	  uint32_t offset = (ev->time.frames > 0 && ev->time.frames < n_samples) ? ev->time.frames : 0;
	  mdata[0] = 0x90; // note on
	  mdata[1] = si+baseNote;
	  mdata[2] = 0x7f;
	  trigger_sample(drmr,si,mdata,offset);
	}
	if (ignvel)
	  drmr->ignore_velocity = ((const LV2_Atom_Bool*)ignvel)->body;
	if (ignno)
	  drmr->ignore_note_off = ((const LV2_Atom_Bool*)ignno)->body;
	if (zerop)
	  drmr->zero_position = ((const LV2_Atom_Int*)zerop)->body;
      } else if (obj->body.otype == drmr->uris.get_state) {
	lv2_atom_forge_frame_time(&drmr->forge, 0);
	build_state_message(drmr);
      }
    }
    //else printf("unrecognized event\n");
  }

  if ((drmr->curReq >= 0) &&
      drmr->request_buf[drmr->curReq] && 
      (!drmr->current_path ||
       strcmp(drmr->current_path,
	      drmr->request_buf[drmr->curReq])))
    pthread_cond_signal(&drmr->load_cond);

  if (current_kit_changed) {
    current_kit_changed = 0;
    lv2_atom_forge_frame_time(&drmr->forge, 0);
    build_update_message(drmr);
  }

  lv2_atom_forge_pop(&drmr->forge, &seq_frame);

  for(i = 0;i<n_samples;i++) {
    drmr->left[i] = 0.0f;
    drmr->right[i] = 0.0f;
  }

  pthread_mutex_lock(&drmr->load_mutex); 
  for (i = 0;i < drmr->num_samples;i++) {
    int pos,lim;
    drmr_sample* cs = drmr->samples+i;
    if ((cs->active || cs->dataoffset) && (cs->limit > 0)) {
      float coef_right, coef_left;
      if (i < 32) {
	float gain = DB_CO(*(drmr->gains[i]));
	float pan_right = ((*drmr->pans[i])+1)/2.0f;
	float pan_left = 1-pan_right;
	coef_right = (pan_right * (DB3SCALE * pan_right + DB3SCALEPO))*gain*cs->velocity;
	coef_left = (pan_left * (DB3SCALE * pan_left + DB3SCALEPO))*gain*cs->velocity;
      }
      else {
	coef_right = coef_left = 1.0f;
      }

      int datastart, dataend;
      if (cs->active) {
          datastart = cs->dataoffset;
          dataend = n_samples;
      } else {
          datastart = 0;
          dataend = cs->dataoffset;
      }
      cs->dataoffset = 0;

      if (cs->info->channels == 1) { // play mono sample
	lim = (n_samples < (cs->limit - cs->offset)?n_samples:(cs->limit-cs->offset));
	for (pos = datastart; pos < lim && pos < dataend; pos++) {
	  drmr->left[pos]  += cs->data[cs->offset]*coef_left;
	  drmr->right[pos] += cs->data[cs->offset]*coef_right;
	  cs->offset++;
	}
      } else { // play stereo sample
	lim = (cs->limit-cs->offset)/cs->info->channels;
	if (lim > n_samples) lim = n_samples;
	for (pos = datastart; pos < lim && pos < dataend; pos++) {
	  drmr->left[pos]  += cs->data[cs->offset++]*coef_left;
	  drmr->right[pos] += cs->data[cs->offset++]*coef_right;
	}
      }
      if (cs->offset >= cs->limit) cs->active = 0;
    }
  }
  pthread_mutex_unlock(&drmr->load_mutex); 
}

static void cleanup(LV2_Handle instance) {
  DrMr* drmr = (DrMr*)instance;
  pthread_cancel(drmr->load_thread);
  pthread_join(drmr->load_thread, 0);
  if (drmr->num_samples > 0)
    free_samples(drmr->samples,drmr->num_samples);
  free(drmr->gains);
  free(instance);
}

static LV2_State_Status
save_state(LV2_Handle                 instance,
	   LV2_State_Store_Function   store,
	   void*                      handle,
	   uint32_t                   flags,
	   const LV2_Feature *const * features) {
  DrMr *drmr = (DrMr*)instance;
  LV2_State_Map_Path* map_path = NULL;
  int32_t flag;
  LV2_State_Status stat = LV2_STATE_SUCCESS;

  while(*features) {
    if (!strcmp((*features)->URI, LV2_STATE__mapPath))
      map_path = (LV2_State_Map_Path*)((*features)->data);
    features++;
  }

  if (map_path == NULL) {
    fprintf(stderr,"Host does not support map_path, cannot save state\n");
    return LV2_STATE_ERR_NO_FEATURE;
  }

  if (drmr->current_path != NULL) {
	char* mapped_path = map_path->abstract_path(map_path->handle,
	                                            drmr->current_path);

	stat = store(handle,
	             drmr->uris.kit_path,
	             mapped_path,
	             strlen(mapped_path) + 1,
	             drmr->uris.string_urid,
	             LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
	if (stat) return stat;
  }

  flag = drmr->ignore_velocity?1:0;
  stat = store(handle,
	       drmr->uris.velocity_toggle,
	       &flag,
	       sizeof(int32_t),
	       drmr->uris.bool_urid,
	       LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
  if (stat) return stat;

  flag = drmr->ignore_note_off?1:0;
  stat = store(handle,
	       drmr->uris.note_off_toggle,
	       &flag,
	       sizeof(uint32_t),
	       drmr->uris.bool_urid,
	       LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
  if (stat) return stat;

  stat = store(handle,
	       drmr->uris.zero_position,
	       &drmr->zero_position,
	       sizeof(int),
	       drmr->uris.int_urid,
	       LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
  return stat;
}

static LV2_State_Status 
restore_state(LV2_Handle                  instance,
	      LV2_State_Retrieve_Function retrieve,
	      void*                       handle,
	      uint32_t                    flags,
	      const LV2_Feature *const *  features) {
  DrMr* drmr = (DrMr*)instance;
  size_t      size;
  uint32_t    type;
  uint32_t    fgs;

  LV2_State_Map_Path* map_path = NULL;
  while(*features) {
    if (!strcmp((*features)->URI, LV2_STATE__mapPath))
      map_path = (LV2_State_Map_Path*)((*features)->data);
    features++;
  }

  if (map_path == NULL) {
    fprintf(stderr,"Host does not support map_path, cannot restore state\n");
    return LV2_STATE_ERR_NO_FEATURE;
  }


  const char* abstract_path =
    retrieve(handle, drmr->uris.kit_path, &size, &type, &fgs);

  if (!abstract_path) {
    fprintf(stderr,"Found no path in state, not restoring\n");
    return LV2_STATE_ERR_NO_PROPERTY;
  }

  char *kit_path = map_path->absolute_path(map_path->handle,abstract_path);

  if (kit_path) { // safe as we're in "Instantiation" threading class
    int reqPos = (drmr->curReq+1)%REQ_BUF_SIZE;
    char *tmp = NULL;
    if (reqPos >= 0 && drmr->request_buf[reqPos])
      tmp = drmr->request_buf[reqPos];
    drmr->request_buf[reqPos] = strdup(kit_path);
    drmr->curReq = reqPos;
    if (tmp) free(tmp);
  }

  const uint32_t* ignore_velocity =
    retrieve(handle, drmr->uris.velocity_toggle, &size, &type, &fgs);
  if (ignore_velocity)
    drmr->ignore_velocity = *ignore_velocity?true:false;

  const uint32_t* ignore_note_off =
    retrieve(handle, drmr->uris.note_off_toggle, &size, &type, &fgs);
  if (ignore_note_off)
    drmr->ignore_note_off = *ignore_note_off?true:false;

  const int* zero_position =
    retrieve(handle, drmr->uris.zero_position, &size, &type, &fgs);
  if (zero_position)
    drmr->zero_position = *zero_position;

  return LV2_STATE_SUCCESS;
}


static const void* extension_data(const char* uri) {
  static const LV2_State_Interface state_iface = { save_state, restore_state };
  if (!strcmp(uri, LV2_STATE__interface)) return &state_iface;
  return NULL;
}

static const LV2_Descriptor descriptor = {
  DRMR_URI,
  instantiate,
  connect_port,
  NULL, // activate
  run,
  NULL, // deactivate
  cleanup,
  extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
  switch (index) {
  case 0:
    return &descriptor;
  default:
    return NULL;
  }
}
