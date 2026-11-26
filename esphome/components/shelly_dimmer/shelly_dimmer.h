#pragma once

#ifdef USE_ESP8266

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

#include "esphome/core/frame_reader.h"

namespace esphome::shelly_dimmer {

class ShellyDimmer final : public PollingComponent, public light::LightOutput, public uart::UARTDevice {
  SUB_SENSOR(power)
  SUB_SENSOR(voltage)
  SUB_SENSOR(current)

 private:
  // Holds the largest reply, a 24 byte poll
  static constexpr uint16_t SHELLY_DIMMER_BUFFER_SIZE = 32;
  static constexpr uint8_t SHELLY_DIMMER_PROTO_START_BYTE = 0x01;
  static constexpr uint8_t SHELLY_DIMMER_PROTO_END_BYTE = 0x04;

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

 protected:
  GPIOPin *pin_nrst_;
  GPIOPin *pin_boot0_;

  uint8_t seq_{0};
  // Start byte, seq, cmd, payload length, payload, checksum (2), end byte
  using PayloadLength = PayloadLengthU8<LengthField{.offset = 3, .trailer = 3}>;
  LengthFrameReader<SHELLY_DIMMER_BUFFER_SIZE, FrameFormat<FrameHeader<SHELLY_DIMMER_PROTO_START_BYTE>, PayloadLength>>
      reader_;

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

  /// Reads a response frame.
  bool read_frame_(uint32_t now, uint32_t &last_rx);

  /// Handles a complete frame.
  bool handle_frame_(const uint8_t *frame);

  /// Reset STM32 into the bootloader (boot0) or the firmware, with the UART parity each needs.
  void reset_(bool boot0);
};

}  // namespace esphome::shelly_dimmer

#endif  // USE_ESP8266
