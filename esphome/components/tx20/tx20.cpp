#include "tx20.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include <vector>

namespace esphome {
namespace tx20 {

static const char *const TAG = "tx20";
static const uint8_t MAX_BUFFER_SIZE = 41;
static const uint16_t TX20_MAX_TIME = MAX_BUFFER_SIZE * 1200 + 5000;
static const uint16_t TX20_BIT_TIME = 1200;
static const char *const DIRECTIONS[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
                                         "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};

void Tx20Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Tx20");
  this->pin_->setup();

  this->store_.buffer = new uint16_t[MAX_BUFFER_SIZE];
  this->store_.pin = this->pin_->to_isr();
  this->store_.reset();

  this->pin_->attach_interrupt(Tx20ComponentStore::gpio_intr, &this->store_, gpio::INTERRUPT_ANY_EDGE);
}
void Tx20Component::dump_config() {
  ESP_LOGCONFIG(TAG, "Tx20:");

  LOG_SENSOR("  ", "Wind speed:", this->wind_speed_sensor_);
  LOG_SENSOR("  ", "Wind direction degrees:", this->wind_direction_degrees_sensor_);

  LOG_PIN("  Pin: ", this->pin_);
}
void Tx20Component::loop() {
  if (this->store_.tx20_available) {
    this->decode_and_publish_();
    this->store_.reset();
  }
}

float Tx20Component::get_setup_priority() const { return setup_priority::DATA; }

std::string Tx20Component::get_wind_cardinal_direction() const { return this->wind_cardinal_direction_; }

void Tx20Component::decode_and_publish_() {
  ESP_LOGVV(TAG, "Decode Tx20...");

  std::string string_buffer;
  std::string string_buffer_2;
  std::vector<bool> bit_buffer;
  bool current_bit = true;

  for (int i = 1; i <= this->store_.buffer_index; i++) {
    string_buffer_2 += to_string(this->store_.buffer[i]) + ", ";
    uint8_t repeat = this->store_.buffer[i] / TX20_BIT_TIME;
    // ignore segments at the end that were too short
    string_buffer.append(repeat, current_bit ? '1' : '0');
    bit_buffer.insert(bit_buffer.end(), repeat, current_bit);
    current_bit = !current_bit;
  }
  current_bit = !current_bit;
  if (string_buffer.length() < MAX_BUFFER_SIZE) {
    uint8_t remain = MAX_BUFFER_SIZE - string_buffer.length();
    string_buffer_2 += to_string(remain) + ", ";
    string_buffer.append(remain, current_bit ? '1' : '0');
    bit_buffer.insert(bit_buffer.end(), remain, current_bit);
  }

  uint8_t tx20_sa = 0;
  uint8_t tx20_sb = 0;
  uint8_t tx20_sd = 0;
  uint8_t tx20_se = 0;
  uint16_t tx20_sc = 0;
  uint16_t tx20_sf = 0;
  uint8_t tx20_wind_direction = 0;
  float tx20_wind_speed_kmh = 0;
  uint8_t bit_count = 0;

  for (int i = 41; i > 0; i--) {
    uint8_t bit = bit_buffer.at(bit_count);
    bit_count++;
    if (i > 41 - 5) {
      // start, inverted
      tx20_sa = (tx20_sa << 1) | (bit ^ 1);
    } else if (i > 41 - 5 - 4) {
      // wind dir, inverted
      tx20_sb = tx20_sb >> 1 | ((bit ^ 1) << 3);
    } else if (i > 41 - 5 - 4 - 12) {
      // windspeed, inverted
      tx20_sc = tx20_sc >> 1 | ((bit ^ 1) << 11);
    } else if (i > 41 - 5 - 4 - 12 - 4) {
      // checksum, inverted
      tx20_sd = tx20_sd >> 1 | ((bit ^ 1) << 3);
    } else if (i > 41 - 5 - 4 - 12 - 4 - 4) {
      // wind dir
      tx20_se = tx20_se >> 1 | (bit << 3);
    } else {
      // windspeed
      tx20_sf = tx20_sf >> 1 | (bit << 11);
    }
  }

  uint8_t chk = (tx20_sb + (tx20_sc & 0xf) + ((tx20_sc >> 4) & 0xf) + ((tx20_sc >> 8) & 0xf));
  chk &= 0xf;
  bool value_set = false;
  // checks:
  // 1. Check that the start frame is 00100 (0x04)
  // 2. Check received checksum matches calculated checksum
  // 3. Check that Wind Direction matches Wind Direction (Inverted)
  // 4. Check that Wind Speed matches Wind Speed (Inverted)
  ESP_LOGVV(TAG, "BUFFER %s", string_buffer_2.c_str());
  ESP_LOGVV(TAG, "Decoded bits %s", string_buffer.c_str());

  if (tx20_sa == 4) {
    if (chk == tx20_sd) {
      if (tx20_sf == tx20_sc) {
        tx20_wind_speed_kmh = float(tx20_sc) * 0.36f;
        ESP_LOGV(TAG, "WindSpeed %f", tx20_wind_speed_kmh);
        if (this->wind_speed_sensor_ != nullptr)
          this->wind_speed_sensor_->publish_state(tx20_wind_speed_kmh);
        value_set = true;
      }
      if (tx20_se == tx20_sb) {
        tx20_wind_direction = tx20_se;
        if (tx20_wind_direction >= 0 && tx20_wind_direction < 16) {
          wind_cardinal_direction_ = DIRECTIONS[tx20_wind_direction];
        }
        ESP_LOGV(TAG, "WindDirection %d", tx20_wind_direction);
        if (this->wind_direction_degrees_sensor_ != nullptr)
          this->wind_direction_degrees_sensor_->publish_state(float(tx20_wind_direction) * 22.5f);
        value_set = true;
      }
      if (!value_set) {
        ESP_LOGW(TAG, "No value set!");
      }
    } else {
      ESP_LOGW(TAG, "Checksum wrong!");
    }
  } else {
    ESP_LOGW(TAG, "Start wrong!");
  }
}

void IRAM_ATTR Tx20ComponentStore::gpio_intr(Tx20ComponentStore *arg) {
  arg->pin_state = arg->pin.digital_read();
  const uint32_t now = micros();
  if (!arg->start_time) {
    // only detect a start if the bit is high
    if (!arg->pin_state) {
      return;
    }
    arg->buffer[arg->buffer_index] = 1;
    arg->start_time = now;
    arg->buffer_index++;
    return;
  }
  const uint32_t delay = now - arg->start_time;
  const uint8_t index = arg->buffer_index;

  // first delay has to be ~2400
  if (index == 1 && (delay > 3000 || delay < 2400)) {
    arg->reset();
    return;
  }
  // second delay has to be ~1200
  if (index == 2 && (delay > 1500 || delay < 1200)) {
    arg->reset();
    return;
  }
  // third delay has to be ~2400
  if (index == 3 && (delay > 3000 || delay < 2400)) {
    arg->reset();
    return;
  }

  if (arg->tx20_available || ((arg->spent_time + delay > TX20_MAX_TIME) && arg->start_time)) {
    arg->tx20_available = true;
    return;
  }
  if (index <= MAX_BUFFER_SIZE) {
    arg->buffer[index] = delay;
  }
  arg->spent_time += delay;
  arg->start_time = now;
  arg->buffer_index++;
}
void IRAM_ATTR Tx20ComponentStore::reset() {
  tx20_available = false;
  buffer_index = 0;
  spent_time = 0;
  // rearm it!
  start_time = 0;
}

}  // namespace tx20
}  // namespace esphome
