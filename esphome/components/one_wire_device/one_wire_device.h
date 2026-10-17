#pragma once
#include "esphome/core/defines.h"
#ifdef USE_ESP32
#include <atomic>
#include <cmath>
#include <functional>
#include <utility>
#include <esp_intr_alloc.h>
#include "device_model.h"
#include "source_snapshot.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/gpio.h"

namespace esphome::one_wire_device {
class OneWireDeviceComponent final : public PollingComponent {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  void on_shutdown() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }
  void set_address(uint64_t address) { this->address_ = address; }
  void set_family(uint8_t family) { this->family_ = family; }
  bool set_value(uint8_t channel, float value);
  void set_value_function(uint8_t channel, std::function<float()> function);

 protected:
  static void IRAM_ATTR gpio_isr(void *arg);
  static void install_interrupt(void *arg);
  static void free_interrupt(void *arg);
  void IRAM_ATTR handle_edge_();
  void IRAM_ATTR finish_reset_();
  void IRAM_ATTR clear_interrupt_();
  void IRAM_ATTR enable_interrupt_();
  bool IRAM_ATTR read_() const;
  void IRAM_ATTR release_();
  void IRAM_ATTR pull_low_();
  bool IRAM_ATTR available_() const;
  void raise_value_warning_(const LogString *message);
  bool value_channel_supported_(uint8_t channel) const;
  InternalGPIOPin *pin_{nullptr};
  uint64_t address_{0};
  uint8_t family_{FAMILY_DS18B20}, pin_number_{0}, interrupt_core_{0};
  intr_handle_t interrupt_handle_{nullptr};
  esp_err_t interrupt_error_{ESP_OK};
  std::atomic<int32_t> temperature_{376};  // One scalar snapshot, in 1/16 C; INT32_MIN means unavailable.
  std::atomic<uint32_t> pio_{0x303};       // Input levels and independently initialized validity bits.
  std::atomic<bool> stopping_{false};
  DeviceModel model_;
  bool awaiting_release_{false}, active_session_{false};
  uint32_t falling_us_{0};
  bool value_warning_{false}, configuration_warning_{false};
  std::function<float()> value_functions_[3];
};
template<typename... Ts> class SetValueAction final : public Action<Ts...> {
 public:
  explicit SetValueAction(OneWireDeviceComponent *parent) : parent_(parent) {}
  void set_channel(uint8_t channel) { this->channel_ = channel; }
  TEMPLATABLE_VALUE(float, value)
  void play(Ts... x) override { this->parent_->set_value(this->channel_, this->value_.value(x...)); }

 protected:
  OneWireDeviceComponent *parent_;
  uint8_t channel_{0};
};
}  // namespace esphome::one_wire_device
#endif
