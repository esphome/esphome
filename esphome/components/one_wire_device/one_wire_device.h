#pragma once
#include "esphome/core/defines.h"
#ifdef USE_ESP32
#include <atomic>
#include <esp_intr_alloc.h>
#include "device_model.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/gpio.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome::one_wire_device {
class OneWireDeviceComponent final : public Component {
 public:
  void setup() override;
  void dump_config() override;
  void on_shutdown() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }
  void set_address(uint64_t address) { this->address_ = address; }
  void set_family(uint8_t family) { this->family_ = family; }
  bool set_temperature(float value);
#ifdef USE_SENSOR
  void set_source_sensor(sensor::Sensor *source) {
    this->source_ = source;
    this->temperature_.store(INT32_MIN);
  }
#endif
#ifdef USE_BINARY_SENSOR
  void set_pio_source(uint8_t channel, binary_sensor::BinarySensor *source) {
    this->pio_sources_[channel] = source;
    this->pio_.store(this->pio_.load() & ~(1U << (channel + 8)));
  }
#endif

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
  void set_pio_(uint8_t channel, bool value);
  InternalGPIOPin *pin_{nullptr};
  uint64_t address_{0};
  uint8_t family_{0x28}, pin_number_{0}, interrupt_core_{0};
  intr_handle_t interrupt_handle_{nullptr};
  esp_err_t interrupt_error_{ESP_OK};
  std::atomic<int32_t> temperature_{376};  // One scalar snapshot, in 1/16 C; INT32_MIN means unavailable.
  std::atomic<uint32_t> pio_{0x303};       // Input levels and independently initialized validity bits.
  std::atomic<bool> stopping_{false};
  std::atomic<uint32_t> resets_{0}, slots_{0};
  DeviceModel model_;
  bool awaiting_release_{false}, active_session_{false};
  uint32_t falling_us_{0};
#ifdef USE_SENSOR
  sensor::Sensor *source_{nullptr};
#endif
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *pio_sources_[2]{};
#endif
};
template<typename... Ts> class SetTemperatureAction final : public Action<Ts...> {
 public:
  explicit SetTemperatureAction(OneWireDeviceComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(float, temperature)
  void play(Ts... x) override { this->parent_->set_temperature(this->temperature_.value(x...)); }

 protected:
  OneWireDeviceComponent *parent_;
};
}  // namespace esphome::one_wire_device
#endif
