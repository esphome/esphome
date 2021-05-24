#include "teleinfo.h"
#include "esphome/core/log.h"
#include <time.h>

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
// Convert a Linky timestamp to a Unix epoche time
// Linky timestamp example: "E210519075608"
// Format: SYYMMDDhhmmss
// S = season is 'E' (été = summer) or 'H' (hiver = winter), upcase = Linky is synced
static long linkyTimestampToEpoche(std::string linky_timestamp) {
  struct tm tm;
  time_t t;
  
  // create a parsable representation of the timestamp (separators between parts)
  std::string parsable_timestamp = linky_timestamp.substr(1);
  for(int i=10; i>0; i-=2){
    parsable_timestamp.insert(i, "-");
  }
  ESP_LOGD(TAG, "--> parsable_timestamp: %s", &(parsable_timestamp[0]));

  // read season, lowercase means unsynced time, but we ignore this
  char season = toupper(linky_timestamp[0]);
  ESP_LOGD(TAG, "--> season: %c", season);

  // parse time into tm struct
  if (strptime(&(parsable_timestamp[0]), "%y-%m-%d-%H-%M-%S", &tm) == NULL) {
    ESP_LOGE(TAG, "Invalid Linky timestamp: %s", &(linky_timestamp[0]));
    return 0;
  }
  ESP_LOGD(TAG, "--> parsed timestamp - year: %d; month: %d; day: %d; hour: %d; minute: %d; second: %d", tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

  // convert tm struct to unix epoche
  // and correct timezone and daylight saving to get UTC
  // "Time" integration MUST be added in ESPHome node config, otherwise _timezone is 0
  t = mktime(&tm) - _timezone - (season=='E' ? 3600 : 0);
  if (t < 0){
    ESP_LOGE(TAG, "Could not convert Linky timestamp: %s", &(linky_timestamp[0]));
    return 0;
  }
  ESP_LOGD(TAG, "--> _timezone: %d", _timezone);
  ESP_LOGD(TAG, "--> epoche: %d", t);

  return (long) t;
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
void TeleInfo::setup() { 
  state_ = OFF;
}
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
       * HISTORICAL mode: each group contains tag, data and a CRC separated by 0x20 (Space)
       * 0xa | Tag | 0x20 | Data | 0x20 | CRC | 0xd
       *     ^^^^^^^^^^^^^^^^^^^^
       *
       * STANDARD mode: each group contains tag, timestamp (optional), data and a CRC separated by 0x9 (\t)
       * 0xa | Tag | 0x9 | Data | 0x9 | CRC | 0xd
       *     ^^^^^^^^^^^^^^^^^^^^^^^^^
       * 0xa | Tag | 0x9 | Timestamp | 0x9 | Data | 0x9 | CRC | 0xd
       *     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
       *
       * Checksums (CRC) are computed over the underlined part in the above groups.
       */
      while ((buf_finger = static_cast<char *>(memchr(buf_finger, (int) 0xa, buf_index_ - 1))) &&
             ((buf_finger - buf_) < buf_index_)) {
        // Point to the first char of the group after 0xa
        buf_finger += 1;

        // Group len
        grp_end = static_cast<char *>(memchr(buf_finger, 0xd, buf_end - buf_finger));
        if (!grp_end) {
          ESP_LOGE(TAG, "No group found");
          break;
        }

        if (!check_crc_(buf_finger, grp_end))
          break;

        // Get tag
        field_len = get_field(tag_, buf_finger, grp_end, separator_);
        if (!field_len || field_len >= MAX_TAG_SIZE) {
          ESP_LOGE(TAG, "Invalid tag field");
          break;
        }
        ESP_LOGD(TAG, "--> tag field: %s", tag_);
        // Advance buf_finger to after the tag and the separator.
        buf_finger += field_len + 1;

        // count the number of data fields
        // (historical mode: always 1, standard mode: 1 or 2)
        int data_field_cnt = std::count(buf_finger, grp_end, separator_);
        ESP_LOGD(TAG, "--> data field count: %d", data_field_cnt);

        // if two data fields found: first field is a Linky timestamp
        timestamp_[0] = '\0';
        if(data_field_cnt>1) {
          // Get timestamp (after next separator) */
          field_len = get_field(timestamp_, buf_finger, grp_end, separator_);
          if (!field_len || field_len >= MAX_TIMESTAMP_SIZE) {
            ESP_LOGE(TAG, "Invalid timestamp field");
            break;
          }
          ESP_LOGD(TAG, "--> timestamp field: %s", timestamp_);
          // Advance buf_finger to end of field
          buf_finger += field_len + 1;
        }

        // next field is always the value field
        // Get value (after next separator)
        field_len = get_field(val_, buf_finger, grp_end, separator_);
        if (field_len >= MAX_VAL_SIZE) {
          ESP_LOGE(TAG, "Invalid value field");
          break;
        }
        ESP_LOGD(TAG, "--> value field: %s", val_);
        // Advance buf_finger to end of group
        buf_finger += field_len + 1 + 1 + 1;

        publish_value_(std::string(tag_), std::string(timestamp_), std::string(val_));
      }
      state_ = OFF;
      break;
  }
}
void TeleInfo::publish_value_(std::string tag, std::string timestamp, std::string val) {
  /* It will return 0 if tag is not a float. */
  for (auto element : teleinfo_sensors_)
    if (tag == element->tag){
      std::string device_class = element->sensor->get_device_class();
      if( device_class.compare("timestamp") == 0 && timestamp[0]!='\0'){
        // for a sensor with device_class "timestamp" we use the Linky timestamp field (if we got one)
        float newval = (float)linkyTimestampToEpoche(timestamp);
        ESP_LOGD(TAG, "--> device class \"timestamp\", epoche received: %f", newval);
        element->sensor->publish_state(newval);
      } else {
        // for a sensor with any other device_class (or when Linky timestamp field is missing) we use the value field
        auto newval = parse_float(val);
        element->sensor->publish_state(*newval);
      }
    }
}
void TeleInfo::dump_config() {
  ESP_LOGCONFIG(TAG, "TeleInfo:");
  for (auto element : teleinfo_sensors_) {
    LOG_SENSOR("  ", element->tag, element->sensor);
  }
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
