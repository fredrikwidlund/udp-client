#ifndef INPUT_H_INCLUDED
#define INPUT_H_INCLUDED

typedef struct input_socket input_socket;
typedef struct input_stream input_stream;
typedef struct input input;
typedef struct inputs inputs;

enum input_state
{
  INPUT_STATE_CLOSED,
  INPUT_STATE_BUFFERING,
  INPUT_STATE_RUNNING
};

struct input_socket
{
  input_stream *stream;
  int           port;
  int           fd;
  int           type;
};

struct input_stream
{
  input        *input;
  const char   *id;
  const char   *address;
  list          sockets;
  rtp_receiver  receiver;
  segmenter     segmenter;  
};

struct input
{
  inputs       *inputs;
  int           state;
  size_t        buffer;
  const char   *id;
  list          streams;
};

struct inputs
{
  list          list;
};

int    input_socket_construct(input_socket *, input_stream *, int, json_t *);
void   input_socket_destruct(input_socket *);

int    input_stream_construct(input_stream *, input *, json_t *);
void   input_stream_destruct(input_stream *);

int    input_construct(input *, inputs *, json_t *);
void   input_destruct(input *);
void   input_process(input *);
double input_time(input *);
void   input_update(input *);
int    input_state(input *);

int    inputs_construct(inputs *, json_t *);
void   inputs_destruct(inputs *);
void   inputs_process(inputs *);



#endif /* INPUT_H_INCLUDED */
