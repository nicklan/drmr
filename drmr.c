/*
  LV2 DrMr plugin
  Copyright 2012 Nick Lanham <nick@afternight.org>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "drmr.h"
#include "drmr_hydrogen.h"

int load_sample(char* path, drmr_sample* samp) {
  SNDFILE* sndf;
  int size;
  
  //printf("Loading: %s\n",path);

  samp->active = 0;

  memset(&(samp->info),0,sizeof(SF_INFO));
  sndf = sf_open(path,SFM_READ,&(samp->info));
  
  if (!sndf) {
    fprintf(stderr,"Failed to open sound file: %s - %s\n",path,sf_strerror(sndf));
    return 1;
  }

  if (samp->info.channels > 2) {
    fprintf(stderr, "File has too many channels.  Can only handle mono/stereo samples\n");
    return 1;
  }
  size = samp->info.frames * samp->info.channels;
  samp->limit = size;
  samp->data = malloc(size*sizeof(float));
  if (!samp->data) {
    fprintf(stderr,"Failed to allocate sample memory for %s\n",path);
    return 1;
  }

  sf_read_float(sndf,samp->data,size);
  sf_close(sndf); 
  return 0;
}

static void* load_thread(void* arg) {
  DrMr* drmr = (DrMr*)arg;

  pthread_mutex_lock(&drmr->load_mutex);
  for(;;) {
    pthread_cond_wait(&drmr->load_cond,
		      &drmr->load_mutex);
    if (drmr->curKit >= drmr->kits->num_kits) {
      int os = drmr->num_samples;
      drmr->num_samples = 0;
      if (os > 0) free_samples(drmr->samples,os);
    } else
      load_hydrogen_kit(drmr,drmr->kits->kits[drmr->curKit].path);
  }
  pthread_mutex_unlock(&drmr->load_mutex);
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
  drmr->num_samples = 0;

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
  if (pthread_create(&drmr->load_thread, 0, load_thread, drmr)) {
    fprintf(stderr, "Could not initialize loading thread.\n");
    free(drmr);
    return 0;
  }

  // Map midi uri
  while(*features) {
    if (!strcmp((*features)->URI, LV2_URI_MAP_URI)) {
      drmr->map = (LV2_URI_Map_Feature *)((*features)->data);
      drmr->uris.midi_event = drmr->map->uri_to_id
	(drmr->map->callback_data,
	 "http://lv2plug.in/ns/ext/event",
	 "http://lv2plug.in/ns/ext/midi#MidiEvent");
    }
    features++;
  }
  if (!drmr->map) {
    fprintf(stderr, "LV2 host does not support urid:map.\n");
    free(drmr);
    return 0;
  }
  
  drmr->kits = scan_kits();
  if (!drmr->kits) {
    fprintf(stderr, "No drum kits found\n");
    free(drmr);
    return 0;
  }
  drmr->samples = NULL; // prevent attempted freeing in load
  load_hydrogen_kit(drmr,drmr->kits->kits->path);
  drmr->curKit = 0;

  drmr->gains = malloc(16*sizeof(float*));
  for(i = 0;i<16;i++) drmr->gains[i] = NULL;

  return (LV2_Handle)drmr;
}

static void
connect_port(LV2_Handle instance,
             uint32_t   port,
             void*      data) {
  DrMr* drmr = (DrMr*)instance;
  switch ((DrMrPortIndex)port) {
  case DRMR_MIDI:
    drmr->midi_port = (LV2_Event_Buffer*)data;
    break;
  case DRMR_LEFT:
    drmr->left = (float*)data;
    break;
  case DRMR_RIGHT:
    drmr->right = (float*)data;
    break;
  case DRMR_KITNUM:
    if(data) drmr->kitReq = (float*)data;
    printf("Connected kit\n");
    break;
  case DRMR_GAIN_ONE:
    if (data) drmr->gains[0] = (float*)data;
    break;
  case DRMR_GAIN_TWO:
    if (data) drmr->gains[1] = (float*)data;
    break;
  case DRMR_GAIN_THREE:
    if (data) drmr->gains[2] = (float*)data;
    break;
  case DRMR_GAIN_FOUR:
    if (data) drmr->gains[3] = (float*)data;
    break;
  case DRMR_GAIN_FIVE:
    if (data) drmr->gains[4] = (float*)data;
    break;
  case DRMR_GAIN_SIX:
    if (data) drmr->gains[5] = (float*)data;
    break;
  case DRMR_GAIN_SEVEN:
    if (data) drmr->gains[6] = (float*)data;
    break;
  case DRMR_GAIN_EIGHT:
    if (data) drmr->gains[7] = (float*)data;
    break;
  case DRMR_GAIN_NINE:
    if (data) drmr->gains[8] = (float*)data;
    break;
  case DRMR_GAIN_TEN:
    if (data) drmr->gains[9] = (float*)data;
    break;
  case DRMR_GAIN_ELEVEN:
    if (data) drmr->gains[10] = (float*)data;
    break;
  case DRMR_GAIN_TWELVE:
    if (data) drmr->gains[11] = (float*)data;
    break;
  case DRMR_GAIN_THIRTEEN:
    if (data) drmr->gains[12] = (float*)data;
    break;
  case DRMR_GAIN_FOURTEEN:
    if (data) drmr->gains[13] = (float*)data;
    break;
  case DRMR_GAIN_FIFTEEN:
    if (data) drmr->gains[14] = (float*)data;
    break;
  case DRMR_GAIN_SIXTEEN:
    if (data) drmr->gains[15] = (float*)data;
    break;
  default:
    break;
  }
}

static void activate(LV2_Handle instance) { }


static void run(LV2_Handle instance, uint32_t n_samples) {
  int i,kitInt;
  char first_active, one_active;
  DrMr* drmr = (DrMr*)instance;

  kitInt = (int)floorf(*(drmr->kitReq));
  if (kitInt != drmr->curKit) { // requested a new kit
    drmr->curKit = kitInt;
    pthread_cond_signal(&drmr->load_cond);
  }

  LV2_Event_Iterator eit;
  if (lv2_event_begin(&eit,drmr->midi_port)) { // if we have any events
    LV2_Event *cur_ev;
    uint8_t* data;
    while (lv2_event_is_valid(&eit)) {
      cur_ev = lv2_event_get(&eit,&data);
      if (cur_ev->type == drmr->uris.midi_event) {
	int channel = *data & 15;
	switch ((*data) >> 4) {
	case 8:  // ignore note-offs for now, should probably be a setting
	  //if (drmr->cur_samp) drmr->cur_samp->active = 0;
	  break;
	case 9: {
	  uint8_t nn = data[1];
	  nn-=60; // middle c is our root note (setting?)
	  if (nn >= 0 && nn < drmr->num_samples) {
	    drmr->samples[nn].active = 1;
	    drmr->samples[nn].offset = 0;
	  }
	  break;
	}
	default:
	  printf("Unhandeled status: %i\n",(*data)>>4);
	}
      } else printf("unrecognized event\n");
      lv2_event_increment(&eit);
    } 
  }

  first_active = 1;
  for (i = 0;i < drmr->num_samples;i++) {
    int pos,lim;
    drmr_sample* cs = drmr->samples+i;
    if (cs->active) {
      float gain;
      if (i < 16)
	gain = *(drmr->gains[i]);
      else
	gain = 1.0f;
      one_active = 1;
      if (cs->info.channels == 1) { // play mono sample
	lim = (n_samples < (cs->limit - cs->offset)?n_samples:(cs->limit-cs->offset));
	if (first_active) {
	  for(pos = 0;pos < lim;pos++) {
	    drmr->left[pos]  = cs->data[cs->offset]*gain;
	    drmr->right[pos] = cs->data[cs->offset]*gain;
	    cs->offset++;
	  }
	  first_active = 0;
	} else {
	  for(pos = 0;pos < lim;pos++) {
	    drmr->left[pos]  += cs->data[cs->offset]*gain;
	    drmr->right[pos] += cs->data[cs->offset]*gain;
	    cs->offset++;
	  }
	}
      } else { // play stereo sample
	lim = (cs->limit-cs->offset)/cs->info.channels;
	if (lim > n_samples) lim = n_samples;
	if (first_active) {
	  for (pos=0;pos<lim;pos++) {
	    drmr->left[pos]  = cs->data[cs->offset++]*gain;
	    drmr->right[pos] = cs->data[cs->offset++]*gain;
	  }
	  first_active = 0;
	} else {
	  for (pos=0;pos<lim;pos++) {
	    drmr->left[pos]  += cs->data[cs->offset++]*gain;
	    drmr->right[pos] += cs->data[cs->offset++]*gain;
	  }
	}
      }
      if (cs->offset >= cs->limit) cs->active = 0;
    }
  }
  if (first_active) { // didn't find any samples
    int pos;
    for(pos = 0;pos<n_samples;pos++) {
      drmr->left[pos] = 0.0f;
      drmr->right[pos] = 0.0f;
    }
  }
}

static void deactivate(LV2_Handle instance) {}

static void cleanup(LV2_Handle instance) {
  free(instance);
}

const void* extension_data(const char* uri) {
  return NULL;
}

static const LV2_Descriptor descriptor = {
  DRMR_URI,
  instantiate,
  connect_port,
  activate,
  run,
  deactivate,
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
