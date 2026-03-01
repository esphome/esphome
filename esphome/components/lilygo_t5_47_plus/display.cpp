#include "display.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include <esp32-hal-gpio.h>
#include <esp_sleep.h>

namespace esphome {
namespace lilygo_t5_47_plus {

static const char *const TAG = "lilygo_t5_47_plus";

void LilygoT547PlusDisplay::setup() {
  ESP_LOGV(TAG, "Initialize called");

  ESP_LOGV(TAG, "Free heap: %d bytes", heap_caps_get_free_size(MALLOC_CAP_8BIT));
  ESP_LOGV(TAG, "Free PSRAM: %d bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  ESP_LOGV(TAG, "Largest free PSRAM block: %d bytes", heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));

  epd_init();
  uint32_t buffer_size = this->get_buffer_length_();
  ESP_LOGD(TAG, "Buffer size: %u bytes", buffer_size);

  this->buffer_ = (uint8_t *) heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (this->buffer_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate display buffer (%u bytes). Is PSRAM enabled?", buffer_size);
    this->mark_failed();
    return;
  }

  memset(this->buffer_, 0xFF, buffer_size);
}

float LilygoT547PlusDisplay::get_setup_priority() const { return setup_priority::PROCESSOR; }
size_t LilygoT547PlusDisplay::get_buffer_length_() {
  return this->get_width_internal() * this->get_height_internal() / 2;
}

void LilygoT547PlusDisplay::update() {
  uint32_t t0 = esphome::millis();
  this->do_update_();
  uint32_t t1 = esphome::millis();
  this->display();
  uint32_t t2 = esphome::millis();
  ESP_LOGV(TAG, "update(): do_update_=%ums, display=%ums, total=%ums", t1 - t0, t2 - t1, t2 - t0);
}

void HOT LilygoT547PlusDisplay::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0)
    return;
  uint8_t gs = 255 - ((color.red * 2126 / 10000) + (color.green * 7152 / 10000) + (color.blue * 722 / 10000));
  epd_draw_pixel(x, y, gs, this->buffer_);
}

void LilygoT547PlusDisplay::fill(Color color) {
  if (this->buffer_ == nullptr)
    return;
  uint8_t gs = 255 - ((color.red * 2126 / 10000) + (color.green * 7152 / 10000) + (color.blue * 722 / 10000));
  // 4-bit per pixel: pack same value into both nibbles
  uint8_t fill_byte = (gs & 0xF0) | (gs >> 4);
  memset(this->buffer_, fill_byte, this->get_buffer_length_());
}

void LilygoT547PlusDisplay::dump_config() {
  LOG_DISPLAY("", "LilygoT547PlusDisplay", this);
  LOG_UPDATE_INTERVAL(this);
  // Log why the ESP32-S3 woke up — helps verify deep sleep is working
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  switch (cause) {
    case ESP_SLEEP_WAKEUP_UNDEFINED:
      ESP_LOGCONFIG(TAG, "  Wakeup cause: power-on / normal reset");
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      ESP_LOGCONFIG(TAG, "  Wakeup cause: timer (deep sleep)");
      break;
    case ESP_SLEEP_WAKEUP_EXT0:
      ESP_LOGCONFIG(TAG, "  Wakeup cause: ext0 pin");
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      ESP_LOGCONFIG(TAG, "  Wakeup cause: ext1 pin(s)");
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      ESP_LOGCONFIG(TAG, "  Wakeup cause: touch pad");
      break;
    default:
      ESP_LOGCONFIG(TAG, "  Wakeup cause: other (%d)", (int) cause);
      break;
  }
}

void LilygoT547PlusDisplay::display() {
  uint32_t t0 = esphome::millis();
  ESP_LOGD(TAG, "Refreshing EPD...");

  epd_poweron();
  uint32_t t1 = esphome::millis();

  epd_clear();
  uint32_t t2 = esphome::millis();

  epd_draw_grayscale_image(epd_full_screen(), this->buffer_);
  uint32_t t3 = esphome::millis();

  epd_poweroff();
  uint32_t t4 = esphome::millis();

  ESP_LOGD(TAG, "EPD refresh done: poweron=%ums clear=%ums draw=%ums poweroff=%ums total=%ums", t1 - t0, t2 - t1,
           t3 - t2, t4 - t3, t4 - t0);
}

void LilygoT547PlusDisplay::on_safe_shutdown() {
  // Fully power off the EPD supply before entering deep sleep.
  // The e-paper retains its image without power, so this is safe.
  epd_poweroff_all();
  ESP_LOGD(TAG, "EPD powered off for shutdown");
}

}  // namespace lilygo_t5_47_plus
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
