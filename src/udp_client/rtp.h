#ifndef RTP_H_INCLUDED
#define RTP_H_INLCUDED

#define RTP_MAX_DISTANCE   10
#define RTP_MAX_DATA_COUNT 256

typedef struct rtp rtp;
typedef struct rtp_receiver rtp_receiver;

enum rtp_type
{
  RTP_TYPE_DATA,
  RTP_TYPE_FEC
};

struct rtp
{
  unsigned    v:2;
  unsigned    p:1;
  unsigned    x:1;
  unsigned    cc:4;
  unsigned    m:1;
  unsigned    pt:7;
  uint16_t    sequence_number;
  uint32_t    timestamp;
  uint32_t    ssrc;
  uint32_t   *csrc;
  uint16_t    extension_id;
  uint16_t    extension_length;
  void       *extension;
  void       *data;
  size_t      size;
};

struct rtp_receiver
{
  list       data;
  size_t     data_count;
  rtp      **data_iterator;
  list       fec;
};

rtp     *rtp_new(void *, size_t);
void     rtp_delete(rtp *);

void     rtp_receiver_construct(rtp_receiver *);
void     rtp_receiver_destruct(rtp_receiver *);
ssize_t  rtp_receiver_write(rtp_receiver *, void *, size_t, int);
rtp     *rtp_receiver_read(rtp_receiver *);

#endif /* RTP_H_INCLUDED */
