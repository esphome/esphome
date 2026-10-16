#pragma once

#ifdef USE_ESP32

#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"

#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace esphome::one_wire_device {

class OneWireDeviceComponent final : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;

  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }
  void set_address(uint64_t address) { this->address_ = address; }
  void set_temperature(float temperature) { this->temperature_ = temperature; }
  void set_timing_scale(uint8_t scale) { this->timing_scale_ = scale; }
  void set_interrupt_mode(bool enabled) { this->interrupt_mode_ = enabled; }

 protected:
  enum class SlotResult : uint8_t {
    OK,
    TIMEOUT,
    RESET,
  };

  enum class IsrState : uint8_t {
    RX_ROM_COMMAND,
    SEARCH_ID,
    SEARCH_COMPLEMENT,
    SEARCH_BRANCH,
    RX_MATCH_ROM,
    RX_FUNCTION,
    RX_WRITE_SCRATCHPAD,
    TX_BYTES,
    TX_POWER,
    IGNORE,
  };

  InternalGPIOPin *pin_{nullptr};
  ISRInternalGPIOPin isr_pin_;
  gpio_num_t gpio_num_{GPIO_NUM_NC};
  uint64_t address_{0};
  float temperature_{23.5f};
  uint8_t timing_scale_{1};
  bool interrupt_mode_{false};
  uint8_t rom_[8] = {0};
  uint8_t scratchpad_[9] = {0};
  uint8_t resolution_{12};
  TaskHandle_t task_handle_{nullptr};
  volatile bool stopping_{false};
  uint32_t last_stats_ms_{0};
  volatile uint32_t reset_count_{0};
  volatile uint32_t transaction_count_{0};
  volatile uint32_t rom_command_count_{0};
  volatile uint32_t function_command_count_{0};
  volatile uint32_t send_zero_count_{0};
  volatile uint32_t send_one_count_{0};
  volatile uint32_t slot_error_count_{0};
  volatile uint8_t last_error_status_{0};
  volatile IsrState isr_state_{IsrState::RX_ROM_COMMAND};
  volatile uint8_t isr_bit_index_{0};
  volatile uint8_t isr_byte_index_{0};
  volatile uint8_t isr_rx_byte_{0};
  volatile uint8_t isr_rx_buffer_[8] = {0};
  volatile uint8_t isr_tx_buffer_[9] = {0};
  volatile uint8_t isr_tx_length_{0};
  volatile uint8_t isr_search_bit_{0};
  volatile bool isr_drive_active_{false};
  volatile bool isr_ignore_edge_{false};
  IsrState isr_after_tx_state_{IsrState::RX_ROM_COMMAND};
  int16_t temperature_raw_{0};

  // The ISR path must stay bounded, allocation-free, log-free, and IRAM-safe.
  static void task_entry(void *arg);
  static void IRAM_ATTR gpio_isr(void *arg);
  void handle_isr_edge_();
  void isr_reset_();
  void isr_receive_bit_(bool bit);
  void isr_complete_byte_(uint8_t value);
  void isr_transmit_bit_();
  void task_loop_();

  void configure_rom_();
  void configure_scratchpad_();
  void update_scratchpad_crc_();
  static uint8_t IRAM_ATTR crc8(const uint8_t *data, uint8_t length);

  void release_bus_();
  void drive_bus_low_();
  bool wait_for_low_(uint32_t timeout_us);
  bool wait_for_high_(uint32_t timeout_us);
  bool handle_reset_();
  SlotResult receive_bit_(bool *value);
  SlotResult send_bit_(bool value);
  SlotResult receive_byte_(uint8_t *value);
  SlotResult send_byte_(uint8_t value);
  SlotResult receive_bytes_(uint8_t *data, uint8_t length);
  SlotResult send_bytes_(const uint8_t *data, uint8_t length);
  void serve_transaction_();
  void drain_notifications_();
  uint32_t scale_(uint32_t value) const { return value * this->timing_scale_; }
};

}  // namespace esphome::one_wire_device

#endif  // USE_ESP32
