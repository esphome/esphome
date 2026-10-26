#pragma once

#ifdef USE_ESP32
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_client/ble_characteristic.h"
#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"
#include "fendt_caravan_hub_base.h"
#include "framing.h"

namespace esphome::fendt_caravan {
using namespace std;

class FendtCaravan : public Component, public ble_client::BLEClientNode {
 public:
  void setup() override {};
  void loop() override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  void set_address(uint64_t address) { address_ = address; }
  void set_mcu_hub(FendtCaravanHubBase *mcu_hub) { this->mcu_hub_ = mcu_hub; }
  void set_alde_unit_hub(FendtCaravanHubBase *alde_unit_hub) { this->alde_unit_hub_ = alde_unit_hub; }
  void set_lighting_unit_hub(FendtCaravanHubBase *lighting_unit_hub) { this->lighting_unit_hub_ = lighting_unit_hub; }
  void set_fridge_unit_hub(FendtCaravanHubBase *fridge_unit_hub) { this->fridge_unit_hub_ = fridge_unit_hub; }

  void dump_config() override;
  void send_command(const std::string &command);

 protected:
  static constexpr size_t QUEUE_SIZE = 4;
  static constexpr size_t REPORT_MAX = 128;

  void add_command_(const std::string &cmd);
  void on_data_received_(const char *data);

 private:
  volatile bool wait_buffer_ = false;
  uint16_t char_handle_ = 0;
  StaticRingBuffer<std::string, QUEUE_SIZE> commands_;
  uint32_t last_command_time_ = 0;
  FrameAssembler<REPORT_MAX> rx_;

  FendtCaravanHubBase *mcu_hub_{nullptr};
  FendtCaravanHubBase *alde_unit_hub_{nullptr};
  FendtCaravanHubBase *lighting_unit_hub_{nullptr};
  FendtCaravanHubBase *fridge_unit_hub_{nullptr};
};
}  // namespace esphome::fendt_caravan
#endif
