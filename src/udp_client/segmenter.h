#ifndef SEGMENTER_H_INCLUDED
#define SEGMENTER_H_INCLUDED

typedef struct segmenter segmenter;
struct segmenter
{
  int        pid;
  ts_packets packets;
  size_t     markers;
  double     time;
};

void    segmenter_construct(segmenter *, int);
void    segmenter_destruct(segmenter *);
ssize_t segmenter_write(segmenter *, void *, size_t);
double  segmenter_time(segmenter *);
ssize_t segmenter_pop(segmenter *, ts_packets *);

#endif /* SEGMENTER_H_INCLUDED */
