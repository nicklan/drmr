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

// Utilities for loading up a hydrogen kit

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "drmr.h"
#include "drmr_hydrogen.h"
#include "expat.h"


#define MAX_CHAR_DATA 512

struct instrument_info {
  int id;
  char* filename;
  char* name;
  struct instrument_info* next;
  // maybe pan/vol/etc..
};

struct kit_info {
  char* name;
  char* desc;
  // linked list of intruments, null terminated
  struct instrument_info* instruments;
};

struct hp_info {
  char in_info;
  char in_instrument_list;
  char in_instrument;
  int  cur_off;
  char cur_buf[MAX_CHAR_DATA];
  struct instrument_info* cur_instrument;
  struct kit_info* kit_info;
};


static void XMLCALL
startElement(void *userData, const char *name, const char **atts)
{
  struct hp_info* info = (struct hp_info*)userData;
  info->cur_off = 0;
  if (info->in_info) {
    if (info->in_instrument_list) {
      if (!strcmp(name,"instrument")) {
	info->in_instrument = 1;
	info->cur_instrument = malloc(sizeof(struct instrument_info));
	memset(info->cur_instrument,0,sizeof(struct instrument_info));
      }
    } else {
      if (!strcmp(name,"instrumentList"))
	info->in_instrument_list = 1;
    }
  } else {
    if (!strcmp(name,"drumkit_info"))
      info->in_info = 1;
  }
}

static void XMLCALL
endElement(void *userData, const char *name)
{
  struct hp_info* info = (struct hp_info*)userData;
  info->cur_buf[info->cur_off]='\0';

  if (info->in_info && !info->in_instrument_list && !strcmp(name,"name"))
    info->kit_info->name = strdup(info->cur_buf);

  if (info->in_instrument) {
    if (!strcmp(name,"id"))
      info->cur_instrument->id = atoi(info->cur_buf);
    if (!strcmp(name,"filename"))
      info->cur_instrument->filename = strdup(info->cur_buf);
    if (!strcmp(name,"name"))
      info->cur_instrument->name = strdup(info->cur_buf);
  }
    

  info->cur_off = 0;

  if (info->in_instrument && !strcmp(name,"instrument")) {
    // ending an instrument, add current struct to end of list
    struct instrument_info * cur_i = info->kit_info->instruments;
    if (cur_i) {
      while(cur_i->next) cur_i = cur_i->next;
      cur_i->next = info->cur_instrument;
    } else
      info->kit_info->instruments = info->cur_instrument;
    info->cur_instrument = NULL;
    info->in_instrument = 0;
  }
  if (info->in_instrument_list && !strcmp(name,"instrumentList")) info->in_instrument_list = 0;
  if (info->in_info && !strcmp(name,"drumkit_info")) info->in_info = 0;
}

static void XMLCALL
charData(void *userData,
	 const char* data,
	 int len) {
  int i;
  struct hp_info* info = (struct hp_info*)userData;
  for(i = 0;i<len;i++) {
    if (info->cur_off < MAX_CHAR_DATA) {
      info->cur_buf[info->cur_off] = data[i];
      info->cur_off++;
    } else
      fprintf(stderr,"Warning, losing data because too much\n");
  }
}


int load_hydrogen_kit(DrMr* drmr, char* path) {
  char* fp_buf;
  FILE* file;
  char buf[BUFSIZ];
  XML_Parser parser;
  int done;
  struct hp_info info;
  struct kit_info kit_info;

  snprintf(buf,BUFSIZ,"%s/drumkit.xml",path);
  
  printf("trying to load: %s\n",buf);

  file = fopen(buf,"r");
  if (!file) {
    perror("Unable to open file:");
    return 1;
  }

  parser = XML_ParserCreate(NULL);
  memset(&info,0,sizeof(struct hp_info));
  memset(&kit_info,0,sizeof(struct kit_info));

  info.kit_info = &kit_info;

  XML_SetUserData(parser, &info);
  XML_SetElementHandler(parser, startElement, endElement);  
  XML_SetCharacterDataHandler(parser, charData);

  do {
    int len = (int)fread(buf, 1, sizeof(buf), file);
    done = len < sizeof(buf);
    if (XML_Parse(parser, buf, len, done) == XML_STATUS_ERROR) {
      fprintf(stderr,
              "%s at line %lu\n",
              XML_ErrorString(XML_GetErrorCode(parser)),
              XML_GetCurrentLineNumber(parser));
      return 1;
    }
  } while (!done);
  XML_ParserFree(parser);

  {
    drmr_sample* samples;
    struct instrument_info * cur_i;
    int i = 0, num_inst = 0;
    printf("Read kit: %s\n",kit_info.name);
    cur_i = kit_info.instruments;
    while(cur_i) { // first count how many samples we have
      num_inst ++;
      cur_i = cur_i->next;
    }
    printf("Loading %i instruments\n",num_inst);
    samples = malloc(num_inst*sizeof(drmr_sample));
    cur_i = kit_info.instruments;
    while(cur_i) {
      snprintf(buf,BUFSIZ,"%s/%s",path,cur_i->filename);
      if (load_sample(buf,samples+i)) {
	fprintf(stderr,"Could not load sample: %s\n",buf);
	// TODO: Memory leak on previously loaded samples
	return 1;
      }
      i++;
      cur_i = cur_i->next;
    }
    if (num_inst > drmr->num_samples) { 
      // we have more, so we can safely swap our sample list in before updating num_samples
      drmr->samples = samples;
      drmr->num_samples = num_inst;
    } else {
      // previous has more, update count first
      drmr->num_samples = num_inst;
      drmr->samples = samples;
    }
  }
  return 0;
  
}
