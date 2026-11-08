#pragma once

#ifdef USE_ESP32
#include <array>
#include <cstring>
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_client/ble_characteristic.h"
#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "fendt_caravan_hub_base.h"
#include "framing.h"

namespace esphome::fendt_caravan {

class FendtCaravan : public Component, public ble_client::BLEClientNode {
 public:
  /// `handshake` is the "net-BT_ID-..." line that opens the session; a string literal from codegen
  explicit FendtCaravan(const char *handshake) : handshake_(handshake) {}
  void loop() override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  void set_mcu_hub(FendtCaravanHubBase *mcu_hub) { this->mcu_hub_ = mcu_hub; }

  void dump_config() override;
  /// Queues a NUL-terminated command; nullptr (a command that did not fit its buffer) is ignored
  void send_command(const char *command);

 protected:
  // The handshake alone takes 3 chunks
  static constexpr size_t QUEUE_SIZE = 8;
  static constexpr size_t REPORT_MAX = 128;

  struct Chunk {
    std::array<char, CHUNK_SIZE + 1> data;
    uint8_t len;
  };

  bool add_command_(const char *cmd, size_t len);
  // Splits the report in place at the first ':'
  void on_data_received_(char *data);
  void reset_session_();

 private:
  const char *handshake_;
  bool wait_handshake_ = false;
  uint16_t char_handle_ = 0;
  StaticRingBuffer<Chunk, QUEUE_SIZE> commands_;
  uint32_t last_command_time_ = 0;
  FrameAssembler<REPORT_MAX> rx_;

  FendtCaravanHubBase *mcu_hub_{nullptr};
};
}  // namespace esphome::fendt_caravan
#endif
