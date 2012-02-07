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


#ifndef DRMR_H
#define DRMR_H

#include <sndfile.h>
#include <pthread.h>

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"
#include "lv2/lv2plug.in/ns/ext/event/event.h"
#include "lv2/lv2plug.in/ns/ext/event/event-helpers.h"
#include "lv2/lv2plug.in/ns/ext/uri-map/uri-map.h"

// drumkit scanned from a hydrogen xml file
typedef struct {
  char* name;
  char* desc;
  char* path;
} scanned_kit;

typedef struct {
  int num_kits;
  scanned_kit* kits;
} kits;

// libsndfile stuff

typedef struct {
  SF_INFO info;
  char active;
  uint32_t offset;
  uint32_t limit;
  float* data;
} drmr_sample;

int load_sample(char* path,drmr_sample* smp);

// lv2 stuff

#define DRMR_URI "http://github.com/nicklan/drmr"

typedef enum {
  DRMR_MIDI = 0,
  DRMR_LEFT,
  DRMR_RIGHT,
  DRMR_KITNUM,
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
  DRMR_NUM_PORTS
} DrMrPortIndex;

typedef struct {
  // Ports
  float* left;
  float* right;
  LV2_Event_Buffer *midi_port;

  // params
  float** gains;
  float* kitReq;

  // URIs
  LV2_URI_Map_Feature* map;
  struct {
    uint32_t midi_event;
  } uris;

  // Available kits
  kits* kits;
  int curKit;

  // Samples
  drmr_sample* samples;
  uint8_t num_samples;

  // loading thread stuff
  pthread_mutex_t load_mutex;
  pthread_cond_t  load_cond;
  pthread_t load_thread;

} DrMr;


#endif // DRMR_H
