/*
 * Outlaw 990 AV Preamp/Processor RS232 Serial Controller
 * Custom Component for ESPHome
 *
 * Steve Richardson (tangentaudio@gmail.com)
 * September, 2022
 *
 * See http://outlawaudio.com/outlaw/docs/990rs232protocol.pdf for protocol details
 */

#include "esphome.h"
#include "esphome/core/log.h"
#include "outlaw_990.h"

namespace esphome {
namespace outlaw_990 {

static const char* TAG = "outlaw_990.component";

Outlaw990::Outlaw990() :
  PollingComponent(1000), rx_state_(0), polling_enabled_(true), powering_off_(false)
{
}

void Outlaw990::dump_config() {
  ESP_LOGCONFIG(TAG, "Outlaw 990");
  LOG_BINARY_SENSOR(TAG, "Power", this->power_binary_sensor_);
  LOG_BINARY_SENSOR(TAG, "Mute", this->mute_binary_sensor_);
  LOG_SENSOR(TAG, "Volume", this->volume_sensor_);
  LOG_TEXT_SENSOR(TAG, "Display", this->display_text_sensor_);
  LOG_TEXT_SENSOR(TAG, "Audio Input", this->audio_in_text_sensor_);
  LOG_TEXT_SENSOR(TAG, "Video Input", this->video_in_text_sensor_);
}

void Outlaw990::setup() {
  register_service(&Outlaw990::on_volume_adj, "outlaw_volume_adj", {"up"});
  register_service(&Outlaw990::on_power, "outlaw_power", {"power"});
  register_service(&Outlaw990::on_mute, "outlaw_mute");
  register_service(&Outlaw990::on_cmd, "outlaw_command", {"cmd"});
}

void Outlaw990::update() {
  if (polling_enabled_ || powering_off_) {
    ESP_LOGD(TAG, "update() polling %s powering_off_ %s", polling_enabled_ ? "enabled" : "disabled", powering_off_ ? "true":"false");
    send_pkt(0x53);
  }
}

void Outlaw990::send_pkt(byte cmd) {
  byte obuf[4];
  obuf[0] = 0x83;
  obuf[1] = 0x45;
  obuf[2] = cmd;
  obuf[3] = obuf[0] + obuf[1] + obuf[2];

  for (int i=0; i<4; i++) {
    write(obuf[i]);
  }

  ESP_LOGD(TAG, "sent serial pkt %2X %2X %2X %2X", obuf[0], obuf[1], obuf[2], obuf[3]);
}

void Outlaw990::on_cmd(int cmd) {
  ESP_LOGD(TAG, "cmd service: %02X", cmd);
  send_pkt(cmd);
}

void Outlaw990::on_volume_adj(bool up) {
  ESP_LOGD(TAG, "volume adjust service: %s", up ? "UP" : "DOWN");
  send_pkt(up ? 0x0F : 0x10);

  if (up && volume < 14) volume++;
  if (!up && volume > -76) volume--;

  if (this->volume_sensor_ != nullptr)
    this->volume_sensor_->publish_state(volume);

}

void Outlaw990::on_power(bool p) {
  ESP_LOGD(TAG, "power service: %s", p ? "ON" : "OFF");
  send_pkt(p ? 0x01 : 0x02);

  powering_off_ = (p == false);

  if (p != power) {
    disp = power ? "POWERING OFF" : "POWERING ON";

    if (this->display_text_sensor_ != nullptr)
      this->display_text_sensor_->publish_state(disp);
  }
}

void Outlaw990::on_mute() {
  ESP_LOGD(TAG, "mute service: TOGGLE");
  send_pkt(0x11);
}

void Outlaw990::send_idle_values() {

  power = false;
  mute = false;
  volume = -76;
  main_av_in = 0x00;
  audio_in = "";
  video_in = "";
  disp = "OFF";

  if (this->power_binary_sensor_ != nullptr)
    this->power_binary_sensor_->publish_state(power);

  if (this->mute_binary_sensor_ != nullptr)
    this->mute_binary_sensor_->publish_state(mute);

  if (this->volume_sensor_ != nullptr)
    this->volume_sensor_->publish_state(volume);

  if (this->audio_in_text_sensor_ != nullptr)
    this->audio_in_text_sensor_->publish_state(audio_in);

  if (this->video_in_text_sensor_ != nullptr)
    this->video_in_text_sensor_->publish_state(video_in);

  if (this->display_text_sensor_ != nullptr)
    this->display_text_sensor_->publish_state(disp);
}

void Outlaw990::parse_packet() {
  byte* text = &rx_buf_[0];
  byte* status = &rx_buf_[13];

  bool just_turned_on = false;

  bool p = status[0] & 0x01;

  if (power != p) {
    ESP_LOGI(TAG, "power state changed: %s->%s", power ? "ON":"OFF", p ? "ON":"OFF");
    if (p) {
      // turned on
      just_turned_on = true;
    }
    power = p;

    if (this->power_binary_sensor_ != nullptr)
      this->power_binary_sensor_->publish_state(power);
  }

  if (power) {
    ESP_LOGD(TAG, "Power is on, parsing event packet");
    bool m = status[0] & 0x08;
    if ((mute != m) || just_turned_on) {
      ESP_LOGI(TAG, "mute state changed: %s->%s", mute ? "ON":"OFF", m ? "ON":"OFF");
      mute = m;
      if (this->mute_binary_sensor_ != nullptr)
        this->mute_binary_sensor_->publish_state(mute);
    }

    int v = (int)status[3] - (int)76;
    if ((volume != v) || just_turned_on) {
      ESP_LOGI(TAG, "volume changed: %d->%d", volume, v);
      volume = v;

      if (this->volume_sensor_ != nullptr)
        this->volume_sensor_->publish_state(volume);
    }

    byte mav = status[1];
    if ((main_av_in != mav) || just_turned_on) {
      ESP_LOGI(TAG, "main AV state changed: %2X->%2X", main_av_in, mav);
      main_av_in = mav;
      byte a_in = (mav & 0x0F);
      byte v_in = (mav & 0xF0) >> 4;

      const char* AUDIO_LABELS[16] = {
        "FM", "AM", "IN_0x02", "Tuner", "CD", "Aux/USB", "Phono", "DVD",
        "Video 1", "Video 2", "Video 3", "Video 4", "Video 5",
        "Tape", "7.1CH Direct", "IN_0x0F"
      };
      const char* VIDEO_LABELS[16] = {
        "IN_0x00", "IN_0x01", "IN_0x02", "IN_0x03", "IN_0x04", "IN_0x05", "IN_0x06",
        "Video 1", "Video 2", "Video 3", "DVD", "Video 4", "Video 5", "Video Direct",
        "IN_0x0E", "IN_0x0F"
      };
      audio_in = std::string(AUDIO_LABELS[a_in]);
      video_in = std::string(VIDEO_LABELS[v_in]);

      if (this->audio_in_text_sensor_ != nullptr)
        this->audio_in_text_sensor_->publish_state(audio_in);

      if (this->video_in_text_sensor_ != nullptr)
        this->video_in_text_sensor_->publish_state(video_in);

      ESP_LOGD(TAG, " a: (%02X) %s v: (%02X) %s", a_in, audio_in.c_str(), v_in, video_in.c_str());
    }

    // extract display string
    std::string d;
    for (int i=0; i<13; i++) {
      d += text[i];
    }
    if ((disp != d) || just_turned_on) {
      disp = d;

      if (this->display_text_sensor_ != nullptr)
        this->display_text_sensor_->publish_state(disp);

      ESP_LOGD(TAG, "|%-13s|", disp.c_str());
    }
  } else {
      ESP_LOGD(TAG, "Power is off, sending off values");
      send_idle_values();

      powering_off_ = false;
  }

  if (polling_enabled_)
    polling_enabled_ = false;
}

void Outlaw990::loop() {
  static uint32_t timeout;

  // event packet
  // 0    1    2    3-15    16..26   27
  // SYS_ID   CNT   TEXT    STATUS   CKSUM
  //                CCCCCCCCCCCCCC-----^

  // example
  // 83:45:18:20:37:2E:31:43:48:20:44:49:52:45:43:54:81:DE:88:3D:32:59:00:00:98:6A:5A:27

  switch (rx_state_) {
    case RX_STATE_INIT:
      flush();
      polling_enabled_ = true;
      send_idle_values();
      rx_state_ = RX_STATE_HEADER_BYTE1;
      break;

    case RX_STATE_HEADER_BYTE1:
      if (available()) {
        if (read() == 0x83) {
          rx_state_ = RX_STATE_HEADER_BYTE2;
        }
      }
      break;

    case RX_STATE_HEADER_BYTE2:
      if (available()) {
        if (read() == 0x45) {
          rx_state_ = RX_STATE_LENGTH;
        } else {
          rx_state_ = RX_STATE_INIT;
        }
      }
      break;

    case RX_STATE_LENGTH:
      if (available()) {
        if (read() == 0x18) {
          rx_state_ = RX_STATE_PAYLOAD;
          timeout = millis();
        } else {
          rx_state_ = RX_STATE_INIT;
        }
      }
      break;

    case RX_STATE_PAYLOAD:
      if (available() >= 25) {
        read_array(rx_buf_, 25);

        // calculate checksum over payload
        byte checksum = 0;
        for (int i=0; i<24; i++) {
          checksum += rx_buf_[i];
        }

        if (checksum == rx_buf_[24]) {
          rx_state_ = RX_STATE_PARSE;
        } else {
          ESP_LOGD(TAG, "Checksum error, calc=%2.2X rx=%2.2X", checksum, rx_buf_[24]);
          rx_state_ = RX_STATE_INIT;
        }
      } else if (millis() - timeout > 100) {
        ESP_LOGE(TAG, "Timed out waiting for payload");
        rx_state_ = RX_STATE_INIT;
      }
      break;

    case RX_STATE_PARSE:
      parse_packet();
      rx_state_ = RX_STATE_HEADER_BYTE1;
      break;

    default:
      rx_state_ = RX_STATE_INIT;
      break;
  }
}

} // namespace
} // namespace
