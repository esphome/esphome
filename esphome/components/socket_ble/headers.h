#pragma once
#include "esphome/core/defines.h"

// Socket types for BLE L2CAP channels; Zephyr has no definitions of its own for these

#ifdef USE_ZEPHYR
#include <cerrno>
#include <cstdint>
#include <sys/types.h>
#include <zephyr/posix/sys/socket.h>

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
#endif  // USE_ZEPHYR
