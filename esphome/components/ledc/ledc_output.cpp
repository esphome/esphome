#include "ledc_output.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#ifdef USE_ARDUINO
#include <esp32-hal-ledc.h>
#endif
#include <driver/ledc.h>

namespace esphome {
namespace ledc {

static const char *const TAG = "ledc.output";

static const int MAX_RES_BITS = LEDC_TIMER_BIT_MAX - 1;
#ifdef USE_ESP_IDF
#if SOC_LEDC_SUPPORT_HS_MODE
// Only ESP32 has LEDC_HIGH_SPEED_MODE
inline ledc_mode_t get_speed_mode(uint8_t channel) { return channel < 8 ? LEDC_HIGH_SPEED_MODE : LEDC_LOW_SPEED_MODE; }
#else
// S2, C3, S3 only support LEDC_LOW_SPEED_MODE
// See
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/peripherals/ledc.html#functionality-overview
inline ledc_mode_t get_speed_mode(uint8_t) { return LEDC_LOW_SPEED_MODE; }
#endif
#endif

float ledc_max_frequency_for_bit_depth(uint8_t bit_depth) { return 80e6f / float(1 << bit_depth); }

float ledc_min_frequency_for_bit_depth(uint8_t bit_depth, bool low_frequency) {
  const float max_div_num = ((1 << MAX_RES_BITS) - 1) / (low_frequency ? 32.0f : 256.0f);
  return 80e6f / (max_div_num * float(1 << bit_depth));
}

optional<uint8_t> ledc_bit_depth_for_frequency(float frequency) {
  ESP_LOGD(TAG, "Calculating resolution bit-depth for frequency %f", frequency);
  for (int i = MAX_RES_BITS; i >= 1; i--) {
    const float min_frequency = ledc_min_frequency_for_bit_depth(i, (frequency < 100));
    const float max_frequency = ledc_max_frequency_for_bit_depth(i);
    if (min_frequency <= frequency && frequency <= max_frequency) {
      ESP_LOGD(TAG, "Resolution calculated as %d", i);
      return i;
    }
  }
  return {};
}

void LEDCOutput::write_state(float state) {
  if (!initialized_) {
    ESP_LOGW(TAG, "LEDC output hasn't been initialized yet!");
    return;
  }

  if (this->pin_->is_inverted())
    state = 1.0f - state;

  this->duty_ = state;
  const uint32_t max_duty = (uint32_t(1) << this->bit_depth_) - 1;
  const float duty_rounded = roundf(state * max_duty);
  auto duty = static_cast<uint32_t>(duty_rounded);

#ifdef USE_ARDUINO
  ledcWrite(this->channel_, duty);
#endif
#ifdef USE_ESP_IDF
  auto speed_mode = get_speed_mode(channel_);
  auto chan_num = static_cast<ledc_channel_t>(channel_ % 8);
  ledc_set_duty(speed_mode, chan_num, duty);
  ledc_update_duty(speed_mode, chan_num);
#endif
}

void LEDCOutput::setup() {
#ifdef USE_ARDUINO
  this->update_frequency(this->frequency_);
  this->turn_off();
  // Attach pin after setting default value
  ledcAttachPin(this->pin_->get_pin(), this->channel_);
#endif
#ifdef USE_ESP_IDF
  auto speed_mode = get_speed_mode(channel_);
  auto timer_num = static_cast<ledc_timer_t>((channel_ % 8) / 2);
  auto chan_num = static_cast<ledc_channel_t>(channel_ % 8);

  bit_depth_ = *ledc_bit_depth_for_frequency(frequency_);
  if (bit_depth_ < 1) {
    ESP_LOGW(TAG, "Frequency %f can't be achieved with any bit depth", frequency_);
    this->status_set_warning();
  }

  ledc_timer_config_t timer_conf{};
  timer_conf.speed_mode = speed_mode;
  timer_conf.duty_resolution = static_cast<ledc_timer_bit_t>(bit_depth_);
  timer_conf.timer_num = timer_num;
  timer_conf.freq_hz = (uint32_t) frequency_;
  timer_conf.clk_cfg = LEDC_AUTO_CLK;
  ledc_timer_config(&timer_conf);

  ledc_channel_config_t chan_conf{};
  chan_conf.gpio_num = pin_->get_pin();
  chan_conf.speed_mode = speed_mode;
  chan_conf.channel = chan_num;
  chan_conf.intr_type = LEDC_INTR_DISABLE;
  chan_conf.timer_sel = timer_num;
  chan_conf.duty = inverted_ == pin_->is_inverted() ? 0 : (1U << bit_depth_);
  chan_conf.hpoint = 0;
  ledc_channel_config(&chan_conf);
  initialized_ = true;
#endif
}

void LEDCOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "LEDC Output:");
  LOG_PIN("  Pin ", this->pin_);
  ESP_LOGCONFIG(TAG, "  LEDC Channel: %u", this->channel_);
  ESP_LOGCONFIG(TAG, "  Frequency: %.1f Hz", this->frequency_);
}

void LEDCOutput::update_frequency(float frequency) {
  auto bit_depth_opt = ledc_bit_depth_for_frequency(frequency);
  if (!bit_depth_opt.has_value()) {
    ESP_LOGW(TAG, "Frequency %f can't be achieved with any bit depth", frequency);
    this->status_set_warning();
  }
  this->bit_depth_ = bit_depth_opt.value_or(8);
  this->frequency_ = frequency;
#ifdef USE_ARDUINO
  ledcSetup(this->channel_, frequency, this->bit_depth_);
  initialized_ = true;
#endif  // USE_ARDUINO
#ifdef USE_ESP_IDF
  if (!initialized_) {
    ESP_LOGW(TAG, "LEDC output hasn't been initialized yet!");
    return;
  }
  auto speed_mode = get_speed_mode(channel_);
  auto timer_num = static_cast<ledc_timer_t>((channel_ % 8) / 2);

  ledc_timer_config_t timer_conf{};
  timer_conf.speed_mode = speed_mode;
  timer_conf.duty_resolution = static_cast<ledc_timer_bit_t>(bit_depth_);
  timer_conf.timer_num = timer_num;
  timer_conf.freq_hz = (uint32_t) frequency_;
  timer_conf.clk_cfg = LEDC_AUTO_CLK;
  ledc_timer_config(&timer_conf);
#endif
  // re-apply duty
  this->write_state(this->duty_);
}

uint8_t next_ledc_channel = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace ledc
}  // namespace esphome

#endif
