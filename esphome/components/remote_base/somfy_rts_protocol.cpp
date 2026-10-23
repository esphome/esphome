#include "somfy_rts_protocol.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

// Timings from https://pushstack.wordpress.com/somfy-rts-protocol/
static constexpr uint8_t FRAME_SIZE_IN_BYTES = 7;
static constexpr uint16_t HALF_BIT_US = 604;

static constexpr uint16_t WAKEUP_HIGH_US = 9415;
static constexpr uint32_t WAKEUP_LOW_US = 89565;

static constexpr uint16_t HW_SYNC_US = 2560;

static constexpr uint16_t SW_SYNC_HIGH_US = 4550;
static constexpr uint16_t SW_SYNC_LOW_US = HALF_BIT_US;

// Wakeup, two hardware syncs and a software sync (8), two entries per bit (112)
static constexpr uint32_t FRAME_ENTRIES = 8 + FRAME_SIZE_IN_BYTES * 8 * 2;

void SomfyRtsProtocol::encode(RemoteTransmitData *dst, const SomfyRtsData &data) {
  dst->reserve(FRAME_ENTRIES);
  dst->set_carrier_frequency(0);
  dst->item(WAKEUP_HIGH_US, WAKEUP_LOW_US);
  dst->item(HW_SYNC_US, HW_SYNC_US);
  dst->item(HW_SYNC_US, HW_SYNC_US);
  dst->item(SW_SYNC_HIGH_US, SW_SYNC_LOW_US);

  uint8_t frame[FRAME_SIZE_IN_BYTES];
  // Some non standard remotes put the control nibble in the key byte and send 0xF in the control nibble
  if (data.control_in_key) {
    frame[0] = (data.key & 0xF0) | (data.control & 0x0F);
    frame[1] = 0xF0;
  } else {
    frame[0] = data.key;
    frame[1] = (data.control & 0x0F) << 4;
  }
  frame[2] = data.rolling_code >> 8;
  frame[3] = data.rolling_code;
  frame[4] = data.address >> 16;
  frame[5] = data.address >> 8;
  frame[6] = data.address;

  uint8_t crc = 0;
  for (uint8_t data_byte : frame) {
    crc ^= (data_byte >> 4) ^ (data_byte & 0xF);
  }
  frame[1] |= (crc & 0x0F);

  for (uint8_t i = 1; i < FRAME_SIZE_IN_BYTES; i++) {
    frame[i] ^= frame[i - 1];
  }

  // Manchester, MSB first: a 1 is space then mark, a 0 is mark then space. Frames repeat with
  // the 30 ms gap through the action's repeat option, not inside the frame.
  for (uint8_t data_byte : frame) {
    for (int8_t y = 7; y >= 0; y--) {
      if (data_byte & (1 << y)) {
        dst->space(HALF_BIT_US);
        dst->mark(HALF_BIT_US);
      } else {
        dst->mark(HALF_BIT_US);
        dst->space(HALF_BIT_US);
      }
    }
  }
}

}  // namespace esphome::remote_base
