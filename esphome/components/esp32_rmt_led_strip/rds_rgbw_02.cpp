#include <cassert>
#include <cinttypes>
#include "rds_rgbw_02.h"

#ifdef USE_ESP32

#include "esphome/components/light/light_color_values.h"
#include "esphome/core/log.h"

namespace esphome {
namespace esp32_rmt_led_strip {

constexpr char TAG[] = "esp32_rmt_led_strip";

RDSRGBW02Color get_rds_rgbw_02_color(const uint8_t *color_buf) {
  // we're assuming these chipsets are all RGB until otherwise indicated.
  const uint8_t red = color_buf[0];
  const uint8_t green = color_buf[1];
  const uint8_t blue = color_buf[2];
  if (red >= green && blue >= green)
    return RDS_RGBW_02_PURPLE;
  if (red >= blue && green >= blue)
    return RDS_RGBW_02_YELLOW;
  if (green >= red && blue >= red)
    return RDS_RGBW_02_CYAN;
  ESP_LOGE(TAG, "this should be unreachable");
  return RDS_RGBW_02_RED;
}

void rds_rgbw_02_rmt_generator(const ESP32RMTLEDStripLightOutput &light, int index, const uint8_t *src,
                               rmt_item32_t *dest, light::LightState *state) {
  const light::LightColorValues &values{state->current_values};
  const bool is_white = (values.get_color_mode() == light::ColorMode::WHITE && !light.is_effect_active()) ||
                        src[3] > std::max(std::max(src[0], src[1]), src[2]);
  const rmt_item32_t &bit0{light.get_bit0()};
  const rmt_item32_t &bit1{light.get_bit1()};
  bool first_item = true;

  // address (4 bits)
  uint8_t address = (uint8_t) index + 1;
  for (int i = 3; i >= 0; i--) {
    dest->val = (address >> i) & 1 ? bit1.val : bit0.val;
    if (first_item && light.get_sync_start() > 0) {
      first_item = false;
      dest->duration0 = light.get_sync_start();
    }
    dest++;
  }

  // color enum (4 bits)
  RDSRGBW02Color color = values.get_state() > 0.f ? RDS_RGBW_02_WHITE : RDS_RGBW_02_OFF;
  if (!is_white) {
    color = get_rds_rgbw_02_color(src);
  }
  for (int i = 3; i >= 0; i--) {
    dest->val = (color >> i) & 1 ? bit1.val : bit0.val;
    dest++;
  }

  // rgb brightness (24 bits)
  for (int i = 0; i < 3; i++) {
    uint8_t color = src[i];
    if (is_white && i == 2) {
      // white brightness uses the same index as blue, at least in ORDER_RGB
      // we'll make the assumption that this is true regardless of RGB order until proven otherwise.
      color = (uint8_t) (values.get_brightness() * std::numeric_limits<uint8_t>::max());
    }
    for (int j = 7; j >= 0; j--) {
      dest->val = (color >> j) & 1 ? bit1.val : bit0.val;
      dest++;
    }
  }
}

};  // namespace esp32_rmt_led_strip
};  // namespace esphome

#endif
