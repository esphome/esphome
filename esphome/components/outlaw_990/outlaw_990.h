/*
 * Outlaw 990 AV Preamp/Processor RS232 Serial Controller
 * Custom Component for ESPHome
 *
 * Steve Richardson (tangentaudio@gmail.com)
 * September, 2022
 *
 */

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/api/custom_api_device.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace outlaw_990 {

class Outlaw990 : public PollingComponent, public uart::UARTDevice, public api::CustomAPIDevice {
public:
  Outlaw990();

  void setup() override;
  void update() override;
  void loop() override;
  void dump_config() override;

  void set_power_binary_sensor(binary_sensor::BinarySensor *bs) { this->power_binary_sensor_ = bs; }
  void set_mute_binary_sensor(binary_sensor::BinarySensor *bs) { this->mute_binary_sensor_ = bs; }
  void set_volume_sensor(sensor::Sensor *s) { this->volume_sensor_ = s; }
  void set_display_text_sensor(text_sensor::TextSensor *ts) { this->display_text_sensor_ = ts; }
  void set_audio_in_text_sensor(text_sensor::TextSensor *ts) { this->audio_in_text_sensor_ = ts; }
  void set_video_in_text_sensor(text_sensor::TextSensor *ts) { this->video_in_text_sensor_ = ts; }

  enum RX_STATES {
    RX_STATE_INIT = 0,
    RX_STATE_HEADER_BYTE1,
    RX_STATE_HEADER_BYTE2,
    RX_STATE_LENGTH,
    RX_STATE_PAYLOAD,
    RX_STATE_PARSE
  };

  std::string disp, audio_in, video_in;
  uint8_t main_av_in;
  bool power, mute;
  int volume;

protected:
  binary_sensor::BinarySensor *power_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *mute_binary_sensor_{nullptr};
  sensor::Sensor *volume_sensor_{nullptr};
  text_sensor::TextSensor *display_text_sensor_{nullptr};
  text_sensor::TextSensor *audio_in_text_sensor_{nullptr};
  text_sensor::TextSensor *video_in_text_sensor_{nullptr};

  void send_pkt(uint8_t cmd);

  void on_cmd(int cmd);
  void on_volume_adj(bool up);
  void on_power(bool p);
  void on_mute();

  void send_idle_values();
  void parse_packet();

  int rx_state_;
  uint8_t rx_buf_[25];
  bool polling_enabled_;
  bool powering_off_;
};

} // namespace
} // namespace
