#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <netdb.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <err.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <jansson.h>
#include <dynamic.h>
#include <reactor.h>
#include <ts.h>

#include "rtp.h"

typedef struct input_socket input_socket;
typedef struct input input;

struct input_socket
{
  input        *input;
  int           fd;
  int           type;
};

struct input
{  
  size_t        index;
  list          sockets;
  rtp_receiver  rtp;
};

static void usage()
{
  extern char *__progname;

  (void) fprintf(stderr, "Usage: %s [CONFIGURATION FILE]\n", __progname);
  exit(EXIT_FAILURE);
}

/*
static uint64_t ntime(void)
{
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

static void input_socket_event(void *state, int type, void *data)
{
  input_socket *socket = state;
  uint8_t block[16384];
  ssize_t n;

  (void) data;
  switch (type)
    {
    case REACTOR_CORE_FD_EVENT_READ:
      n = read(socket->fd, block, sizeof block);
      if (n == -1)
	break;
      //if (n == 1328)
      //debug(input, block + 12, n - 12);
      (void) fprintf(stderr, "read %ld\n", n);
      break;
    default:
      errx(1, "invalid event");
    }
}

input_socket *input_socket_new(json_t *conf, input *input)
{
  input_socket *s;
  const char *node, *service, *type_string;
  struct addrinfo *addrinfo;
  struct sockaddr_in *sin;
  int e, fd, type;

  node = json_string_value(json_object_get(conf, "node"));
  service = json_string_value(json_object_get(conf, "service"));
  type_string = json_string_value(json_object_get(conf, "type"));
  if (!node || !service || !type_string)
    return NULL;
  if (strcmp(type_string, "data") == 0)
    type = 0;
  else if (strcmp(type_string, "fec") == 0)
    type = 1;
  else
    return NULL;

  e = getaddrinfo(node, service, (struct addrinfo[]){{.ai_family = AF_INET, .ai_socktype = SOCK_DGRAM, .ai_flags = AI_NUMERICHOST | AI_NUMERICSERV}}, &addrinfo);
  if (e == -1)
    return NULL;

  fd = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
  if (fd == -1)
    return NULL;

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
          return NULL;
        }
    }

  e = fcntl(fd, F_SETFL, O_NONBLOCK);
  if (e == -1)
    {
      (void) close(fd);
      return NULL;
    }
  
  e = bind(fd, addrinfo->ai_addr, addrinfo->ai_addrlen);
  if (e == -1)
    {
      (void) close(fd);
      return NULL;
    }

  s = malloc(sizeof *s);
  if (!s)
    abort();
  *s = (input_socket) {.input = input, .fd = fd, .type = type};  

  reactor_core_fd_register(fd, input_socket_event, s, REACTOR_CORE_FD_MASK_READ);
  return s;
}

void input_socket_delete(input_socket *s)
{
  reactor_core_fd_deregister(s->fd);
  (void) close(s->fd);
  free(s);
}

int input_open(input *input, json_t *conf)
{
  json_t *socket_conf;
  input_socket *socket;
  size_t i;

  json_array_foreach(json_object_get(conf, "sockets"), i, socket_conf)
    {
      socket = input_socket_new(socket_conf, input);
      if (!socket)
        return -1;
      list_push_back(&input->sockets, &socket, sizeof socket);
    }        

  return 0;
}

static void input_sockets_release(void *object)
{
  input_socket_delete(*(input_socket **) object);
}

void input_delete(input *input)
{
  list_destruct(&input->sockets, input_sockets_release);
  free(input);
}

input *input_new(size_t index, json_t *conf)
{
  input *input;
  int e;

  input = malloc(sizeof *input);
  if (!input)
    abort();
  *input = (struct input) {.index = index};
  list_construct(&input->sockets);
  rtp_receiver_construct(&input->rtp);

  e = input_open(input, conf);
  if (e == -1)
    {
      input_delete(input);
      return NULL;
    }

  return input;
}

int main(int argc, char **argv)
{
  json_t *conf, *input_conf;
  list inputs;
  input *input;
  size_t i;
  int e;

  if (argc != 2)
    usage();
  conf = json_load_file(argv[1], 0, NULL);
  if (!conf)
    err(1, "invalid configuration");
  list_construct(&inputs);

  reactor_core_construct();

  json_array_foreach(json_object_get(conf, "inputs"), i, input_conf)
    {
      input = input_new(i, input_conf);
      if (!input)
	errx(1, "error creating input");
      list_push_back(&inputs, &input, sizeof input);
    }

  e = reactor_core_run();
  if (e == -1)
    err(1, "reactor_core_run");

  reactor_core_destruct();

  exit(EXIT_FAILURE);
}
