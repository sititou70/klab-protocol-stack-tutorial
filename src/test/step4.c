#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "driver/loopback.h"
#include "net.h"
#include "test.h"
#include "util.h"

static volatile sig_atomic_t terminate;

static void on_signal(int s) {
  (void)s;
  terminate = 1;
}

int main(int argc, char *argv[]) {
  signal(SIGINT, on_signal);
  if (net_init() == -1) {
    errorf("net_init() failure");
    return -1;
  }

  struct net_device *dev;
  dev = loopback_init();
  if (!dev) {
    errorf("loopback_init() failure");
    return -1;
  }
  if (net_run() == -1) {
    errorf("net_run() failure");
    return -1;
  }
  while (!terminate) {
    if (net_device_output(dev, NET_PROTOCOL_TYPE_IP, test_data, sizeof(test_data), NULL) == -1) {
      errorf("net_device_output() failure");
      break;
    }
    sleep(1);
  }

  net_shutdown();

  return 0;
}