#pragma once

#include "esphome/core/defines.h"
#ifdef USE_ESP32

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "ecocomfort2_child.h"

namespace esphome::ecocomfort2 {

namespace espbt = esphome::esp32_ble_tracker;

// C_OPER byte 0: operating mode
static constexpr uint8_t OPER_OFF = 0x00;
static constexpr uint8_t OPER_IN = 0x01;
static constexpr uint8_t OPER_OUT = 0x02;
static constexpr uint8_t OPER_IN_OUT = 0x03;
static constexpr uint8_t OPER_SENSOR_OR_AUTO = 0x04;

// C_OPER byte 1: speed in the low nibble, flags in the high nibble
static constexpr uint8_t SPEED_OFF = 0x00;
static constexpr uint8_t SPEED_SLEEP = 0x01;
static constexpr uint8_t SPEED_VEL1 = 0x02;
static constexpr uint8_t SPEED_VEL2 = 0x03;
static constexpr uint8_t SPEED_VEL3 = 0x04;
static constexpr uint8_t SPEED_MASK = 0x0F;
static constexpr uint8_t FLAG_AUTO = 0x10;
static constexpr uint8_t FLAG_BOOST = 0x40;
static constexpr uint8_t FLAG_NIGHT = 0x80;

/** Hub component for Ecocomfort 2.0 VMC via BLE. */
class Ecocomfort2Hub : public esphome::ble_client::BLEClientNode, public PollingComponent {
 public:
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BLUETOOTH; }

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;

#ifdef ECOCOMFORT2_CHILD_COUNT
  void register_child(Ecocomfort2Client *obj);
#endif

  /// True once the link is established and paired.
  bool is_ready() const { return this->node_state == espbt::ClientState::ESTABLISHED && this->parent_->is_paired(); }

  /// Write C_OPER (mode byte, speed/flags byte) and read it back shortly after.
  void write_oper(uint8_t mode, uint8_t speed);

  /// Raw C_OPER values from the last readback.
  uint8_t get_mode() const { return this->mode_; }
  uint8_t get_speed() const { return this->speed_; }

 protected:
  static constexpr uint32_t AUTH_TIMEOUT_ID = 0;
  static constexpr uint32_t READBACK_TIMEOUT_ID = 1;
  static constexpr uint32_t AUTH_TIMEOUT_MS = 10000;
  static constexpr uint32_t READBACK_DELAY_MS = 300;

  void on_paired_();

#ifdef ECOCOMFORT2_CHILD_COUNT
  StaticVector<Ecocomfort2Client *, ECOCOMFORT2_CHILD_COUNT> children_;
#endif
  uint16_t char_handle_oper_{0};
  uint8_t mode_{OPER_OFF};
  uint8_t speed_{SPEED_OFF};
};

}  // namespace esphome::ecocomfort2

#endif  // USE_ESP32
