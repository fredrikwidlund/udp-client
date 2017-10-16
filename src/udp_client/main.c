#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <netdb.h>
#include <time.h>
#include <err.h>
#include <sys/queue.h>

#include <jansson.h>
#include <dynamic.h>
#include <reactor.h>
#include <ts.h>

#include "rtp.h"
#include "segmenter.h"
#include "input.h"
#include "output.h"

typedef struct mixer mixer;
struct mixer
{
  double        duration;
  reactor_timer timer;
  inputs        inputs;
  output        output;
};

static void usage()
{
  extern char *__progname;

  (void) fprintf(stderr, "Usage: %s [CONFIGURATION FILE]\n", __progname);
  exit(EXIT_FAILURE);
}

static uint64_t ntime(void)
{
  struct timespec ts;

  (void) clock_gettime(CLOCK_REALTIME_COARSE, &ts);
  return ((uint64_t) ts.tv_sec * 1000000000) + ((uint64_t) ts.tv_nsec);
}

static void mixer_process(mixer *mixer)
{
  double t, time;
  input *input, **i;
  list *streams;
  ssize_t n;

  t = (double) ntime() / 1000000000;
  streams = NULL;
  list_foreach(&mixer->inputs.list, i)
    {
      n = input_pop(*i, &streams);
      if (n == 1)
        break;
    }

  if (!streams)
    {
      fprintf(stderr, "no streams %.03f\n", t);
      return;
    }
  
}

static void mixer_event(void *state, int type, void *data)
{
  mixer *mixer = state;

  (void) data;
  switch (type)
    {
    case REACTOR_TIMER_EVENT_CALL:
      mixer_process(mixer);
      break;
    default:
      errx(1, "unexpected event: %d", type);
    }
}

static int mixer_construct(mixer *mixer, json_t *conf)
{
  int e;

  if (!conf)
    return -1;

  *mixer = (struct mixer) {0};
  mixer->duration = json_number_value(json_object_get(conf, "duration"));

  e = inputs_construct(&mixer->inputs, json_object_get(conf, "inputs"));
  if (e == -1)
    return -1;

  e = output_construct(&mixer->output, json_object_get(conf, "output"));
  if (e == -1)
    return -1;

  reactor_timer_open(&mixer->timer, mixer_event, mixer, mixer->duration * 1000000000, mixer->duration * 1000000000);
  mixer_process(mixer);
  return 0;
}

int main(int argc, char **argv)
{
  json_t *conf;
  mixer mixer;
  int e;

  if (argc != 2)
    usage();

  conf = json_load_file(argv[1], 0, NULL);
  if (!conf)
    err(1, "json_load_file %s", argv[1]);

  reactor_core_construct();  
  e = mixer_construct(&mixer, conf);
  if (e == -1)
    err(1, "mixer_construct");

  e = reactor_core_run();
  if (e == -1)
    err(1, "reactor_core_run");

  reactor_core_destruct();

  exit(EXIT_FAILURE);
}
