#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "driver/ether_tap.h"
#include "icmp.h"
#include "ip.h"
#include "net.h"
#include "tcp.h"
#include "test.h"
#include "util.h"

static volatile sig_atomic_t terminate;

static void on_signal(int s) {
  (void)s;
  terminate = 1;
  net_raise_event();
}

static int setup(void) {
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
  if (ip_route_set_default_gateway(iface, DEFAULT_GATEWAY) == -1) {
    errorf("ip_route_set_default_gateway() failure");
    return -1;
  }

  if (net_run() == -1) {
    errorf("net_run() failure");
    return -1;
  }

  return 0;
}

static void cleanup(void) {
  sleep(1);
  net_shutdown();
}

int main(int argc, char *argv[]) {
  if (setup() == -1) {
    errorf("setup() failure");
    return -1;
  }

  char *response =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 64\r\n"
      "\r\n"
      "<html><head><title>hello</title></head><body>world</body></html>";

  struct ip_endpoint local;
  ip_endpoint_pton("192.168.70.2:80", &local);
  uint8_t buf[2048];
  while (!terminate) {
    int soc = tcp_open_rfc793(&local, NULL, 0);
    if (soc == -1) {
      errorf("tcp_open_rfc793() failure");
      return -1;
    }

    while (!terminate) {
      ssize_t ret = tcp_receive(soc, buf, sizeof(buf));
      hexdump(stderr, buf, ret);
      if (ret == 0) break;

      tcp_send(soc, (uint8_t *)response, strlen(response));
    }

    tcp_close(soc);
  }

  cleanup();

  return 0;
}