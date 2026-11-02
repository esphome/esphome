#include "epaper_spi_uc8151.h"

#include <algorithm>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

ESPHOME_LOG_TAG(TAG, "epaper_spi.uc8151");

static constexpr uint8_t CMD_PSR = 0x00;
static constexpr uint8_t CMD_PWR = 0x01;
static constexpr uint8_t CMD_BTST = 0x06;
static constexpr uint8_t CMD_LUT_VCOM = 0x20;
static constexpr uint8_t CMD_PLL = 0x30;
static constexpr uint8_t CMD_VDCS = 0x82;

static constexpr uint16_t BUSY_ASSERT_MS = 100;

// Partial-refresh waveform from GxEPD2_290_T5, as used by the waveshare_epaper GDEW029T5 driver.
// The VCOM table is 44 bytes, the four pixel-transition tables (WW, BW, WB, BB, commands 0x21 to 0x24) 42 bytes.
static constexpr size_t LUT_VCOM_SIZE = 44;
static constexpr size_t LUT_SIZE = 42;
static constexpr size_t LUT_COUNT = 5;
static constexpr uint8_t LUT_PARTIAL[LUT_COUNT][LUT_VCOM_SIZE] PROGMEM = {
    {0x00, 0x20, 0x01, 0x00, 0x00, 0x01},  // VCOM
    {0x00, 0x20, 0x01, 0x00, 0x00, 0x01},  // WW
    {0x80, 0x20, 0x01, 0x00, 0x00, 0x01},  // BW
    {0x40, 0x20, 0x01, 0x00, 0x00, 0x01},  // WB
    {0x00, 0x20, 0x01, 0x00, 0x00, 0x01},  // BB
};

bool EPaperUC8151::reset() {
  // A partial update follows one that left the controller configured, in standby, with DTM1 holding the image
  if (this->partial_)
    return true;
  return EPaperBase::reset();
}

void EPaperUC8151::send_partial_settings_() {
  // Settings from the waveshare_epaper GDEW029T5 driver for its register waveform
  this->cmd_data(CMD_PWR, {0x03, 0x00, 0x2B, 0x2B, 0x03});  // VGH/VGL and VDH/VDL
  this->cmd_data(CMD_BTST, {0x17, 0x17, 0x17});             // booster soft start
  this->cmd_data(CMD_PSR, {0xBF, 0x0D});                    // LUT from register, black/white
  this->cmd_data(CMD_PLL, {0x3A});                          // 100 Hz
  this->cmd_data(CMD_VDCS, {0x08});                         // VCOM DC
  this->cmd_data(CMD_CDI, {0x17});                          // floating border, same data polarity as full refresh
  uint8_t lut[LUT_VCOM_SIZE];
  for (size_t i = 0; i != LUT_COUNT; i++) {
    const size_t size = i == 0 ? LUT_VCOM_SIZE : LUT_SIZE;
    progmem_memcpy(lut, LUT_PARTIAL[i], size);
    this->cmd_data(static_cast<uint8_t>(CMD_LUT_VCOM + i), lut, size);
  }
}

bool EPaperUC8151::initialise(bool partial) {
  if (partial) {
    this->send_partial_settings_();
  } else {
    EPaperBase::initialise(partial);  // the model init sequence sets up the OTP waveform
  }
  ESP_LOGV(TAG, "Power on");
  // Power on here, so the state machine busy-waits for it before the data transfer
  this->command(CMD_POWER_ON);
  this->next_delay_ = BUSY_ASSERT_MS;
  return true;
}

bool HOT EPaperUC8151::write_plane_(uint8_t command) {
  const uint32_t start_time = millis();
  const size_t buffer_length = this->buffer_length_;
  if (this->current_data_index_ == 0)
    this->command(command);
  this->start_data_();
  while (this->current_data_index_ != buffer_length) {
    size_t length;
    const uint8_t *data = this->buffer_.get_span(this->current_data_index_, length);
    length = std::min({length, MAX_TRANSFER_SIZE, buffer_length - this->current_data_index_});
    this->write_array(data, length);
    this->current_data_index_ += length;
    if (this->current_data_index_ != buffer_length && millis() - start_time > MAX_TRANSFER_TIME) {
      this->disable();
      return false;
    }
  }
  this->disable();
  this->current_data_index_ = 0;
  return true;
}

bool HOT EPaperUC8151::transfer_data() {
  if (this->partial_ && this->current_data_index_ == 0)
    this->set_partial_window_(0, this->width_ - 1, 0, this->height_ - 1);
  return this->write_plane_(CMD_DTM2);
}

void EPaperUC8151::power_on() {
  // Power-on is sent at the end of initialise() instead, because the data transfer must follow it
}

void EPaperUC8151::refresh_screen(bool partial) {
  ESP_LOGV(TAG, "Refresh screen (%s)", partial ? LOG_STR_LITERAL("partial") : LOG_STR_LITERAL("full"));
  this->command(CMD_REFRESH);
  // The busy line takes a short time to assert after the refresh command
  this->next_delay_ = BUSY_ASSERT_MS;
}

bool HOT EPaperUC8151::after_refresh() {
  // While partial updates are in use, DTM1 holds the image on the panel for the next partial comparison
  if (!this->is_using_partial_update_())
    return true;
  if (this->current_data_index_ == 0 && this->partial_)
    this->command(CMD_PARTIAL_OUT);
  return this->write_plane_(CMD_DTM1);
}

void EPaperUC8151::power_off() {
  ESP_LOGV(TAG, "Power off");
  this->command(CMD_POWER_OFF);
}

void EPaperUC8151::set_partial_window_(uint16_t x_start, uint16_t x_end, uint16_t y_start, uint16_t y_end) {
  this->command(CMD_PARTIAL_IN);
  this->cmd_data(CMD_PARTIAL_WINDOW, {static_cast<uint8_t>(x_start & 0xF8), static_cast<uint8_t>(x_end | 0x07),
                                      static_cast<uint8_t>(y_start >> 8), static_cast<uint8_t>(y_start & 0xFF),
                                      static_cast<uint8_t>(y_end >> 8), static_cast<uint8_t>(y_end & 0xFF), 0x01});
}

}  // namespace esphome::epaper_spi
