#pragma once

#include "esphome/core/defines.h"

#ifdef USE_VBUS_CS_PLUS
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace esphome::vbus::cs_plus {

struct SettingSpec {
  int32_t minimum;
  int32_t maximum;
  float scale;
  uint32_t enum_mask;
  uint16_t index;
  uint16_t step;
  uint16_t schemes;
  std::array<uint16_t, 6> dependencies;
  uint8_t dependency_count;
};

static constexpr size_t SETTING_COUNT = 84;
std::optional<SettingSpec> setting_at(size_t position);
std::optional<size_t> setting_position(uint16_t index);
std::optional<SettingSpec> find_setting(uint16_t index);

inline bool setting_shape_valid(const SettingSpec &s, int32_t value) {
  return value >= s.minimum && value <= s.maximum && (value - s.minimum) % s.step == 0 &&
         (!s.enum_mask || (value >= 0 && value < 32 && (s.enum_mask & (1u << value))));
}
template<typename Get> bool setting_visible(const SettingSpec &s, const Get &get) {
  const int32_t scheme = get(1);
  if (scheme < 1 || scheme > 10 || !(s.schemes & (1u << scheme)))
    return false;
  switch (s.index) {
    case 22:
    case 23:
      return get(21) == 0 || get(21) == 1 || get(21) == 5 || get(21) == 6;
    case 48:
    case 50:
      return get(12549) != 0;
    case 49:
    case 51:
      return get(12550) != 0;
    case 59:
    case 62:
      return get(2569) == 1 && get(2768) == 0;
    case 64:
      return get(71) == 1 && get(2768) == 0;
    case 66:
      return get(71) == 1 && get(64) == 1 && get(2768) == 0;
    case 69:
      return get(21) == 0 || get(21) == 1;
    case 71:
    case 461:
    case 462:
    case 544:
    case 545:
    case 2569:
      return get(2768) == 0;
    case 85:
      return get(81) == 1 && get(74) != 0;
    case 89:
      return get(81) == 1 && (get(74) == 0 || get(85) == 0);
    case 90:
      return get(81) == 1;
    case 91:
      return get(81) == 1 && (get(90) == 1 || get(90) == 2);
    case 455:
      return get(461) == 1 && get(2768) == 0;
    case 458:
      return get(462) == 1 && get(2768) == 0;
    case 497:
      return get(512) == 1;
    case 500:
      return get(513) == 1;
    case 529:
      return get(544) == 1 && get(2768) == 0;
    case 532:
      return get(545) == 1 && get(2768) == 0;
    case 2561:
    case 2563:
    case 2565:
    case 2566:
      return get(2560) == 1;
    case 2769:
    case 2771:
    case 2773:
    case 2775:
      return get(2768) == 1;
    case 3904:
    case 3905:
    case 3906:
    case 3907:
      return get(3897) == 1;
    default:
      return true;
  }
}

}  // namespace esphome::vbus::cs_plus
#endif
