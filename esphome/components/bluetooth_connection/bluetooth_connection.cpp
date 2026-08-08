#include "bluetooth_connection.h"

#ifdef BLUETOOTH_CONNECTION_HAS_GATT

#include "esphome/components/api/api_pb2.h"
#include "esphome/core/log.h"

namespace esphome::bluetooth_connection {

static const char *const TAG = "bluetooth_connection";

BatchClose close_service_batch(api::BluetoothGATTGetServicesResponse &resp, size_t &current_size, int16_t &send_service,
                               uint8_t connection_index, const char *address_str) {
  // Calculate the actual size of just this service (+1 for the field tag)
  size_t service_size = resp.services.back().calculate_size() + 1;

  if (current_size + service_size > MAX_PACKET_SIZE) {
    if (resp.services.size() > 1) {
      // We would go over -- pop the last service and retry it in the next batch
      resp.services.pop_back();
      ESP_LOGD(TAG, "[%d] [%s] Service %d would exceed limit (current: %u + service: %u > %u), sending current batch",
               connection_index, address_str, send_service, (unsigned) current_size, (unsigned) service_size,
               (unsigned) MAX_PACKET_SIZE);
      // Don't advance send_service -- the popped service goes into the next batch
    } else {
      // This single service is too large, but we have to send it anyway;
      // advance so we don't get stuck
      ESP_LOGW(TAG, "[%d] [%s] Service %d is too large (%u bytes) but sending anyway", connection_index, address_str,
               send_service, (unsigned) service_size);
      send_service++;
    }
    return BatchClose::SEND;
  }

  current_size += service_size;
  send_service++;
  return BatchClose::CONTINUE;
}

}  // namespace esphome::bluetooth_connection

#endif  // BLUETOOTH_CONNECTION_HAS_GATT
