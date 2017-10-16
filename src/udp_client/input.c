#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <err.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <jansson.h>
#include <dynamic.h>
#include <reactor.h>
#include <ts.h>

#include "rtp.h"
#include "segmenter.h"
#include "input.h"

static void input_error(input *input, const char *format, ...)
{
  char reason[4096];
  va_list ap;

  (void) input;
  va_start(ap, format);
  (void) vsnprintf(reason, sizeof reason, format, ap);
  va_end(ap);

  err(1, "input %s error: %s", input->id, reason);
}

static int input_socket_open(const char *node, int port)
{
  char service[32];
  struct addrinfo *addrinfo;
  struct sockaddr_in *sin;
  int e, fd;

  (void) snprintf(service, sizeof service, "%d", port);
  
  e = getaddrinfo(node, service, (struct addrinfo[]){{.ai_family = AF_INET, .ai_socktype = SOCK_DGRAM,
          .ai_flags = AI_NUMERICHOST | AI_NUMERICSERV}}, &addrinfo);
  if (e == -1)
    return -1;
  
  fd = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
  if (fd == -1)
    return -1;
  
  (void) setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (int[]){1}, sizeof(int));
  (void) setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (int[]){1048576}, sizeof(int));

  sin = (struct sockaddr_in *) addrinfo->ai_addr;
  if (IN_MULTICAST(ntohl(sin->sin_addr.s_addr)))
    {
      e = setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                     (struct ip_mreq[]){{.imr_multiaddr = sin->sin_addr, .imr_interface.s_addr = htonl(INADDR_ANY)}},
                     sizeof(struct ip_mreq));
      if (e == -1)
        {
          (void) close(fd);
          return -1;
        }
    }

  e = fcntl(fd, F_SETFL, O_NONBLOCK);
  if (e == -1)
    {
      (void) close(fd);
      return -1;
    }
  
  e = bind(fd, addrinfo->ai_addr, addrinfo->ai_addrlen);
  if (e == -1)
    {
      (void) close(fd);
      return -1;
    }

  return fd;
}

void input_socket_event(void *state, int type, void *data)
{
  input_socket *socket = state;
  uint8_t block[16384];
  ssize_t n;
  void *base;
  size_t size;

  (void) data;
  switch (type)
    {
    case REACTOR_CORE_FD_EVENT_READ:
      n = read(socket->fd, block, sizeof block);
      if (n == -1)
        err(1, "read");
 
      n = rtp_receiver_write(&socket->stream->receiver, block, n, socket->type);
      if (n == -1)
        errx(1, "rtp_receiver_write");

      while (1)
        {
          n = rtp_receiver_read(&socket->stream->receiver, &base, &size);
          if (n == -1)
            errx(1, "rtp_receiver_read");

          if (!n)
            break;

          n = segmenter_write(&socket->stream->segmenter, base, size);
          if (n == -1)
            errx(1, "segmenter_write");

          if (n == 1)
            input_update(socket->stream->input);
        }
      break;
    default:
      errx(1, "invalid event");
    }
}


int input_socket_construct(input_socket *socket, input_stream *stream, int type, json_t *conf)
{
  *socket = (input_socket) {.stream = stream, .type = type};
  socket->port = json_number_value(conf);
  socket->fd = input_socket_open(socket->stream->address, socket->port);
  if (socket->fd == -1)
    {
      input_socket_destruct(socket);
      return -1;
    }
  reactor_core_fd_register(socket->fd, input_socket_event, socket, REACTOR_CORE_FD_MASK_READ);

  return 0;
}

void input_socket_destruct(input_socket *socket)
{
  fprintf(stderr, "[inputs socket %p]\n", (void *) socket); 
}

int input_stream_construct(input_stream *stream, input *input, json_t *conf)
{
  json_t *socket_conf;
  input_socket *socket;
  size_t i;
  int e;

  *stream = (input_stream) {.input = input};
  stream->id = json_string_value(json_object_get(conf, "id"));
  stream->address = json_string_value(json_object_get(conf, "address"));

  list_construct(&stream->sockets);
  rtp_receiver_construct(&stream->receiver);
  segmenter_construct(&stream->segmenter, json_number_value(json_object_get(conf, "pid")));

  json_array_foreach(json_object_get(conf, "ports"), i, socket_conf)
    {
      socket = malloc(sizeof *socket);
      if (!socket)
        abort();
      e = input_socket_construct(socket, stream, i == 0 ? RTP_TYPE_DATA : RTP_TYPE_FEC, socket_conf);
      if (e == -1)
        {
          free(socket);
          input_stream_destruct(stream);
          return -1;
        }
    }

  return 0;
}

void input_stream_destruct(input_stream *stream)
{
  fprintf(stderr, "[input stream %p]\n", (void *) stream); 
}


int input_construct(input *input, inputs *inputs, json_t *conf)
{
  json_t *stream_conf;
  input_stream *stream;
  size_t i;
  int e;

  *input = (struct input) {.inputs = inputs, .state = INPUT_STATE_BUFFERING};
  input->id = json_string_value(json_object_get(conf, "id"));
  input->buffer = json_integer_value(json_object_get(conf, "buffer"));
  list_construct(&input->streams);
  
  json_array_foreach(json_object_get(conf, "streams"), i, stream_conf)
    {
      stream = malloc(sizeof *stream);
      if (!stream)
        abort();
      e = input_stream_construct(stream, input, stream_conf);
      if (e == -1)
        {
          free(stream);
          input_destruct(input);
          return -1;
        }
      list_push_back(&input->streams, &stream, sizeof stream);
    }

  return 0;
}

void input_destruct(input *input)
{
  fprintf(stderr, "[input destruct %p]\n", (void *) input); 
}

void input_align(input *input)
{
  input_stream **i;
  segmenter *s;
  uint64_t max;

  max = 0;
  list_foreach(&input->streams, i)
    {
      s = &(*i)->segmenter;
      if (segmenter_time(s) > 0 && segmenter_time(s) > max)
        max = segmenter_time(s);
    }
  if (max == 0)
    return;

  list_foreach(&input->streams, i)
    {
      s = &(*i)->segmenter;
      while (segmenter_time(s) > 0 && max - segmenter_time(s) > 0.1)
        (void) segmenter_pop(s, NULL);
      if (!segmenter_time(s))
        return;
    }  
}

double input_time(input *input)
{
  input_stream **i;
  segmenter *s;
  double min;
  
  input_align(input);
  min = UINT32_MAX;
  list_foreach(&input->streams, i)
    {
      s = &(*i)->segmenter;
      if (s->markers < input->buffer + 1)
        return 0;
      if (segmenter_time(s) < min)
        min = segmenter_time(s);
    }
  input->state = INPUT_STATE_RUNNING;
  return min;
}

void input_update(input *input)
{
  input_align(input);
}

int input_state(input *input)
{
  return input->state;
}

int inputs_construct(inputs *inputs, json_t *conf)
{
  json_t *input_conf;
  input *input;
  size_t i;
  int e;

  list_construct(&inputs->list);
  json_array_foreach(conf, i, input_conf)
    {
      input = malloc(sizeof *input);
      if (!input)
        abort();
      e = input_construct(input, inputs, input_conf);
      if (e == -1)
        {
          free(input);
          inputs_destruct(inputs);
          return -1;
        }
      list_push_back(&inputs->list, &input, sizeof input);
    }

  return 0;  
}

void inputs_destruct(inputs *inputs)
{
  fprintf(stderr, "[inputs destruct %p]\n", (void *) inputs);
}

/*
void inputs_process(inputs *inputs)
{
  input **i;

  list_foreach(&inputs->list, i)
    input_process(*i);
}

void input_process(input *input)
{
  input_stream **i;
  segmenter *s;
  double max, min;
  ts_packets packets;
  ts_units units;
  ts_stream stream;
  ts_stream_pes **sp;
  ssize_t n;
  char path[PATH_MAX];
  int e, pid;
  
  max = 0;
  list_foreach(&input->streams, i)
    {
      s = &(*i)->segmenter;
      if (segmenter_time(s) > 0 && segmenter_time(s) > max)
        max = segmenter_time(s);
    }
  if (max == 0)
    return;

  min = UINT32_MAX;
  list_foreach(&input->streams, i)
    {
      s = &(*i)->segmenter;
      while (segmenter_time(s) > 0 && max - segmenter_time(s) > 0.1)
        (void) segmenter_pop(s, NULL);
      if (!segmenter_time(s))
        return;
      if (min > segmenter_time(s))
        min = segmenter_time(s);
    }
  
  (void) snprintf(path, sizeof path, "./work/%.03f", min);
  e = mkdir(path, 0755);
  if (e == -1)
    err(1, "mkdir %s", path);

  list_foreach(&input->streams, i)
    {
      s = &(*i)->segmenter;

      ts_packets_construct(&packets);
      n = segmenter_pop(s, &packets);

      ts_units_construct(&units);
      n = ts_units_unpack(&units, &packets);
      ts_packets_destruct(&packets);
      if (n == -1)
        input_error(input, "ts_units_unpack stream %s", (*i)->id);
      
      ts_stream_construct(&stream);
      n = ts_stream_unpack(&stream, &units);
      ts_units_destruct(&units);
      if (n == -1)
        input_error(input, "ts_stream_unpack %s", (*i)->id);

      do
        {
          pid = 0;
          list_foreach(&stream.streams, sp)
            if ((*sp)->pid != (*i)->segmenter.pid)
              pid = (*sp)->pid;
          if (pid)
            ts_stream_delete(&stream, pid);
        }
      while (pid);

      (void) snprintf(path, sizeof path, "./work/%.03f/%s.ts", min, (*i)->id);
      n = ts_stream_save(&stream, path);
      if (n == -1)
        input_error(input, "ts_stream_save %s", (*i)->id);

      ts_stream_destruct(&stream);
    }

  (void) fprintf(stdout, "%.03f\n", max);
}
*/
