#include "mcp4728.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp4728 {

static const char *const TAG = "mcp4728";

void MCP4728Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MCP4728 (0x%02X)...", this->address_);
  auto err = this->write(nullptr, 0);
  if (err != i2c::ERROR_OK) {
    this->mark_failed();
    return;
  }
}

void MCP4728Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP4728:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with MCP4728 failed!");
  }
}

void MCP4728Component::loop() {
  if (this->update_) {
    this->update_ = false;
    if (this->store_in_eeprom_) {
      if (!this->seq_write_()) {
        this->status_set_error();
      } else {
        this->status_clear_error();
      }
    } else {
      if (!this->multi_write_()) {
        this->status_set_error();
      } else {
        this->status_clear_error();
      }
    }
  }
}

void MCP4728Component::set_channel_value_(MCP4728ChannelIdx channel, uint16_t value) {
  uint8_t cn = 0;
  if (channel == MCP4728_CHANNEL_A) {
    cn = 'A';
  } else if (channel == MCP4728_CHANNEL_B) {
    cn = 'B';
  } else if (channel == MCP4728_CHANNEL_C) {
    cn = 'C';
  } else {
    cn = 'D';
  }
  ESP_LOGV(TAG, "Setting MCP4728 channel %c to %d!", cn, value);
  reg_[channel].data = value;
  this->update_ = true;
}

bool MCP4728Component::multi_write_() {
  i2c::ErrorCode err[4];
  for (uint8_t i = 0; i < 4; ++i) {
    uint8_t wd[3];
    wd[0] = ((uint8_t) CMD::MULTI_WRITE | (i << 1)) & 0xFE;
    wd[1] = ((uint8_t) reg_[i].vref << 7) | ((uint8_t) reg_[i].pd << 5) | ((uint8_t) reg_[i].gain << 4) |
            (reg_[i].data >> 8);
    wd[2] = reg_[i].data & 0xFF;
    err[i] = this->write(wd, sizeof(wd));
  }
  bool ok = true;
  for (auto &e : err) {
    if (e != i2c::ERROR_OK) {
      ok = false;
      break;
    }
  }
  return ok;
}

bool MCP4728Component::seq_write_() {
  uint8_t wd[9];
  wd[0] = (uint8_t) CMD::SEQ_WRITE;
  for (uint8_t i = 0; i < 4; i++) {
    wd[i * 2 + 1] = ((uint8_t) reg_[i].vref << 7) | ((uint8_t) reg_[i].pd << 5) | ((uint8_t) reg_[i].gain << 4) |
                    (reg_[i].data >> 8);
    wd[i * 2 + 2] = reg_[i].data & 0xFF;
  }
  auto err = this->write(wd, sizeof(wd));
  return err == i2c::ERROR_OK;
}

void MCP4728Component::select_vref_(MCP4728ChannelIdx channel, MCP4728Vref vref) {
  reg_[channel].vref = vref;

  this->update_ = true;
}

void MCP4728Component::select_power_down_(MCP4728ChannelIdx channel, MCP4728PwrDown pd) {
  reg_[channel].pd = pd;

  this->update_ = true;
}

void MCP4728Component::select_gain_(MCP4728ChannelIdx channel, MCP4728Gain gain) {
  reg_[channel].gain = gain;

  this->update_ = true;
}

}  // namespace mcp4728
}  // namespace esphome
