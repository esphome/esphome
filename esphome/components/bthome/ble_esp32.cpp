#include "ble_esp32.h"

#include "esphome/core/log.h"

namespace esphome::bthome {

#ifdef USE_ESP32

static const char *const TAG = "bthome";

bool ESP32BLEListener::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  bool matched = false;
  for (auto &service_data : device.get_service_datas()) {
    if (!service_data.uuid.contains(BTHOME_SVC_UUID_LOW, BTHOME_SVC_UUID_HIGH)) {
      continue;
    }

    const uint8_t *data = service_data.data.data();
    size_t data_size = service_data.data.size();

    if (data_size < sizeof(BTHomeHeader)) {
      ESP_LOGVV(TAG, "BTHome data too short: %zu", data_size);
      continue;
    }

    const BTHomeHeader &header = *reinterpret_cast<const BTHomeHeader *>(data);
    if (header.version != BTHOME_VERSION_2) {
      ESP_LOGVV(TAG, "Unsupported BTHome version %u", header.version);
      continue;
    }

    if (this->listener_->on_bthome_data(device.address(), data, data_size))
      matched = true;
  }
  return matched;
}

#endif  // USE_ESP32

}  // namespace esphome::bthome
