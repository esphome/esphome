#pragma once
// Test only: swaps the first two records of an ATT Read By Type Response whose first record declares characteristic
// 0xFFF1, the way some peripherals answer out of handle order (esphome#22518). Count of swaps in attswap_count.
#include <cstdint>
#include <cstring>
#include "esphome/core/log.h"

struct AttSwapHdr {
  uint16_t event, len, offset, layer_specific;
};
extern "C" void __real_l2c_rcv_acl_data(void *p_msg);
static volatile uint32_t attswap_count = 0;

extern "C" void __wrap_l2c_rcv_acl_data(void *p_msg) {
  auto *h = static_cast<AttSwapHdr *>(p_msg);
  uint8_t *p = reinterpret_cast<uint8_t *>(h + 1) + h->offset;
  // HCI ACL header (4), L2CAP header (4), then ATT: opcode, record length, records
  if (h->len >= 4 + 4 + 2 + 14) {
    uint16_t cid = p[6] | (p[7] << 8);
    uint8_t *att = p + 8;
    // record: handle(2) properties(1) value handle(2) uuid16(2)
    if (cid == 0x0004 && att[0] == 0x09 && att[1] == 7 && att[2 + 5] == 0xF1 && att[2 + 6] == 0xFF) {
      uint8_t tmp[7];
      memcpy(tmp, att + 2, 7);
      memcpy(att + 2, att + 9, 7);
      memcpy(att + 9, tmp, 7);
      attswap_count = attswap_count + 1;
    }
  }
  __real_l2c_rcv_acl_data(p_msg);
}
