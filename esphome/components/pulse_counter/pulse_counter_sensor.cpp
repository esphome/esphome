#include "pulse_counter_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pulse_counter {

static const char *const TAG = "pulse_counter";

const char *const EDGE_MODE_TO_STRING[] = {"DISABLE", "INCREMENT", "DECREMENT"};

#ifdef ARDUINO_ARCH_ESP8266
void ICACHE_RAM_ATTR PulseCounterStorage::gpio_intr(PulseCounterStorage *arg) {
  const uint32_t now = micros();
  const bool discard = now - arg->last_pulse < arg->filter_us;
  arg->last_pulse = now;
  if (discard)
    return;

  PulseCounterCountMode mode = arg->isr_pin->digital_read() ? arg->rising_edge_mode : arg->falling_edge_mode;
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
bool PulseCounterStorage::pulse_counter_setup(GPIOPin *pin) {
  this->pin = pin;
  this->pin->setup();
  this->isr_pin = this->pin->to_isr();
  this->pin->attach_interrupt(PulseCounterStorage::gpio_intr, this, CHANGE);
  return true;
}
pulse_counter_t PulseCounterStorage::read_raw_value() {
  pulse_counter_t counter = this->counter;
  pulse_counter_t ret = counter - this->last_value;
  this->last_value = counter;
  return ret;
}
#endif

#ifdef ARDUINO_ARCH_ESP32
bool PulseCounterStorage::pulse_counter_setup(GPIOPin *pin) {
  this->pin = pin;
  this->pin->setup();
  this->pcnt_unit = next_pcnt_unit;
  next_pcnt_unit = pcnt_unit_t(int(next_pcnt_unit) + 1);  // NOLINT

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
    uint16_t filter_val = std::min(this->filter_us * 80u, 1023u);
    ESP_LOGCONFIG(TAG, "    Filter Value: %uus (val=%u)", this->filter_us, filter_val);
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
pulse_counter_t PulseCounterStorage::read_raw_value() {
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

void PulseCounterSensor::dump_config() {
  LOG_SENSOR("", "Pulse Counter", this);
  LOG_PIN("  Pin: ", this->pin_);
  ESP_LOGCONFIG(TAG, "  Rising Edge: %s", EDGE_MODE_TO_STRING[this->storage_.rising_edge_mode]);
  ESP_LOGCONFIG(TAG, "  Falling Edge: %s", EDGE_MODE_TO_STRING[this->storage_.falling_edge_mode]);
  ESP_LOGCONFIG(TAG, "  Filtering pulses shorter than %u µs", this->storage_.filter_us);
  LOG_UPDATE_INTERVAL(this);
}

void PulseCounterSensor::update() {
  pulse_counter_t raw = this->storage_.read_raw_value();
  float value = (60000.0f * raw) / float(this->get_update_interval());  // per minute

  ESP_LOGD(TAG, "'%s': Retrieved counter: %0.2f pulses/min", this->get_name().c_str(), value);
  this->publish_state(value);

  if (this->total_sensor_ != nullptr) {
    current_total_ += raw;
    ESP_LOGD(TAG, "'%s': Total : %i pulses", this->get_name().c_str(), current_total_);
    this->total_sensor_->publish_state(current_total_);
  }
}

#ifdef ARDUINO_ARCH_ESP32
pcnt_unit_t next_pcnt_unit = PCNT_UNIT_0;
#endif

}  // namespace pulse_counter
}  // namespace esphome
