
#pragma once

#ifdef USE_ESP32

#include <numbers>
#include "esphome/core/log.h"
//#include "sensor/caravan_device.h"
#include "esphome/core/component.h"
#include "esphome/core/string_ref.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_client/ble_characteristic.h"

namespace esphome {
namespace fendt_caravan {
using namespace std;

class FendtCaravan : public PollingComponent, public ble_client::BLEClientNode {
 public:
  void setup() override;
  void loop() override;
  void update() override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  void set_address(uint64_t address) { address_ = address; };
  // void set_control_unit(CaravanDevice *control_unit_device) {
  //   this->control_unit_device_ = control_unit_device;
  //   this->control_unit_device_->set_command_send_callback(
  //       [this](const std::string &cmd) { this->on_command_send(cmd); });
  // }
  //  void set_alde_device(CaravanDevice *device) {
  //    this->alde_device_ = device;
  //    this->alde_device_->set_command_send_callback([this](const std::string &cmd) { this->on_command_send(cmd); });
  //  }
  //  void set_fridge_device(CaravanDevice *device) {
  //    this->fridge_device_ = device;
  //    this->fridge_device_->set_command_send_callback([this](const std::string &cmd) { this->on_command_send(cmd); });
  //  }
  //  void set_lighting_device(CaravanDevice *device) {
  //    this->lighting_device_ = device;
  //    this->lighting_device_->set_command_send_callback([this](const std::string &cmd) { this->on_command_send(cmd);
  //    });
  //  }
  void dump_config() override;
  void on_command_send(const std::string &command);

 protected:
  void add_command_(const std::string &cmd);
  void on_data_received_(const std::string &data);

 private:
  const char *service_uuid_ = "C7841029-FE7C-4894-8532-F97908EF1AE4";  // değiştirilmesi gerekebilir
  const uint16_t characteristic_uuid_ = 0x0001;
  bool command_enabled_ = false;
  volatile bool wait_buffer_ = false;
  uint16_t char_handle_;
  std::vector<std::string> commands_{};
  uint32_t last_command_time_ = 0;
  std::string last_response_ = {};
  // CaravanDevice *control_unit_device_{nullptr};
  // CaravanDevice *alde_device_{nullptr};
  // CaravanDevice *fridge_device_{nullptr};
  // CaravanDevice *lighting_device_{nullptr};
};
}  // namespace fendt_caravan
}  // namespace esphome
#endif
