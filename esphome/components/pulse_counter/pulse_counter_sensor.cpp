#include "pulse_counter_sensor.h"
#include "esphome/core/log.h"
#ifdef CONF_USE_ULP
#include "esp32/ulp.h"
#include "ulp_main.h"
#include "soc/rtc_periph.h"
#include "driver/rtc_io.h"
#endif

namespace esphome {
namespace pulse_counter {

static const char *const TAG = "pulse_counter";

const char *const EDGE_MODE_TO_STRING[] = {"DISABLE", "INCREMENT", "DECREMENT"};

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

/* === ULP === */

#ifdef CONF_USE_ULP

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");


bool UlpPulseCounterStorage::pulse_counter_setup(InternalGPIOPin *pin) {
  this->pin = pin;
  this->pin->setup();

  uint32_t rising = 0;
  uint32_t falling = 0;
  switch (this->rising_edge_mode) {
    case PULSE_COUNTER_DISABLE:
      rising = 0;
      break;
    case PULSE_COUNTER_INCREMENT:
      rising = +1;
      break;
    case PULSE_COUNTER_DECREMENT:
      rising = -1;
      break;
  }
  switch (this->falling_edge_mode) {
    case PULSE_COUNTER_DISABLE:
      falling = 0;
      break;
    case PULSE_COUNTER_INCREMENT:
      falling = +1;
      break;
    case PULSE_COUNTER_DECREMENT:
      falling = -1;
      break;
  }

  esp_err_t error = ulp_load_binary(0, ulp_main_bin_start, (ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t));
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "Loading ULP binary failed: %s", esp_err_to_name(error));
    return false;
  }

  /* GPIO used for pulse counting. */
  gpio_num_t gpio_num = static_cast<gpio_num_t>(pin->get_pin());
  int rtcio_num = rtc_io_number_get(gpio_num);
  if (!rtc_gpio_is_valid_gpio(gpio_num)) {
    ESP_LOGE(TAG, "GPIO used for pulse counting must be an RTC IO");
  }

  /* Initialize some variables used by ULP program.
   * Each 'ulp_xyz' variable corresponds to 'xyz' variable in the ULP program.
   * These variables are declared in an auto generated header file,
   * 'ulp_main.h', name of this file is defined in component.mk as ULP_APP_NAME.
   * These variables are located in RTC_SLOW_MEM and can be accessed both by the
   * ULP and the main CPUs.
   *
   * Note that the ULP reads only the lower 16 bits of these variables.
   */
  ulp_debounce_counter = 3;
  ulp_debounce_max_count = 3;
  ulp_next_edge = 0;
  ulp_io_number = rtcio_num; /* map from GPIO# to RTC_IO# */
  ulp_edge_count_to_wake_up = 10;

  /* Initialize selected GPIO as RTC IO, enable input */
  rtc_gpio_init(gpio_num);
  rtc_gpio_set_direction(gpio_num, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_hold_en(gpio_num);

  /* Set ULP wake up period to T = 20ms.
   * Minimum pulse width has to be T * (ulp_debounce_counter + 1) = 80ms.
   */
  ulp_set_wakeup_period(0, 20000);

  /* Start the program */
  error = ulp_run(&ulp_entry - RTC_SLOW_MEM);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "Starting ULP program failed: %s", esp_err_to_name(error));
    return false;
  }

  // TODO Support Filter

  return true;
}

pulse_counter_t UlpPulseCounterStorage::read_raw_value() {
  // TODO count edges separately
  uint32_t count = ulp_edge_count;
  ulp_edge_count = 0;
  return count;
}

#endif

/* === END ULP ===*/

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
