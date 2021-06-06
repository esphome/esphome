#include "ble_component_handler_base.h"

#include <BLE2902.h>

#include "esphome/core/log.h"

#include "esp32_ble_controller.h"
#include "ble_utils.h"

namespace esphome {
namespace esp32_ble_controller {

static const char *TAG = "ble_component_handler_base";

BLEComponentHandlerBase::BLEComponentHandlerBase(Nameable* component, const BLECharacteristicInfoForHandler& characteristic_info) 
  : component(component), characteristic_info(characteristic_info)
{}

BLEComponentHandlerBase::~BLEComponentHandlerBase() 
{}

void BLEComponentHandlerBase::setup(BLEServer* ble_server) {
  const string& object_id = component->get_object_id();

  ESP_LOGCONFIG(TAG, "Setting up BLE characteristic for component %s", object_id.c_str());

  // Get or create the BLE service.
  const string& service_UUID = characteristic_info.service_UUID;
  BLEService* service = ble_server->getServiceByUUID(service_UUID);
  if (service == nullptr) {
    service = ble_server->createService(service_UUID);
  }

  // Create the BLE characteristic.
  const string& characteristic_UUID = characteristic_info.characteristic_UUID;
  if (can_receive_writes()) {
    characteristic = create_writeable_ble_characteristic(service, characteristic_UUID, this, get_component_description(), characteristic_info.use_BLE2902);
  } else {
    characteristic = create_read_only_ble_characteristic(service, characteristic_UUID, get_component_description(), characteristic_info.use_BLE2902);
  }

  service->start();

  ESP_LOGCONFIG(TAG, "%s: SRV %s - CHAR %s", object_id.c_str(), service_UUID.c_str(), characteristic_UUID.c_str());
}

void BLEComponentHandlerBase::send_value(float value) {
  const string& object_id = component->get_object_id();
  ESP_LOGD(TAG, "Update component %s to %f", object_id.c_str(), value);

  characteristic->setValue(value);
  characteristic->notify();
}

void BLEComponentHandlerBase::send_value(string value) {
  const string& object_id = component->get_object_id();
  ESP_LOGD(TAG, "Update component %s to %s", object_id.c_str(), value.c_str());

  characteristic->setValue(value);
  characteristic->notify();
}

void BLEComponentHandlerBase::send_value(bool raw_value) {
  const string& object_id = component->get_object_id();
  ESP_LOGD(TAG, "Update component %s to %d", object_id.c_str(), raw_value);

  uint16_t value = raw_value;
  characteristic->setValue(value);
  characteristic->notify();
}

void BLEComponentHandlerBase::onWrite(BLECharacteristic *characteristic) {
  global_ble_controller->execute_in_loop([this](){ on_characteristic_written(); });
}

bool BLEComponentHandlerBase::is_security_enabled() {
  return global_ble_controller->get_security_enabled();
}

} // namespace esp32_ble_controller
} // namespace esphome
