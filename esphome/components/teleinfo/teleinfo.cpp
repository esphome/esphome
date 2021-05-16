#include "teleinfo.h"
#include "esphome/core/log.h"

namespace esphome {
namespace teleinfo {

static const char *TAG = "teleinfo";

/* Helpers */
static int get_field(char *dest, char *buf_start, char *buf_end, int sep) {
  char *field_end;
  int len;

  field_end = static_cast<char *>(memchr(buf_start, sep, buf_end - buf_start));
  if (!field_end)
    return 0;
  len = field_end - buf_start;
  strncpy(dest, buf_start, len);
  dest[len] = '\0';

  return len;
}
/* TeleInfo methods */
bool TeleInfo::check_crc_(const char *grp, const char *grp_end) {
  int grp_len = grp_end - grp;
  uint8_t raw_crc = grp[grp_len - 1];
  uint8_t crc_tmp = 0;
  int i;

  for (i = 0; i < grp_len - checksum_area_end_; i++)
    crc_tmp += grp[i];

  crc_tmp &= 0x3F;
  crc_tmp += 0x20;
  if (raw_crc != crc_tmp) {
    ESP_LOGE(TAG, "bad crc: got %d except %d", raw_crc, crc_tmp);
    return false;
  }

  return true;
}
bool TeleInfo::read_chars_until_(bool drop, uint8_t c) {
  uint8_t received;
  int j = 0;

  while (available() > 0 && j < 128) {
    j++;
    received = read();
    if (received == c)
      return true;
    if (drop)
      continue;
    /*
     * Internal buffer is full, switch to OFF mode.
     * Data will be retrieved on next update.
     */
    if (buf_index_ >= (MAX_BUF_SIZE - 1)) {
      ESP_LOGW(TAG, "Internal buffer full");
      state_ = OFF;
      return false;
    }
    buf_[buf_index_++] = received;
  }

  return false;
}
void TeleInfo::setup() { state_ = OFF; }
void TeleInfo::update() {
  if (state_ == OFF) {
    buf_index_ = 0;
    state_ = ON;
  }
}
void TeleInfo::loop() {
  switch (state_) {
    case OFF:
      break;
    case ON:
      /* Dequeue chars until start frame (0x2) */
      if (read_chars_until_(true, 0x2))
        state_ = START_FRAME_RECEIVED;
      break;
    case START_FRAME_RECEIVED:
      /* Dequeue chars until end frame (0x3) */
      if (read_chars_until_(false, 0x3))
        state_ = END_FRAME_RECEIVED;
      break;
    case END_FRAME_RECEIVED:
      char *buf_finger;
      char *grp_end;
      char *buf_end;
      int field_len;

      buf_finger = buf_;
      buf_end = buf_ + buf_index_;

      /* Each frame is composed of multiple groups starting by 0xa(Line Feed) and ending by
       * 0xd ('\r').
       *
       * Historical mode: each group contains tag, data and a CRC separated by 0x20 (Space)
       * 0xa | Tag | 0x20 | Data | 0x20 | CRC | 0xd
       *     ^^^^^^^^^^^^^^^^^^^^
       * Checksum is computed on the above in historical mode.
       *
       * Standard mode: each group contains tag, data and a CRC separated by 0x9 (\t)
       * 0xa | Tag | 0x9 | Data | 0x9 | CRC | 0xd
       *     ^^^^^^^^^^^^^^^^^^^^^^^^^
       * Checksum is computed on the above in standard mode.
       */
      while ((buf_finger = static_cast<char *>(memchr(buf_finger, (int) 0xa, buf_index_ - 1))) &&
             ((buf_finger - buf_) < buf_index_)) {
        /* Point to the first char of the group after 0xa */
        buf_finger += 1;

        /* Group len */
        grp_end = static_cast<char *>(memchr(buf_finger, 0xd, buf_end - buf_finger));
        if (!grp_end) {
          ESP_LOGE(TAG, "No group found");
          break;
        }

        if (!check_crc_(buf_finger, grp_end))
          break;

        /* Get tag */
        field_len = get_field(tag_, buf_finger, grp_end, separator_);
        if (!field_len || field_len >= MAX_TAG_SIZE) {
          ESP_LOGE(TAG, "Invalid tag.");
          break;
        }

        /* Advance buf_finger to after the tag and the separator. */
        buf_finger += field_len + 1;

        /* Get value (after next separator) */
        field_len = get_field(val_, buf_finger, grp_end, separator_);
        if (!field_len || field_len >= MAX_VAL_SIZE) {
          ESP_LOGE(TAG, "Invalid Value");
          break;
        }

        /* Advance buf_finger to end of group */
        buf_finger += field_len + 1 + 1 + 1;

        publish_value_(std::string(tag_), std::string(val_));
      }
      state_ = OFF;
      break;
  }
}
void TeleInfo::publish_value_(std::string tag, std::string val) {
  /* It will return 0 if tag is not a float. */
  auto newval = parse_float(val);
  for (auto element : teleinfo_sensors_)
    if (tag == element->tag)
      element->sensor->publish_state(*newval);
}
void TeleInfo::dump_config() {
  ESP_LOGCONFIG(TAG, "TeleInfo:");
  for (auto element : teleinfo_sensors_)
    LOG_SENSOR("  ", element->tag, element->sensor);
  this->check_uart_settings(baud_rate_, 1, uart::UART_CONFIG_PARITY_EVEN, 7);
}
TeleInfo::TeleInfo(bool historical_mode) {
  if (historical_mode) {
    /*
     * Historical mode doesn't contain last separator between checksum and data.
     */
    checksum_area_end_ = 2;
    separator_ = 0x20;
    baud_rate_ = 1200;
  } else {
    checksum_area_end_ = 1;
    separator_ = 0x9;
    baud_rate_ = 9600;
  }
}
void TeleInfo::register_teleinfo_sensor(const char *tag, sensor::Sensor *sensor) {
  const TeleinfoSensorElement *teleinfo_sensor = new TeleinfoSensorElement{tag, sensor};
  teleinfo_sensors_.push_back(teleinfo_sensor);
}

}  // namespace teleinfo
}  // namespace esphome
