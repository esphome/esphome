#pragma once

#ifdef USE_ESP8266

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

#include <array>

namespace esphome {
namespace shelly_dimmer {

class ShellyDimmer : public PollingComponent, public light::LightOutput, public uart::UARTDevice {
 private:
  static constexpr uint16_t SHELLY_DIMMER_BUFFER_SIZE = 256;

 public:
  float get_setup_priority() const override { return setup_priority::LATE; }

  bool is_running_configured_version() const;
  void handle_firmware();
  void setup() override;
  void update() override;
  void dump_config() override;

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
    return traits;
  }

  void setup_state(light::LightState *state) override { this->state_ = state; }
  void write_state(light::LightState *state) override;

  void set_nrst_pin(GPIOPin *nrst_pin) { this->pin_nrst_ = nrst_pin; }
  void set_boot0_pin(GPIOPin *boot0_pin) { this->pin_boot0_ = boot0_pin; }

  void set_leading_edge(bool leading_edge) { this->leading_edge_ = leading_edge; }
  void set_warmup_brightness(uint16_t warmup_brightness) { this->warmup_brightness_ = warmup_brightness; }
  void set_warmup_time(uint16_t warmup_time) { this->warmup_time_ = warmup_time; }
  void set_fade_rate(uint16_t fade_rate) { this->fade_rate_ = fade_rate; }
  void set_min_brightness(uint16_t min_brightness) { this->min_brightness_ = min_brightness; }
  void set_max_brightness(uint16_t max_brightness) { this->max_brightness_ = max_brightness; }

  void set_power_sensor(sensor::Sensor *power_sensor) { this->power_sensor_ = power_sensor; }
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { this->voltage_sensor_ = voltage_sensor; }
  void set_current_sensor(sensor::Sensor *current_sensor) { this->current_sensor_ = current_sensor; }

 protected:
  GPIOPin *pin_nrst_;
  GPIOPin *pin_boot0_;

  // Frame parser state.
  uint8_t seq_{0};
  std::array<uint8_t, SHELLY_DIMMER_BUFFER_SIZE> buffer_;
  uint8_t buffer_pos_{0};

  // Firmware version.
  uint8_t version_major_;
  uint8_t version_minor_;

  // Configuration.
  bool leading_edge_{false};
  uint16_t warmup_brightness_{100};
  uint16_t warmup_time_{20};
  uint16_t fade_rate_{0};
  uint16_t min_brightness_{0};
  uint16_t max_brightness_{1000};

  light::LightState *state_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};

  bool ready_{false};
  uint16_t brightness_;

  /// Convert relative brightness into a dimmer brightness value.
  uint16_t convert_brightness_(float brightness);

  /// Sends the given brightness value.
  void send_brightness_(uint16_t brightness);

  /// Sends dimmer configuration.
  void send_settings_();

  /// Performs a firmware upgrade.
  bool upgrade_firmware_();

  /// Sends a command and waits for an acknowledgement.
  bool send_command_(uint8_t cmd, const uint8_t *payload, uint8_t len);

  /// Frames a given command payload.
  size_t frame_command_(uint8_t *data, uint8_t cmd, const uint8_t *payload, size_t len);

  /// Handles a single byte as part of a protocol frame.
  ///
  /// Returns -1 on failure, 0 when finished and 1 when more bytes needed.
  int handle_byte_(uint8_t c);

  /// Reads a response frame.
  bool read_frame_();

  /// Handles a complete frame.
  bool handle_frame_();

  /// Reset STM32 with the BOOT0 pin set to the given value.
  void reset_(bool boot0);

  /// Reset STM32 to boot the regular firmware.
  void reset_normal_boot_();

  /// Reset STM32 to boot into DFU mode to enable firmware upgrades.
  void reset_dfu_boot_();
};

}  // namespace shelly_dimmer
}  // namespace esphome

#endif  // USE_ESP8266
