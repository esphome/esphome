#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/color.h"

namespace esphome {
namespace light {

struct ESPHSVColor {
  union {
    struct {
      union {
        uint8_t hue;
        uint8_t h;
      };
      union {
        uint8_t saturation;
        uint8_t s;
      };
      union {
        uint8_t value;
        uint8_t v;
      };
    };
    uint8_t raw[3];
  };
  inline ESPHSVColor() ESPHOME_ALWAYS_INLINE : h(0), s(0), v(0) {  // NOLINT
  }
  inline ESPHSVColor(uint8_t hue, uint8_t saturation, uint8_t value) ESPHOME_ALWAYS_INLINE : hue(hue),
                                                                                             saturation(saturation),
                                                                                             value(value) {}
  Color to_rgb() const;
};

}  // namespace light
}  // namespace esphome
