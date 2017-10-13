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

typedef struct input input;
struct input
{
  int fd;
};

static void usage()
{
  extern char *__progname;

  (void) fprintf(stderr, "Usage: %s [CONFIGURATION FILE]\n", __progname);
  exit(EXIT_FAILURE);
}

static void input_event(void *state, int type, void *data)
{
  fprintf(stderr, "%p %d %p\n", state, type, data);
}

int input_open(input *input, const char *node, const char *service)
{
  struct addrinfo *addrinfo;
  struct sockaddr_in *sin;
  int e;

  if (!node || !service)
    return -1;

  e = getaddrinfo(node, service, (struct addrinfo[]){{.ai_family = AF_INET, .ai_socktype = SOCK_DGRAM, .ai_flags = AI_NUMERICHOST | AI_NUMERICSERV}}, &addrinfo);
  if (e == -1)
    return -1;

  input->fd = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
  if (input->fd == -1)
    return -1;

  (void) setsockopt(input->fd, SOL_SOCKET, SO_REUSEADDR, (int[]){1}, sizeof(int));
  (void) setsockopt(input->fd, SOL_SOCKET, SO_RCVBUF, (int[]){1048576}, sizeof(int));

  sin = (struct sockaddr_in *) addrinfo->ai_addr;
  if (IN_MULTICAST(ntohl(sin->sin_addr.s_addr)))
    {
      e = setsockopt(input->fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                     (struct ip_mreq[]){{.imr_multiaddr = sin->sin_addr, .imr_interface.s_addr = htonl(INADDR_ANY)}},
                     sizeof(struct ip_mreq));
      if (e == -1)
        return -1;
    }

  e = fcntl(input->fd, F_SETFL, O_NONBLOCK);
  if (e == -1)
    return -1;

  e = bind(input->fd, addrinfo->ai_addr, addrinfo->ai_addrlen);
  if (e == -1)
    return -1;

  reactor_core_fd_register(input->fd, input_event, input, REACTOR_CORE_FD_MASK_READ);
  return 0;
}

void input_delete(input *input)
{
  if (input->fd >= 0)
    close(input->fd);
  free(input);
}

input *input_new(json_t *conf)
{
  input *input;
  int e;

  input = malloc(sizeof *input);
  if (!input)
    abort();
  *input = (struct input) {.fd = -1};

  e = input_open(input, json_string_value(json_object_get(conf, "node")), json_string_value(json_object_get(conf, "service")));
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
  list_construct(&inputs);

  reactor_core_construct();

  json_array_foreach(json_object_get(conf, "inputs"), i, input_conf)
    {
      input = input_new(input_conf);
      list_push_back(&inputs, &input, sizeof input);
    }

  e = reactor_core_run();
  if (e == -1)
    err(1, "reactor_core_run");

  reactor_core_destruct();

  exit(EXIT_FAILURE);
}
