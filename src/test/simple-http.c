#include <string.h>

#include "driver/ether_tap.h"
#include "ip.h"
#include "net.h"
#include "tcp.h"
#include "test.h"
#include "util.h"

int main(int argc, char *argv[]) {
  /*
   * setup
   */

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

  struct ip_endpoint local;
  ip_endpoint_pton("192.168.70.2:80", &local);
  int soc = tcp_open_rfc793(&local, NULL, 0);
  if (soc == -1) {
    errorf("tcp_open_rfc793() failure");
    return -1;
  }

  uint8_t buf[2048];
  tcp_receive(soc, buf, sizeof(buf));

  char *response =
      "HTTP/1.1 200 OK\r\n"
      "\r\n"
      "<html><head><title>hello</title></head><body>world</body></html>";
  tcp_send(soc, (uint8_t *)response, strlen(response));

  tcp_close(soc);

  /*
   * cleanup
   */

  sleep(1);
  net_shutdown();

  return 0;
}