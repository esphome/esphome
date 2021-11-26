#include "exposure_notifications.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP32

namespace esphome {
namespace exposure_notifications {

using namespace esp32_ble_tracker;

static const char *const TAG = "exposure_notifications";

bool ExposureNotificationTrigger::parse_device(const ESPBTDevice &device) {
  // See also https://blog.google/documents/70/Exposure_Notification_-_Bluetooth_Specification_v1.2.2.pdf
  if (device.get_service_uuids().size() != 1)
    return false;

  // Exposure notifications have Service UUID FD 6F
  ESPBTUUID uuid = device.get_service_uuids()[0];
  // constant service identifier
  const ESPBTUUID expected_uuid = ESPBTUUID::from_uint16(0xFD6F);
  if (uuid != expected_uuid)
    return false;
  if (device.get_service_datas().size() != 1)
    return false;

  // The service data should be 20 bytes
  // First 16 bytes are the rolling proximity identifier (RPI)
  // Then 4 bytes of encrypted metadata follow which can be used to get the transmit power level.
  ServiceData service_data = device.get_service_datas()[0];
  if (service_data.uuid != expected_uuid)
    return false;
  auto data = service_data.data;
  if (data.size() != 20)
    return false;
  ExposureNotification notification{};
  memcpy(&notification.address[0], device.address(), 6);
  memcpy(&notification.rolling_proximity_identifier[0], &data[0], 16);
  memcpy(&notification.associated_encrypted_metadata[0], &data[16], 4);
  notification.rssi = device.get_rssi();
  this->trigger(notification);
  return true;
}

}  // namespace exposure_notifications
}  // namespace esphome

#endif
