#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <dynamic.h>

#include "rtp.h"

int rtp_construct(rtp *rtp, void *data, size_t size)
{
  stream s;
  uint32_t v;
  uint8_t *p;
  size_t i, n;
  int valid;

  *rtp = (struct rtp) {0};
  stream_construct(&s, data, size);
  v = stream_read16(&s);
  rtp->v = stream_read_bits(v, 16, 0, 2);
  rtp->p = stream_read_bits(v, 16, 2, 1);
  rtp->x = stream_read_bits(v, 16, 3, 1);
  rtp->cc = stream_read_bits(v, 16, 4, 4);
  rtp->m = stream_read_bits(v, 16, 8, 1);
  rtp->pt = stream_read_bits(v, 16, 9, 7);
  rtp->sequence_number = stream_read16(&s);
  rtp->timestamp = stream_read32(&s);
  rtp->ssrc = stream_read32(&s);
  if (rtp->cc)
    {
      rtp->csrc = calloc(rtp->cc, sizeof *rtp->csrc);
      for (i = 0; i < rtp->cc; i ++)
        rtp->csrc[i] = stream_read32(&s);
    }
  if (rtp->x)
    {
      rtp->extension_id = stream_read16(&s);
      rtp->extension_length = stream_read16(&s);
      if (rtp->extension_length > stream_size(&s))
        {
          stream_destruct(&s);
          return -1;
        }
      rtp->extension = malloc(rtp->extension_length);
      stream_read(&s, rtp->extension, rtp->extension_length);
    }

  p = stream_data(&s);
  n = stream_size(&s);
  valid = stream_valid(&s);
  stream_destruct(&s);
  if (!valid)
    return -1;
  if (rtp->p)
    {
      if (!n || p[n - 1] > n)
        return -1;
      n -= p[n - 1];
    }
  rtp->size = n;
  rtp->data = malloc(rtp->size);
  if (!rtp->data)
    abort();
  memcpy(rtp->data, p, rtp->size);

  return 0;
}

void rtp_destruct(rtp *rtp)
{
  free(rtp->csrc);
  free(rtp->extension);
  free(rtp->data);
}

rtp *rtp_new(void *data, size_t size)
{
  rtp *rtp;
  int e;

  rtp = malloc(sizeof *rtp);
  if (!rtp)
    abort();
  e = rtp_construct(rtp, data, size);
  if (e == -1)
    {
      rtp_delete(rtp);
      return NULL;
    }
  return rtp;
}

void rtp_delete(rtp *rtp)
{
  rtp_destruct(rtp);
  free(rtp);
}

int16_t rtp_distance(rtp *a, rtp *b)
{
  return b->sequence_number - a->sequence_number;
}

void rtp_receiver_construct(rtp_receiver *r)
{
  *r = (rtp_receiver) {0};
  list_construct(&r->data);
}

void rtp_receiver_destruct(rtp_receiver *r)
{
  (void) r;
}

static int rtp_receiver_enqueue_data(rtp_receiver *r, rtp *f)
{
  rtp **i;
  int16_t d;
  
  if (r->data_count >= RTP_MAX_DATA_COUNT)
    {
      rtp_delete(f);
      return -1;
    }
  
  list_foreach_reverse(&r->data, i)
    {
      d = rtp_distance(*i, f);
      if (d > RTP_MAX_DISTANCE || d < -RTP_MAX_DISTANCE)  
        {
          rtp_delete(f);
          return -1;
        }
      if (d > 0)
        break;
      if (!d)
        {
          rtp_delete(f);
          return 0;
        }
    }

  list_insert(list_next(i), &f, sizeof f);
  r->data_count ++;
  return 1;
}

ssize_t rtp_receiver_write(rtp_receiver *r, void *data, size_t size, int type)
{
  rtp *f;
  int e;
  
  (void) type;
  
  f = rtp_new(data, size);
  if (!f)
    return -1;
  switch (type)
    {
    case RTP_TYPE_DATA:
      e = rtp_receiver_enqueue_data(r, f);
      return e == -1 ? -1 : (ssize_t) size;
    case RTP_TYPE_FEC:
      rtp_delete(f);
      return 0;
    }

  return -1;
}

static void rtp_receiver_data_release(void *object)
{
  rtp_delete(*(rtp **) object);
}

static void rtp_receiver_flush(rtp_receiver *r)
{
  rtp **i;
  
  if (!r->data_iterator)
    return;

  while (1)
    {
      i = list_front(&r->data);
      if (rtp_distance(*i, *(r->data_iterator)) <= 0)
        break;
      list_erase(i, rtp_receiver_data_release);
      r->data_count --;
    }
}

rtp *rtp_receiver_read(rtp_receiver *r)
{
  rtp **i;

  if (list_empty(&r->data))
    return NULL;
  
  if (!r->data_iterator)
    i = list_front(&r->data);
  else
    {
      i = list_next(r->data_iterator);
      if (i == list_end(&r->data))
        return NULL;

      if (rtp_distance(*(r->data_iterator), *i) != 1)
        return NULL;
    }

  r->data_iterator = i;
  rtp_receiver_flush(r);
  return *r->data_iterator;
}
