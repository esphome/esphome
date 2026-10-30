#include "epaper_spi_ssd2677.h"

#include <cmath>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

ESPHOME_LOG_TAG(TAG, "epaper_spi.ssd2677");

static constexpr uint8_t CMD_FORCE_TEMPERATURE = 0xE6;
static constexpr uint8_t CMD_LOAD_WAVEFORM = 0xA5;
static constexpr uint8_t CCSET_FORCE_TEMPERATURE = 0x12;
static constexpr uint32_t RESET_SETTLE_MS = 10;
static constexpr uint32_t WAVEFORM_SETTLE_MS = 10;

static constexpr int DEFAULT_TEMPERATURE = 25;

// Moves the 8 bits of value to the even bit positions of the result
static constexpr uint16_t spread_bits(uint8_t value) {
  uint16_t bits = value;
  bits = (bits | (bits << 4)) & 0x0F0F;
  bits = (bits | (bits << 2)) & 0x3333;
  return (bits | (bits << 1)) & 0x5555;
}

// Waveform steps from Seeed's sample code for this panel
static constexpr uint8_t partial_waveform(int temperature) {
  if (temperature <= 10)
    return 0x08;
  if (temperature <= 20)
    return 0x0F;
  if (temperature <= 30)
    return 0x19;
  return 0x23;
}

void EPaperSSD2677::dump_config() {
  EPaperBase::dump_config();
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Temperature Sensor", this->temperature_sensor_);
#endif
}

int EPaperSSD2677::whole_degrees_(float reading) {
  return std::isnan(reading) ? DEFAULT_TEMPERATURE : (int) std::lround(reading);
}

float EPaperSSD2677::temperature_reading_() const {
#ifdef USE_SENSOR
  if (this->temperature_sensor_ != nullptr)
    return this->temperature_sensor_->state;
#endif
  return NAN;
}

uint8_t EPaperSSD2677::full_waveform(int temperature) const {
  if (temperature <= 10)
    return 0xE8;
  if (temperature <= 20)
    return 0xEB;
  if (temperature <= 30)
    return 0xEE;
  return 0xF1;
}

bool EPaperSSD2677::reset() {
  if (!this->hardware_reset_())
    return false;
  this->next_delay_ = RESET_SETTLE_MS;
  return true;
}

bool EPaperSSD2677::initialise(bool partial) {
  if (!this->init_sequence_sent_) {
    EPaperUltraChip::initialise(partial);
    this->init_sequence_sent_ = true;
    this->wait_for_idle_(true);
    return false;
  }
  this->init_sequence_sent_ = false;
  this->cmd_data(CMD_CCSET, {CCSET_FORCE_TEMPERATURE});
  const int temperature = whole_degrees_(this->temperature_reading_());
  const uint8_t waveform = this->is_partial_() ? partial_waveform(temperature) : this->full_waveform(temperature);
  ESP_LOGD(TAG, "Temperature %d C, waveform 0x%02X", temperature, waveform);
  this->cmd_data(CMD_FORCE_TEMPERATURE, {waveform});
  this->command(CMD_LOAD_WAVEFORM);
  this->next_delay_ = WAVEFORM_SETTLE_MS;
  return true;
}

void HOT EPaperSSD2677::stream_row(size_t y, const uint8_t *plane, uint8_t *out) {
  const size_t row_length = this->plane_row_length_();
  const size_t data_idx = y * row_length;
  const bool partial = this->is_partial_();
  for (size_t i = 0; i != row_length; i++) {
    const uint8_t previous = partial ? this->sent_[data_idx + i] : plane[i];
    const uint16_t pairs = (spread_bits(previous) << 1) | spread_bits(plane[i]);
    out[2 * i] = pairs >> 8;
    out[2 * i + 1] = pairs & 0xFF;
  }
}

bool HOT EPaperSSD2677::transfer_data() {
  const auto start_time = millis();
  if (this->current_data_index_ == 0)
    this->command(CMD_DTM1);
  const size_t row_length = this->plane_row_length_();
  SmallBufferWithHeapFallback<128> plane_alloc(row_length);
  SmallBufferWithHeapFallback<256> stream_alloc(row_length * 2);
  this->start_data_();
  // current_data_index_ counts rows
  while (this->current_data_index_ != this->height_) {
    const size_t y = this->current_data_index_++;
    this->plane_row(y, plane_alloc.get());
    this->stream_row(y, plane_alloc.get(), stream_alloc.get());
    this->write_array(stream_alloc.get(), row_length * 2);
    this->record_sent_(y * row_length, plane_alloc.get(), row_length);
    if (this->current_data_index_ != this->height_ && millis() - start_time > MAX_TRANSFER_TIME) {
      // Let the main loop run and come back next loop
      this->disable();
      return false;
    }
  }
  this->disable();
  this->current_data_index_ = 0;
  return true;
}

void EPaperSSD2677::power_on() { this->command(CMD_POWER_ON); }

void EPaperSSD2677::refresh_screen(bool partial) { this->cmd_data(CMD_REFRESH, {0x00}); }

void EPaperSSD2677::power_off() { this->cmd_data(CMD_POWER_OFF, {0x00}); }

void HOT EPaperSSD2677Gray4::stream_row(size_t y, const uint8_t *plane, uint8_t *out) {
  if (this->is_partial_()) {
    EPaperSSD2677::stream_row(y, plane, out);
    return;
  }
  const size_t src_row = y * this->row_width_;
  for (size_t i = 0; i != this->row_width_; i++)
    out[i] = this->buffer_[src_row + i];
}

uint8_t EPaperSSD2677Gray4::full_waveform(int temperature) const { return temperature <= 15 ? 0xF1 : 0xFA; }

}  // namespace esphome::epaper_spi
