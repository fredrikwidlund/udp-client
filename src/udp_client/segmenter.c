#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#include <dynamic.h>
#include <ts.h>

#include "segmenter.h"

void segmenter_construct(segmenter *s, int pid)
{
  *s = (segmenter) {.pid = pid};
  ts_packets_construct(&s->packets);
}

void segmenter_destruct(segmenter *s)
{
  ts_packets_destruct(&s->packets);
}

void segmenter_ntp_stamp(segmenter *s, uint64_t t)
{
  const uint64_t unix_epoch = 2208988800UL;

  s->time = (double) (t >> 32) + (double) (t & 0xffffffff) / (double) (1ULL << 32) - unix_epoch;
  s->time = round(s->time * 1000) / 1000;
}

ssize_t segmenter_write(segmenter *s, void *data, size_t size)
{
  stream stream;
  ssize_t n;
  ts_packets packets;
  ts_packet **i;
  ssize_t new_marker;
 
  stream_construct(&stream, data, size);
  ts_packets_construct(&packets);
  n = ts_packets_unpack(&packets, &stream);
  stream_destruct(&stream);
  if (n == -1)
    return -1;

  new_marker = 0;
  list_foreach(ts_packets_list(&packets), i)
    if (s->pid == (*i)->pid && (*i)->adaptation_field.ebp.marker)
      {
        if (!s->markers)
          segmenter_ntp_stamp(s, (*i)->adaptation_field.ebp.time);
        s->markers ++;
        new_marker = 1;
      }

  ts_packets_append(&s->packets, &packets);  
  ts_packets_destruct(&packets);

  return new_marker;
}

double segmenter_time(segmenter *s)
{
  return s->markers >= 2 ? s->time : 0;
}

ssize_t segmenter_pop(segmenter *s, ts_packets *packets)
{
  ts_packet **i;
  ssize_t count;

  if (s->markers < 2)
    return - 1;

  count = 0;
  while (!list_empty(ts_packets_list(&s->packets)))
    {
      i = list_front(ts_packets_list(&s->packets));
      if (count == 0 && !(s->pid == (*i)->pid && (*i)->adaptation_field.ebp.marker))
        {
          ts_packet_destruct(*i);
          free(*i);
          list_erase(i, NULL);
          continue;
        }
      if (count > 0 && s->pid == (*i)->pid && (*i)->adaptation_field.ebp.marker)
        {
          segmenter_ntp_stamp(s, (*i)->adaptation_field.ebp.time);
          s->markers --;
          break;
        }
      if (packets)
        list_push_back(&packets->list, i, sizeof *i);
      else
        {
          ts_packet_destruct(*i);
          free(*i);
        }
      list_erase(i, NULL);
      count ++;
    }

  return count;
}
