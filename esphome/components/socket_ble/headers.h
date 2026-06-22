#pragma once
#include "esphome/core/defines.h"

// Helper file to include all socket-related system headers (or use our own
// definitions where system ones don't exist)

#include <cerrno>
#include <cstdint>
#include <sys/types.h>

#if defined(USE_ZEPHYR)
#include <zephyr/posix/sys/socket.h>
#elif defined(USE_HOST)
#include <sys/socket.h>
#elif defined(USE_ESP32)
#include <lwip/sockets.h>
#else
using socklen_t = uint32_t;
using sa_family_t = uint8_t;
#ifndef IOVEC
#define IOVEC
// NOLINTNEXTLINE(readability-identifier-naming)
struct iovec {
  void *iov_base;
  size_t iov_len;
};
#endif
#endif

#ifndef AF_BLUETOOTH
#define AF_BLUETOOTH 31
#endif

#ifndef BTPROTO_L2CAP
#define BTPROTO_L2CAP 0
#endif

#ifndef SOCK_SEQPACKET
#define SOCK_SEQPACKET 5
#endif

static constexpr size_t BDADDR_STR_LEN = 18;
using bdaddr_t = uint8_t[6];

// NOLINTNEXTLINE(readability-identifier-naming)
struct sockaddr_l2 {
  sa_family_t l2_family;
  uint16_t l2_psm;
  bdaddr_t l2_bdaddr;
  uint16_t l2_cid;
  uint8_t l2_bdaddr_type;
};
