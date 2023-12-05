#include <b64/cencode.h>
#include <openssl/sha.h>
#include <signal.h>
#include <stddef.h>
#include <string.h>

#include "driver/ether_tap.h"
#include "tcp.h"
#include "test.h"
#include "util.h"

#define ENDPOINT "192.168.70.2:80"
#define SEC_WEBSOCKET_KEY "Sec-WebSocket-Key: "
#define SEC_WEBSOCKET_MAGIC "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

static char *switching_protocols =
    "HTTP/1.1 101 Switching Protocols\r\n"
    "Upgrade: websocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Accept: ";

static char *welcome_message =
    "Welcome to Simple WebSocket Echo Server\n"
    "Only short payload (<= 125) is supported\n";

/*
 * utils
 */

// from: https://github.com/libb64/libb64/blob/ce864b17ea0e24a91e77c7dd3eb2d1ac4175b3f0/examples/c-example1.c#L30
void base64_encode(const char *input, size_t input_cnt, char *output) {
  /* keep track of our encoded position */
  char *c = output;
  /* store the number of bytes encoded by a single call */
  int cnt = 0;
  /* we need an encoder state */
  base64_encodestate s;

  /*---------- START ENCODING ----------*/
  /* initialise the encoder state */
  base64_init_encodestate(&s);
  /* gather data from the input and send it to the output */
  cnt = base64_encode_block(input, input_cnt, c, &s);
  c += cnt;
  /* since we have encoded the entire input string, we know that
     there is no more input data; finalise the encoding */
  cnt = base64_encode_blockend(c, &s);
  c += cnt;
  /*---------- STOP ENCODING  ----------*/

  /* we want to print the encoded data, so null-terminate it: */
  *c = 0;

  // remove 0x0a
  *(c - 1) = 0;
}

/*
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-------+-+-------------+-------------------------------+
|F|R|R|R| opcode|M| Payload len |    Extended payload length    |
|I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
|N|V|V|V|       |S|             |   (if payload len==126/127)   |
| |1|2|3|       |K|             |                               |
+-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
|     Extended payload length continued, if payload len == 127  |
+ - - - - - - - - - - - - - - - +-------------------------------+
|                               |Masking-key, if MASK set to 1  |
+-------------------------------+-------------------------------+
| Masking-key (continued)       |          Payload Data         |
+-------------------------------- - - - - - - - - - - - - - - - +
:                     Payload Data continued ...                :
+ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
|                     Payload Data continued ...                |
+---------------------------------------------------------------+
*/
int ws_receive(int soc, uint8_t *data) {
  uint8_t buf[256];
  ssize_t ret = tcp_receive(soc, buf, sizeof(buf));
  if (ret <= 0) return -1;

  // check close opcode
  if (buf[0] & 0x08) {
    return -1;
  }

  // check mask bit
  if (!(buf[1] & 0x80)) {
    errorf("no mask bit in client message");
    return -1;
  }

  size_t payload_len = buf[1] & 0x7f;
  if (payload_len > 125) {
    errorf("long payload is not supported");
    return -1;
  }

  uint8_t *masking_key = buf + 2;
  for (size_t i = 0; i < payload_len; i++) {
    data[i] = buf[6 + i] ^ masking_key[i % 4];
  }

  return payload_len;
}

int ws_send(int soc, uint8_t *data, size_t data_len) {
  uint8_t buf[256];
  // flgs: FIN    text
  buf[0] = 0x80 | 0x01;

  // payload len
  if (data_len > 125) {
    errorf("long payload is not supported");
    return -1;
  }
  buf[1] = data_len & 0x7f;

  memcpy(buf + 2, data, data_len);

  tcp_send(soc, buf, data_len + 2);

  return data_len;
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

  struct ip_endpoint local;
  ip_endpoint_pton(ENDPOINT, &local);
  uint8_t reqbuf[1024];
  while (!terminate) {
    int soc = tcp_open_rfc793(&local, NULL, 0);
    if (soc == -1) {
      errorf("tcp_open_rfc793() failure");
      return -1;
    }

    // receive
    ssize_t ret = tcp_receive(soc, reqbuf, sizeof(reqbuf));
    if (ret <= 0) break;
    hexdump(stdout, reqbuf, ret);

    // parse Sec-WebSocket-Key
    char *sec_header = strstr((char *)reqbuf, SEC_WEBSOCKET_KEY);
    if (sec_header == NULL) {
      errorf("Sec-WebSocket-Key header not found");
      return -1;
    }
    char *sec_header_end = strstr(sec_header, "\r\n");
    if (sec_header_end == NULL) {
      errorf("end of Sec-WebSocket-Key header not found");
      return -1;
    }

    // caclurate accept value
    uint8_t sec_websocket_concatenate_buf[128] = {};
    size_t sec_header_value_len = sec_header_end - sec_header - strlen(SEC_WEBSOCKET_KEY);
    memcpy(sec_websocket_concatenate_buf, sec_header + strlen(SEC_WEBSOCKET_KEY), sec_header_value_len);
    memcpy(sec_websocket_concatenate_buf + sec_header_value_len, SEC_WEBSOCKET_MAGIC, strlen(SEC_WEBSOCKET_MAGIC));
    //// for debugging
    //// see: https://developer.mozilla.org/ja/docs/Web/API/WebSockets_API/Writing_WebSocket_servers
    //// memcpy(sec_websocket_concatenate_buf, "dGhlIHNhbXBsZSBub25jZQ==258EAFA5-E914-47DA-95CA-C5AB0DC85B11", 60);
    unsigned char sec_websocket_sha1_buf[SHA_DIGEST_LENGTH];
    SHA1(sec_websocket_concatenate_buf, strlen((char *)sec_websocket_concatenate_buf), sec_websocket_sha1_buf);
    char sec_websocket_accept_buf[128] = {};
    base64_encode((char *)sec_websocket_sha1_buf, sizeof(sec_websocket_sha1_buf), sec_websocket_accept_buf);

    // switch protocol
    tcp_send(soc, (uint8_t *)switching_protocols, strlen(switching_protocols));
    tcp_send(soc, (uint8_t *)sec_websocket_accept_buf, strlen(sec_websocket_accept_buf));
    tcp_send(soc, (uint8_t *)"\r\n\r\n", 4);

    // welcome message
    ws_send(soc, (uint8_t *)welcome_message, strlen(welcome_message));

    // echo loop
    while (!terminate) {
      int ret = ws_receive(soc, reqbuf);
      if (ret < 0) break;

      ret = ws_send(soc, reqbuf, ret);
      if (ret < 0) break;
    }

    tcp_close(soc);
  }

  /*
   * cleanup
   */

  sleep(1);
  net_shutdown();

  return 0;
}