#include "epaper_spi_ultrachip.h"

#include "esphome/core/log.h"

namespace esphome::epaper_spi {

ESPHOME_LOG_TAG(TAG, "epaper_spi.ultrachip");

static constexpr uint8_t hi(uint16_t v) { return static_cast<uint8_t>(v >> 8); }
static constexpr uint8_t lo(uint16_t v) { return static_cast<uint8_t>(v & 0xFF); }

void EPaperUltraChip::deep_sleep() {
  // Partial updates compare against the previous image, which deep sleep would lose; standby keeps it
  if (this->is_using_partial_update_())
    return;
  this->send_deep_sleep_();
}

void EPaperUltraChip::send_deep_sleep_() {
  ESP_LOGV(TAG, "Deep sleep");
  this->cmd_data(CMD_DEEP_SLEEP, {DEEP_SLEEP_CHECK});
}

void EPaperUltraChip::on_safe_shutdown() { this->send_deep_sleep_(); }

void EPaperUltraChip::set_partial_window_(uint16_t x_start, uint16_t x_end, uint16_t y_start, uint16_t y_end) {
  this->command(CMD_PARTIAL_IN);
  this->cmd_data(CMD_PARTIAL_WINDOW,
                 {hi(x_start), static_cast<uint8_t>(lo(x_start) & 0xF8), hi(x_end),
                  static_cast<uint8_t>(lo(x_end) | 0x07), hi(y_start), lo(y_start), hi(y_end), lo(y_end), 0x01});
}

}  // namespace esphome::epaper_spi
