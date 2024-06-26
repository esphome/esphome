#include "pulse_counter_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pulse_counter {

static const char *const TAG = "pulse_counter";

const char *const EDGE_MODE_TO_STRING[] = {"DISABLE", "INCREMENT", "DECREMENT"};

#ifdef HAS_PCNT
PulseCounterStorageBase *get_storage(bool hw_pcnt) {
  return (hw_pcnt ? (PulseCounterStorageBase *) (new HwPulseCounterStorage)
                  : (PulseCounterStorageBase *) (new BasicPulseCounterStorage));
}
#else
PulseCounterStorageBase *get_storage(bool) { return new BasicPulseCounterStorage; }
#endif

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
    case PULSE_COUNTER_INCREMENT:
      arg->counter++;
      break;
    case PULSE_COUNTER_DECREMENT:
      arg->counter--;
      break;
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
  static pcnt_unit_t next_pcnt_unit = PCNT_UNIT_0;
  this->pin = pin;
  this->pin->setup();
  this->pcnt_unit = next_pcnt_unit;
  next_pcnt_unit = pcnt_unit_t(int(next_pcnt_unit) + 1);

  ESP_LOGCONFIG(TAG, "    PCNT Unit Number: %u", this->pcnt_unit);

  pcnt_count_mode_t rising = PCNT_COUNT_DIS, falling = PCNT_COUNT_DIS;
  switch (this->rising_edge_mode) {
    case PULSE_COUNTER_DISABLE:
      rising = PCNT_COUNT_DIS;
      break;
    case PULSE_COUNTER_INCREMENT:
      rising = PCNT_COUNT_INC;
      break;
    case PULSE_COUNTER_DECREMENT:
      rising = PCNT_COUNT_DEC;
      break;
  }
  switch (this->falling_edge_mode) {
    case PULSE_COUNTER_DISABLE:
      falling = PCNT_COUNT_DIS;
      break;
    case PULSE_COUNTER_INCREMENT:
      falling = PCNT_COUNT_INC;
      break;
    case PULSE_COUNTER_DECREMENT:
      falling = PCNT_COUNT_DEC;
      break;
  }

  pcnt_config_t pcnt_config = {
      .pulse_gpio_num = this->pin->get_pin(),
      .ctrl_gpio_num = PCNT_PIN_NOT_USED,
      .lctrl_mode = PCNT_MODE_KEEP,
      .hctrl_mode = PCNT_MODE_KEEP,
      .pos_mode = rising,
      .neg_mode = falling,
      .counter_h_lim = 0,
      .counter_l_lim = 0,
      .unit = this->pcnt_unit,
      .channel = PCNT_CHANNEL_0,
  };
  esp_err_t error = pcnt_unit_config(&pcnt_config);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "Configuring Pulse Counter failed: %s", esp_err_to_name(error));
    return false;
  }

  if (this->filter_us != 0) {
    uint16_t filter_val = std::min(static_cast<unsigned int>(this->filter_us * 80u), 1023u);
    ESP_LOGCONFIG(TAG, "    Filter Value: %" PRIu32 "us (val=%u)", this->filter_us, filter_val);
    error = pcnt_set_filter_value(this->pcnt_unit, filter_val);
    if (error != ESP_OK) {
      ESP_LOGE(TAG, "Setting filter value failed: %s", esp_err_to_name(error));
      return false;
    }
    error = pcnt_filter_enable(this->pcnt_unit);
    if (error != ESP_OK) {
      ESP_LOGE(TAG, "Enabling filter failed: %s", esp_err_to_name(error));
      return false;
    }
  }

  error = pcnt_counter_pause(this->pcnt_unit);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "Pausing pulse counter failed: %s", esp_err_to_name(error));
    return false;
  }
  error = pcnt_counter_clear(this->pcnt_unit);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "Clearing pulse counter failed: %s", esp_err_to_name(error));
    return false;
  }
  error = pcnt_counter_resume(this->pcnt_unit);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "Resuming pulse counter failed: %s", esp_err_to_name(error));
    return false;
  }
  return true;
}
pulse_counter_t HwPulseCounterStorage::read_raw_value() {
  pulse_counter_t counter;
  pcnt_get_counter_value(this->pcnt_unit, &counter);
  pulse_counter_t ret = counter - this->last_value;
  this->last_value = counter;
  return ret;
}
#endif

void PulseCounterSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up pulse counter '%s'...", this->name_.c_str());
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
  ESP_LOGCONFIG(TAG, "  Rising Edge: %s", EDGE_MODE_TO_STRING[this->storage_.rising_edge_mode]);
  ESP_LOGCONFIG(TAG, "  Falling Edge: %s", EDGE_MODE_TO_STRING[this->storage_.falling_edge_mode]);
  ESP_LOGCONFIG(TAG, "  Filtering pulses shorter than %" PRIu32 " µs", this->storage_.filter_us);
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
