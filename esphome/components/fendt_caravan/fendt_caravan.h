#pragma once

#ifdef USE_ESP32
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/core/helpers.h"
#include "framing.h"

#include <cstdint>

namespace esphome::fendt_caravan {

class MainControlUnitHub;

/// The BLE link to the caravan panel: writes commands in chunks and joins the report fragments it notifies.
/// BLEClient drives loop() while the client is connected.
class FendtCaravan : public ble_client::BLEClientNode {
 public:
  /// `handshake` is the "net-BT_ID-..." line that opens the session; a string literal from codegen
  explicit FendtCaravan(const char *handshake) : handshake_(handshake) {}
  void loop() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  void set_hub(MainControlUnitHub *hub) { this->hub_ = hub; }
  const char *get_handshake() const { return this->handshake_; }
  /// Queues a command, a string literal, to be written after the ones before it; false while the link is down
  /// or the queue is full
  bool send_command(const char *command);

 protected:
  static constexpr size_t QUEUE_SIZE = 4;
  static constexpr size_t REPORT_MAX = 128;

  void write_chunk_();
  void finish_write_(bool ok);
  /// `sent` is false when the command was given up before the panel could have taken it
  void pop_command_(bool sent);
  void reset_link_();
  void dispatch_(char *report);

  FrameAssembler<REPORT_MAX> rx_;
  StaticRingBuffer<const char *, QUEUE_SIZE> queue_;
  MainControlUnitHub *hub_{nullptr};
  const char *handshake_;
  uint32_t last_write_ms_{0};
  uint32_t last_fragment_ms_{0};
  uint16_t char_handle_{0};
  uint8_t chunk_pos_{0};  // bytes of the front command the panel has taken
  uint8_t retries_{0};
  bool write_pending_{false};
};

}  // namespace esphome::fendt_caravan
#endif
