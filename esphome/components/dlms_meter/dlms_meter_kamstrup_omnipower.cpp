#include "dlms_meter.h"

#include <algorithm>

namespace esphome::dlms_meter {

static constexpr const char *TAG = "dlms_meter";

void DlmsMeterComponent::decode_kamstrup_omnipower_obis_(uint8_t *plaintext, uint16_t message_length) {
  ESP_LOGV(TAG, "Decoding Kamstrup OMNIPOWER payload");
  MeterData data = this->parse_kamstrup_omnipower_data_(plaintext, message_length);
  this->publish_parsed_data_(data);
}

MeterData DlmsMeterComponent::parse_kamstrup_omnipower_data_(uint8_t *plaintext, uint16_t message_length) {
  MeterData data{};

  if (message_length < 15) {
    return data;
  }

  if (plaintext[0] == DATA_NOTIFICATION && plaintext[5] == TIMESTAMP_DATETIME) {
    uint16_t year = encode_uint16(plaintext[6], plaintext[7]);
    uint8_t month = plaintext[8];
    uint8_t day = plaintext[9];
    uint8_t hour = plaintext[11];
    uint8_t minute = plaintext[12];
    uint8_t second = plaintext[13];
    if (year <= 9999 && month <= 12 && day <= 31 && hour <= 23 && minute <= 59 && second <= 59) {
      snprintf(data.timestamp, sizeof(data.timestamp), "%04u-%02u-%02uT%02u:%02u:%02uZ", year, month, day, hour, minute,
               second);
    }
  }

  const auto find_obis = [&](uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f) -> const uint8_t * {
    const std::array<uint8_t, 8> needle = {DataType::OCTET_STRING, OBIS_CODE_LENGTH_STANDARD, a, b, c, d, e, f};
    auto *const it = std::search(plaintext, plaintext + message_length, needle.begin(), needle.end());
    if (it == plaintext + message_length) {
      return nullptr;
    }
    return it + needle.size();
  };

  const auto read_u16 = [&](uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f, float &target,
                            float multiplier = 1.0f) {
    const uint8_t *value = find_obis(a, b, c, d, e, f);
    if (value == nullptr || value + 3 > plaintext + message_length || value[0] != DataType::LONG_UNSIGNED) {
      return;
    }
    target = static_cast<float>(encode_uint16(value[1], value[2])) * multiplier;
  };

  const auto read_u32 = [&](uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f, float &target,
                            float multiplier = 1.0f) {
    const uint8_t *value = find_obis(a, b, c, d, e, f);
    if (value == nullptr || value + 5 > plaintext + message_length || value[0] != DataType::DOUBLE_LONG_UNSIGNED) {
      return;
    }
    target = static_cast<float>(encode_uint32(value[1], value[2], value[3], value[4])) * multiplier;
  };

  const auto read_string = [&](uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f, char *target,
                               size_t target_size) {
    const uint8_t *value = find_obis(a, b, c, d, e, f);
    if (value == nullptr || value + 2 > plaintext + message_length) {
      return;
    }
    if (value[0] != DataType::OCTET_STRING && value[0] != DataType::VISIBLE_STRING) {
      return;
    }
    const uint8_t length = value[1];
    if (value + 2 + length > plaintext + message_length) {
      return;
    }
    snprintf(target, target_size, "%.*s", length, reinterpret_cast<const char *>(value + 2));
  };

  const auto read_u32_string = [&](uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f, char *target,
                                   size_t target_size) {
    const uint8_t *value = find_obis(a, b, c, d, e, f);
    if (value == nullptr || value + 5 > plaintext + message_length || value[0] != DataType::DOUBLE_LONG_UNSIGNED) {
      return;
    }
    snprintf(target, target_size, "%lu",
             static_cast<unsigned long>(encode_uint32(value[1], value[2], value[3], value[4])));
  };

  read_u32_string(0x01, 0x01, 0x00, 0x00, 0x01, 0xFF, data.meter_number, sizeof(data.meter_number));
  read_string(0x01, 0x01, 0x00, 0x02, 0x81, 0xFF, data.obis_list_version, sizeof(data.obis_list_version));

  read_u32(0x01, 0x01, 0x01, 0x07, 0x00, 0xFF, data.active_power_plus);
  read_u32(0x01, 0x01, 0x02, 0x07, 0x00, 0xFF, data.active_power_minus);
  read_u32(0x01, 0x01, 0x03, 0x07, 0x00, 0xFF, data.reactive_power_plus);
  read_u32(0x01, 0x01, 0x04, 0x07, 0x00, 0xFF, data.reactive_power_minus);
  read_u32(0x01, 0x01, 0x15, 0x07, 0x00, 0xFF, data.active_power_plus_l1);
  read_u32(0x01, 0x01, 0x16, 0x07, 0x00, 0xFF, data.active_power_minus_l1);
  read_u32(0x01, 0x01, 0x29, 0x07, 0x00, 0xFF, data.active_power_plus_l2);
  read_u32(0x01, 0x01, 0x2A, 0x07, 0x00, 0xFF, data.active_power_minus_l2);
  read_u32(0x01, 0x01, 0x3D, 0x07, 0x00, 0xFF, data.active_power_plus_l3);
  read_u32(0x01, 0x01, 0x3E, 0x07, 0x00, 0xFF, data.active_power_minus_l3);

  read_u16(0x01, 0x01, 0x20, 0x07, 0x00, 0xFF, data.voltage_l1);
  read_u16(0x01, 0x01, 0x34, 0x07, 0x00, 0xFF, data.voltage_l2);
  read_u16(0x01, 0x01, 0x48, 0x07, 0x00, 0xFF, data.voltage_l3);

  read_u32(0x01, 0x01, 0x1F, 0x07, 0x00, 0xFF, data.current_l1, 0.01f);
  read_u32(0x01, 0x01, 0x33, 0x07, 0x00, 0xFF, data.current_l2, 0.01f);
  read_u32(0x01, 0x01, 0x47, 0x07, 0x00, 0xFF, data.current_l3, 0.01f);

  read_u32(0x01, 0x01, 0x01, 0x08, 0x00, 0xFF, data.active_energy_plus, 10.0f);
  read_u32(0x01, 0x01, 0x02, 0x08, 0x00, 0xFF, data.active_energy_minus, 10.0f);
  read_u32(0x01, 0x01, 0x03, 0x08, 0x00, 0xFF, data.reactive_energy_plus, 10.0f);
  read_u32(0x01, 0x01, 0x04, 0x08, 0x00, 0xFF, data.reactive_energy_minus, 10.0f);
  read_u32(0x01, 0x01, 0x15, 0x08, 0x00, 0xFF, data.active_energy_plus_l1, 10.0f);
  read_u32(0x01, 0x01, 0x16, 0x08, 0x00, 0xFF, data.active_energy_minus_l1, 10.0f);
  read_u32(0x01, 0x01, 0x29, 0x08, 0x00, 0xFF, data.active_energy_plus_l2, 10.0f);
  read_u32(0x01, 0x01, 0x2A, 0x08, 0x00, 0xFF, data.active_energy_minus_l2, 10.0f);
  read_u32(0x01, 0x01, 0x3D, 0x08, 0x00, 0xFF, data.active_energy_plus_l3, 10.0f);
  read_u32(0x01, 0x01, 0x3E, 0x08, 0x00, 0xFF, data.active_energy_minus_l3, 10.0f);

  read_u16(0x01, 0x01, 0x21, 0x07, 0x00, 0xFF, data.power_factor_l1);
  read_u16(0x01, 0x01, 0x35, 0x07, 0x00, 0xFF, data.power_factor_l2);
  read_u16(0x01, 0x01, 0x49, 0x07, 0x00, 0xFF, data.power_factor_l3);
  read_u16(0x01, 0x01, 0x0D, 0x07, 0x00, 0xFF, data.power_factor_total);

  return data;
}

}  // namespace esphome::dlms_meter
