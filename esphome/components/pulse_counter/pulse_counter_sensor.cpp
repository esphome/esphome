#include "pulse_counter_sensor.h"
#include "esphome/core/log.h"

#ifdef HAS_PCNT
#include <esp_private/esp_clk.h>
#include <hal/pcnt_ll.h>
#endif

namespace esphome {
namespace pulse_counter {

static const char *const TAG = "pulse_counter";

const char *const EDGE_MODE_TO_STRING[] = {"DISABLE", "INCREMENT", "DECREMENT"};

#ifdef HAS_PCNT
PulseCounterStorageBase *get_storage(bool hw_pcnt) {
  return (hw_pcnt ? (PulseCounterStorageBase *) (new HwPulseCounterStorage)
                  : (PulseCounterStorageBase *) (new BasicPulseCounterStorage));
}
#else   // HAS_PCNT
PulseCounterStorageBase *get_storage(bool) { return new BasicPulseCounterStorage; }
#endif  // HAS_PCNT

void IRAM_ATTR BasicPulseCounterStorage::gpio_intr(BasicPulseCounterStorage *arg) {
  const uint32_t now = micros();
  const bool discard = now - arg->last_pulse < arg->filter_us;
  arg->last_pulse = now;
  if (discard)
    return;

  PulseCounterCountMode mode = arg->isr_pin.digital_read() ? arg->rising_edge_mode : arg->falling_edge_mode;
  switch (mode) {
    case PULSE_COUNTER_DISABLE:
      break;
    case PULSE_COUNTER_INCREMENT: {
      auto x = arg->counter + 1;
      arg->counter = x;
    } break;
    case PULSE_COUNTER_DECREMENT: {
      auto x = arg->counter - 1;
      arg->counter = x;
    } break;
  }
}

bool BasicPulseCounterStorage::pulse_counter_setup(InternalGPIOPin *pin) {
  this->pin = pin;
  this->pin->setup();
  this->isr_pin = this->pin->to_isr();
  this->pin->attach_interrupt(BasicPulseCounterStorage::gpio_intr, this, gpio::INTERRUPT_ANY_EDGE);
  return true;
}

pulse_counter_t BasicPulseCounterStorage::read_raw_value() {
  pulse_counter_t counter = this->counter;
  pulse_counter_t ret = counter - this->last_value;
  this->last_value = counter;
  return ret;
}

#ifdef HAS_PCNT
bool HwPulseCounterStorage::pulse_counter_setup(InternalGPIOPin *pin) {
  this->pin = pin;
  this->pin->setup();

  pcnt_unit_config_t unit_config = {
      .low_limit = INT16_MIN,
      .high_limit = INT16_MAX,
      .flags = {.accum_count = true},
  };
  esp_err_t error = pcnt_new_unit(&unit_config, &this->pcnt_unit);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "Creating PCNT unit failed: %s", esp_err_to_name(error));
    return false;
  }

  pcnt_chan_config_t chan_config = {
      .edge_gpio_num = this->pin->get_pin(),
      .level_gpio_num = -1,
  };
  error = pcnt_new_channel(this->pcnt_unit, &chan_config, &this->pcnt_channel);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "Creating PCNT channel failed: %s", esp_err_to_name(error));
    return false;
  }

  pcnt_channel_edge_action_t rising = PCNT_CHANNEL_EDGE_ACTION_HOLD;
  pcnt_channel_edge_action_t falling = PCNT_CHANNEL_EDGE_ACTION_HOLD;
  switch (this->rising_edge_mode) {
    case PULSE_COUNTER_DISABLE:
      rising = PCNT_CHANNEL_EDGE_ACTION_HOLD;
      break;
    case PULSE_COUNTER_INCREMENT:
      rising = PCNT_CHANNEL_EDGE_ACTION_INCREASE;
      break;
    case PULSE_COUNTER_DECREMENT:
      rising = PCNT_CHANNEL_EDGE_ACTION_DECREASE;
      break;
  }
  switch (this->falling_edge_mode) {
    case PULSE_COUNTER_DISABLE:
      falling = PCNT_CHANNEL_EDGE_ACTION_HOLD;
      break;
    case PULSE_COUNTER_INCREMENT:
      falling = PCNT_CHANNEL_EDGE_ACTION_INCREASE;
      break;
    case PULSE_COUNTER_DECREMENT:
      falling = PCNT_CHANNEL_EDGE_ACTION_DECREASE;
      break;
  }

  error = pcnt_channel_set_edge_action(this->pcnt_channel, rising, falling);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "Setting PCNT edge action failed: %s", esp_err_to_name(error));
    return false;
  }

  if (this->filter_us != 0) {
    uint32_t max_glitch_ns = PCNT_LL_MAX_GLITCH_WIDTH * 1000u / ((uint32_t) esp_clk_apb_freq() / 1000000u);
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = std::min(this->filter_us * 1000u, max_glitch_ns),
    };
    error = pcnt_unit_set_glitch_filter(this->pcnt_unit, &filter_config);
    if (error != ESP_OK) {
      ESP_LOGE(TAG, "Setting PCNT glitch filter failed: %s", esp_err_to_name(error));
      return false;
    }
  }

  error = pcnt_unit_add_watch_point(this->pcnt_unit, INT16_MIN);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "Adding PCNT low limit watch point failed: %s", esp_err_to_name(error));
    return false;
  }
  error = pcnt_unit_add_watch_point(this->pcnt_unit, INT16_MAX);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "Adding PCNT high limit watch point failed: %s", esp_err_to_name(error));
    return false;
  }

  error = pcnt_unit_enable(this->pcnt_unit);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "Enabling PCNT unit failed: %s", esp_err_to_name(error));
    return false;
  }
  error = pcnt_unit_clear_count(this->pcnt_unit);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "Clearing PCNT unit failed: %s", esp_err_to_name(error));
    return false;
  }
  error = pcnt_unit_start(this->pcnt_unit);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "Starting PCNT unit failed: %s", esp_err_to_name(error));
    return false;
  }
  return true;
}

pulse_counter_t HwPulseCounterStorage::read_raw_value() {
  int count;
  pcnt_unit_get_count(this->pcnt_unit, &count);
  pulse_counter_t ret = count - this->last_value;
  this->last_value = count;
  return ret;
}
#endif  // HAS_PCNT

void PulseCounterSensor::setup() {
  if (!this->storage_.pulse_counter_setup(this->pin_)) {
    this->mark_failed();
    return;
  }
}

void PulseCounterSensor::set_total_pulses(uint32_t pulses) {
  this->current_total_ = pulses;
  this->total_sensor_->publish_state(pulses);
}

void PulseCounterSensor::dump_config() {
  LOG_SENSOR("", "Pulse Counter", this);
  LOG_PIN("  Pin: ", this->pin_);
  ESP_LOGCONFIG(TAG,
                "  Rising Edge: %s\n"
                "  Falling Edge: %s\n"
                "  Filtering pulses shorter than %" PRIu32 " µs",
                EDGE_MODE_TO_STRING[this->storage_.rising_edge_mode],
                EDGE_MODE_TO_STRING[this->storage_.falling_edge_mode], this->storage_.filter_us);
  LOG_UPDATE_INTERVAL(this);
}

void PulseCounterSensor::update() {
  pulse_counter_t raw = this->storage_.read_raw_value();
  uint32_t now = millis();
  if (this->last_time_ != 0) {
    uint32_t interval = now - this->last_time_;
    float value = (60000.0f * raw) / float(interval);  // per minute
    ESP_LOGD(TAG, "'%s': Retrieved counter: %0.2f pulses/min", this->get_name().c_str(), value);
    this->publish_state(value);
  }

  if (this->total_sensor_ != nullptr) {
    current_total_ += raw;
    ESP_LOGD(TAG, "'%s': Total : %" PRIu32 " pulses", this->get_name().c_str(), current_total_);
    this->total_sensor_->publish_state(current_total_);
  }
  this->last_time_ = now;
}

}  // namespace pulse_counter
}  // namespace esphome
