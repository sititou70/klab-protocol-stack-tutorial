#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include "driver/ether_tap.h"
#include "driver/loopback.h"
#include "icmp.h"
#include "ip.h"
#include "net.h"
#include "tcp.h"
#include "test.h"
#include "udp.h"
#include "util.h"

static volatile sig_atomic_t terminate;

static void on_signal(int s) {
  (void)s;
  terminate = 1;
  net_raise_event();
}

static int setup(void) {
  struct net_device *dev;
  struct ip_iface *iface;

  signal(SIGINT, on_signal);
  if (net_init() == -1) {
    errorf("net_init() failure");
    return -1;
  }
  dev = loopback_init();
  if (!dev) {
    errorf("loopback_init() failure");
    return -1;
  }
  iface = ip_iface_alloc(LOOPBACK_IP_ADDR, LOOPBACK_NETMASK);
  if (!iface) {
    errorf("ip_iface_alloc() failure");
    return -1;
  }
  if (ip_iface_register(dev, iface) == -1) {
    errorf("ip_iface_register() failure");
    return -1;
  }
  dev = ether_tap_init(ETHER_TAP_NAME, ETHER_TAP_HW_ADDR);
  if (!dev) {
    errorf("ether_tap_init() failure");
    return -1;
  }
  iface = ip_iface_alloc(ETHER_TAP_IP_ADDR, ETHER_TAP_NETMASK);
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

static void cleanup(void) { net_shutdown(); }

int main(int argc, char *argv[]) {
  if (setup() == -1) {
    errorf("setup() failure");
    return -1;
  }

  struct ip_endpoint local;
  ip_endpoint_pton("0.0.0.0:7", &local);
  int soc = tcp_open_rfc793(&local, NULL, 0);
  if (soc == -1) {
    errorf("tcp_open_rfc793() failure");
    return -1;
  }

  uint8_t buf[2048];
  while (!terminate) {
    ssize_t ret = tcp_receive(soc, buf, sizeof(buf));
    if (ret <= 0) {
      break;
    }
    hexdump(stderr, buf, ret);
    tcp_send(soc, buf, ret);
  }

  cleanup();

  return 0;
}