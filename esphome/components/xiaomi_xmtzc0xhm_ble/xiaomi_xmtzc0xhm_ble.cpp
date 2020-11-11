#include "xiaomi_xmtzc0xhm_ble.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace xiaomi_xmtzc0xhm_ble {

static const char *TAG = "xiaomi_xmtzc0xhm_ble";

bool parse_xiaomi_data_byte(uint8_t data_type, const uint8_t *data, uint8_t data_length, XiaomiParseResult &result) {
  switch (data_type) {
    case 0x0D: {  // temperature+humidity, 4 bytes, 16-bit signed integer (LE) each, 0.1 °C, 0.1 %
      if (data_length != 4)
        return false;
      const int16_t temperature = uint16_t(data[0]) | (uint16_t(data[1]) << 8);
      const int16_t humidity = uint16_t(data[2]) | (uint16_t(data[3]) << 8);
      result.temperature = temperature / 10.0f;
      result.humidity = humidity / 10.0f;
      return true;
    }
    case 0x0A: {  // battery, 1 byte, 8-bit unsigned integer, 1 %
      if (data_length != 1)
        return false;
      result.battery_level = data[0];
      return true;
    }
    case 0x06: {  // humidity, 2 bytes, 16-bit signed integer (LE), 0.1 %
      if (data_length != 2)
        return false;
      const int16_t humidity = uint16_t(data[0]) | (uint16_t(data[1]) << 8);
      result.humidity = humidity / 10.0f;
      return true;
    }
    case 0x04: {  // temperature, 2 bytes, 16-bit signed integer (LE), 0.1 °C
      if (data_length != 2)
        return false;
      const int16_t temperature = uint16_t(data[0]) | (uint16_t(data[1]) << 8);
      result.temperature = temperature / 10.0f;
      return true;
    }
    case 0x09: {  // conductivity, 2 bytes, 16-bit unsigned integer (LE), 1 µS/cm
      if (data_length != 2)
        return false;
      const uint16_t conductivity = uint16_t(data[0]) | (uint16_t(data[1]) << 8);
      result.conductivity = conductivity;
      return true;
    }
    case 0x07: {  // illuminance, 3 bytes, 24-bit unsigned integer (LE), 1 lx
      if (data_length != 3)
        return false;
      const uint32_t illuminance = uint32_t(data[0]) | (uint32_t(data[1]) << 8) | (uint32_t(data[2]) << 16);
      result.illuminance = illuminance;
      return true;
    }
    case 0x08: {  // soil moisture, 1 byte, 8-bit unsigned integer, 1 %
      if (data_length != 1)
        return false;
      result.moisture = data[0];
      return true;
    }
    case 0x16: {  // weight, 2 bytes, 16-bit  unsigned integer, 1 kg
      if (result.type == XiaomiParseResult::TYPE_XMTZC0XHM) {
        switch (data_length) {
          case 10: {
            const uint16_t weight = uint16_t(data[1]) | (uint16_t(data[2]) << 8);
            if (data[0] == 0x22 || data[0] == 0xa2)
              result.weight = weight * 0.01f / 2.0f;
            else if (data[0] == 0x12 || data[0] == 0xb2)
              result.weight = weight * 0.01f * 0.6;
            else if (data[0] == 0x03 || data[0] == 0xb3)
              result.weight = weight * 0.01f * 0.453592;
            else
              return false;

            return true;
          }
          case 13: {
            const uint16_t weight = uint16_t(data[11]) | (uint16_t(data[12]) << 8);
            const uint16_t impedance = uint16_t(data[9]) | (uint16_t(data[10]) << 8);
            result.impedance = impedance;

            if (data[0] == 0x02) 
              result.weight = weight * 0.01f / 2.0f;
            else if (data[0] == 0x03) 
              result.weight = weight * 0.01f * 0.453592;
            else
              return false;

            return true;
          }
        }
      }
    }
    default:
      return false;
  }
}

bool parse_xiaomi_service_data(XiaomiParseResult &result, const esp32_ble_tracker::ServiceData &service_data) {
  if (!service_data.uuid.contains(0x95, 0xFE) && !service_data.uuid.contains(0x1D, 0x18)) {
    ESP_LOGVV(TAG, "Xiaomi no service data UUID magic bytes");
    return false;
  }

  const auto raw = service_data.data;

  if (raw.size() < 13) {
    ESP_LOGVV(TAG, "Xiaomi service data too short!");
    return false;
  }

  bool is_lywsdcgq = (raw[1] & 0x20) == 0x20 && raw[2] == 0xAA && raw[3] == 0x01;
  bool is_hhccjcy01 = (raw[1] & 0x20) == 0x20 && raw[2] == 0x98 && raw[3] == 0x00;
  bool is_lywsd02 = (raw[1] & 0x20) == 0x20 && raw[2] == 0x5b && raw[3] == 0x04;

  bool is_cgg1 = ((raw[1] & 0x30) == 0x30 || (raw[1] & 0x20) == 0x20) && raw[2] == 0x47 && raw[3] == 0x03;
  bool is_xmtzc0xhm = service_data.uuid.contains(0x1D, 0x18);
  bool is_mibfs = service_data.uuid.contains(0x1B, 0x18);

  if (!is_lywsdcgq && !is_hhccjcy01 && !is_lywsd02 && !is_cgg1 && !is_xmtzc0xhm && !is_mibfs && !is_lywsd02) {
    ESP_LOGVV(TAG, "Xiaomi no magic bytes");
    return false;
  }

  result.type = XiaomiParseResult::TYPE_HHCCJCY01;
  if (is_lywsdcgq) {
    result.type = XiaomiParseResult::TYPE_LYWSDCGQ;
  } else if (is_lywsd02) {
    result.type = XiaomiParseResult::TYPE_LYWSD02;
  } else if (is_cgg1) {
    result.type = XiaomiParseResult::TYPE_CGG1;
  } else if (is_xmtzc0xhm) {
    result.type = XiaomiParseResult::TYPE_XMTZC0XHM;
  } else if (is_mibfs) {
    result.type = XiaomiParseResult::TYPE_XMTZC0XHM;
  }

  uint8_t raw_offset = is_lywsdcgq || is_cgg1 ? 11 : 12;

  // Data point specs
  // Byte 0: type
  // Byte 1: fixed 0x10
  // Byte 2: length
  // Byte 3..3+len-1: data point value

  const uint8_t *raw_data = &raw[raw_offset];
  uint8_t data_offset = 0;
  uint8_t data_length = raw.size() - raw_offset;
  bool success = false;

  while (true) {
    if (data_length < 4)
      // at least 4 bytes required
      // type, fixed 0x10, length, 1 byte value
      break;

    const uint8_t datapoint_type = raw_data[data_offset + 0];
    const uint8_t datapoint_length = raw_data[data_offset + 2];

    if (data_length < 3 + datapoint_length)
      // 3 fixed bytes plus value length
      break;

    const uint8_t *datapoint_data = &raw_data[data_offset + 3];

    if (parse_xiaomi_data_byte(datapoint_type, datapoint_data, datapoint_length, result))
      success = true;

    data_length -= data_offset + 3 + datapoint_length;
    data_offset += 3 + datapoint_length;
  }
  // Hack for MiScale
  if (is_xmtzc0xhm || is_mibfs) {
    const uint8_t *datapoint_data = &raw[0];  // raw data
    if (parse_xiaomi_data_byte(0x16, datapoint_data, raw.size(), result))
      success = true;
  }

  return success;
}
optional<XiaomiParseResult> parse_xiaomi(const esp32_ble_tracker::ESPBTDevice &device) {
  XiaomiParseResult result;
  bool success = false;
  for (auto &service_data : device.get_service_datas()) {
    if (parse_xiaomi_service_data(result, service_data))
      success = true;
  }
  if (!success)
    return {};
  return result;
}

bool XiaomiListener::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  auto res = parse_xiaomi(device);
  if (!res.has_value())
    return false;

  const char *name = "HHCCJCY01";
  if (res->type == XiaomiParseResult::TYPE_LYWSDCGQ) {
    name = "LYWSDCGQ";
  } else if (res->type == XiaomiParseResult::TYPE_LYWSD02) {
    name = "LYWSD02";
  } else if (res->type == XiaomiParseResult::TYPE_CGG1) {
    name = "CGG1";
  } else if (res->type == XiaomiParseResult::TYPE_XMTZC0XHM) {
    name = "XMTZC0XHM";
  }

  ESP_LOGD(TAG, "Got Xiaomi %s (%s):", name, device.address_str().c_str());

  if (res->temperature.has_value()) {
    ESP_LOGD(TAG, "  Temperature: %.1f°C", *res->temperature);
  }
  if (res->humidity.has_value()) {
    ESP_LOGD(TAG, "  Humidity: %.1f%%", *res->humidity);
  }
  if (res->battery_level.has_value()) {
    ESP_LOGD(TAG, "  Battery Level: %.0f%%", *res->battery_level);
  }
  if (res->conductivity.has_value()) {
    ESP_LOGD(TAG, "  Conductivity: %.0fµS/cm", *res->conductivity);
  }
  if (res->illuminance.has_value()) {
    ESP_LOGD(TAG, "  Illuminance: %.0flx", *res->illuminance);
  }
  if (res->moisture.has_value()) {
    ESP_LOGD(TAG, "  Moisture: %.0f%%", *res->moisture);
  }
  if (res->weight.has_value()) {
    ESP_LOGD(TAG, "  Weight: %.1fkg", *res->weight);
  }
  if (res->impedance.has_value()) {
    ESP_LOGD(TAG, "  Impedance: %.0f", *res->impedance);
  }

  return true;
}

}  // namespace xiaomi_xmtzc0xhm_ble
}  // namespace esphome

#endif
