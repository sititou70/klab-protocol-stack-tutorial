
#include "udp.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "ip.h"
#include "util.h"

struct pseudo_hdr {
  uint32_t src;
  uint32_t dst;
  uint8_t zero;
  uint8_t protocol;
  uint16_t len;
};

struct udp_hdr {
  uint16_t src;
  uint16_t dst;
  uint16_t len;
  uint16_t sum;
};

static void udp_dump(const uint8_t *data, size_t len) {
  struct udp_hdr *hdr;

  flockfile(stderr);
  hdr = (struct udp_hdr *)data;
  fprintf(stderr, "        src: %u\n", ntoh16(hdr->src));
  fprintf(stderr, "        dst: %u\n", ntoh16(hdr->dst));
  fprintf(stderr, "        len: %u\n", ntoh16(hdr->len));
  fprintf(stderr, "        sum: 0x%04x\n", ntoh16(hdr->sum));
#ifdef HEXDUMP
  hexdump(stderr, data, len);
#endif
  funlockfile(stderr);
}

static void udp_input(const uint8_t *data, size_t len, ip_addr_t src, ip_addr_t dst, struct ip_iface *iface) {
  struct pseudo_hdr pseudo;
  uint16_t psum = 0;
  struct udp_hdr *hdr;
  char addr1[IP_ADDR_STR_LEN];
  char addr2[IP_ADDR_STR_LEN];

  if (len < sizeof(*hdr)) {
    errorf("too short");
    return;
  }
  hdr = (struct udp_hdr *)data;
  if (len != ntoh16(hdr->len)) { /* just to make sure */
    errorf("length error: len=%zu, hdr->len=%u", len, ntoh16(hdr->len));
    return;
  }
  pseudo.src = src;
  pseudo.dst = dst;
  pseudo.zero = 0;
  pseudo.protocol = IP_PROTOCOL_UDP;
  pseudo.len = hton16(len);
  psum = ~cksum16((uint16_t *)&pseudo, sizeof(pseudo), 0);
  if (cksum16((uint16_t *)hdr, len, psum) != 0) {
    errorf("checksum error: sum=0x%04x, verify=0x%04x", ntoh16(hdr->sum),
           ntoh16(cksum16((uint16_t *)hdr, len, -hdr->sum + psum)));
    return;
  }
  debugf("%s:%d => %s:%d, len=%zu (payload=%zu)", ip_addr_ntop(src, addr1, sizeof(addr1)), ntoh16(hdr->src),
         ip_addr_ntop(dst, addr2, sizeof(addr2)), ntoh16(hdr->dst), len, len - sizeof(*hdr));

  udp_dump(data, len);
}

ssize_t udp_output(struct ip_endpoint *src, struct ip_endpoint *dst, const uint8_t *data, size_t len) {
  struct udp_hdr *hdr;

  if (len > IP_PAYLOAD_SIZE_MAX - sizeof(*hdr)) {
    errorf("too long");
    return -1;
  }

  uint16_t udp_len = sizeof(*hdr) + len;

  struct pseudo_hdr pseudo;
  pseudo.src = src->addr;
  pseudo.dst = dst->addr;
  pseudo.zero = 0;
  pseudo.protocol = IP_PROTOCOL_UDP;
  pseudo.len = hton16(udp_len);
  uint16_t psum = ~cksum16((uint16_t *)&pseudo, sizeof(pseudo), 0);

  uint8_t buf[IP_PAYLOAD_SIZE_MAX];
  hdr = (struct udp_hdr *)buf;
  hdr->src = src->port;
  hdr->dst = dst->port;
  hdr->len = hton16(udp_len);
  hdr->sum = 0;
  memcpy(hdr + 1, data, len);
  hdr->sum = cksum16((uint16_t *)hdr, udp_len, psum);

  char ep1[IP_ENDPOINT_STR_LEN];
  char ep2[IP_ENDPOINT_STR_LEN];
  debugf("%s => %s, len=%zu (payload=%zu)", ip_endpoint_ntop(src, ep1, sizeof(ep1)),
         ip_endpoint_ntop(dst, ep2, sizeof(ep2)), udp_len, len);
  udp_dump((uint8_t *)hdr, udp_len);

  if (ip_output(IP_PROTOCOL_UDP, buf, udp_len, src->addr, dst->addr) == -1) {
    errorf("ip_output() failure");
    return -1;
  }

  return len;
}

int udp_init(void) {
  if (ip_protocol_register(IP_PROTOCOL_UDP, udp_input) == -1) {
    errorf("ip_protocol_register() failure");
    return -1;
  }

  return 0;
}