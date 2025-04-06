#include "xiaomi_miscale.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace xiaomi_miscale {

static const char *const TAG = "xiaomi_miscale";

void XiaomiMiscale::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi Miscale");
  ESP_LOGCONFIG(TAG, "  Bindkey: %s", format_hex_pretty(this->bindkey_, 16).c_str());
  LOG_SENSOR("  ", "Weight", this->weight_);
  LOG_SENSOR("  ", "Impedance", this->impedance_);
  LOG_SENSOR("  ", "Impedance Low", this->impedance_low_);
  LOG_SENSOR("  ", "Heart Rate", this->heart_rate_);
  LOG_SENSOR("  ", "Timestamp", this->timestamp_);
  LOG_SENSOR("  ", "Profile ID", this->profile_id_);
}

void XiaomiMiscale::set_bindkey(const std::string &bindkey) {
  memset(bindkey_, 0, 16);
  if (bindkey.size() != 32) {
    return;
  }
  char temp[3] = {0};
  for (int i = 0; i < 16; i++) {
    strncpy(temp, &(bindkey.c_str()[i * 2]), 2);
    bindkey_[i] = std::strtoul(temp, nullptr, 16);
  }
}

bool XiaomiMiscale::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  if (device.address_uint64() != this->address_) {
    ESP_LOGVV(TAG, "parse_device(): unknown MAC address.");
    return false;
  }
  ESP_LOGVV(TAG, "parse_device(): MAC address %s found.", device.address_str().c_str());

  bool success = false;
  for (auto &service_data : device.get_service_datas()) {
    auto res = parse_header_(service_data);
    if (!res.has_value())
      continue;

    if (!parse_message_(service_data.data, *res))
      continue;

    if (!report_results_(res, device.address_str()))
      continue;

    if (res->model == Model::MODEL_S400 &&
        (!res->profile_id.has_value() ||
         (!this->allow_any_profile_id_ && !this->allowed_profile_ids_.count(*res->profile_id)))) {
      continue;
    }

    if (res->weight.has_value() && this->weight_ != nullptr)
      this->weight_->publish_state(*res->weight);
    if (this->impedance_ != nullptr) {
      if (res->model == Model::MODEL_V1) {
        ESP_LOGW(TAG, "Impedance is only supported on version 2 and S400. Your scale was identified as version 1.");
      } else {
        if (res->impedance.has_value()) {
          this->impedance_->publish_state(*res->impedance);
        } else {
          if (this->clear_impedance_)
            this->impedance_->publish_state(NAN);
        }
      }
    }
    if (this->impedance_low_ != nullptr) {
      if (res->model == Model::MODEL_V1 || res->model == Model::MODEL_V2) {
        ESP_LOGW(TAG, "Impedance is only supported on S400. Your scale was identified as version %d.", res->model);
      } else if (res->impedance_low.has_value()) {
        this->impedance_low_->publish_state(*res->impedance_low);
      }
    }
    if (this->heart_rate_ != nullptr) {
      if (res->model == Model::MODEL_V1 || res->model == Model::MODEL_V2) {
        ESP_LOGW(TAG, "Heart rate is only supported on S400. Your scale was identified as version %d.", res->model);
      } else if (res->heart_rate.has_value()) {
        this->heart_rate_->publish_state(*res->heart_rate);
      }
    }
    if (this->timestamp_ != nullptr) {
      if (res->model == Model::MODEL_V1 || res->model == Model::MODEL_V2) {
        ESP_LOGW(TAG, "Timestamp is only supported on S400. Your scale was identified as version %d.", res->model);
      } else if (res->timestamp.has_value()) {
        this->timestamp_->publish_state(*res->timestamp);
      }
    }
    if (this->profile_id_ != nullptr) {
      if (res->model == Model::MODEL_V1 || res->model == Model::MODEL_V2) {
        ESP_LOGW(TAG, "Timestamp is only supported on S400. Your scale was identified as version %d.", res->model);
      } else if (res->profile_id.has_value()) {
        this->profile_id_->publish_state(*res->profile_id);
      }
    }

    success = true;
  }

  return success;
}

optional<ParseResult> XiaomiMiscale::parse_header_(const esp32_ble_tracker::ServiceData &service_data) {
  ParseResult result;
  if (service_data.uuid == esp32_ble_tracker::ESPBTUUID::from_uint16(0x181D) && service_data.data.size() == 10) {
    result.model = Model::MODEL_V1;
  } else if (service_data.uuid == esp32_ble_tracker::ESPBTUUID::from_uint16(0x181B) && service_data.data.size() == 13) {
    result.model = Model::MODEL_V2;
  } else if (service_data.uuid == esp32_ble_tracker::ESPBTUUID::from_uint16(0xFE95) && service_data.data.size() == 24) {
    result.model = Model::MODEL_S400;
  } else {
    ESP_LOGVV(TAG,
              "parse_header(): Couldn't identify scale version or data size was not correct. UUID: %s, data_size: %d",
              service_data.uuid.to_string().c_str(), service_data.data.size());
    return {};
  }

  return result;
}

bool XiaomiMiscale::parse_message_(const std::vector<uint8_t> &message, ParseResult &result) {
  if (result.model == Model::MODEL_V1) {
    return parse_message_v1_(message, result);
  } else if (result.model == Model::MODEL_V2) {
    return parse_message_v2_(message, result);
  } else if (result.model == Model::MODEL_S400) {
    return parse_message_s400_(message, result);
  } else {
    ESP_LOGVV(TAG, "parse_message(): unsupported model: %s", get_model_name_(result.model).c_str());
    return false;
  }
}

bool XiaomiMiscale::parse_message_v1_(const std::vector<uint8_t> &message, ParseResult &result) {
  // message size is checked in parse_header
  // 1-2 Weight (MISCALE 181D)
  // 3-4 Years (MISCALE 181D)
  // 5 month (MISCALE 181D)
  // 6 day (MISCALE 181D)
  // 7 hour (MISCALE 181D)
  // 8 minute (MISCALE 181D)
  // 9 second (MISCALE 181D)

  const uint8_t *data = message.data();

  // weight, 2 bytes, 16-bit  unsigned integer, 1 kg
  const int16_t weight = uint16_t(data[1]) | (uint16_t(data[2]) << 8);
  if (data[0] == 0x22 || data[0] == 0xa2) {
    result.weight = weight * 0.01f / 2.0f;  // unit 'kg'
  } else if (data[0] == 0x12 || data[0] == 0xb2) {
    result.weight = weight * 0.01f * 0.6f;  // unit 'jin'
  } else if (data[0] == 0x03 || data[0] == 0xb3) {
    result.weight = weight * 0.01f * 0.453592f;  // unit 'lbs'
  }

  return true;
}

bool XiaomiMiscale::parse_message_v2_(const std::vector<uint8_t> &message, ParseResult &result) {
  // message size is checked in parse_header
  // 2-3 Years (MISCALE 2 181B)
  // 4 month (MISCALE 2 181B)
  // 5 day (MISCALE 2 181B)
  // 6 hour (MISCALE 2 181B)
  // 7 minute (MISCALE 2 181B)
  // 8 second (MISCALE 2 181B)
  // 9-10 impedance (MISCALE 2 181B)
  // 11-12 weight (MISCALE 2 181B)

  const uint8_t *data = message.data();

  bool has_impedance = ((data[1] & (1 << 1)) != 0);
  bool is_stabilized = ((data[1] & (1 << 5)) != 0);
  bool load_removed = ((data[1] & (1 << 7)) != 0);

  if (!is_stabilized || load_removed) {
    return false;
  }

  // weight, 2 bytes, 16-bit  unsigned integer, 1 kg
  const int16_t weight = uint16_t(data[11]) | (uint16_t(data[12]) << 8);
  if (data[0] == 0x02) {
    result.weight = weight * 0.01f / 2.0f;  // unit 'kg'
  } else if (data[0] == 0x03) {
    result.weight = weight * 0.01f * 0.453592f;  // unit 'lbs'
  }

  if (has_impedance) {
    // impedance, 2 bytes, 16-bit
    const int16_t impedance = uint16_t(data[9]) | (uint16_t(data[10]) << 8);
    result.impedance = impedance;

    if (impedance == 0 || impedance >= 3000) {
      return false;
    }
  }

  return true;
}

bool XiaomiMiscale::parse_message_s400_(const std::vector<uint8_t> &message, ParseResult &result) {
  const uint8_t *raw = message.data();

  const bool has_data = raw[0] & 0x40;
  if (!has_data) {
    ESP_LOGVV(TAG, "parse_message_s400(): service data has no DATA flag.");
    return false;
  }

  const uint16_t device_id = encode_uint16(raw[3], raw[2]);
  if (device_id != 0x3BD5) {
    ESP_LOGVV(TAG, "parse_message_s400(): unknown device ID (%X).", device_id);
    return false;
  }

  static uint8_t last_frame_count = 0;
  if (last_frame_count == raw[4]) {
    ESP_LOGVV(TAG, "parse_message_s400(): duplicate data packet received (%d).", static_cast<int>(last_frame_count));
    return false;
  }
  last_frame_count = raw[4];

  const bool has_encryption = raw[0] & 0x08;
  if (has_encryption && this->bindkey_[0] == 0) {  // unset
    ESP_LOGW(TAG, "parse_message_s400(): received encrypted packet but bindkey is not set.");
    return false;
  } else if (has_encryption && (!(xiaomi_ble::decrypt_xiaomi_payload(const_cast<std::vector<uint8_t> &>(message),
                                                                     this->bindkey_, this->address_)))) {
    ESP_LOGW(TAG, "parse_message_s400(): decryption failed, bindkey might be wrong.");
    return false;
  }

  uint8_t raw_offset = 5;
  const uint16_t value_type = encode_uint16(raw[raw_offset + 1], raw[raw_offset + 0]);
  if (value_type != 0x6E16) {
    ESP_LOGVV(TAG, "parse_message_s400(): unknown value type (%X).", value_type);
    return false;
  }
  const uint8_t value_length = raw[raw_offset + 2];
  if (value_length != 9) {
    ESP_LOGVV(TAG, "parse_message_s400(): value has wrong size (%d)!", value_length);
    return false;
  }

  const uint8_t *data = raw + raw_offset + 3;
  // 0 profile ID (S400 FE95)
  // 1-4 metrics (int(impedance * 10) << 18) + ((heart_rate - 50) << 11) + int(weight * 10)
  //     bits 0-10 weight
  //     bits 11-17 heart rate
  //     bits 18-31 impedance high if weight == 0 and heart rate == 0, otherwise impedance low
  // 5-8 UNIX epoch timestamp
  // ref: https://github.com/AlexxIT/XiaomiGateway3/issues/1388#issuecomment-2444669822

  // timestamp, 4 bytes, 32-bit unsigned integer, UNIX epoch
  result.timestamp = encode_uint32(data[8], data[7], data[6], data[5]);
  // compressed metrics, 4 bytes
  const uint32_t data_int = encode_uint32(data[4], data[3], data[2], data[1]);
  // weight, 11-bit unsigned integer, 10x float, 0.1 kg
  const uint16_t weight = data_int & 0x7FF;
  if (weight != 0) {
    result.weight = weight / 10.0f;
  }
  // heart rate, 7-bit unsigned integer, -50 offset, 1 bpm
  const uint8_t heart_rate = (data_int >> 11) & 0x7F;
  if (heart_rate > 0 && heart_rate < 127) {
    result.heart_rate = heart_rate + 50;
  }
  // impedance high or low, 14-bit unsigned integer, 10x float, 0.1 Ohm
  const uint16_t impedance = data_int >> 18;
  if (impedance != 0) {
    if (weight == 0 && heart_rate == 0) {  // second packet with only impedance low metric
      result.impedance_low = impedance / 10.0f;
    } else {
      result.impedance = impedance / 10.0f;
    }
  }
  // profile ID, 1 byte, 8-bit unsigned integer, 1
  result.profile_id = data[0];

  return true;
}

std::string XiaomiMiscale::get_model_name_(const Model model) {
  switch (model) {
    case Model::MODEL_V1:
      return "V1";
    case Model::MODEL_V2:
      return "V2";
    case Model::MODEL_S400:
      return "S400";
    default:
      return "UNKNOWN";
  }
}

bool XiaomiMiscale::report_results_(const optional<ParseResult> &result, const std::string &address) {
  if (!result.has_value()) {
    ESP_LOGVV(TAG, "report_results(): no results available.");
    return false;
  }

  ESP_LOGI(TAG, "Got Xiaomi Miscale %s (%s):", get_model_name_(result->model).c_str(), address.c_str());

  if (result->weight.has_value()) {
    ESP_LOGI(TAG, "  Weight: %.2fkg", *result->weight);
  }
  if (result->impedance.has_value()) {
    ESP_LOGI(TAG, "  Impedance: %.1fohm", *result->impedance);
  }
  if (result->impedance_low.has_value()) {
    ESP_LOGI(TAG, "  Impedance Low: %.1fohm", *result->impedance_low);
  }
  if (result->heart_rate.has_value()) {
    ESP_LOGI(TAG, "  Heart Rate: %ubpm", *result->heart_rate);
  }
  if (result->timestamp.has_value()) {
    ESP_LOGI(TAG, "  Timestamp: %u", *result->timestamp);
  }
  if (result->profile_id.has_value()) {
    ESP_LOGI(TAG, "  Profile ID: %u", *result->profile_id);
  }

  return true;
}

}  // namespace xiaomi_miscale
}  // namespace esphome

#endif
