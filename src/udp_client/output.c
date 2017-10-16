#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <netdb.h>
#include <sys/queue.h>
#include <err.h>

#include <jansson.h>
#include <dynamic.h>
#include <reactor.h>
#include <ts.h>

#include "rtp.h"
#include "segmenter.h"
#include "input.h"
#include "output.h"

int output_construct(output *output, json_t *conf)
{
  *output = (struct output) {0};

  (void) conf;
  return 0;
}

void output_destruct(output *output)
{
  (void) output;
}
