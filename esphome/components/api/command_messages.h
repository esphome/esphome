#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "api_message.h"

namespace esphome {
namespace api {

#ifdef USE_COVER
enum LegacyCoverCommand {
  LEGACY_COVER_COMMAND_OPEN = 0,
  LEGACY_COVER_COMMAND_CLOSE = 1,
  LEGACY_COVER_COMMAND_STOP = 2,
};

class CoverCommandRequest : public APIMessage {
 public:
  bool decode_varint(uint32_t field_id, uint32_t value) override;
  bool decode_32bit(uint32_t field_id, uint32_t value) override;
  APIMessageType message_type() const override;
  uint32_t get_key() const;
  optional<LegacyCoverCommand> get_legacy_command() const;
  optional<float> get_position() const;
  optional<float> get_tilt() const;
  bool get_stop() const { return this->stop_; }

 protected:
  uint32_t key_{0};
  bool has_legacy_command_{false};
  LegacyCoverCommand legacy_command_{LEGACY_COVER_COMMAND_OPEN};
  bool has_position_{false};
  float position_{0.0f};
  bool has_tilt_{false};
  float tilt_{0.0f};
  bool stop_{false};
};
#endif

#ifdef USE_FAN
class FanCommandRequest : public APIMessage {
 public:
  bool decode_varint(uint32_t field_id, uint32_t value) override;
  bool decode_32bit(uint32_t field_id, uint32_t value) override;
  APIMessageType message_type() const override;
  uint32_t get_key() const;
  optional<bool> get_state() const;
  optional<fan::FanSpeed> get_speed() const;
  optional<bool> get_oscillating() const;

 protected:
  uint32_t key_{0};
  bool has_state_{false};
  bool state_{false};
  bool has_speed_{false};
  fan::FanSpeed speed_{fan::FAN_SPEED_LOW};
  bool has_oscillating_{false};
  bool oscillating_{false};
};
#endif

#ifdef USE_LIGHT
class LightCommandRequest : public APIMessage {
 public:
  bool decode_varint(uint32_t field_id, uint32_t value) override;
  bool decode_length_delimited(uint32_t field_id, const uint8_t *value, size_t len) override;
  bool decode_32bit(uint32_t field_id, uint32_t value) override;
  APIMessageType message_type() const override;
  uint32_t get_key() const;
  optional<bool> get_state() const;
  optional<float> get_brightness() const;
  optional<float> get_red() const;
  optional<float> get_green() const;
  optional<float> get_blue() const;
  optional<float> get_white() const;
  optional<float> get_color_temperature() const;
  optional<uint32_t> get_transition_length() const;
  optional<uint32_t> get_flash_length() const;
  optional<std::string> get_effect() const;

 protected:
  uint32_t key_{0};
  bool has_state_{false};
  bool state_{false};
  bool has_brightness_{false};
  float brightness_{0.0f};
  bool has_rgb_{false};
  float red_{0.0f};
  float green_{0.0f};
  float blue_{0.0f};
  bool has_white_{false};
  float white_{0.0f};
  bool has_color_temperature_{false};
  float color_temperature_{0.0f};
  bool has_transition_length_{false};
  uint32_t transition_length_{0};
  bool has_flash_length_{false};
  uint32_t flash_length_{0};
  bool has_effect_{false};
  std::string effect_{};
};
#endif

#ifdef USE_SWITCH
class SwitchCommandRequest : public APIMessage {
 public:
  bool decode_varint(uint32_t field_id, uint32_t value) override;
  bool decode_32bit(uint32_t field_id, uint32_t value) override;
  APIMessageType message_type() const override;
  uint32_t get_key() const;
  bool get_state() const;

 protected:
  uint32_t key_{0};
  bool state_{false};
};
#endif

#ifdef USE_ESP32_CAMERA
class CameraImageRequest : public APIMessage {
 public:
  bool decode_varint(uint32_t field_id, uint32_t value) override;
  bool get_single() const;
  bool get_stream() const;
  APIMessageType message_type() const override;

 protected:
  bool single_{false};
  bool stream_{false};
};
#endif

#ifdef USE_CLIMATE
class ClimateCommandRequest : public APIMessage {
 public:
  bool decode_varint(uint32_t field_id, uint32_t value) override;
  bool decode_32bit(uint32_t field_id, uint32_t value) override;
  APIMessageType message_type() const override;
  uint32_t get_key() const;
  optional<climate::ClimateMode> get_mode() const;
  optional<float> get_target_temperature() const;
  optional<float> get_target_temperature_low() const;
  optional<float> get_target_temperature_high() const;
  optional<bool> get_away() const;

 protected:
  uint32_t key_{0};
  bool has_mode_{false};
  climate::ClimateMode mode_{climate::CLIMATE_MODE_OFF};
  bool has_target_temperature_{false};
  float target_temperature_{0.0f};
  bool has_target_temperature_low_{false};
  float target_temperature_low_{0.0f};
  bool has_target_temperature_high_{false};
  float target_temperature_high_{0.0f};
  bool has_away_{false};
  bool away_{false};
};
#endif

}  // namespace api
}  // namespace esphome
