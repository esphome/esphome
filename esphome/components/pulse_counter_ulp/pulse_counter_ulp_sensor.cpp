#include "pulse_counter_ulp_sensor.h"
#include "esphome/core/log.h"
#include "esp32/ulp.h"
#include "ulp_main.h"
#include "soc/rtc_periph.h"
#include "driver/rtc_io.h"
#include <esp_sleep.h>

namespace esphome {
namespace pulse_counter {

static const char *const TAG = "pulse_counter_ulp";

const char *const EDGE_MODE_TO_STRING[] = {"DISABLE", "INCREMENT", "DECREMENT"};

/* === ULP === */

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[] asm("_binary_ulp_main_bin_end");

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

  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) {
    ESP_LOGD(TAG, "Did not wake up from sleep, assuming restart or first boot and setting up ULP program");
  } else {
    ESP_LOGD(TAG, "Woke up from sleep, skipping set-up of ULP program");
    return true;
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

/* === END ULP ===*/

void PulseCounterSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up pulse counter '%s'...", this->name_.c_str());
  if (!this->storage_.pulse_counter_setup(this->pin_)) {
    this->mark_failed();
    return;
  }
#ifdef CONF_USE_TIME
  this->time_id_->add_on_time_sync_callback([this]() {
    this->time_is_synchronized_ = true;
    this->update();
  });
  this->pref_ = global_preferences->make_preference<timestamp_t>(this->get_object_id_hash());
  this->pref_.load(&this->last_time_);
#endif
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
#ifdef CONF_USE_TIME
  // Can't clear the pulse count until we can report the rate, so there's
  // nothing to do until the time is synchronized
  if (!time_is_synchronized_) {
    return;
  }
#endif

  pulse_counter_t raw = this->storage_.read_raw_value();
  timestamp_t now;
  timestamp_t interval;
#ifdef CONF_USE_TIME
  // Convert to ms to match units when not using a Time component.
  now = this->time_id_->timestamp_now() * 1000;
#else
  now = millis();
#endif
  interval = now - this->last_time_;
  if (this->last_time_ != 0) {
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
#ifdef CONF_USE_TIME
  this->pref_.save(&this->last_time_);
#endif
}

}  // namespace pulse_counter
}  // namespace esphome
