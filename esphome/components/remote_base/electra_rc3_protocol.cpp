#include "electra_rc3_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const ELECTRA_RC3_TAG = "remote.electra_rc3";

static const uint16_t ELECTRA_RC3_FREQ = 38000;
static const uint16_t ELECTRA_RC3_UNIT = 1000;
static const uint16_t ELECTRA_RC3_HEADER_MARK = (3 * ELECTRA_RC3_UNIT);
static const uint16_t ELECTRA_RC3_HEADER_SPACE = (3 * ELECTRA_RC3_UNIT);
static const uint16_t ELECTRA_RC3_FOOTER_MARK = (4 * ELECTRA_RC3_UNIT);
static const uint16_t ELECTRA_RC3_NUM_BITS = 34;
static const uint16_t ELECTRA_RC3_RETEATS = 3;

void ElectraRC3Protocol::encode(RemoteTransmitData *dst, const ElectraRC3Data &data) {
  dst->reserve(44);
  dst->set_carrier_frequency(ELECTRA_RC3_FREQ);

  for (uint8_t i = 0; i < ELECTRA_RC3_RETEATS; ++i) {
    // Encode header
    dst->mark(ELECTRA_RC3_HEADER_MARK);
    uint16_t next_value = ELECTRA_RC3_HEADER_SPACE;
    bool is_next_space = true;

    // Encode Data
    for (int bit_index = ELECTRA_RC3_NUM_BITS - 1; bit_index >= 0; --bit_index) {
      uint8_t bit = (data.q_word >> bit_index) & 1;

      if (is_next_space) {
        if (bit == 1) {
          dst->space(next_value + ELECTRA_RC3_UNIT);
          next_value = ELECTRA_RC3_UNIT;
          is_next_space = false;

        } else {
          dst->space(next_value);
          dst->mark(ELECTRA_RC3_UNIT);
          next_value = ELECTRA_RC3_UNIT;
          is_next_space = true;
        }

      } else {
        if (bit == 1) {
          dst->mark(next_value);
          dst->space(ELECTRA_RC3_UNIT);
          next_value = ELECTRA_RC3_UNIT;
          is_next_space = false;

        } else {
          dst->mark(next_value + ELECTRA_RC3_UNIT);
          next_value = ELECTRA_RC3_UNIT;
          is_next_space = true;
        }
      }
    }
    dst->space(next_value);
  }

  // Encode footer
  dst->mark(ELECTRA_RC3_FOOTER_MARK);
}

optional<ElectraRC3Data> ElectraRC3Protocol::decode(RemoteReceiveData src) {
  ElectraRC3Data data;

  return data;
}

void ElectraRC3Protocol::dump(const ElectraRC3Data &data) {
  ESP_LOGD(ELECTRA_RC3_TAG,
           "Received Electra RC3: power = 0x%X, mode = 0x%X, fan = 0x%X, swing = 0x%X, ifeel = 0x%X, temperature = "
           "0x%X, sleep = 0x%X",
           data.power, data.mode, data.fan, data.swing, data.ifeel, data.temperature, data.sleep);
}

}  // namespace remote_base
}  // namespace esphome
