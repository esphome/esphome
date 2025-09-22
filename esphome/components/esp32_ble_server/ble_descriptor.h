#pragma once

#include "esphome/components/esp32_ble/ble_uuid.h"
#include "esphome/components/event_emitter/event_emitter.h"
#include "esphome/components/bytebuffer/bytebuffer.h"

#ifdef USE_ESP32

#include <esp_gatt_defs.h>
#include <esp_gatts_api.h>

namespace esphome {
namespace esp32_ble_server {

using namespace esp32_ble;
using namespace bytebuffer;
using namespace event_emitter;

class BLECharacteristic;

namespace BLEDescriptorEvt {
enum VectorEvt {
  ON_WRITE,
};
}  // namespace BLEDescriptorEvt

class BLEDescriptor : public EventEmitter<BLEDescriptorEvt::VectorEvt, std::vector<uint8_t>, uint16_t> {
 public:
  BLEDescriptor(ESPBTUUID uuid, uint16_t max_len = 100, bool read = true, bool write = true);
  virtual ~BLEDescriptor();
  void do_create(BLECharacteristic *characteristic);
  ESPBTUUID get_uuid() const { return this->uuid_; }

  void set_value(std::vector<uint8_t> buffer);
  void set_value(ByteBuffer buffer) { this->set_value(buffer.get_data()); }

  void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

  bool is_created() { return this->state_ == CREATED; }
  bool is_failed() { return this->state_ == FAILED; }

 protected:
  BLECharacteristic *characteristic_{nullptr};
  ESPBTUUID uuid_;
  uint16_t handle_{0xFFFF};

  esp_attr_value_t value_{};

  esp_gatt_perm_t permissions_{};

  enum State : uint8_t {
    FAILED = 0x00,
    INIT,
    CREATING,
    CREATED,
  } state_{INIT};
};

}  // namespace esp32_ble_server
}  // namespace esphome

#endif
