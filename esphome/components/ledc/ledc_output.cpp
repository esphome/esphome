#include "ledc_output.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#include <driver/ledc.h>
#include <cinttypes>
#include <esp_private/periph_ctrl.h>
#if !defined(SOC_LEDC_SUPPORT_FADE_STOP)
#include <hal/ledc_ll.h>
#endif

#define CLOCK_FREQUENCY 80e6f

#ifdef SOC_LEDC_SUPPORT_APB_CLOCK
#define DEFAULT_CLK LEDC_USE_APB_CLK
#else
#define DEFAULT_CLK LEDC_AUTO_CLK
#endif

static const uint8_t SETUP_ATTEMPT_COUNT_MAX = 5;

namespace esphome::ledc {

static const char *const TAG = "ledc.output";
static bool ledc_peripheral_reset_done = false;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static const int MAX_RES_BITS = LEDC_TIMER_BIT_MAX - 1;
#if SOC_LEDC_SUPPORT_HS_MODE
// Only ESP32 has LEDC_HIGH_SPEED_MODE
inline ledc_mode_t get_speed_mode(uint8_t channel) { return channel < 8 ? LEDC_HIGH_SPEED_MODE : LEDC_LOW_SPEED_MODE; }
#else
// S2, C3, S3 only support LEDC_LOW_SPEED_MODE
// See
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/peripherals/ledc.html#functionality-overview
inline ledc_mode_t get_speed_mode(uint8_t) { return LEDC_LOW_SPEED_MODE; }
#endif

#if !defined(SOC_LEDC_SUPPORT_FADE_STOP)
// Classic ESP32 (currently the only target without SOC_LEDC_SUPPORT_FADE_STOP) can block in
// ledc_ll_set_duty_start() while duty_start is set. We check the same conf1.duty_start bit here
// to defer updates and avoid entering IDF's unbounded wait loop.
//
// This intentionally depends on the classic ESP32 LEDC register layout used by IDF's own LL HAL.
// If another target without SOC_LEDC_SUPPORT_FADE_STOP is introduced, revisit this helper.
static_assert(
#if defined(CONFIG_IDF_TARGET_ESP32)
    true,
#else
    false,
#endif
    "LEDC duty_start pending check assumes classic ESP32 register layout; "
    "re-evaluate for this target");

static bool ledc_duty_update_pending(ledc_mode_t speed_mode, ledc_channel_t chan_num) {
  auto *hw = LEDC_LL_GET_HW();
  return hw->channel_group[speed_mode].channel[chan_num].conf1.duty_start != 0;
}
#endif

float ledc_max_frequency_for_bit_depth(uint8_t bit_depth) {
  return static_cast<float>(CLOCK_FREQUENCY) / static_cast<float>(1 << bit_depth);
}

float ledc_min_frequency_for_bit_depth(uint8_t bit_depth, bool low_frequency) {
  const float max_div_num = ((1 << MAX_RES_BITS) - 1) / (low_frequency ? 32.0f : 256.0f);
  return static_cast<float>(CLOCK_FREQUENCY) / (max_div_num * static_cast<float>(1 << bit_depth));
}

optional<uint8_t> ledc_bit_depth_for_frequency(float frequency) {
  ESP_LOGV(TAG, "Calculating resolution bit-depth for frequency %f", frequency);
  for (int i = MAX_RES_BITS; i >= 1; i--) {
    const float min_frequency = ledc_min_frequency_for_bit_depth(i, (frequency < 100));
    const float max_frequency = ledc_max_frequency_for_bit_depth(i);
    if (min_frequency <= frequency && frequency <= max_frequency) {
      ESP_LOGV(TAG, "Resolution calculated as %d", i);
      return i;
    }
  }
  return {};
}

esp_err_t configure_timer_frequency(ledc_mode_t speed_mode, ledc_timer_t timer_num, ledc_channel_t chan_num,
                                    uint8_t channel, uint8_t &bit_depth, float frequency) {
  auto bit_depth_opt = ledc_bit_depth_for_frequency(frequency);
  bit_depth = bit_depth_opt.value_or(0);
  if (bit_depth < 1) {
    ESP_LOGE(TAG, "Frequency %f can't be achieved with any bit depth", frequency);
  }

  ledc_timer_config_t timer_conf{};
  timer_conf.speed_mode = speed_mode;
  timer_conf.duty_resolution = static_cast<ledc_timer_bit_t>(bit_depth);
  timer_conf.timer_num = timer_num;
  timer_conf.freq_hz = (uint32_t) frequency;
  timer_conf.clk_cfg = DEFAULT_CLK;

  // Configure the time with fallback in case of error
  int attempt_count_max = SETUP_ATTEMPT_COUNT_MAX;
  esp_err_t init_result = ESP_FAIL;
  while (attempt_count_max > 0 && init_result != ESP_OK) {
    init_result = ledc_timer_config(&timer_conf);
    if (init_result != ESP_OK) {
      ESP_LOGW(TAG, "Unable to initialize timer with frequency %.1f and bit depth of %u", frequency, bit_depth);
      if (bit_depth <= 1) {
        break;
      }
      // try again with a lower bit depth
      timer_conf.duty_resolution = static_cast<ledc_timer_bit_t>(--bit_depth);
    }
    attempt_count_max--;
  }

  return init_result;
}

constexpr int ledc_angle_to_htop(float angle, uint8_t bit_depth) {
  return static_cast<int>(angle * ((1U << bit_depth) - 1) / 360.0f);
}

void LEDCOutput::write_state(float state) {
  if (!this->initialized_) {
    ESP_LOGW(TAG, "Not yet initialized");
    return;
  }

  if (this->pin_->is_inverted())
    state = 1.0f - state;

  this->duty_ = state;
  const uint32_t max_duty = (uint32_t(1) << this->bit_depth_) - 1;
  const float duty_rounded = roundf(state * max_duty);
  auto duty = static_cast<uint32_t>(duty_rounded);
  if (duty == this->last_duty_) {
    return;
  }

  ESP_LOGV(TAG, "Setting duty: %" PRIu32 " on channel %u", duty, this->channel_);
  auto speed_mode = get_speed_mode(this->channel_);
  auto chan_num = static_cast<ledc_channel_t>(this->channel_ % 8);
  int hpoint = ledc_angle_to_htop(this->phase_angle_, this->bit_depth_);
  if (duty == max_duty) {
    ledc_stop(speed_mode, chan_num, 1);
    this->last_duty_ = duty;
  } else if (duty == 0) {
    ledc_stop(speed_mode, chan_num, 0);
    this->last_duty_ = duty;
  } else {
#if !defined(SOC_LEDC_SUPPORT_FADE_STOP)
    if (ledc_duty_update_pending(speed_mode, chan_num)) {
      ESP_LOGV(TAG, "Skipping LEDC duty update on channel %u while previous duty_start is still set", this->channel_);
      return;
    }
#endif
    ledc_set_duty_with_hpoint(speed_mode, chan_num, duty, hpoint);
    ledc_update_duty(speed_mode, chan_num);
    this->last_duty_ = duty;
  }
}

void LEDCOutput::setup() {
  if (!ledc_peripheral_reset_done) {
    ESP_LOGV(TAG, "Resetting LEDC peripheral to clear stale state after reboot");
    periph_module_reset(PERIPH_LEDC_MODULE);
    ledc_peripheral_reset_done = true;
  }

  auto speed_mode = get_speed_mode(this->channel_);
  auto timer_num = static_cast<ledc_timer_t>((this->channel_ % 8) / 2);
  auto chan_num = static_cast<ledc_channel_t>(this->channel_ % 8);

  esp_err_t timer_init_result =
      configure_timer_frequency(speed_mode, timer_num, chan_num, this->channel_, this->bit_depth_, this->frequency_);

  if (timer_init_result != ESP_OK) {
    ESP_LOGE(TAG, "Frequency %f can't be achieved with computed bit depth %u", this->frequency_, this->bit_depth_);
    this->status_set_error();
    return;
  }
  int hpoint = ledc_angle_to_htop(this->phase_angle_, this->bit_depth_);

  ESP_LOGV(TAG, "Configured frequency %f with bit depth %u, angle %.1f° hpoint %u", this->frequency_, this->bit_depth_,
           this->phase_angle_, hpoint);

  ledc_channel_config_t chan_conf{};
  chan_conf.gpio_num = this->pin_->get_pin();
  chan_conf.speed_mode = speed_mode;
  chan_conf.channel = chan_num;
  chan_conf.intr_type = LEDC_INTR_DISABLE;
  chan_conf.timer_sel = timer_num;
  chan_conf.duty = this->inverted_ == this->pin_->is_inverted() ? 0 : (1U << this->bit_depth_);
  chan_conf.hpoint = hpoint;
  ledc_channel_config(&chan_conf);
  this->initialized_ = true;
  this->status_clear_error();
}

void LEDCOutput::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Output:\n"
                "  Channel: %u\n"
                "  PWM Frequency: %.1f Hz\n"
                "  Phase angle: %.1f°\n"
                "  Bit depth: %u",
                this->channel_, this->frequency_, this->phase_angle_, this->bit_depth_);
  LOG_PIN("  Pin ", this->pin_);
  ESP_LOGV(TAG,
           "  Max frequency for bit depth: %f\n"
           "  Min frequency for bit depth: %f\n"
           "  Max frequency for bit depth-1: %f\n"
           "  Min frequency for bit depth-1: %f\n"
           "  Max frequency for bit depth+1: %f\n"
           "  Min frequency for bit depth+1: %f\n"
           "  Max res bits: %d\n"
           "  Clock frequency: %f",
           ledc_max_frequency_for_bit_depth(this->bit_depth_),
           ledc_min_frequency_for_bit_depth(this->bit_depth_, (this->frequency_ < 100)),
           ledc_max_frequency_for_bit_depth(this->bit_depth_ - 1),
           ledc_min_frequency_for_bit_depth(this->bit_depth_ - 1, (this->frequency_ < 100)),
           ledc_max_frequency_for_bit_depth(this->bit_depth_ + 1),
           ledc_min_frequency_for_bit_depth(this->bit_depth_ + 1, (this->frequency_ < 100)), MAX_RES_BITS,
           CLOCK_FREQUENCY);
}

void LEDCOutput::update_frequency(float frequency) {
  auto bit_depth_opt = ledc_bit_depth_for_frequency(frequency);
  if (!bit_depth_opt.has_value()) {
    ESP_LOGE(TAG, "Frequency %f can't be achieved with any bit depth", this->frequency_);
    this->status_set_error();
  }
  this->bit_depth_ = bit_depth_opt.value_or(8);
  this->frequency_ = frequency;

  if (!this->initialized_) {
    ESP_LOGW(TAG, "Not yet initialized");
    return;
  }

  auto speed_mode = get_speed_mode(this->channel_);
  auto timer_num = static_cast<ledc_timer_t>((this->channel_ % 8) / 2);
  auto chan_num = static_cast<ledc_channel_t>(this->channel_ % 8);

  esp_err_t timer_init_result =
      configure_timer_frequency(speed_mode, timer_num, chan_num, this->channel_, this->bit_depth_, this->frequency_);

  if (timer_init_result != ESP_OK) {
    ESP_LOGE(TAG, "Frequency %f can't be achieved with computed bit depth %u", this->frequency_, this->bit_depth_);
    this->status_set_error();
    return;
  }

  this->status_clear_error();

  // re-apply duty
  this->last_duty_ = UINT32_MAX;
  this->write_state(this->duty_);
}

uint8_t next_ledc_channel = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::ledc

#endif
