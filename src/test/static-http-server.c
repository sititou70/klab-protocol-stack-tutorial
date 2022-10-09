#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "driver/ether_tap.h"
#include "icmp.h"
#include "ip.h"
#include "net.h"
#include "tcp.h"
#include "test.h"
#include "util.h"

#define ENDPOINT "192.168.70.2:80"
#define MAX_URI_LEN 512
#define REQ_BUF_SIZE 2048
#define WORKER_THREAD_NUM 8
#define INDEX "index.html"
#define INDEX_LEN 11

/*
 * utils
 */

size_t uri_decode(const char *src, const size_t srclen, char *dst) {
  int src_i = 0;
  int dst_i = 0;

  while (src_i < srclen) {
    if (src[src_i] == '%' && src_i + 2 < srclen) {
      char code[] = {src[src_i + 1], src[src_i + 2], 0};
      dst[dst_i] = (char)strtol(code, NULL, 16);

      src_i += 3;
      dst_i += 1;
    } else {
      dst[dst_i] = src[src_i];

      src_i += 1;
      dst_i += 1;
    }
  }

  return dst_i;
}

/*
 * http handler
 */

static char *response_400 =
    "HTTP/1.1 400 Bad Request\r\n"
    "Content-Length: 0\r\n"
    "\r\n";
static char *response_404 =
    "HTTP/1.1 404 Not Found\r\n"
    "Content-Length: 0\r\n"
    "\r\n";
static char *response_414 =
    "HTTP/1.1 414 URI Too Long\r\n"
    "Content-Length: 0\r\n"
    "\r\n";
static char *response_500 =
    "HTTP/1.1 500 Internal Server Error\r\n"
    "Content-Length: 0\r\n"
    "\r\n";

void http_handler(int soc, uint8_t *reqbuf, size_t reqsize) {
  // assert start of request
  if (strncmp(reqbuf, "GET ", 4) != 0) {
    debugf("error");
    return;
  }

  // parse uri
  strtok((char *)reqbuf, " ?");
  char *orig_uri = strtok(NULL, " ?");
  char *orig_uri_end = strtok(NULL, " ?") - 1;
  if (orig_uri == NULL) {
    tcp_send(soc, (uint8_t *)response_400, strlen(response_400));
    return;
  }
  ssize_t orig_uri_len = orig_uri_end - orig_uri;
  if (orig_uri_len > MAX_URI_LEN) {
    tcp_send(soc, (uint8_t *)response_414, strlen(response_414));
    return;
  }

  char uri[MAX_URI_LEN + INDEX_LEN + 1] = {};
  ssize_t uri_len = uri_decode(orig_uri, orig_uri_len, uri);
  if (uri[uri_len - 1] == '/') {
    sprintf(uri + uri_len, INDEX);
  }

  // open and stat file
  int fd = open(uri + 1, O_RDONLY);
  if (fd == -1) {
    tcp_send(soc, (uint8_t *)response_404, strlen(response_404));
    return;
  }
  struct stat finfo;
  if (fstat(fd, &finfo) != 0) {
    tcp_send(soc, (uint8_t *)response_404, strlen(response_404));
    close(fd);
    return;
  }

  // response
  debugf("!! response: %s", uri);
  char headerbuf[64];
  sprintf(headerbuf,
          "HTTP/1.1 200 OK\r\n"
          "Content-Length: %ld\r\n"
          "\r\n",
          finfo.st_size);
  tcp_send(soc, (uint8_t *)headerbuf, strlen(headerbuf));

  uint8_t bodybuf[1400];
  int n_read;
  while ((n_read = read(fd, bodybuf, sizeof(bodybuf))) != 0) {
    if (n_read < 0) {
      tcp_send(soc, (uint8_t *)response_500, strlen(response_500));

      close(fd);
      return;
    }

    tcp_send(soc, bodybuf, n_read);
  }

  close(fd);
  return;
}

/*
 * main
 */

static volatile sig_atomic_t terminate;

static void on_signal(int s) {
  (void)s;
  terminate = 1;
  net_raise_event();
}

void *worker_thread(void *param) {
  struct ip_endpoint local;
  ip_endpoint_pton(ENDPOINT, &local);
  uint8_t reqbuf[REQ_BUF_SIZE];
  while (!terminate) {
    int soc = tcp_open_rfc793(&local, NULL, 0);
    if (soc == -1) {
      errorf("tcp_open_rfc793() failure");
      return NULL;
    }

    while (!terminate) {
      ssize_t ret = tcp_receive(soc, reqbuf, sizeof(reqbuf));
      hexdump(stderr, reqbuf, ret);
      if (ret == 0) break;

      http_handler(soc, reqbuf, ret);
    }

    tcp_close(soc);
  }

  return NULL;
}

int main(int argc, char *argv[]) {
  /*
   * setup
   */

  signal(SIGINT, on_signal);

  if (net_init() == -1) {
    errorf("net_init() failure");
    return -1;
  }

  struct net_device *dev = ether_tap_init(ETHER_TAP_NAME, ETHER_TAP_HW_ADDR);
  if (!dev) {
    errorf("ether_tap_init() failure");
    return -1;
  }

  struct ip_iface *iface = ip_iface_alloc(ETHER_TAP_IP_ADDR, ETHER_TAP_NETMASK);
  if (!iface) {
    errorf("ip_iface_alloc() failure");
    return -1;
  }
  if (ip_iface_register(dev, iface) == -1) {
    errorf("ip_iface_register() failure");
    return -1;
  }

  if (net_run() == -1) {
    errorf("net_run() failure");
    return -1;
  }

  /*
   * main
   */

  pthread_t thread;
  for (int i = 0; i < WORKER_THREAD_NUM; i++) {
    if (pthread_create(&thread, NULL, worker_thread, NULL) != 0) {
      errorf("failed for creating worker thread");
      exit(1);
    }
  }
  if (pthread_join(thread, NULL) != 0) {
    errorf("failed for pthread_join");
    exit(1);
  }

  /*
   * cleanup
   */

  sleep(1);
  net_shutdown();

  return 0;
}