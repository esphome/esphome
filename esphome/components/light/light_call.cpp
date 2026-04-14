#include <cinttypes>

#include "light_call.h"
#include "light_state.h"
#include "esphome/core/log.h"
#include "esphome/core/optional.h"
#include "esphome/core/progmem.h"

namespace esphome::light {

static const char *const TAG = "light";

// Cold-path helper: called only when the caller has already determined the
// value is out of range. Keeping the range check at the caller avoids the
// call-site spill/reload and prologue on the hot path (in-range). The
// `param_name_progmem` argument points into the FIELD_NAMES table in flash;
// `progmem_read_ptr` is a plain `*addr` inline on non-ESP8266 platforms.
static void log_out_of_range_and_clamp_(const char *name, float &value, const LogString *const *param_name_progmem,
                                        float min, float max) {
  const auto *param_name =
      reinterpret_cast<const LogString *>(progmem_read_ptr(reinterpret_cast<const char *const *>(param_name_progmem)));
  ESP_LOGW(TAG, "'%s': %s value %.2f is out of range [%.1f - %.1f]", name, LOG_STR_ARG(param_name), value, min, max);
  value = clamp(value, min, max);
}

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_WARN
static void log_feature_not_supported(const char *name, const LogString *feature) {
  ESP_LOGW(TAG, "'%s': %s not supported", name, LOG_STR_ARG(feature));
}

static void log_color_mode_not_supported(const char *name, const LogString *feature) {
  ESP_LOGW(TAG, "'%s': color mode does not support setting %s", name, LOG_STR_ARG(feature));
}

static void log_invalid_parameter(const char *name, const LogString *message) {
  ESP_LOGW(TAG, "'%s': %s", name, LOG_STR_ARG(message));
}
#else
#define log_feature_not_supported(name, feature)
#define log_color_mode_not_supported(name, feature)
#define log_invalid_parameter(name, message)
#endif

// Macro to reduce repetitive setter code
#define IMPLEMENT_LIGHT_CALL_SETTER(name, type, flag) \
  LightCall &LightCall::set_##name(optional<type>(name)) { \
    if ((name).has_value()) { \
      this->name##_ = (name).value(); \
    } \
    this->set_flag_(flag, (name).has_value()); \
    return *this; \
  } \
  LightCall &LightCall::set_##name(type name) { \
    this->name##_ = name; \
    this->set_flag_(flag); \
    return *this; \
  }

// Color mode human-readable strings indexed by ColorModeBitPolicy::to_bit() (0-9)
// Index 0 is Unknown (for ColorMode::UNKNOWN), also used as fallback for out-of-range
PROGMEM_STRING_TABLE(ColorModeHumanStrings, "Unknown", "On/Off", "Brightness", "White", "Color temperature",
                     "Cold/warm white", "RGB", "RGBW", "RGB + color temperature", "RGB + cold/warm white");

static const LogString *color_mode_to_human(ColorMode color_mode) {
  return ColorModeHumanStrings::get_log_str(ColorModeBitPolicy::to_bit(color_mode), 0);
}

// Helper to log percentage values
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
static void log_percent(const LogString *param, float value) {
  ESP_LOGV(TAG, "  %s: %.0f%%", LOG_STR_ARG(param), value * 100.0f);
}
#else
#define log_percent(param, value)
#endif

void LightCall::perform() {
  const char *name = this->parent_->get_name().c_str();
  LightColorValues v = this->validate_();
  const bool publish = this->get_publish_();

  if (publish) {
    ESP_LOGV(TAG, "'%s' Setting:", name);

    // Only print color mode when it's being changed
    ColorMode current_color_mode = this->parent_->remote_values.get_color_mode();
    ColorMode target_color_mode = this->has_color_mode() ? this->color_mode_ : current_color_mode;
    if (target_color_mode != current_color_mode) {
      ESP_LOGV(TAG, "  Color mode: %s", LOG_STR_ARG(color_mode_to_human(v.get_color_mode())));
    }

    // Only print state when it's being changed
    bool current_state = this->parent_->remote_values.is_on();
    bool target_state = this->has_state() ? this->state_ : current_state;
    if (target_state != current_state) {
      ESP_LOGV(TAG, "  State: %s", ONOFF(v.is_on()));
    }

    if (this->has_brightness()) {
      log_percent(LOG_STR("Brightness"), v.get_brightness());
    }

    if (this->has_color_brightness()) {
      log_percent(LOG_STR("Color brightness"), v.get_color_brightness());
    }
    if (this->has_red() || this->has_green() || this->has_blue()) {
      ESP_LOGV(TAG, "  Red: %.0f%%, Green: %.0f%%, Blue: %.0f%%", v.get_red() * 100.0f, v.get_green() * 100.0f,
               v.get_blue() * 100.0f);
    }

    if (this->has_white()) {
      log_percent(LOG_STR("White"), v.get_white());
    }
    if (this->has_color_temperature()) {
      ESP_LOGV(TAG, "  Color temperature: %.1f mireds", v.get_color_temperature());
    }

    if (this->has_cold_white() || this->has_warm_white()) {
      ESP_LOGV(TAG, "  Cold white: %.0f%%, warm white: %.0f%%", v.get_cold_white() * 100.0f,
               v.get_warm_white() * 100.0f);
    }
  }

  if (this->has_flash_()) {
    // FLASH
    if (publish) {
      ESP_LOGV(TAG, "  Flash length: %.1fs", this->flash_length_ / 1e3f);
    }

    this->parent_->start_flash_(v, this->flash_length_, publish);
  } else if (this->has_transition_()) {
    // TRANSITION
    if (publish) {
      ESP_LOGV(TAG, "  Transition length: %.1fs", this->transition_length_ / 1e3f);
    }

    // Special case: Transition and effect can be set when turning off
    if (this->has_effect_()) {
      if (publish) {
        ESP_LOGV(TAG, "  Effect: 'None'");
      }
      this->parent_->stop_effect_();
    }

    this->parent_->start_transition_(v, this->transition_length_, publish);

  } else if (this->has_effect_()) {
    // EFFECT
    StringRef effect_s;
    if (this->effect_ == 0u) {
      effect_s = StringRef::from_lit("None");
    } else {
      effect_s = this->parent_->effects_[this->effect_ - 1]->get_name();
    }

    if (publish) {
      ESP_LOGV(TAG, "  Effect: '%.*s'", (int) effect_s.size(), effect_s.c_str());
    }

    this->parent_->start_effect_(this->effect_);

    // Also set light color values when starting an effect
    // For example to turn off the light
    this->parent_->set_immediately_(v, true);
  } else {
    // INSTANT CHANGE
    this->parent_->set_immediately_(v, publish);
  }

  if (!this->has_transition_() && this->parent_->target_state_reached_listeners_) {
    for (auto *listener : *this->parent_->target_state_reached_listeners_) {
      listener->on_light_target_state_reached();
    }
  }
  if (publish) {
    this->parent_->publish_state();
  }
  if (this->get_save_()) {
    this->parent_->save_remote_values_();
  }
}

void LightCall::log_and_clear_unsupported_(FieldFlags flag, const LogString *feature, bool use_color_mode_log) {
  auto *name = this->parent_->get_name().c_str();
  if (use_color_mode_log) {
    log_color_mode_not_supported(name, feature);
  } else {
    log_feature_not_supported(name, feature);
  }
  this->clear_flag_(flag);
}

LightColorValues LightCall::validate_() {
  auto *name = this->parent_->get_name().c_str();
  auto traits = this->parent_->get_traits();

  // Color mode check
  if (this->has_color_mode() && !traits.supports_color_mode(this->color_mode_)) {
    ESP_LOGW(TAG, "'%s' does not support color mode %s", name, LOG_STR_ARG(color_mode_to_human(this->color_mode_)));
    this->clear_flag_(FLAG_HAS_COLOR_MODE);
  }

  // Ensure there is always a color mode set
  if (!this->has_color_mode()) {
    this->color_mode_ = this->compute_color_mode_(traits);
    this->set_flag_(FLAG_HAS_COLOR_MODE);
  }
  auto color_mode = this->color_mode_;

  // Transform calls that use non-native parameters for the current mode.
  this->transform_parameters_(traits);

  // Business logic adjustments before validation
  // Flag whether an explicit turn off was requested, in which case we'll also stop the effect.
  bool explicit_turn_off_request = this->has_state() && !this->state_;

  // Turn off when brightness is set to zero, and reset brightness (so that it has nonzero brightness when turned on).
  if (this->has_brightness() && this->brightness_ == 0.0f) {
    this->state_ = false;
    this->set_flag_(FLAG_HAS_STATE);
    if (color_mode & ColorCapability::BRIGHTNESS) {
      // Reset brightness so the light has nonzero brightness when turned back on.
      this->brightness_ = 1.0f;
    } else {
      // Light doesn't support brightness; clear the flag to avoid a spurious
      // "brightness not supported" warning during capability validation.
      this->clear_flag_(FLAG_HAS_BRIGHTNESS);
    }
  }

  // Set color brightness to 100% if currently zero and a color is set.
  if ((this->has_red() || this->has_green() || this->has_blue()) && !this->has_color_brightness() &&
      this->parent_->remote_values.get_color_brightness() == 0.0f) {
    this->color_brightness_ = 1.0f;
    this->set_flag_(FLAG_HAS_COLOR_BRIGHTNESS);
  }

  // Capability validation
  if (this->has_brightness() && this->brightness_ > 0.0f && !(color_mode & ColorCapability::BRIGHTNESS))
    this->log_and_clear_unsupported_(FLAG_HAS_BRIGHTNESS, LOG_STR("brightness"), false);

  // Transition length possible check
  if (this->has_transition_() && this->transition_length_ != 0 && !(color_mode & ColorCapability::BRIGHTNESS))
    this->log_and_clear_unsupported_(FLAG_HAS_TRANSITION, LOG_STR("transitions"), false);

  if (this->has_color_brightness() && this->color_brightness_ > 0.0f && !(color_mode & ColorCapability::RGB))
    this->log_and_clear_unsupported_(FLAG_HAS_COLOR_BRIGHTNESS, LOG_STR("RGB brightness"), true);

  // RGB exists check
  if (((this->has_red() && this->red_ > 0.0f) || (this->has_green() && this->green_ > 0.0f) ||
       (this->has_blue() && this->blue_ > 0.0f)) &&
      !(color_mode & ColorCapability::RGB)) {
    log_color_mode_not_supported(name, LOG_STR("RGB color"));
    this->clear_flag_(FLAG_HAS_RED);
    this->clear_flag_(FLAG_HAS_GREEN);
    this->clear_flag_(FLAG_HAS_BLUE);
  }

  // White value exists check
  if (this->has_white() && this->white_ > 0.0f &&
      !(color_mode & ColorCapability::WHITE || color_mode & ColorCapability::COLD_WARM_WHITE))
    this->log_and_clear_unsupported_(FLAG_HAS_WHITE, LOG_STR("white value"), true);

  // Color temperature exists check
  if (this->has_color_temperature() &&
      !(color_mode & ColorCapability::COLOR_TEMPERATURE || color_mode & ColorCapability::COLD_WARM_WHITE))
    this->log_and_clear_unsupported_(FLAG_HAS_COLOR_TEMPERATURE, LOG_STR("color temperature"), true);

  // Cold/warm white value exists check
  if (((this->has_cold_white() && this->cold_white_ > 0.0f) || (this->has_warm_white() && this->warm_white_ > 0.0f)) &&
      !(color_mode & ColorCapability::COLD_WARM_WHITE)) {
    log_color_mode_not_supported(name, LOG_STR("cold/warm white value"));
    this->clear_flag_(FLAG_HAS_COLD_WHITE);
    this->clear_flag_(FLAG_HAS_WARM_WHITE);
  }

  // Create color values and validate+apply ranges in one step to eliminate duplicate checks
  auto v = this->parent_->remote_values;
  if (this->has_color_mode())
    v.set_color_mode(this->color_mode_);
  if (this->has_state())
    v.set_state(this->state_);

  // Clamp the eight [0.0, 1.0] fields and copy them from `this` into `v`.
  //
  // LightCall and LightColorValues both declare the same eight float fields in
  // the same order (brightness_, color_brightness_, red_, green_, blue_,
  // white_, cold_white_, warm_white_), and their corresponding flag bits are
  // also 0-7 in that order. Under that layout the LightCall offset for field i
  // is `offsetof(LightCall, brightness_) + i * 4`, and the LightColorValues
  // offset is exactly 12 bytes lower (enforced by the static_asserts below).
  // Iterating via bit-position arithmetic lets us collapse eight inlined
  // clamp/copy blocks into a single loop.
  constexpr size_t SRC_BASE = offsetof(LightCall, brightness_);
  constexpr size_t SRC_TO_DST_DELTA = SRC_BASE - offsetof(LightColorValues, brightness_);

  // Per-field layout assertions: each clamp field must sit at its bit-indexed
  // slot in both LightCall and LightColorValues, with the same byte-offset
  // delta. A reorder of any single field (in either struct) trips the assert
  // pointing at that field, so failures name the exact member at fault.
  // The one case these cannot catch is a synchronized reorder in both structs
  // plus FIELD_NAMES — that would compile silently, but requires deliberate
  // three-place changes by the refactorer.
#define ESPHOME_LIGHT_ASSERT_CLAMP_FIELD(bit, flag_suffix, member) \
  static_assert(FLAG_HAS_##flag_suffix == 1u << (bit), "FLAG_HAS_" #flag_suffix " bit position"); \
  static_assert(offsetof(LightCall, member) == SRC_BASE + (bit) * sizeof(float), \
                "LightCall::" #member " must be at bit-indexed slot"); \
  static_assert(offsetof(LightColorValues, member) == SRC_BASE + (bit) * sizeof(float) - SRC_TO_DST_DELTA, \
                "LightColorValues::" #member " must match LightCall delta")
  ESPHOME_LIGHT_ASSERT_CLAMP_FIELD(0, BRIGHTNESS, brightness_);
  ESPHOME_LIGHT_ASSERT_CLAMP_FIELD(1, COLOR_BRIGHTNESS, color_brightness_);
  ESPHOME_LIGHT_ASSERT_CLAMP_FIELD(2, RED, red_);
  ESPHOME_LIGHT_ASSERT_CLAMP_FIELD(3, GREEN, green_);
  ESPHOME_LIGHT_ASSERT_CLAMP_FIELD(4, BLUE, blue_);
  ESPHOME_LIGHT_ASSERT_CLAMP_FIELD(5, WHITE, white_);
  ESPHOME_LIGHT_ASSERT_CLAMP_FIELD(6, COLD_WHITE, cold_white_);
  ESPHOME_LIGHT_ASSERT_CLAMP_FIELD(7, WARM_WHITE, warm_white_);
#undef ESPHOME_LIGHT_ASSERT_CLAMP_FIELD

  static const LogString *const FIELD_NAMES[8] PROGMEM = {
      LOG_STR("Brightness"),        // FLAG_HAS_BRIGHTNESS       (bit 0)
      LOG_STR("Color brightness"),  // FLAG_HAS_COLOR_BRIGHTNESS (bit 1)
      LOG_STR("Red"),               // FLAG_HAS_RED              (bit 2)
      LOG_STR("Green"),             // FLAG_HAS_GREEN            (bit 3)
      LOG_STR("Blue"),              // FLAG_HAS_BLUE             (bit 4)
      LOG_STR("White"),             // FLAG_HAS_WHITE            (bit 5)
      LOG_STR("Cold white"),        // FLAG_HAS_COLD_WHITE       (bit 6)
      LOG_STR("Warm white"),        // FLAG_HAS_WARM_WHITE       (bit 7)
  };

  // The static_asserts above guarantee the eight clampable floats are laid
  // out consecutively starting at brightness_ in both structs, so we can
  // treat `&brightness_` as the base of an 8-element float array and index
  // by bit position directly. Iterate only the set bits via __builtin_ctz +
  // clear-lowest-bit: HA can drive high-frequency automations through
  // perform(), so the hot path runs in O(popcount) instead of always
  // scanning all eight slots.
  //
  // The range check is done on the IEEE 754 bit pattern as an unsigned int,
  // not on the float itself. Values in [0.0f, 1.0f] have bits in
  // [0x00000000, 0x3F800000]; anything greater (as unsigned) is out of range:
  // values > 1.0f have a larger bit pattern, and negative values have the
  // sign bit (0x80000000) set which makes their unsigned interpretation
  // enormous. One unsigned compare replaces two soft-float __ltsf2/__gtsf2
  // calls on ESP8266 and is essentially free on targets with an FPU too.
  constexpr uint32_t ONE_F_BITS = 0x3F800000u;  // bit pattern of 1.0f
  float *const src_fields = &this->brightness_;
  float *const dst_fields = &v.brightness_;
  unsigned active = this->flags_ & CLAMP_FLAGS_MASK;
  while (active != 0) {
    unsigned bit = __builtin_ctz(active);
    active &= active - 1;  // clear lowest set bit
    float &value = src_fields[bit];
    // Union type-pun (GCC/Clang extension): bit_cast/memcpy don't optimize to
    // a no-op on xtensa-gcc, same reasoning as api/proto.h float_to_raw().
    union {
      float f;
      uint32_t u;
    } pun;
    pun.f = value;
    if (pun.u > ONE_F_BITS)
      log_out_of_range_and_clamp_(name, value, &FIELD_NAMES[bit], 0.0f, 1.0f);
    dst_fields[bit] = value;
  }

  // color_temperature uses a dynamic range from the light's traits and is
  // handled separately.
  if (this->has_color_temperature()) {
    static const LogString *const CT_NAME PROGMEM = LOG_STR("Color temperature");
    const float ct_min = traits.get_min_mireds();
    const float ct_max = traits.get_max_mireds();
    if (this->color_temperature_ < ct_min || this->color_temperature_ > ct_max)
      log_out_of_range_and_clamp_(name, this->color_temperature_, &CT_NAME, ct_min, ct_max);
    v.color_temperature_ = this->color_temperature_;
  }

  v.normalize_color();

  // Flash length check
  if (this->has_flash_() && this->flash_length_ == 0) {
    log_invalid_parameter(name, LOG_STR("flash length must be >0"));
    this->clear_flag_(FLAG_HAS_FLASH);
  }

  // validate transition length/flash length/effect not used at the same time
  bool supports_transition = color_mode & ColorCapability::BRIGHTNESS;

  // If effect is already active, remove effect start
  if (this->has_effect_() && this->effect_ == this->parent_->active_effect_index_) {
    this->clear_flag_(FLAG_HAS_EFFECT);
  }

  // validate effect index
  if (this->has_effect_() && this->effect_ > this->parent_->effects_.size()) {
    ESP_LOGW(TAG, "'%s': invalid effect index %" PRIu32, name, this->effect_);
    this->clear_flag_(FLAG_HAS_EFFECT);
  }

  if (this->has_effect_() && (this->has_transition_() || this->has_flash_())) {
    log_invalid_parameter(name, LOG_STR("effect cannot be used with transition/flash"));
    this->clear_flag_(FLAG_HAS_TRANSITION);
    this->clear_flag_(FLAG_HAS_FLASH);
  }

  if (this->has_flash_() && this->has_transition_()) {
    log_invalid_parameter(name, LOG_STR("flash cannot be used with transition"));
    this->clear_flag_(FLAG_HAS_TRANSITION);
  }

  if (!this->has_transition_() && !this->has_flash_() && (!this->has_effect_() || this->effect_ == 0) &&
      supports_transition) {
    // nothing specified and light supports transitions, set default transition length
    this->transition_length_ = this->parent_->default_transition_length_;
    this->set_flag_(FLAG_HAS_TRANSITION);
  }

  if (this->has_transition_() && this->transition_length_ == 0) {
    // 0 transition is interpreted as no transition (instant change)
    this->clear_flag_(FLAG_HAS_TRANSITION);
  }

  if (this->has_transition_() && !supports_transition)
    this->log_and_clear_unsupported_(FLAG_HAS_TRANSITION, LOG_STR("transitions"), false);

  // If not a flash and turning the light off, then disable the light
  // Do not use light color values directly, so that effects can set 0% brightness
  // Reason: When user turns off the light in frontend, the effect should also stop
  bool target_state = this->has_state() ? this->state_ : v.is_on();
  if (!this->has_flash_() && !target_state) {
    if (this->has_effect_()) {
      log_invalid_parameter(name, LOG_STR("cannot start effect when turning off"));
      this->clear_flag_(FLAG_HAS_EFFECT);
    } else if (this->parent_->active_effect_index_ != 0 && explicit_turn_off_request) {
      // Auto turn off effect
      this->effect_ = 0;
      this->set_flag_(FLAG_HAS_EFFECT);
    }
  }

  // Disable saving for flashes
  if (this->has_flash_())
    this->clear_flag_(FLAG_SAVE);

  return v;
}
void LightCall::transform_parameters_(const LightTraits &traits) {
  // Allow CWWW modes to be set with a white value and/or color temperature.
  // This is used in three cases in HA:
  // - CW/WW lights, which set the "brightness" and "color_temperature"
  // - RGBWW lights with color_interlock=true, which also sets "brightness" and
  //   "color_temperature" (without color_interlock, CW/WW are set directly)
  // - Legacy Home Assistant (pre-colormode), which sets "white" and "color_temperature"

  // Cache min/max mireds to avoid repeated calls
  const float min_mireds = traits.get_min_mireds();
  const float max_mireds = traits.get_max_mireds();

  if (((this->has_white() && this->white_ > 0.0f) || this->has_color_temperature()) &&  //
      (this->color_mode_ & ColorCapability::COLD_WARM_WHITE) &&                         //
      !(this->color_mode_ & ColorCapability::WHITE) &&                                  //
      !(this->color_mode_ & ColorCapability::COLOR_TEMPERATURE) &&                      //
      min_mireds > 0.0f && max_mireds > 0.0f) {
    ESP_LOGV(TAG, "'%s': setting cold/warm white channels using white/color temperature values",
             this->parent_->get_name().c_str());
    // Only compute cold_white/warm_white from color_temperature if they're not already explicitly set.
    // This is important for state restoration, where both color_temperature and cold_white/warm_white
    // are restored from flash - we want to preserve the saved cold_white/warm_white values.
    if (this->has_color_temperature() && !this->has_cold_white() && !this->has_warm_white()) {
      const float color_temp = clamp(this->color_temperature_, min_mireds, max_mireds);
      const float range = max_mireds - min_mireds;
      const float ww_fraction = (color_temp - min_mireds) / range;
      const float cw_fraction = 1.0f - ww_fraction;
      const float max_cw_ww = std::max(ww_fraction, cw_fraction);
      this->cold_white_ = this->parent_->gamma_uncorrect_lut(cw_fraction / max_cw_ww);
      this->warm_white_ = this->parent_->gamma_uncorrect_lut(ww_fraction / max_cw_ww);
      this->set_flag_(FLAG_HAS_COLD_WHITE);
      this->set_flag_(FLAG_HAS_WARM_WHITE);
    }
    if (this->has_white()) {
      this->brightness_ = this->white_;
      this->set_flag_(FLAG_HAS_BRIGHTNESS);
    }
  }
}
ColorMode LightCall::compute_color_mode_(const LightTraits &traits) {
  auto supported_modes = traits.get_supported_color_modes();
  int supported_count = supported_modes.size();

  // Some lights don't support any color modes (e.g. monochromatic light), leave it at unknown.
  if (supported_count == 0)
    return ColorMode::UNKNOWN;

  // In the common case of lights supporting only a single mode, use that one.
  if (supported_count == 1)
    return *supported_modes.begin();

  // Don't change if the light is being turned off.
  ColorMode current_mode = this->parent_->remote_values.get_color_mode();
  if (this->has_state() && !this->state_)
    return current_mode;

  // If no color mode is specified, we try to guess the color mode. This is needed for backward compatibility to
  // pre-colormode clients and automations, but also for the MQTT API, where HA doesn't let us know which color mode
  // was used for some reason.
  // Compute intersection of suitable and supported modes using bitwise AND
  color_mode_bitmask_t intersection = this->get_suitable_color_modes_mask_() & supported_modes.get_mask();

  // Don't change if the current mode is in the intersection (suitable AND supported)
  if (ColorModeMask::mask_contains(intersection, current_mode)) {
    ESP_LOGV(TAG, "'%s': color mode not specified; retaining %s", this->parent_->get_name().c_str(),
             LOG_STR_ARG(color_mode_to_human(current_mode)));
    return current_mode;
  }

  // Use the preferred suitable mode.
  if (intersection != 0) {
    ColorMode mode = ColorModeMask::first_value_from_mask(intersection);
    ESP_LOGV(TAG, "'%s': color mode not specified; using %s", this->parent_->get_name().c_str(),
             LOG_STR_ARG(color_mode_to_human(mode)));
    return mode;
  }

  // There's no supported mode for this call, so warn, use the current more or a mode at random and let validation strip
  // out whatever we don't support.
  auto color_mode = current_mode != ColorMode::UNKNOWN ? current_mode : *supported_modes.begin();
  ESP_LOGW(TAG, "'%s': no suitable color mode supported; defaulting to %s", this->parent_->get_name().c_str(),
           LOG_STR_ARG(color_mode_to_human(color_mode)));
  return color_mode;
}
// PROGMEM lookup table for get_suitable_color_modes_mask_().
// Maps 4-bit key (white | ct<<1 | cwww<<2 | rgb<<3) to color mode bitmask.
// Packed into uint8_t by right-shifting by PACK_SHIFT since the lower bits
// (UNKNOWN, ON_OFF, BRIGHTNESS) are never present in suitable mode masks.
static constexpr unsigned PACK_SHIFT = ColorModeBitPolicy::to_bit(ColorMode::WHITE);
// clang-format off
static constexpr uint8_t SUITABLE_COLOR_MODES[] PROGMEM = {
    // [0] none - all modes with brightness
    static_cast<uint8_t>(ColorModeMask({ColorMode::RGB_WHITE, ColorMode::RGB_COLOR_TEMPERATURE,
        ColorMode::RGB_COLD_WARM_WHITE, ColorMode::RGB, ColorMode::WHITE, ColorMode::COLOR_TEMPERATURE,
        ColorMode::COLD_WARM_WHITE}).get_mask() >> PACK_SHIFT),
    // [1] white only
    static_cast<uint8_t>(ColorModeMask({ColorMode::WHITE, ColorMode::RGB_WHITE, ColorMode::RGB_COLOR_TEMPERATURE,
        ColorMode::COLD_WARM_WHITE, ColorMode::RGB_COLD_WARM_WHITE}).get_mask() >> PACK_SHIFT),
    // [2] ct only
    static_cast<uint8_t>(ColorModeMask({ColorMode::COLOR_TEMPERATURE, ColorMode::RGB_COLOR_TEMPERATURE,
        ColorMode::COLD_WARM_WHITE, ColorMode::RGB_COLD_WARM_WHITE}).get_mask() >> PACK_SHIFT),
    // [3] white + ct
    static_cast<uint8_t>(ColorModeMask({ColorMode::COLD_WARM_WHITE, ColorMode::RGB_COLOR_TEMPERATURE,
        ColorMode::RGB_COLD_WARM_WHITE}).get_mask() >> PACK_SHIFT),
    // [4] cwww only
    static_cast<uint8_t>(ColorModeMask({ColorMode::COLD_WARM_WHITE,
        ColorMode::RGB_COLD_WARM_WHITE}).get_mask() >> PACK_SHIFT),
    0,  // [5] white + cwww (conflicting)
    0,  // [6] ct + cwww (conflicting)
    0,  // [7] white + ct + cwww (conflicting)
    // [8] rgb only
    static_cast<uint8_t>(ColorModeMask({ColorMode::RGB, ColorMode::RGB_WHITE, ColorMode::RGB_COLOR_TEMPERATURE,
        ColorMode::RGB_COLD_WARM_WHITE}).get_mask() >> PACK_SHIFT),
    // [9] rgb + white
    static_cast<uint8_t>(ColorModeMask({ColorMode::RGB_WHITE, ColorMode::RGB_COLOR_TEMPERATURE,
        ColorMode::RGB_COLD_WARM_WHITE}).get_mask() >> PACK_SHIFT),
    // [10] rgb + ct
    static_cast<uint8_t>(ColorModeMask({ColorMode::RGB_COLOR_TEMPERATURE,
        ColorMode::RGB_COLD_WARM_WHITE}).get_mask() >> PACK_SHIFT),
    // [11] rgb + white + ct
    static_cast<uint8_t>(ColorModeMask({ColorMode::RGB_COLOR_TEMPERATURE,
        ColorMode::RGB_COLD_WARM_WHITE}).get_mask() >> PACK_SHIFT),
    // [12] rgb + cwww
    static_cast<uint8_t>(ColorModeMask({ColorMode::RGB_COLD_WARM_WHITE}).get_mask() >> PACK_SHIFT),
    0,  // [13] rgb + white + cwww (conflicting)
    0,  // [14] rgb + ct + cwww (conflicting)
    0,  // [15] all (conflicting)
};
// clang-format on

color_mode_bitmask_t LightCall::get_suitable_color_modes_mask_() {
  bool has_white = this->has_white() && this->white_ > 0.0f;
  bool has_ct = this->has_color_temperature();
  bool has_cwww =
      (this->has_cold_white() && this->cold_white_ > 0.0f) || (this->has_warm_white() && this->warm_white_ > 0.0f);
  bool has_rgb = (this->has_color_brightness() && this->color_brightness_ > 0.0f) ||
                 (this->has_red() || this->has_green() || this->has_blue());

  // Build key from flags: [rgb][cwww][ct][white]
  uint8_t key = has_white | (has_ct << 1) | (has_cwww << 2) | (has_rgb << 3);
  return static_cast<color_mode_bitmask_t>(progmem_read_byte(&SUITABLE_COLOR_MODES[key])) << PACK_SHIFT;
}

LightCall &LightCall::set_effect(const char *effect, size_t len) {
  if (len == 4 && strncasecmp(effect, "none", 4) == 0) {
    this->set_effect(uint32_t{0});
    return *this;
  }

  bool found = false;
  StringRef effect_ref(effect, len);
  for (uint32_t i = 0; i < this->parent_->effects_.size(); i++) {
    if (str_equals_case_insensitive(effect_ref, this->parent_->effects_[i]->get_name())) {
      this->set_effect(i + 1);
      found = true;
      break;
    }
  }
  if (!found) {
    ESP_LOGW(TAG, "'%s': no such effect '%.*s'", this->parent_->get_name().c_str(), (int) len, effect);
  }
  return *this;
}
LightCall &LightCall::from_light_color_values(const LightColorValues &values) {
  this->set_state(values.is_on());
  this->set_brightness_if_supported(values.get_brightness());
  this->set_color_brightness_if_supported(values.get_color_brightness());
  this->set_color_mode_if_supported(values.get_color_mode());
  this->set_red_if_supported(values.get_red());
  this->set_green_if_supported(values.get_green());
  this->set_blue_if_supported(values.get_blue());
  this->set_white_if_supported(values.get_white());
  this->set_color_temperature_if_supported(values.get_color_temperature());
  this->set_cold_white_if_supported(values.get_cold_white());
  this->set_warm_white_if_supported(values.get_warm_white());
  return *this;
}
ColorMode LightCall::get_active_color_mode_() {
  return this->has_color_mode() ? this->color_mode_ : this->parent_->remote_values.get_color_mode();
}
LightCall &LightCall::set_transition_length_if_supported(uint32_t transition_length) {
  if (this->get_active_color_mode_() & ColorCapability::BRIGHTNESS)
    this->set_transition_length(transition_length);
  return *this;
}
LightCall &LightCall::set_brightness_if_supported(float brightness) {
  if (this->get_active_color_mode_() & ColorCapability::BRIGHTNESS)
    this->set_brightness(brightness);
  return *this;
}
LightCall &LightCall::set_color_mode_if_supported(ColorMode color_mode) {
  if (this->parent_->get_traits().supports_color_mode(color_mode))
    this->set_color_mode(color_mode);
  return *this;
}
LightCall &LightCall::set_color_brightness_if_supported(float brightness) {
  if (this->get_active_color_mode_() & ColorCapability::RGB)
    this->set_color_brightness(brightness);
  return *this;
}
LightCall &LightCall::set_red_if_supported(float red) {
  if (this->get_active_color_mode_() & ColorCapability::RGB)
    this->set_red(red);
  return *this;
}
LightCall &LightCall::set_green_if_supported(float green) {
  if (this->get_active_color_mode_() & ColorCapability::RGB)
    this->set_green(green);
  return *this;
}
LightCall &LightCall::set_blue_if_supported(float blue) {
  if (this->get_active_color_mode_() & ColorCapability::RGB)
    this->set_blue(blue);
  return *this;
}
LightCall &LightCall::set_white_if_supported(float white) {
  if (this->get_active_color_mode_() & ColorCapability::WHITE)
    this->set_white(white);
  return *this;
}
LightCall &LightCall::set_color_temperature_if_supported(float color_temperature) {
  if (this->get_active_color_mode_() & ColorCapability::COLOR_TEMPERATURE ||
      this->get_active_color_mode_() & ColorCapability::COLD_WARM_WHITE)
    this->set_color_temperature(color_temperature);
  return *this;
}
LightCall &LightCall::set_cold_white_if_supported(float cold_white) {
  if (this->get_active_color_mode_() & ColorCapability::COLD_WARM_WHITE)
    this->set_cold_white(cold_white);
  return *this;
}
LightCall &LightCall::set_warm_white_if_supported(float warm_white) {
  if (this->get_active_color_mode_() & ColorCapability::COLD_WARM_WHITE)
    this->set_warm_white(warm_white);
  return *this;
}
IMPLEMENT_LIGHT_CALL_SETTER(state, bool, FLAG_HAS_STATE)
IMPLEMENT_LIGHT_CALL_SETTER(transition_length, uint32_t, FLAG_HAS_TRANSITION)
IMPLEMENT_LIGHT_CALL_SETTER(flash_length, uint32_t, FLAG_HAS_FLASH)
IMPLEMENT_LIGHT_CALL_SETTER(brightness, float, FLAG_HAS_BRIGHTNESS)
IMPLEMENT_LIGHT_CALL_SETTER(color_mode, ColorMode, FLAG_HAS_COLOR_MODE)
IMPLEMENT_LIGHT_CALL_SETTER(color_brightness, float, FLAG_HAS_COLOR_BRIGHTNESS)
IMPLEMENT_LIGHT_CALL_SETTER(red, float, FLAG_HAS_RED)
IMPLEMENT_LIGHT_CALL_SETTER(green, float, FLAG_HAS_GREEN)
IMPLEMENT_LIGHT_CALL_SETTER(blue, float, FLAG_HAS_BLUE)
IMPLEMENT_LIGHT_CALL_SETTER(white, float, FLAG_HAS_WHITE)
IMPLEMENT_LIGHT_CALL_SETTER(color_temperature, float, FLAG_HAS_COLOR_TEMPERATURE)
IMPLEMENT_LIGHT_CALL_SETTER(cold_white, float, FLAG_HAS_COLD_WHITE)
IMPLEMENT_LIGHT_CALL_SETTER(warm_white, float, FLAG_HAS_WARM_WHITE)
LightCall &LightCall::set_effect(optional<std::string> effect) {
  if (effect.has_value())
    this->set_effect(*effect);
  return *this;
}
LightCall &LightCall::set_effect(uint32_t effect_number) {
  this->effect_ = effect_number;
  this->set_flag_(FLAG_HAS_EFFECT);
  return *this;
}
LightCall &LightCall::set_effect(optional<uint32_t> effect_number) {
  if (effect_number.has_value()) {
    this->effect_ = effect_number.value();
  }
  this->set_flag_(FLAG_HAS_EFFECT, effect_number.has_value());
  return *this;
}
LightCall &LightCall::set_publish(bool publish) {
  this->set_flag_(FLAG_PUBLISH, publish);
  return *this;
}
LightCall &LightCall::set_save(bool save) {
  this->set_flag_(FLAG_SAVE, save);
  return *this;
}
LightCall &LightCall::set_rgb(float red, float green, float blue) {
  this->set_red(red);
  this->set_green(green);
  this->set_blue(blue);
  return *this;
}
LightCall &LightCall::set_rgbw(float red, float green, float blue, float white) {
  this->set_rgb(red, green, blue);
  this->set_white(white);
  return *this;
}

}  // namespace esphome::light
