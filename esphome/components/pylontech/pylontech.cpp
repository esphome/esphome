#include "pylontech.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pylontech {

static const char *const TAG = "pylontech";
static const int MAX_DATA_LENGTH_BYTES = 256;
static const uint8_t ASCII_LF = 0x0A;

PylontechComponent::PylontechComponent() {}

void PylontechComponent::dump_config() {
  this->check_uart_settings(115200, 1, esphome::uart::UART_CONFIG_PARITY_NONE, 8);
  ESP_LOGCONFIG(TAG, "pylontech:");
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Connection with pylontech failed!");
  }
  ESP_LOGCONFIG(TAG, " expecting %d batteries", this->max_battery_index_);

  for (int bat_num = 0; bat_num < this->max_battery_index_; bat_num++) {
    ESP_LOGCONFIG(TAG, " Battery %d", bat_num + 1);
    if (this->voltage_[bat_num]) {
      LOG_SENSOR("  ", "Voltage", this->voltage_[bat_num]);
    }
    if (this->current_[bat_num]) {
      LOG_SENSOR("  ", "Current", this->current_[bat_num]);
    }
    if (this->temperature_[bat_num]) {
      LOG_SENSOR("  ", "Temperature", this->temperature_[bat_num]);
    }
    if (this->temperature_low_[bat_num]) {
      LOG_SENSOR("  ", "Temperature low", this->temperature_low_[bat_num]);
    }
    if (this->temperature_high_[bat_num]) {
      LOG_SENSOR("  ", "Temperature high", this->temperature_high_[bat_num]);
    }
    if (this->voltage_low_[bat_num]) {
      LOG_SENSOR("  ", "Voltage low", this->voltage_low_[bat_num]);
    }
    if (this->voltage_high_[bat_num]) {
      LOG_SENSOR("  ", "Voltage high", this->voltage_high_[bat_num]);
    }
    if (this->coulomb_[bat_num]) {
      LOG_SENSOR("  ", "Coulomb", this->coulomb_[bat_num]);
    }
    if (this->mos_temperature_[bat_num]) {
      LOG_SENSOR("  ", "MOS Temperature", this->mos_temperature_[bat_num]);
    }
#ifdef USE_TEXT_SENSOR
    if (this->base_state_[bat_num]) {
      LOG_TEXT_SENSOR("  ", "Base state", this->base_state_[bat_num]);
    }
    if (this->voltage_state_[bat_num]) {
      LOG_TEXT_SENSOR("  ", "Voltage state", this->voltage_state_[bat_num]);
    }
    if (this->current_state_[bat_num]) {
      LOG_TEXT_SENSOR("  ", "Current state", this->current_state_[bat_num]);
    }
    if (this->temperature_state_[bat_num]) {
      LOG_TEXT_SENSOR("  ", "Temperature state", this->temperature_state_[bat_num]);
    }
#endif
  }

  LOG_UPDATE_INTERVAL(this);
}

void PylontechComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up pylontech...");
  while (this->available() != 0) {
    this->read();
  }
}

void PylontechComponent::update() { this->write_str("pwr\n"); }

void PylontechComponent::loop() {
  uint8_t data;

  // pylontech sends a lot of data very suddenly
  // we need to quickly put it all into our own buffer, otherwise the uart's buffer will overflow
  while (this->available() > 0) {
    if (this->read_byte(&data)) {
      buffer_[buffer_index_write_] += (char) data;
      if (buffer_[buffer_index_write_].back() == static_cast<char>(ASCII_LF) ||
          buffer_[buffer_index_write_].length() >= MAX_DATA_LENGTH_BYTES) {
        // complete line received
        buffer_index_write_ = (buffer_index_write_ + 1) % NUM_BUFFERS;
      }
    }
  }

  // only process one line per call of loop() to not block esphome for too long
  if (buffer_index_read_ != buffer_index_write_) {
    this->process_line_(buffer_[buffer_index_read_]);
    buffer_[buffer_index_read_].clear();
    buffer_index_read_ = (buffer_index_read_ + 1) % NUM_BUFFERS;
  }
}

void PylontechComponent::process_line_(std::string &buffer) {
  ESP_LOGV(TAG, "Read from serial: %s", buffer.substr(0, buffer.size() - 2).c_str());
  // clang-format off
  // example line to parse:
  // Power Volt  Curr Tempr Tlow  Thigh  Vlow Vhigh Base.St Volt.St Curr.St Temp.St Coulomb Time                B.V.St B.T.St MosTempr M.T.St
  // 1    50548  8910 25000 24200 25000  3368 3371  Charge  Normal  Normal  Normal  97%     2021-06-30 20:49:45 Normal Normal 22700    Normal
  // clang-format on

  int bat_num = 0, volt, curr, tempr, tlow, thigh, vlow, vhigh, coulomb, mostempr;
  char base_st[TEXT_SENSOR_MAX_LEN], volt_st[TEXT_SENSOR_MAX_LEN], curr_st[TEXT_SENSOR_MAX_LEN],
      temp_st[TEXT_SENSOR_MAX_LEN];
  const int parsed = sscanf(                                                                                  // NOLINT
      buffer.c_str(), "%d %d %d %d %d %d %d %d %7s %7s %7s %7s %d%% %*d-%*d-%*d %*d:%*d:%*d %*s %*s %d %*s",  // NOLINT
      &bat_num, &volt, &curr, &tempr, &tlow, &thigh, &vlow, &vhigh, base_st, volt_st,                         // NOLINT
      curr_st, temp_st, &coulomb, &mostempr);                                                                 // NOLINT

  if (bat_num <= 0 || bat_num > this->max_battery_index_) {
    ESP_LOGD(TAG, "invalid bat_num in line %s", buffer.substr(0, buffer.size() - 2).c_str());
    return;
  }
  if (parsed != 14) {
    ESP_LOGW(TAG, "invalid line: found only %d items in %s", parsed, buffer.substr(0, buffer.size() - 2).c_str());
    return;
  }
  bat_num--;

  if (this->voltage_[bat_num]) {
    this->voltage_[bat_num]->publish_state(((float) volt) / 1000);
  }
  if (this->current_[bat_num]) {
    this->current_[bat_num]->publish_state(((float) curr) / 1000);
  }
  if (this->temperature_[bat_num]) {
    this->temperature_[bat_num]->publish_state(((float) tempr) / 1000);
  }
  if (this->temperature_low_[bat_num]) {
    this->temperature_low_[bat_num]->publish_state(((float) tlow) / 1000);
  }
  if (this->temperature_high_[bat_num]) {
    this->temperature_high_[bat_num]->publish_state(((float) thigh) / 1000);
  }
  if (this->voltage_low_[bat_num]) {
    this->voltage_low_[bat_num]->publish_state(((float) vlow) / 1000);
  }
  if (this->voltage_high_[bat_num]) {
    this->voltage_high_[bat_num]->publish_state(((float) vhigh) / 1000);
  }
  if (this->coulomb_[bat_num]) {
    this->coulomb_[bat_num]->publish_state(coulomb);
  }
  if (this->mos_temperature_[bat_num]) {
    this->mos_temperature_[bat_num]->publish_state(((float) mostempr) / 1000);
  }
#ifdef USE_TEXT_SENSOR
  if (this->base_state_[bat_num]) {
    this->base_state_[bat_num]->publish_state(std::string(base_st));
  }
  if (this->voltage_state_[bat_num]) {
    this->voltage_state_[bat_num]->publish_state(std::string(volt_st));
  }
  if (this->current_state_[bat_num]) {
    this->voltage_state_[bat_num]->publish_state(std::string(curr_st));
  }
  if (this->temperature_state_[bat_num]) {
    this->temperature_state_[bat_num]->publish_state(std::string(temp_st));
  }
#endif
}

float PylontechComponent::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace pylontech
}  // namespace esphome
