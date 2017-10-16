
/*
static uint64_t ntime(void)
{r
  struct timespec ts;

  (void) clock_gettime(CLOCK_REALTIME, &ts);
  return ((uint64_t) ts.tv_sec * 1000000000) + ((uint64_t) ts.tv_nsec);
}

static void debug(input *input, uint8_t *data, size_t size)
{
  ts_packet p;
  stream s;
  ssize_t n;
  double t1, t2;
  const uint64_t unix_epoch = 2208988800UL;

  while (size >= 188)
    {
      stream_construct(&s, data, 188);
      ts_packet_construct(&p);
      n = ts_packet_unpack(&p, &s);
      if (n == -1)
	errx(1, "ts_packet_unpack");

      if (p.adaptation_field.ebp.marker)
	{
	  t1 = (double) (p.adaptation_field.ebp.time >> 32) + (double) (p.adaptation_field.ebp.time & 0xffffffff) / (double) (1ULL << 32);
	  t1 -= unix_epoch;
	  t2 = (double) ntime() / (double) 1000000000;
	  (void) fprintf(stderr, "index %lu time ebp %f local %f diff %f\n", input->index, t1, t2, t2 - t1);
	}
      ts_packet_destruct(&p);
      stream_destruct(&s);
      data += 188;
      size -= 188;
    }
}
*/
