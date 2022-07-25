#ifndef TEST_H
#define TEST_H

#include <stdint.h>

/* Scope of Internet host loopback address. see https://tools.ietf.org/html/rfc5735 */
#define LOOPBACK_IP_ADDR "127.0.0.1"
#define LOOPBACK_NETMASK "255.0.0.0"

#define ETHER_TAP_NAME "tap0"
/* Scope of EUI-48 Documentation Values. see https://tools.ietf.org/html/rfc7042 */
#define ETHER_TAP_HW_ADDR "00:00:5e:00:53:01"
/* Scope of Documentation Address Blocks (TEST-NET-1). see https://tools.ietf.org/html/rfc5731 */
#define ETHER_TAP_IP_ADDR "192.168.70.2"
#define ETHER_TAP_NETMASK "255.255.255.0"

#define DEFAULT_GATEWAY "192.168.70.2"

const uint8_t test_data[] = {0x45, 0x00, 0x00, 0x30, 0x00, 0x80, 0x00, 0x00, 0xff, 0x01, 0xbd, 0x4a,
                             0x7f, 0x00, 0x00, 0x01, 0x7f, 0x00, 0x00, 0x01, 0x08, 0x00, 0x35, 0x64,
                             0x00, 0x80, 0x00, 0x01, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
                             0x39, 0x30, 0x21, 0x40, 0x23, 0x24, 0x25, 0x5e, 0x26, 0x2a, 0x28, 0x29};

#endif
