#include "virtualwire_protocol.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace remote_base {

// CRC - Remove this section after PR#4798 is merged - https://github.com/esphome/esphome/pull/4798
static const uint16_t CRC16_8408_LE_LUT_L[] = {0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
                                               0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7};
static const uint16_t CRC16_8408_LE_LUT_H[] = {0x0000, 0x1081, 0x2102, 0x3183, 0x4204, 0x5285, 0x6306, 0x7387,
                                               0x8408, 0x9489, 0xa50a, 0xb58b, 0xc60c, 0xd68d, 0xe70e, 0xf78f};

uint16_t crc16(const uint8_t *data, uint16_t len, uint16_t crc, uint16_t reverse_poly, bool refin, bool refout) {
  if (refin) {
    crc ^= 0xffff;
  }
  if (reverse_poly == 0x8408) {
    while (len--) {
      uint8_t combo = crc ^ (uint8_t) *data++;
      crc = (crc >> 8) ^ CRC16_8408_LE_LUT_L[combo & 0x0F] ^ CRC16_8408_LE_LUT_H[combo >> 4];
    }
  }
  return refout ? (crc ^ 0xffff) : crc;
}
// End of CRC

static const char *const TAG = "remote.virtualwire";

static const std::vector<tx_data_t> VIRTUALWIRE_SYMBOLS = {0xd,  0xe,  0x13, 0x15, 0x16, 0x19, 0x1a, 0x1c,
                                                           0x23, 0x25, 0x26, 0x29, 0x2a, 0x2c, 0x32, 0x34};

static const std::vector<tx_data_t> VIRTUALWIRE_HEADER = {0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x38, 0x2c};

bool VirtualWireData::is_valid() const {
  if (this->data_.size() < 4)
    return false;
  if (this->data_[0] < 4)
    return false;
  if (this->data_[0] > MAX_LENGTH)
    return false;
  if (this->data_.size() < this->data_[0])
    return false;
  uint16_t crc = this->calc_crc_();
  return (this->data_[this->data_[0] - 2] == (crc & 0xff)) && (this->data_[this->data_[0] - 1] == (crc >> 8));
}

uint16_t VirtualWireData::calc_crc_() const {
  if (this->data_.size() < 3)
    return 0;

  return crc16(this->data_.data(), this->size() - 2, 0xffff, 0x8408, false, true);
}

void VirtualWireProtocol::encode_final_(RemoteTransmitData *dst, const std::vector<tx_data_t> &data,
                                        uint32_t bit_length_us) const {
  bool prev_level = false;
  uint32_t prev_length = 0;
  for (const tx_data_t &d : data) {
    for (tx_data_t mask = 1; mask & 0x3f; mask <<= 1) {
      bool level = (d & mask);
      if (level == prev_level) {
        prev_length += bit_length_us;
      } else {
        if (prev_level) {
          dst->mark(prev_length);
        } else {
          dst->space(prev_length);
        }
        prev_level = level;
        prev_length = bit_length_us;
      }
    }
  }
  if (prev_level) {
    dst->mark(prev_length);
  } else {
    dst->space(prev_length);
  }
}

void VirtualWireProtocol::encode(RemoteTransmitData *dst, const VirtualWireData &src) {
  dst->set_carrier_frequency(0);
  dst->reserve(8 + 2 * src.size());

  std::vector<tx_data_t> tx_buffer = VIRTUALWIRE_HEADER;
  for (const uint8_t &d : src.get_raw_data()) {
    tx_buffer.push_back(VIRTUALWIRE_SYMBOLS[d >> 4]);
    tx_buffer.push_back(VIRTUALWIRE_SYMBOLS[d & 0xf]);
  }
  ESP_LOGVV(TAG, "TX: %s", format_hex_pretty(tx_buffer.data(), tx_buffer.size()).c_str());
  this->encode_final_(dst, tx_buffer, src.get_bit_length());
}

int8_t VirtualWireProtocol::peek_mark_(RemoteReceiveData &src, uint32_t bit_length_us) {
  if (src.peek_mark(bit_length_us))
    return 1;
  if (src.peek_mark(2 * bit_length_us))
    return 2;
  if (src.peek_mark(3 * bit_length_us))
    return 3;
  if (src.peek_mark(4 * bit_length_us))
    return 4;
  return 0;
}

int8_t VirtualWireProtocol::peek_space_(RemoteReceiveData &src, uint32_t bit_length_us) {
  if (src.peek_space(bit_length_us))
    return -1;
  if (src.peek_space(2 * bit_length_us))
    return -2;
  if (src.peek_space(3 * bit_length_us))
    return -3;
  if (src.peek_space_at_least(4 * bit_length_us))
    return -4;
  return 0;
}

bool VirtualWireProtocol::decode_symbol_6to4_(uint8_t symbol, uint8_t &decoded) {
  for (uint8_t i = (symbol >> 2) & 8, count = 8; count--; i++) {
    if (symbol == VIRTUALWIRE_SYMBOLS[i]) {
      decoded = i;
      return true;
    }
  }
  return false;
}

bool VirtualWireProtocol::decode_4bit_(RemoteReceiveData &src, int8_t &remaining, bool &last_element, uint8_t &data,
                                       uint32_t bit_length_us) {
  std::vector<int8_t> received_data = {remaining};
  uint8_t num_elements = std::abs(remaining);
  while (num_elements < 6) {
    int8_t received_element;
    if (last_element) {
      received_element = this->peek_space_(src, bit_length_us);
      src.advance();
      ESP_LOGVV(TAG, "peek_space: %d", received_element);
      num_elements += -received_element;
      last_element = false;
    } else {
      received_element = this->peek_mark_(src, bit_length_us);
      src.advance();
      ESP_LOGVV(TAG, "peek_mark: %d", received_element);
      num_elements += received_element;
      last_element = true;
    }
    if (received_element == 0) {
      ESP_LOGW(TAG, "Timing out of range");
      return false;
    }
    received_data.push_back(received_element);
  }
  tx_data_t mask = 1;
  tx_data_t encoded = 0;
  for (const int8_t &element : received_data) {
    ESP_LOGVV(TAG, "Got element: %d", element);
    for (uint8_t i = std::abs(element); i; i--) {
      if (element > 0) {
        encoded |= mask;
      }
      mask <<= 1;
      if (!(mask & 0x3f)) {
        break;
      }
    }
  }
  if (!decode_symbol_6to4_(encoded, data)) {
    ESP_LOGW(TAG, "6to4 code not found, encoded: %02x", encoded);
    return false;
  }
  if (num_elements > 6) {
    num_elements -= 6;
    remaining = received_data[received_data.size() - 1] > 0 ? num_elements : -num_elements;
  } else {
    remaining = 0;
  }
  ESP_LOGV(TAG, "4bit decode success, data: %01x, remaining: %d", data, remaining);
  return true;
}

bool VirtualWireProtocol::decode_byte_(RemoteReceiveData &src, int8_t &remaining, bool &last_element, uint8_t &data,
                                       uint32_t bit_length_us) {
  uint8_t first, second;
  if (!this->decode_4bit_(src, remaining, last_element, first, bit_length_us))
    return false;
  if (!this->decode_4bit_(src, remaining, last_element, second, bit_length_us))
    return false;
  data = ((first & 0x3f) << 4) | second;
  return true;
}

optional<VirtualWireData> VirtualWireProtocol::decode(RemoteReceiveData src, uint32_t bit_length_us) {
  if (src.size() < 12)
    return {};

  if (bit_length_us == 0) {
    bool skip = false;
    if (src.peek(0) < 0) {
      skip = true;
      src.advance(1);
    }

    // Speed detection
    bit_length_us = (src.peek(0) - src.peek(1) + src.peek(2) - src.peek(3) + src.peek(4) - src.peek(5)) / 6;
    ESP_LOGI(TAG, "Trying to detect speed, bit_length_us: %i, skip: %i", bit_length_us, skip);
  }

  if (src.expect_item(bit_length_us, bit_length_us) && src.expect_item(bit_length_us, bit_length_us) &&
      src.expect_item(bit_length_us, bit_length_us)) {
    ESP_LOGI(TAG, "Header start");

    while (src.peek_item(bit_length_us, bit_length_us)) {
      src.advance(2);
    }

    if (!(src.expect_item(bit_length_us, 3 * bit_length_us) && src.expect_item(3 * bit_length_us, 2 * bit_length_us) &&
          src.expect_item(2 * bit_length_us, bit_length_us))) {
      ESP_LOGW(TAG, "Header error");
      return {};
    }

    int8_t mark = this->peek_mark_(src, bit_length_us);
    src.advance();
    if (mark == 0)
      return {};

    ESP_LOGI(TAG, "Header complete");
    int8_t remaining = mark - 1;
    bool last_element = true;
    uint8_t expected_length = 4;
    uint8_t data;

    if (!this->decode_byte_(src, remaining, last_element, expected_length, bit_length_us))
      return {};
    std::vector<uint8_t> out_data = {expected_length};
    expected_length--;
    for (; expected_length; expected_length--) {
      if (!this->decode_byte_(src, remaining, last_element, data, bit_length_us)) {
        ESP_LOGW(TAG, "Error decoding byte");
        return {};
      }
      ESP_LOGI(TAG, "Decoded byte %02x", data);
      out_data.push_back(data);
    }
    VirtualWireData out = VirtualWireData(out_data);
    out.set_bit_length(bit_length_us);

    if (!out.is_valid()) {
      ESP_LOGW(TAG, "Checksum error");
      return {};
    }
    return out;
  }
  return {};
}

void VirtualWireProtocol::dump(const VirtualWireData &data) {
  ESP_LOGD(TAG, "Received VirtualWire: %s", data.to_string().c_str());
}

}  // namespace remote_base
}  // namespace esphome
