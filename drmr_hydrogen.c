/* drmr_hydrogen.c
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

// Utilities for loading up a hydrogen kit

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <math.h>

#include "samplerate.h"
#include "drmr.h"
#include "drmr_hydrogen.h"
#include "expat.h"

/* Below is a list of the locations that DrMr will
 * search for drumkit files.  It will scan each sub-directory
 * in these locations (non-recursive) for a drumkit.xml
 * file, and if found, parse it and add it to the list
 * of available kits.
 *
 * Strings that start with a ~ will be expanded to the HOME
 * environment variable.  NB: only a ~ at the start of a string
 * will be expanded, ones in the middle will be left in place.
 */
static char* default_drumkit_locations[] = {
  "/usr/share/hydrogen/data/drumkits/",
  "/usr/local/share/hydrogen/data/drumkits/",
  "/usr/share/drmr/drumkits/",
  "~/.hydrogen/data/drumkits/",
  "~/.drmr/drumkits/",
  NULL
};

// Quality of conversion for libsamplerate.
// See http://www.mega-nerd.com/SRC/api_misc.html#Converters
// for info about availble qualities
#define RATE_CONV_QUALITY SRC_SINC_MEDIUM_QUALITY

#define MAX_CHAR_DATA 512

char *unknownstr = "(Unknown)";

struct instrument_layer {
  char* filename;
  float min;
  float max;
  float gain;
  struct instrument_layer *next;
};

struct instrument_info {
  int id;
  char* filename;
  char* name;
  float gain;
  struct instrument_layer *layers;
  struct instrument_info *next;
  // maybe pan/vol/etc..
};

struct kit_info {
  char* name;
  char* desc;
  // linked list of intruments, null terminated
  struct instrument_info* instruments;
};

struct hp_info {
  char scan_only;
  char in_info;
  char in_instrument_list;
  char in_instrument;
  char in_layer;
  char counted_cur_inst;
  int  cur_off;
  char cur_buf[MAX_CHAR_DATA];
  struct instrument_info* cur_instrument;
  struct instrument_layer* cur_layer;
  struct kit_info* kit_info;
};


static void XMLCALL
startElement(void *userData, const char *name, const char **atts)
{
  struct hp_info* info = (struct hp_info*)userData;
  info->cur_off = 0;
  if (info->in_info) {
    if (info->in_instrument) {
      if (!strcmp(name,"layer") && !info->scan_only) { 
	info->in_layer = 1;
	info->cur_layer = malloc(sizeof(struct instrument_layer));
	memset(info->cur_layer,0,sizeof(struct instrument_layer));
      }
    }
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
  if (info->cur_off == MAX_CHAR_DATA) info->cur_off--;
  info->cur_buf[info->cur_off]='\0';

  if (info->in_info && !info->in_instrument_list && !strcmp(name,"name"))
    info->kit_info->name = strdup(info->cur_buf);
  if (info->scan_only && info->in_info && !info->in_instrument_list && !strcmp(name,"info"))
    info->kit_info->desc = strdup(info->cur_buf);

  if (info->in_layer && !info->scan_only) {
    if (!strcmp(name,"filename"))
      info->cur_layer->filename = strdup(info->cur_buf);
    if (!strcmp(name,"min"))
      info->cur_layer->min = atof(info->cur_buf);
    if (!strcmp(name,"max"))
      info->cur_layer->max = atof(info->cur_buf);
    if (!strcmp(name,"gain"))
      info->cur_layer->gain = atof(info->cur_buf);
  }

  if (info->in_instrument && !info->in_layer) {
    if (!strcmp(name,"id"))
      info->cur_instrument->id = atoi(info->cur_buf);
    if (!info->scan_only && !strcmp(name,"filename"))
      info->cur_instrument->filename = strdup(info->cur_buf);
    if (!strcmp(name,"name"))
      info->cur_instrument->name = strdup(info->cur_buf);
  }

  info->cur_off = 0;

  if (!info->scan_only &&
      info->in_layer &&
      !strcmp(name,"layer") &&
      info->cur_layer->filename) {
    struct instrument_layer *cur_l = info->cur_instrument->layers;
    if (cur_l) {
      while(cur_l->next) cur_l = cur_l->next;
      cur_l->next = info->cur_layer;
    } else
      info->cur_instrument->layers = info->cur_layer;
    info->cur_layer = NULL;
    info->in_layer = 0;
  }


  if (info->in_instrument && info->cur_instrument && !strcmp(name,"instrument")) {
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
  if (!info->in_info) return;
  for(i = 0;i<len;i++) {
    if (info->cur_off < MAX_CHAR_DATA) {
      info->cur_buf[info->cur_off] = data[i];
      info->cur_off++;
    }
  }
}

struct kit_list {
  scanned_kit* skit;
  struct kit_list* next;
};

// see note above at default_drumkit_locations
// for how this function works
static char* expand_path(char* path, char* buf) {
  char *home_dir;
  int n;
  if (*path != '~') return path;
  home_dir = getenv("HOME");
  if (!home_dir) {
    fprintf(stderr,"Home dir not set, can't expand ~ paths\n");
    return 0;
  }
  n = snprintf(buf,BUFSIZ,"%s%s",home_dir,path+1);
  if (n >= BUFSIZ) {
    fprintf(stderr,"Path too long for buffer, can't expand: %s\n",path);
    return 0;
  }
  return buf;
}

kits* scan_kits() {
  DIR* dp;
  FILE* file;
  XML_Parser parser;
  int done;
  struct hp_info info;
  struct kit_info kit_info;
  struct dirent *ep;
  int cp = 0;
  char* cur_path = default_drumkit_locations[cp++];
  kits* ret = malloc(sizeof(kits));
  struct kit_list* scanned_kits = NULL;
  char buf[BUFSIZ], path_buf[BUFSIZ];

  ret->num_kits = 0;

  while (cur_path) {
    cur_path = expand_path(cur_path,path_buf);
    if (!cur_path) {
      cur_path = default_drumkit_locations[cp++];
      continue;
    }
    dp = opendir (cur_path);
    if (dp != NULL) {
      while ((ep = readdir (dp))) {
	if (ep->d_name[0]=='.') continue;
	if (snprintf(buf,BUFSIZ,"%s/%s/drumkit.xml",cur_path,ep->d_name) >= BUFSIZ) {
	  fprintf(stderr,"Warning: Skipping scan of %s as path name is too long\n",cur_path);
	  continue;
	}
	file = fopen(buf,"r");
	if (!file) continue; // couldn't open file
	parser = XML_ParserCreate(NULL);
	memset(&info,0,sizeof(struct hp_info));
	memset(&kit_info,0,sizeof(struct kit_info));
	info.kit_info = &kit_info;
	info.scan_only = 1;
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
	    break;
	  }
	} while (!done);
	XML_ParserFree(parser);
	if (info.kit_info->name) {
	  int i = 0;
	  scanned_kit* kit = malloc(sizeof(scanned_kit));
	  struct kit_list* node = malloc(sizeof(struct kit_list));
	  memset(kit,0,sizeof(scanned_kit));
	  memset(node,0,sizeof(struct kit_list));
	  kit->name = info.kit_info->name;
	  kit->desc = info.kit_info->desc;
	  
	  struct instrument_info *cur_i = info.kit_info->instruments;
	  while (cur_i) {
	    kit->samples++;
	    cur_i = cur_i->next;
	  }
	  kit->sample_names = malloc(kit->samples*sizeof(char*));
	  cur_i = info.kit_info->instruments;
	  while (cur_i) {
	    struct instrument_info *to_free = cur_i;
	    if (cur_i->name)
	      kit->sample_names[i++] = cur_i->name;
	    else
	      kit->sample_names[i++] = unknownstr;
	    cur_i = cur_i->next;
	    free(to_free);
	  }

	  snprintf(buf,BUFSIZ,"%s/%s/",cur_path,ep->d_name);
	  kit->path = strdup(buf);
	  node->skit = kit;
	  struct kit_list * cur_k = scanned_kits;
	  if (cur_k) {
	    while(cur_k->next) cur_k = cur_k->next;
	    cur_k->next = node;
	  } else
	    scanned_kits = node;
	}
      }
      (void) closedir (dp);
    }
    else if (errno != ENOENT)
      fprintf(stderr,"Couldn't open %s: %s\n",cur_path,strerror(errno));
    cur_path = default_drumkit_locations[cp++];
  }

  // valid kits are in scanned_kits at this point
  cp = 0;
  struct kit_list * cur_k = scanned_kits;
  while(cur_k) {
    //printf("found kit: %s\nat:%s\n\n",cur_k->skit->name,cur_k->skit->path);
    cur_k = cur_k->next;
    cp++;
  }

  printf("found %i kits\n",cp);
  ret->num_kits = cp;
  ret->kits = malloc(cp*sizeof(scanned_kit));

  cur_k = scanned_kits;
  cp = 0;
  while(cur_k) {
    ret->kits[cp].name = cur_k->skit->name;
    ret->kits[cp].desc = cur_k->skit->desc;
    ret->kits[cp].path = cur_k->skit->path;
    ret->kits[cp].samples = cur_k->skit->samples;
    ret->kits[cp].sample_names = cur_k->skit->sample_names;
    cp++;
    free(cur_k->skit);
    cur_k = cur_k->next;
    // free each node as we go along
    free(scanned_kits);
    scanned_kits = cur_k;
  }

  return ret;
}

void free_samples(drmr_sample* samples, int num_samples) {
  int i,j;
  for (i=0;i<num_samples;i++) {
    if (samples[i].layer_count == 0) {
      if (samples[i].info) free(samples[i].info);
      if (samples[i].data) free(samples[i].data);
    } else {
      for (j = 0;j < samples[i].layer_count;j++) {
	if (samples[i].layers[j].info) free(samples[i].layers[j].info);
	if (samples[i].layers[j].data) free(samples[i].layers[j].data);
      }
      free(samples[i].layers);
    }
  }
  free(samples);
}

void free_kits(kits* kits) {
  int i;
  for (i = 0;i < kits->num_kits;i++) {
    free(kits->kits[i].name);
    free(kits->kits[i].desc);
    free(kits->kits[i].path);
  }
  free(kits->kits);
  free(kits);
}

int load_sample(char* path, drmr_layer* layer, double target_rate) {
  SNDFILE* sndf;
  long size;
  
  //printf("Loading: %s\n",path);

  layer->info = malloc(sizeof(SF_INFO));
  memset(layer->info,0,sizeof(SF_INFO));
  sndf = sf_open(path,SFM_READ,layer->info);
  
  if (!sndf) {
    fprintf(stderr,"Failed to open sound file: %s - %s\n",path,sf_strerror(sndf));
    free(layer->info);
    return 1;
  }

  if (layer->info->channels > 2) {
    fprintf(stderr, "File has too many channels.  Can only handle mono/stereo samples\n");
    free(layer->info);
    return 1;
  }

  size = layer->info->frames * layer->info->channels;
  layer->limit = size;
  layer->data = malloc(size*sizeof(float));
  if (!layer->data) {
    fprintf(stderr,"Failed to allocate sample memory for %s\n",path);
    free(layer->info);
    return 1;
  }

  sf_read_float(sndf,layer->data,size);
  sf_close(sndf); 

  // convert rate if needed
  if (layer->info->samplerate != target_rate) {
    SRC_DATA src_data;
    int stat;
    double ratio = (target_rate/layer->info->samplerate);
    long out_frames = (long)ceil(layer->info->frames * ratio);
    long out_size = out_frames*layer->info->channels;
    float *data_out = malloc(sizeof(float)*out_size);

    src_data.data_in = layer->data;
    src_data.input_frames = layer->info->frames;
    src_data.data_out = data_out;
    src_data.output_frames = out_frames;
    src_data.src_ratio = ratio;

    stat = src_simple(&src_data,RATE_CONV_QUALITY,layer->info->channels);
    if (stat) {
      fprintf(stderr,"Failed to convert rate for %s: %s.  Using original rate\n",
	      path,src_strerror(stat));
      free(data_out);
      return 0;
    }

    if (src_data.input_frames_used != layer->info->frames)
      fprintf(stderr,"Didn't consume all input frames. used: %li  had: %li  gened: %li\n",
	      src_data.input_frames_used, layer->info->frames,src_data.output_frames_gen);

    free(layer->data);

    layer->data = data_out;
    layer->limit = src_data.output_frames_gen*layer->info->channels;
    layer->info->samplerate = target_rate;
    layer->info->frames = src_data.output_frames_gen;
  }
  return 0;
}

drmr_sample* load_hydrogen_kit(char *path, double rate, int *num_samples) {
  FILE* file;
  char buf[BUFSIZ];
  XML_Parser parser;
  int done;
  struct hp_info info;
  struct kit_info kit_info;
  drmr_sample *samples;
  struct instrument_info * cur_i, *i_to_free;
  int i = 0, num_inst = 0;

  snprintf(buf,BUFSIZ,"%s/drumkit.xml",path);
  
  printf("trying to load: %s\n",buf);

  file = fopen(buf,"r");
  if (!file) {
    perror("Unable to open file:");
    return NULL;
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
      return NULL;
    }
  } while (!done);
  XML_ParserFree(parser);

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
    if (cur_i->filename) { // top level filename, just make one dummy layer
      drmr_layer *layer = malloc(sizeof(drmr_layer));
      layer->min = 0;
      layer->max = 1;
      snprintf(buf,BUFSIZ,"%s/%s",path,cur_i->filename);
      if (load_sample(buf,layer,rate)) {
	fprintf(stderr,"Could not load sample: %s\n",buf);
	// set limit to zero, will never try and play
	layer->info = NULL;
	layer->limit = 0;
	layer->data = NULL;
      }
      samples[i].layer_count = 0;
      samples[i].layers = NULL;
      samples[i].offset = 0;
      samples[i].info = layer->info;
      samples[i].limit = layer->limit;
      samples[i].data = layer->data;
      free(layer);
    } else if (cur_i->layers) {
      int layer_count = 0;
      int j;
      struct instrument_layer *cur_l = cur_i->layers;
      while(cur_l) {
	layer_count++;
	cur_l = cur_l->next;
      }
      samples[i].layer_count = layer_count;
      samples[i].layers = malloc(sizeof(drmr_layer)*layer_count);
      cur_l = cur_i->layers;
      j = 0;
      while(cur_l) {
	snprintf(buf,BUFSIZ,"%s/%s",path,cur_l->filename);
	if (load_sample(buf,samples[i].layers+j,rate)) {
	  fprintf(stderr,"Could not load sample: %s\n",buf);
	  // set limit to zero, will never try and play
	  samples[i].layers[j].info = NULL;
	  samples[i].layers[j].limit = 0;
	  samples[i].layers[j].data = NULL;
	}
	samples[i].layers[j].min = cur_l->min;
	samples[i].layers[j].max = cur_l->max;
	j++;
	cur_l = cur_l->next;
      }
    } else { // no layer or file, empty inst
      samples[i].layer_count = 0;
      samples[i].layers = NULL;
      samples[i].offset = 0;
      samples[i].info = NULL;
      samples[i].limit = 0;
      samples[i].data = NULL;
    }
    samples[i].active = 0;
    i_to_free = cur_i;
    cur_i = cur_i->next;

    if (i_to_free->name) free(i_to_free->name);
    if (i_to_free->filename) free(i_to_free->filename);
    if (samples[i].layer_count > 0) {
      struct instrument_layer *ltf = i_to_free->layers;
      while (ltf) {
	free(ltf->filename);
	ltf = ltf->next;
      }
    }
    free(i_to_free);
    i++;
  }
  if (kit_info.name) free(kit_info.name);
  *num_samples = num_inst;
  return samples;
}

#ifdef _TEST_HYDROGEN_PARSER

int main(int argc, char* argv[]) {
  kits *kits;
  int i,j;
  kits = scan_kits();
  for (i=0;i<kits->num_kits;i++) {
    printf("\t%s:\n\t\tpath: %s\n\t\tsamples: %i\n",kits->kits[i].name,kits->kits[i].path,kits->kits[i].samples);
    printf("\t\t");
    for (j=0;j<kits->kits[i].samples;j++) {
      printf("%s, ",kits->kits[i].sample_names[j]);
    }
    printf("\n\n");
  }

  return 0;
}

#endif // _TEST_HYDROGEN_PARSER
