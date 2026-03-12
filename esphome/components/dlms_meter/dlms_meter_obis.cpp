#include "dlms_meter.h"

namespace esphome::dlms_meter {

static constexpr const char *TAG = "dlms_meter";

namespace {

void assign_numeric_obis_value(MeterData &data, uint16_t obis_cd, float value, bool &power_factor_found) {
  switch (obis_cd) {
    case OBIS_VOLTAGE_L1:
      data.voltage_l1 = value;
      break;
    case OBIS_VOLTAGE_L2:
      data.voltage_l2 = value;
      break;
    case OBIS_VOLTAGE_L3:
      data.voltage_l3 = value;
      break;
    case OBIS_CURRENT_L1:
      data.current_l1 = value;
      break;
    case OBIS_CURRENT_L2:
      data.current_l2 = value;
      break;
    case OBIS_CURRENT_L3:
      data.current_l3 = value;
      break;
    case OBIS_ACTIVE_POWER_PLUS:
      data.active_power_plus = value;
      break;
    case OBIS_ACTIVE_POWER_MINUS:
      data.active_power_minus = value;
      break;
    case OBIS_ACTIVE_POWER_PLUS_L1:
      data.active_power_plus_l1 = value;
      break;
    case OBIS_ACTIVE_POWER_MINUS_L1:
      data.active_power_minus_l1 = value;
      break;
    case OBIS_ACTIVE_POWER_PLUS_L2:
      data.active_power_plus_l2 = value;
      break;
    case OBIS_ACTIVE_POWER_MINUS_L2:
      data.active_power_minus_l2 = value;
      break;
    case OBIS_ACTIVE_POWER_PLUS_L3:
      data.active_power_plus_l3 = value;
      break;
    case OBIS_ACTIVE_POWER_MINUS_L3:
      data.active_power_minus_l3 = value;
      break;
    case OBIS_REACTIVE_POWER_PLUS:
      data.reactive_power_plus = value;
      break;
    case OBIS_REACTIVE_POWER_MINUS:
      data.reactive_power_minus = value;
      break;
    case OBIS_ACTIVE_ENERGY_PLUS:
      data.active_energy_plus = value;
      break;
    case OBIS_ACTIVE_ENERGY_MINUS:
      data.active_energy_minus = value;
      break;
    case OBIS_ACTIVE_ENERGY_PLUS_L1:
      data.active_energy_plus_l1 = value;
      break;
    case OBIS_ACTIVE_ENERGY_MINUS_L1:
      data.active_energy_minus_l1 = value;
      break;
    case OBIS_ACTIVE_ENERGY_PLUS_L2:
      data.active_energy_plus_l2 = value;
      break;
    case OBIS_ACTIVE_ENERGY_MINUS_L2:
      data.active_energy_minus_l2 = value;
      break;
    case OBIS_ACTIVE_ENERGY_PLUS_L3:
      data.active_energy_plus_l3 = value;
      break;
    case OBIS_ACTIVE_ENERGY_MINUS_L3:
      data.active_energy_minus_l3 = value;
      break;
    case OBIS_REACTIVE_ENERGY_PLUS:
      data.reactive_energy_plus = value;
      break;
    case OBIS_REACTIVE_ENERGY_MINUS:
      data.reactive_energy_minus = value;
      break;
    case OBIS_POWER_FACTOR:
      data.power_factor = value;
      data.power_factor_total = value;
      power_factor_found = true;
      break;
    case OBIS_POWER_FACTOR_L1:
      data.power_factor_l1 = value;
      break;
    case OBIS_POWER_FACTOR_L2:
      data.power_factor_l2 = value;
      break;
    case OBIS_POWER_FACTOR_L3:
      data.power_factor_l3 = value;
      break;
    default:
      ESP_LOGW(TAG, "Unsupported OBIS code 0x%04X", obis_cd);
      break;
  }
}

void copy_string_value(char *target, size_t target_size, uint8_t data_length, const uint8_t *value) {
  snprintf(target, target_size, "%.*s", data_length, value);
}

}  // namespace

void DlmsMeterComponent::decode_obis_(uint8_t *plaintext, uint16_t message_length) {
  if (this->provider_ == PROVIDER_KAMSTRUP_OMNIPOWER) {
    this->decode_kamstrup_omnipower_obis_(plaintext, message_length);
    return;
  }

  ESP_LOGV(TAG, "Decoding payload");
  MeterData data{};
  uint16_t current_position = DECODER_START_OFFSET;
  bool power_factor_found = false;
  bool has_break_after_data = true;

  while (current_position + OBIS_CODE_OFFSET <= message_length) {
    has_break_after_data = true;
    if (plaintext[current_position + OBIS_TYPE_OFFSET] != DataType::OCTET_STRING) {
      ESP_LOGE(TAG, "OBIS: Unsupported OBIS header type: %x", plaintext[current_position + OBIS_TYPE_OFFSET]);
      this->receive_buffer_.clear();
      return;
    }

    uint8_t obis_code_length = plaintext[current_position + OBIS_LENGTH_OFFSET];
    if (obis_code_length != OBIS_CODE_LENGTH_STANDARD && obis_code_length != OBIS_CODE_LENGTH_EXTENDED) {
      ESP_LOGE(TAG, "OBIS: Unsupported OBIS header length: %x", obis_code_length);
      this->receive_buffer_.clear();
      return;
    }
    if (current_position + OBIS_CODE_OFFSET + obis_code_length > message_length) {
      ESP_LOGE(TAG, "OBIS: Buffer too short for OBIS code");
      this->receive_buffer_.clear();
      return;
    }

    uint8_t *obis_code = &plaintext[current_position + OBIS_CODE_OFFSET];
    uint8_t obis_medium = obis_code[OBIS_A];
    uint16_t obis_cd = encode_uint16(obis_code[OBIS_C], obis_code[OBIS_D]);
    uint32_t obis_cde = encode_obis_cde(obis_code[OBIS_C], obis_code[OBIS_D], obis_code[OBIS_E]);

    bool timestamp_found = false;
    bool meter_number_found = false;
    bool kamstrup_omnipower_meter_number_found = false;
    bool kamstrup_omnipower_obis_list_version_found = false;
    if (this->provider_ == PROVIDER_NETZNOE) {
      if ((obis_code_length == OBIS_CODE_LENGTH_EXTENDED) && (current_position == DECODER_START_OFFSET)) {
        timestamp_found = true;
      } else if (power_factor_found) {
        meter_number_found = true;
        power_factor_found = false;
      } else {
        current_position += obis_code_length + OBIS_CODE_OFFSET;
      }
    } else {
      current_position += obis_code_length + OBIS_CODE_OFFSET;
    }
    if (this->provider_ == PROVIDER_KAMSTRUP_OMNIPOWER) {
      kamstrup_omnipower_meter_number_found = obis_cde == OBIS_METER_NUMBER;
      kamstrup_omnipower_obis_list_version_found = obis_cde == OBIS_LIST_VERSION_IDENTIFIER;
    }
    if (!timestamp_found && !meter_number_found && obis_medium != Medium::ELECTRICITY &&
        obis_medium != Medium::ABSTRACT) {
      ESP_LOGE(TAG, "OBIS: Unsupported OBIS medium: %x", obis_medium);
      this->receive_buffer_.clear();
      return;
    }

    if (current_position >= message_length) {
      ESP_LOGE(TAG, "OBIS: Buffer too short for data type");
      this->receive_buffer_.clear();
      return;
    }

    float value = 0.0f;
    uint8_t value_size = 0;
    uint8_t data_type = plaintext[current_position];
    current_position++;

    switch (data_type) {
      case DataType::DOUBLE_LONG_UNSIGNED: {
        constexpr uint8_t read_size = 4;
        value_size = read_size;
        if (current_position + read_size > message_length) {
          ESP_LOGE(TAG, "OBIS: Buffer too short for DOUBLE_LONG_UNSIGNED");
          this->receive_buffer_.clear();
          return;
        }
        uint32_t raw_value = encode_uint32(plaintext[current_position + 0], plaintext[current_position + 1],
                                           plaintext[current_position + 2], plaintext[current_position + 3]);
        value = raw_value;
        if (kamstrup_omnipower_meter_number_found) {
          snprintf(data.meter_number, sizeof(data.meter_number), "%lu", static_cast<unsigned long>(raw_value));
          value_size = 0;
        }
        current_position += read_size;
        break;
      }
      case DataType::LONG_UNSIGNED: {
        constexpr uint8_t read_size = 2;
        value_size = read_size;
        if (current_position + read_size > message_length) {
          ESP_LOGE(TAG, "OBIS: Buffer too short for LONG_UNSIGNED");
          this->receive_buffer_.clear();
          return;
        }
        uint16_t raw_value = encode_uint16(plaintext[current_position + 0], plaintext[current_position + 1]);
        value = raw_value;
        if (kamstrup_omnipower_meter_number_found) {
          snprintf(data.meter_number, sizeof(data.meter_number), "%u", raw_value);
          value_size = 0;
        }
        current_position += read_size;
        break;
      }
      case DataType::OCTET_STRING: {
        uint8_t data_length = plaintext[current_position];
        current_position++;
        if (current_position + data_length > message_length) {
          ESP_LOGE(TAG, "OBIS: Buffer too short for OCTET_STRING");
          this->receive_buffer_.clear();
          return;
        }
        if (obis_cd == OBIS_TIMESTAMP || timestamp_found) {
          if (data_length < 8) {
            ESP_LOGE(TAG, "OBIS: Timestamp data too short: %u", data_length);
            this->receive_buffer_.clear();
            return;
          }
          uint16_t year = encode_uint16(plaintext[current_position + 0], plaintext[current_position + 1]);
          uint8_t month = plaintext[current_position + 2];
          uint8_t day = plaintext[current_position + 3];
          uint8_t hour = plaintext[current_position + 5];
          uint8_t minute = plaintext[current_position + 6];
          uint8_t second = plaintext[current_position + 7];
          if (year > 9999 || month > 12 || day > 31 || hour > 23 || minute > 59 || second > 59) {
            ESP_LOGE(TAG, "Invalid timestamp values: %04u-%02u-%02uT%02u:%02u:%02uZ", year, month, day, hour, minute,
                     second);
            this->receive_buffer_.clear();
            return;
          }
          snprintf(data.timestamp, sizeof(data.timestamp), "%04u-%02u-%02uT%02u:%02u:%02uZ", year, month, day, hour,
                   minute, second);
        } else if (meter_number_found) {
          copy_string_value(data.meternumber, sizeof(data.meternumber), data_length, &plaintext[current_position]);
        } else if (kamstrup_omnipower_meter_number_found) {
          copy_string_value(data.meter_number, sizeof(data.meter_number), data_length, &plaintext[current_position]);
        } else if (kamstrup_omnipower_obis_list_version_found) {
          copy_string_value(data.obis_list_version, sizeof(data.obis_list_version), data_length,
                            &plaintext[current_position]);
        }
        current_position += data_length;
        has_break_after_data = false;
        break;
      }
      case DataType::VISIBLE_STRING: {
        uint8_t data_length = plaintext[current_position];
        current_position++;
        if (current_position + data_length > message_length) {
          ESP_LOGE(TAG, "OBIS: Buffer too short for VISIBLE_STRING");
          this->receive_buffer_.clear();
          return;
        }
        if (kamstrup_omnipower_meter_number_found) {
          copy_string_value(data.meter_number, sizeof(data.meter_number), data_length, &plaintext[current_position]);
        } else if (kamstrup_omnipower_obis_list_version_found) {
          copy_string_value(data.obis_list_version, sizeof(data.obis_list_version), data_length,
                            &plaintext[current_position]);
        }
        current_position += data_length;
        has_break_after_data = false;
        break;
      }
      default:
        ESP_LOGE(TAG, "OBIS: Unsupported OBIS data type: %x", data_type);
        this->receive_buffer_.clear();
        return;
    }

    if (has_break_after_data && this->provider_ == PROVIDER_NETZNOE) {
      if (!timestamp_found) {
        current_position += 2;
      }
    } else if (has_break_after_data) {
      current_position += 2;
    }

    if (current_position < message_length && plaintext[current_position] == DataType::INTEGER) {
      if (current_position + 1 < message_length) {
        int8_t scaler = static_cast<int8_t>(plaintext[current_position + 1]);
        if (scaler != 0) {
          value *= pow10_int(scaler);
        }
      }

      if (this->provider_ == PROVIDER_NETZNOE) {
        current_position += 4;
      } else {
        current_position += 6;
      }
    }

    if (value_size > 0) {
      assign_numeric_obis_value(data, obis_cd, value, power_factor_found);
    }
  }

  this->publish_parsed_data_(data);
}

}  // namespace esphome::dlms_meter
