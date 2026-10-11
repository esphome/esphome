#pragma once

#ifdef USE_ESP32

#include <esp_gattc_api.h>
#include <array>
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome::renogy_inverter_ble {

namespace espbt = esphome::esp32_ble_tracker;

// Largest Modbus response: 3 header + 2 * 32 data + 2 CRC bytes
static constexpr size_t MAX_FRAME = 69;

// Renogy inverter BLE GATT (RIV1230PU). Notify char 0xFFF1 (svc 0xFFF0), write char 0xFFD1
// (svc 0xFFD0), init char 0xFFD4 (svc 0xFFD0). Modbus device id 0x20, function 0x03.
class RenogyInverterBle : public ble_client::BLEClientNode, public PollingComponent {
  SUB_SENSOR(ac_input_voltage)
  SUB_SENSOR(ac_output_voltage)
  SUB_SENSOR(ac_output_current)
  SUB_SENSOR(ac_output_frequency)
  SUB_SENSOR(input_frequency)
  SUB_SENSOR(battery_voltage)
  SUB_SENSOR(temperature)
  SUB_SENSOR(load_current)
  SUB_SENSOR(load_active_power)
  SUB_SENSOR(load_apparent_power)

 public:
  void dump_config() override;
  void update() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

 protected:
  // Read cycle: IDLE -> (update) INIT read(0xFFD4) -> MAIN read(4000) -> LOAD read(4408) -> IDLE
  enum class State : uint8_t { IDLE, INIT, MAIN, LOAD };

  void start_cycle_();
  void read_register_(uint16_t start_register, uint16_t word_count);
  void on_frame_complete_(uint16_t len);
  void abort_cycle_();

  State state_{State::IDLE};
  uint8_t frame_len_{0};
  uint16_t notify_handle_{0};
  uint16_t write_handle_{0};
  uint16_t init_handle_{0};
  // Reassembles the Modbus response, which arrives in MTU-sized notification chunks
  std::array<uint8_t, MAX_FRAME> frame_;
};

}  // namespace esphome::renogy_inverter_ble

#endif  // USE_ESP32
