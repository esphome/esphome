#include "fsl_scoreboard_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.fsl_scoreboard";

static constexpr uint32_t BIT_TIME_US = 528;

void FSLScoreboardProtocol::encode(RemoteTransmitData *dst, const FSLScoreboardData &data) {
  ESP_LOGD(TAG, "Sending FSL Scoreboard: field=%d, value=%d", data.field, data.value);

  // Extract digits
  uint8_t hundreds = (data.value >= 100) ? (data.value / 100) % 10 : 0xF;
  uint8_t tens = (data.value >= 10) ? (data.value / 10) % 10 : 0xF;
  uint8_t units = data.value % 10;

  // Build 32-bit data: 3 F 2 H 1 T 0 U
  uint32_t payload = 0;
  payload |= (uint32_t) 0x3 << 28;  // Position marker 3
  payload |= (uint32_t) (data.field & 0xF) << 24;
  payload |= (uint32_t) 0x2 << 20;  // Position marker 2
  payload |= (uint32_t) (hundreds & 0xF) << 16;
  payload |= (uint32_t) 0x1 << 12;  // Position marker 1
  payload |= (uint32_t) (tens & 0xF) << 8;
  payload |= (uint32_t) 0x0 << 4;  // Position marker 0
  payload |= (uint32_t) (units & 0xF);

  dst->reserve(38 + 72 * 10);

  // Preamble: 38 bits of alternating 10101010...
  for (int i = 0; i < 19; i++) {
    dst->item(BIT_TIME_US, BIT_TIME_US);
  }

  // Send 10 blocks
  for (int block = 0; block < 10; block++) {
    // Sync: 111
    dst->mark(BIT_TIME_US * 3);

    // Manchester encode 32 bits
    for (int i = 31; i >= 0; i--) {
      if (payload & (1U << i)) {
        // 1 = 01
        dst->space(BIT_TIME_US);
        dst->mark(BIT_TIME_US);
      } else {
        // 0 = 10
        dst->mark(BIT_TIME_US);
        dst->space(BIT_TIME_US);
      }
    }

    // Postamble: 000
    dst->space(BIT_TIME_US * 3);
  }
}

optional<FSLScoreboardData> FSLScoreboardProtocol::decode(RemoteReceiveData src) {
  // Look for preamble: alternating pattern
  int preamble_count = 0;
  while (src.peek_item(BIT_TIME_US, BIT_TIME_US)) {
    src.advance(1);
    preamble_count++;
    if (preamble_count >= 15)
      break;
  }

  if (preamble_count < 15)
    return {};

  // Look for sync: 111 (3 * BIT_TIME_US mark)
  if (!src.expect_mark(BIT_TIME_US * 3))
    return {};

  // Manchester decode 32 bits
  uint32_t payload = 0;
  for (int i = 0; i < 32; i++) {
    if (src.expect_space(BIT_TIME_US) && src.expect_mark(BIT_TIME_US)) {
      // 01 = 1
      payload = (payload << 1) | 1;
    } else if (src.expect_mark(BIT_TIME_US) && src.expect_space(BIT_TIME_US)) {
      // 10 = 0
      payload = (payload << 1) | 0;
    } else {
      return {};
    }
  }

  // Extract nybbles
  uint8_t pos3 = (payload >> 28) & 0xF;
  uint8_t field = (payload >> 24) & 0xF;
  uint8_t pos2 = (payload >> 20) & 0xF;
  uint8_t hundreds = (payload >> 16) & 0xF;
  uint8_t pos1 = (payload >> 12) & 0xF;
  uint8_t tens = (payload >> 8) & 0xF;
  uint8_t pos0 = (payload >> 4) & 0xF;
  uint8_t units = payload & 0xF;

  // Validate position markers
  if (pos3 != 0x3 || pos2 != 0x2 || pos1 != 0x1 || pos0 != 0x0)
    return {};

  // Calculate value
  uint16_t value = 0;
  if (hundreds != 0xF)
    value += hundreds * 100;
  if (tens != 0xF)
    value += tens * 10;
  if (units != 0xF)
    value += units;

  return FSLScoreboardData{
      .field = field,
      .value = value,
  };
}

void FSLScoreboardProtocol::dump(const FSLScoreboardData &data) {
  ESP_LOGI(TAG, "Received FSL Scoreboard: field=%d, value=%d", data.field, data.value);
}

}  // namespace remote_base
}  // namespace esphome
