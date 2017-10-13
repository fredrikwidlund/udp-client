#ifndef RTP_H_INCLUDED
#define RTP_H_INLCUDED

typedef struct rtp_receiver rtp_receiver;
struct rtp_receiver
{
  list data;
  list fec;
};

void rtp_receiver_construct(rtp_receiver *);

#endif /* RTP_H_INCLUDED */
