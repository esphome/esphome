#pragma once
#include "esphome/core/defines.h"

// Helper file to include all socket-related system headers (or use our own
// definitions where system ones don't exist)

#ifdef USE_SOCKET_IMPL_LWIP_TCP

#define LWIP_INTERNAL
#include "lwip/inet.h"
#include <cerrno>
#include <cstdint>
#include <sys/types.h>

/* Address families.  */
#define AF_UNSPEC 0
#define AF_INET 2
#define AF_INET6 10
#define PF_INET AF_INET
#define PF_INET6 AF_INET6
#define PF_UNSPEC AF_UNSPEC
#define IPPROTO_IP 0
#define IPPROTO_TCP 6
#define IPPROTO_IPV6 41
#define IPPROTO_ICMPV6 58

#define TCP_NODELAY 0x01

#define F_GETFL 3
#define F_SETFL 4
#define O_NONBLOCK 1

#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2

/* Socket protocol types (TCP/UDP/RAW) */
#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_RAW 3

#define SO_REUSEADDR 0x0004 /* Allow local address reuse */
#define SO_KEEPALIVE 0x0008 /* keep connections alive */
#define SO_BROADCAST 0x0020 /* permit to send and to receive broadcast messages (see IP_SOF_BROADCAST option) */

#define SOL_SOCKET 0xfff /* options for socket level */

using sa_family_t = uint8_t;
using in_port_t = uint16_t;

// NOLINTNEXTLINE(readability-identifier-naming)
struct sockaddr_in {
  uint8_t sin_len;
  sa_family_t sin_family;
  in_port_t sin_port;
  struct in_addr sin_addr;
#define SIN_ZERO_LEN 8
  char sin_zero[SIN_ZERO_LEN];
};

// NOLINTNEXTLINE(readability-identifier-naming)
struct sockaddr_in6 {
  uint8_t sin6_len;          /* length of this structure    */
  sa_family_t sin6_family;   /* AF_INET6                    */
  in_port_t sin6_port;       /* Transport layer port #      */
  uint32_t sin6_flowinfo;    /* IPv6 flow information       */
  struct in6_addr sin6_addr; /* IPv6 address                */
  uint32_t sin6_scope_id;    /* Set of interfaces for scope */
};

// NOLINTNEXTLINE(readability-identifier-naming)
struct sockaddr {
  uint8_t sa_len;
  sa_family_t sa_family;
  char sa_data[14];
};

// NOLINTNEXTLINE(readability-identifier-naming)
struct sockaddr_storage {
  uint8_t s2_len;
  sa_family_t ss_family;
  char s2_data1[2];
  uint32_t s2_data2[3];
  uint32_t s2_data3[3];
};
using socklen_t = uint32_t;

// NOLINTNEXTLINE(readability-identifier-naming)
struct iovec {
  void *iov_base;
  size_t iov_len;
};

#if defined(USE_ESP8266) || defined(USE_RP2040)
// arduino-esp8266 declares a global vars called INADDR_NONE/ANY which are invalid with the define
#ifdef INADDR_ANY
#undef INADDR_ANY
#endif
#ifdef INADDR_NONE
#undef INADDR_NONE
#endif

#define ESPHOME_INADDR_ANY ((uint32_t) 0x00000000UL)
#define ESPHOME_INADDR_NONE ((uint32_t) 0xFFFFFFFFUL)
#else  // !USE_ESP8266
#define ESPHOME_INADDR_ANY INADDR_ANY
#define ESPHOME_INADDR_NONE INADDR_NONE
#endif

#endif  // USE_SOCKET_IMPL_LWIP_TCP

#ifdef USE_SOCKET_IMPL_BSD_SOCKETS

#include <cstdint>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#ifdef USE_ARDUINO
// arduino-esp32 declares a global var called INADDR_NONE which is replaced
// by the define
#ifdef INADDR_NONE
#undef INADDR_NONE
#endif
// not defined for ESP32
using socklen_t = uint32_t;

#define ESPHOME_INADDR_ANY ((uint32_t) 0x00000000UL)
#define ESPHOME_INADDR_NONE ((uint32_t) 0xFFFFFFFFUL)
#else  // !USE_ESP32
#define ESPHOME_INADDR_ANY INADDR_ANY
#define ESPHOME_INADDR_NONE INADDR_NONE
#endif

#endif  // USE_SOCKET_IMPL_BSD_SOCKETS
