#pragma once

#ifdef USE_ESP32
#include "esphome/components/esp32_ble_client/ble_characteristic.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/core/application.h"
#include "esphome/core/string_ref.h"
#include "fendt_caravan_hub_base.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome::fendt_caravan {
using namespace std;

class FendtCaravan : public Component, public ble_client::BLEClientNode {
 public:
  void setup() override {};
  void loop() override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  void set_address(uint64_t address) { address_ = address; };
  void set_mcu_hub(FendtCaravanHubBase *mcu_hub) { this->mcu_hub_ = mcu_hub; };
  void set_alde_unit_hub(FendtCaravanHubBase *alde_unit_hub) { this->alde_unit_hub_ = alde_unit_hub; };
  void set_lighting_unit_hub(FendtCaravanHubBase *lighting_unit_hub) { this->lighting_unit_hub_ = lighting_unit_hub; };
  void set_fridge_unit_hub(FendtCaravanHubBase *fridge_unit_hub) { this->fridge_unit_hub_ = fridge_unit_hub; };

  void dump_config() override;
  void send_command(const std::string &command);

 protected:
  void add_command_(const std::string &cmd);
  void on_data_received_(const std::string &data);

 private:
  const char *service_uuid_ = "C7841029-FE7C-4894-8532-F97908EF1AE4";  // değiştirilmesi gerekebilir
  const uint16_t characteristic_uuid_ = 0x0001;
  bool command_enabled_ = false;
  volatile bool wait_buffer_ = false;
  uint16_t char_handle_ = 0;
  std::vector<std::string> commands_;
  uint32_t last_command_time_ = 0;
  std::string last_response_ = "";

  FendtCaravanHubBase *mcu_hub_{nullptr};
  FendtCaravanHubBase *alde_unit_hub_{nullptr};
  FendtCaravanHubBase *lighting_unit_hub_{nullptr};
  FendtCaravanHubBase *fridge_unit_hub_{nullptr};
};
}  // namespace esphome::fendt_caravan
#endif
