#include "epaper_spi_uc8279.h"

#include <algorithm>
#include <cstring>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {
ESPHOME_LOG_TAG(TAG, "epaper_spi.uc8279");

static constexpr uint8_t CMD_POWER_OFF = 0x02;
static constexpr uint8_t CMD_PFS = 0x03;
static constexpr uint8_t CMD_POWER_ON = 0x04;
static constexpr uint8_t CMD_DEEP_SLEEP = 0x07;
static constexpr uint8_t CMD_DTM1 = 0x10;
static constexpr uint8_t CMD_REFRESH = 0x12;
static constexpr uint8_t CMD_DTM2 = 0x13;
static constexpr uint8_t CMD_PSR = 0x00;
static constexpr uint8_t CMD_CDI = 0x50;
static constexpr uint8_t CMD_PARTIAL_WINDOW = 0x90;
static constexpr uint8_t CMD_PARTIAL_IN = 0x91;
static constexpr uint8_t CMD_PARTIAL_OUT = 0x92;
static constexpr uint8_t CMD_CCSET = 0xE0;
static constexpr uint8_t CMD_GATE_SCAN = 0xE1;
static constexpr uint8_t CMD_TSSET = 0xE5;

static constexpr uint8_t PSR0_OTP_LUT = 0x17;
static constexpr uint8_t PSR1 = 0x4D;
static constexpr uint8_t PFS = 0x20;
static constexpr uint8_t GATE_SCAN = 0x02;
static constexpr uint8_t CCSET = 0x02;
static constexpr uint8_t TSSET_FULL = 0x1E;
static constexpr uint8_t TSSET_PARTIAL = 0x5A;
static constexpr uint8_t CDI_FULL = 0x97;
static constexpr uint8_t CDI_PARTIAL = 0xD7;
static constexpr uint8_t DEEP_SLEEP_CHECK = 0xA5;

static constexpr uint16_t RESET_SETTLE_MS = 50;
static constexpr uint16_t BUSY_ASSERT_MS = 10;
static constexpr uint16_t REFRESH_START_MS = 50;

static constexpr uint8_t hi(uint16_t v) { return static_cast<uint8_t>(v >> 8); }
static constexpr uint8_t lo(uint16_t v) { return static_cast<uint8_t>(v & 0xFF); }

void EPaperUC8279::loop() {
  // Rewriting DTM1 holds the state machine back until it is done
  if (this->writing_previous_image_) {
    if (!this->write_previous_image_())
      return;
    this->writing_previous_image_ = false;
    this->command(CMD_POWER_OFF);
    this->set_state_(EPaperState::DEEP_SLEEP, BUSY_ASSERT_MS);
    return;
  }
  EPaperBase::loop();
}

bool EPaperUC8279::reset() {
  // A partial update follows one that left the controller configured, in standby, with DTM1 holding the image
  this->partial_ = this->update_count_ != 0;
  if (this->partial_)
    return true;
  if (!EPaperBase::reset())
    return false;
  this->next_delay_ = RESET_SETTLE_MS;
  return true;
}

bool EPaperUC8279::initialise(bool partial) {
  if (this->partial_)
    return true;
  return EPaperBase::initialise(partial);
}

void EPaperUC8279::write_gate_(size_t gate, bool white) {
  const size_t row_width = this->row_width_;
  if (white || gate < this->gate_offset_) {
    uint8_t white_bytes[MAX_TRANSFER_SIZE];
    memset(white_bytes, 0xFF, sizeof(white_bytes));
    for (size_t written = 0; written != row_width;) {
      const size_t length = std::min(row_width - written, MAX_TRANSFER_SIZE);
      this->write_array(white_bytes, length);
      written += length;
    }
    return;
  }
  size_t index = (gate - this->gate_offset_) * row_width;
  for (size_t remaining = row_width; remaining != 0;) {
    size_t length;
    const uint8_t *data = this->buffer_.get_span(index, length);
    length = std::min(length, remaining);
    this->write_array(data, length);
    index += length;
    remaining -= length;
  }
}

// DTM2 gets the new image; a full refresh also gets a white DTM1, a partial one compares against DTM1 as it is
bool HOT EPaperUC8279::transfer_data() {
  const uint32_t start_time = millis();
  const size_t gates = this->gate_count_();
  const size_t total = (this->partial_ ? 1 : 2) * gates;
  while (this->current_data_index_ != total) {
    const size_t gate = this->current_data_index_ % gates;
    const bool new_image = this->current_data_index_ < gates;
    if (gate == 0)
      this->command(new_image ? CMD_DTM2 : CMD_DTM1);
    this->start_data_();
    this->write_gate_(gate, !new_image);
    this->disable();
    ++this->current_data_index_;
    if (this->current_data_index_ != total && millis() - start_time > MAX_TRANSFER_TIME)
      return false;
  }
  this->current_data_index_ = 0;
  return true;
}

bool HOT EPaperUC8279::write_previous_image_() {
  const uint32_t start_time = millis();
  const size_t gates = this->gate_count_();
  if (this->previous_image_gate_ == 0)
    this->command(CMD_DTM1);
  this->start_data_();
  while (this->previous_image_gate_ != gates) {
    this->write_gate_(this->previous_image_gate_++, false);
    if (this->previous_image_gate_ != gates && millis() - start_time > MAX_TRANSFER_TIME) {
      this->disable();
      return false;
    }
  }
  this->disable();
  return true;
}

void EPaperUC8279::power_on() {
  ESP_LOGV(TAG, "Power on");
  this->cmd_data(CMD_CDI, {this->partial_ ? CDI_PARTIAL : CDI_FULL});
  this->cmd_data(CMD_CCSET, {CCSET});
  this->cmd_data(CMD_TSSET, {this->partial_ ? TSSET_PARTIAL : TSSET_FULL});
  if (this->partial_) {
    this->cmd_data(CMD_PFS, {PFS});
    this->cmd_data(CMD_GATE_SCAN, {GATE_SCAN});
  }
  this->command(CMD_POWER_ON);
  this->next_delay_ = BUSY_ASSERT_MS;
}

void EPaperUC8279::refresh_screen(bool /*partial*/) {
  ESP_LOGV(TAG, "Refresh screen (%s)", this->partial_ ? LOG_STR_LITERAL("partial") : LOG_STR_LITERAL("full"));
  if (this->partial_) {
    const uint16_t x_end = this->width_ - 1;
    const uint16_t y_start = this->gate_offset_;
    const uint16_t y_end = this->gate_count_() - 1;
    this->command(CMD_PARTIAL_IN);
    this->cmd_data(CMD_PARTIAL_WINDOW, {0x00, 0x00, hi(x_end), static_cast<uint8_t>(lo(x_end) | 0x07), hi(y_start),
                                        lo(y_start), hi(y_end), lo(y_end), 0x01});
  }
  // Power-on reloads the panel's stored settings, so the panel setting only takes when written after it
  this->cmd_data(CMD_PSR, {PSR0_OTP_LUT, PSR1});
  this->command(CMD_REFRESH);
  this->next_delay_ = REFRESH_START_MS;
}

void EPaperUC8279::power_off() {
  if (this->partial_)
    this->command(CMD_PARTIAL_OUT);
  if (this->is_using_partial_update_()) {
    // loop() writes the image to DTM1, then powers off
    this->previous_image_gate_ = 0;
    this->writing_previous_image_ = true;
    return;
  }
  this->command(CMD_POWER_OFF);
  this->next_delay_ = BUSY_ASSERT_MS;
}

void EPaperUC8279::deep_sleep() {
  if (this->is_using_partial_update_())
    return;
  ESP_LOGV(TAG, "Deep sleep");
  this->cmd_data(CMD_DEEP_SLEEP, {DEEP_SLEEP_CHECK});
}

// Nothing carries the image across a restart or the controller's deep sleep, so the panel sleeps rather than waiting
// in standby
void EPaperUC8279::on_safe_shutdown() { this->cmd_data(CMD_DEEP_SLEEP, {DEEP_SLEEP_CHECK}); }

}  // namespace esphome::epaper_spi
