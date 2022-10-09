#include "kelvinator_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.kelvinator";

static const int32_t TICK_US = 85;
static const int32_t HEADER_MARK_US = 106 * TICK_US;
static const int32_t HEADER_SPACE_US = 53 * TICK_US;
static const int32_t BIT_MARK_US = 8 * TICK_US;
static const int32_t BIT_ONE_SPACE_US = 18 * TICK_US;
static const int32_t BIT_ZERO_SPACE_US = 6 * TICK_US;
static const int32_t GAP_SPACE_US = 235 * TICK_US;
static const int32_t DOUBLE_GAP_SPACE_US = 2 * GAP_SPACE_US;

void KelvinatorProtocol::encode_data_(RemoteTransmitData *dst, const uint32_t data) {
  const uint8_t *p = reinterpret_cast<const uint8_t *>(&data);

  for (unsigned n = 0; n < 4; n++) {
    uint8_t data = p[n];

    // Read byte
    for (uint8_t bit = 0; bit < 8; bit++) {
      uint8_t mask = 1 << bit;

      dst->mark(BIT_MARK_US);

      if (data & mask) {
        dst->space(BIT_ONE_SPACE_US);
      } else {
        dst->space(BIT_ZERO_SPACE_US);
      }
    }
  }
}

void KelvinatorProtocol::encode_footer_(RemoteTransmitData *dst) {
  dst->item(BIT_MARK_US, BIT_ZERO_SPACE_US);
  dst->item(BIT_MARK_US, BIT_ONE_SPACE_US);
  dst->item(BIT_MARK_US, BIT_ZERO_SPACE_US);
  dst->item(BIT_MARK_US, GAP_SPACE_US);
}

void KelvinatorProtocol::encode(RemoteTransmitData *dst, const KelvinatorData &data) {
  dst->set_carrier_frequency(38000);
  dst->reserve(2 + 2 * 32 + 8 + 2 * 32 + 2 + 2 + 2 * 32 + 8 + 2 * 32);

  auto size = data.data.size();
  for (size_t i = 0; i < size; i++) {
    auto block = data.data[i];
    const uint32_t *p = reinterpret_cast<const uint32_t *>(&block);

    dst->item(HEADER_MARK_US, HEADER_SPACE_US);
    encode_data_(dst, p[0]);
    encode_footer_(dst);
    encode_data_(dst, p[1]);

    if (i != size - 1) {
      dst->item(BIT_MARK_US, DOUBLE_GAP_SPACE_US);
    }
  }
}

bool KelvinatorProtocol::decode_footer_(RemoteReceiveData &src) {
  return src.expect_item(BIT_MARK_US, BIT_ZERO_SPACE_US) && src.expect_item(BIT_MARK_US, BIT_ONE_SPACE_US) &&
         src.expect_item(BIT_MARK_US, BIT_ZERO_SPACE_US) && src.expect_item(BIT_MARK_US, GAP_SPACE_US);
}

bool KelvinatorProtocol::decode_data_(RemoteReceiveData &src, uint32_t *data) {
  uint8_t *p = reinterpret_cast<uint8_t *>(data);

  for (unsigned n = 0; n < 4; n++) {
    uint8_t data = 0;

    // Read byte
    for (uint8_t bit = 0; bit < 8; bit++) {
      uint8_t mask = 1 << bit;

      if (!src.expect_mark(BIT_MARK_US)) {
        return false;
      }
      if (src.expect_space(BIT_ONE_SPACE_US)) {
        data |= mask;
      } else if (!src.expect_space(BIT_ZERO_SPACE_US)) {
        return false;
      }
    }

    p[n] = data;
  }
  return true;
}

bool KelvinatorProtocol::decode_data_(RemoteReceiveData &src, uint64_t *data) {
  uint32_t *p = reinterpret_cast<uint32_t *>(data);

  return src.expect_item(HEADER_MARK_US, HEADER_SPACE_US) && decode_data_(src, &p[0]) && decode_footer_(src) &&
         decode_data_(src, &p[1]);
}

optional<KelvinatorData> KelvinatorProtocol::decode(RemoteReceiveData data) {
  KelvinatorData kelvinator_data;
  do {
    uint64_t block;
    if (!decode_data_(data, &block)) {
      return {};
    }
    kelvinator_data.data.push_back(block);

  } while (data.expect_item(BIT_MARK_US, DOUBLE_GAP_SPACE_US));

  if (!kelvinator_data.is_valid_checksum()) {
    return {};
  }

  ESP_LOGD(TAG, "Kelvinator data valid!");
  kelvinator_data.log();
  return kelvinator_data;
}

const uint8_t KELVINATOR_CHECKSUM_START = 10;

uint8_t KelvinatorData::caclulate_block_checksum(const uint64_t block) {
  const uint8_t *p = reinterpret_cast<const uint8_t *>(&block);

  uint8_t sum = KELVINATOR_CHECKSUM_START;

  // Sum the lower half of the first 4 bytes of this block.
  for (uint8_t i = 0; i < 4; i++, p++)
    sum += (*p & 0b1111);

  // then sum the upper half of the next 3 bytes.
  for (uint8_t i = 4; i < 7; i++, p++)
    sum += (*p >> 4);

  // Trim it down to fit into the 4 bits allowed. i.e. Mod 16.
  return sum & 0b1111;
}

void KelvinatorData::apply_checksum() {
  auto i = 0;
  for (auto block : this->data) {
    auto checksum = caclulate_block_checksum(block);

    uint8_t *state = reinterpret_cast<uint8_t *>(&block);
    state[7] = (state[7] & 0b1111) | checksum << 4;

    data[i] = block;

    ++i;
  }
}
bool KelvinatorData::is_valid_checksum() {
  for (auto block : this->data) {
    auto expected_checksum = caclulate_block_checksum(block);

    const uint8_t *state = reinterpret_cast<const uint8_t *>(&block);

    const uint8_t checksum = (state[7] >> 4) & 0b1111;
    if (checksum != expected_checksum) {
      return false;
    }
  }

  return true;
}

void KelvinatorProtocol::dump(const KelvinatorData &data) {
  data.log();

  KelvinatorData copy;

  for (auto block : data.data) {
    copy.data.push_back(block);
  }

  copy.apply_checksum();

  copy.log();
}

void KelvinatorData::log() const {
  for (size_t i = 0; i < data.size(); i++) {
    auto block = data[i];
    const uint8_t *state = reinterpret_cast<const uint8_t *>(&block);
    ESP_LOGV(TAG, "Recieved Kelvinator: %d %s", i, format_hex_pretty(state, sizeof(uint64_t)).c_str());
  }
}

}  // namespace remote_base
}  // namespace esphome
