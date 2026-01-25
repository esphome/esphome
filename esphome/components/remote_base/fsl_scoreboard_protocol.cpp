#include "fsl_scoreboard_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.fsl_scoreboard";

static constexpr uint32_t BIT_TIME_US = 528;
static constexpr uint8_t SYNC_BITS = 4;

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

  dst->reserve(40 + 72 * 10);

  // Preamble: 20 pairs of [528µs, -528µs] = 40 bits
  for (int i = 0; i < 20; i++) {
    dst->mark(BIT_TIME_US);
    dst->space(BIT_TIME_US);
  }

  // Send 10 blocks
  for (int block = 0; block < 10; block++) {
    // Sync: 111 (3 bits = 1584µs mark)
    dst->mark(BIT_TIME_US * 3);

    // First Manchester bit is always '0' (part of the '1110' pattern)
    // which is 10 in Manchester, so mark then space
    dst->mark(BIT_TIME_US);
    dst->space(BIT_TIME_US);

    // Manchester encode remaining 31 bits (MSB first, skipping bit 31 which we just sent)
    for (int i = 30; i >= 0; i--) {
      if (payload & (1U << i)) {
        // 1 = 01 (space then mark)
        dst->space(BIT_TIME_US);
        dst->mark(BIT_TIME_US);
      } else {
        // 0 = 10 (mark then space)
        dst->mark(BIT_TIME_US);
        dst->space(BIT_TIME_US);
      }
    }

    // Postamble: 0000 (4 bits = 2112µs space)
    dst->space(BIT_TIME_US * 4);
  }
}

optional<FSLScoreboardData> FSLScoreboardProtocol::decode(RemoteReceiveData src) {
  ESP_LOGVV(TAG, "Decode attempt, data size: %d, first items: [%d, %d, %d, %d]", src.size(), src.peek(0), src.peek(1),
            src.peek(2), src.peek(3));

  // Skip to first mark if we start with a space
  if (src.peek(0) < 0)
    src.advance(1);

  // Look for preamble: at least 15 alternating 528µs mark/space pairs
  int preamble_count = 0;
  while (src.peek_item(BIT_TIME_US, BIT_TIME_US)) {
    src.advance(2);  // Advance by 2 since peek_item checks mark+space
    preamble_count++;
  }

  if (preamble_count < 15) {
    ESP_LOGVV(TAG, "Preamble too short: %d, next items: [%d, %d, %d, %d]", preamble_count, src.peek(0), src.peek(1),
              src.peek(2), src.peek(3));
    return {};
  }

  ESP_LOGVV(TAG, "Found preamble: %d pairs", preamble_count);

  // After preamble (ends with space), look for sync: 4 * BIT_TIME_US mark (2112µs)
  if (!src.expect_mark(BIT_TIME_US * 4)) {
    ESP_LOGVV(TAG, "Sync mark not found, got: %d", src.peek(0));
    return {};
  }

  ESP_LOGVV(TAG, "Found sync mark");

  // Sync is 4 PCM bits '1110' - the last '0' is first Manchester bit
  // Build Manchester bitstream from timings
  std::vector<uint8_t> manchester_bits;
  manchester_bits.push_back(1);  // The 4th bit of sync

  while (manchester_bits.size() < 64 && src.peek(0) != 0) {
    int32_t val = src.peek(0);
    int width = abs(val);
    bool is_mark = (val > 0);
    int num_bits = (width + BIT_TIME_US / 2) / BIT_TIME_US;

    for (int i = 0; i < num_bits && manchester_bits.size() < 64; i++) {
      manchester_bits.push_back(is_mark ? 1 : 0);
    }

    src.advance(1);
  }

  if (manchester_bits.size() < 64) {
    ESP_LOGVV(TAG, "Not enough Manchester bits: %zu", manchester_bits.size());
    return {};
  }

  // Decode Manchester pairs: 10=0, 01=1
  uint32_t payload = 0;
  for (size_t i = 0; i < 64; i += 2) {
    if (manchester_bits[i] == 1 && manchester_bits[i + 1] == 0) {
      payload = (payload << 1) | 0;
    } else if (manchester_bits[i] == 0 && manchester_bits[i + 1] == 1) {
      payload = (payload << 1) | 1;
    } else {
      ESP_LOGVV(TAG, "Invalid Manchester pair at bit %zu: %d%d", i, manchester_bits[i], manchester_bits[i + 1]);
      return {};
    }
  }

  ESP_LOGVV(TAG, "Decoded payload: 0x%08X", payload);

  // Extract nybbles
  uint8_t pos3 = (payload >> 28) & 0xF;
  uint8_t field = (payload >> 24) & 0xF;
  uint8_t pos2 = (payload >> 20) & 0xF;
  uint8_t hundreds = (payload >> 16) & 0xF;
  uint8_t pos1 = (payload >> 12) & 0xF;
  uint8_t tens = (payload >> 8) & 0xF;
  uint8_t pos0 = (payload >> 4) & 0xF;
  uint8_t units = payload & 0xF;

  ESP_LOGVV(TAG, "Position markers: %X %X %X %X", pos3, pos2, pos1, pos0);

  // Validate position markers
  if (pos3 != 0x3 || pos2 != 0x2 || pos1 != 0x1 || pos0 != 0x0) {
    ESP_LOGVV(TAG, "Invalid position markers");
    return {};
  }

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
