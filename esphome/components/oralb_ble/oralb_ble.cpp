#include "oralb_ble.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace oralb_ble {

static const char *TAG = "oralb_ble";

bool parse_oralb_data_byte(const esp32_ble_tracker::adv_data_t &adv_data, OralbParseResult &result) {
  result.state = adv_data[3];
  return true;
}
optional<OralbParseResult> parse_oralb(const esp32_ble_tracker::ESPBTDevice &device) {
  bool success = false;
  OralbParseResult result{};
  for (auto &it : device.get_manufacturer_datas()) {
    bool is_oralb = it.uuid.contains(0xDC, 0x00);
    if (!is_oralb)
      continue;

    if (parse_oralb_data_byte(it.data, result))
      success = true;
  }
  if (!success)
    return {};
  return result;
}

bool OralbListener::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  auto res = parse_oralb(device);
  if (!res.has_value())
    return false;

  ESP_LOGD(TAG, "Got OralB (%s):", device.address_str().c_str());

  if (res->state.has_value()) {
    ESP_LOGD(TAG, "  State: %d", *res->state);
  }

  return true;
}

}  // namespace oralb_ble
}  // namespace esphome

#endif
