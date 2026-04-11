#include "pulse_counter_ulp_sensor.h"
#include "driver/rtc_io.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "soc/rtc_periph.h"
#include "ulp_main.h"
#include <esp32/ulp.h>
#include <esp_sleep.h>

namespace esphome {
namespace pulse_counter_ulp {

static const char *const TAG = "pulse_counter_ulp";

namespace {
const char *countmode_to_string(CountMode count_mode) {
  switch (count_mode) {
    case CountMode::DISABLE:
      return "disable";
    case CountMode::INCREMENT:
      return "increment";
    case CountMode::DECREMENT:
      return "decrement";
  }
  return "UNKNOWN MODE";
}
}  // namespace

// These identifiers are defined by ESP-IDF and we don't have control over them
// NOLINTBEGIN(readability-identifier-naming)
extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[] asm("_binary_ulp_main_bin_end");
// NOLINTEND(readability-identifier-naming)

std::unique_ptr<UlpProgram> UlpProgram::start(const Config &config) {
  esp_err_t error = ulp_load_binary(0, ulp_main_bin_start, (ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t));
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "Loading ULP binary failed: %s", esp_err_to_name(error));
    return nullptr;
  }

  /* GPIO used for pulse counting. */
  gpio_num_t gpio_num = static_cast<gpio_num_t>(config.pin_->get_pin());
  int rtcio_num = rtc_io_number_get(gpio_num);
  if (!rtc_gpio_is_valid_gpio(gpio_num)) {
    ESP_LOGE(TAG, "GPIO used for pulse counting must be an RTC IO");
  }

  /* Initialize variables in ULP program.
   * Note that the ULP reads only the lower 16 bits of these variables.  */
  ulp_rising_edge_en = config.rising_edge_mode_ != CountMode::DISABLE;
  ulp_rising_edge_count = 0;
  ulp_falling_edge_en = config.falling_edge_mode_ != CountMode::DISABLE;
  ulp_falling_edge_count = 0;
  ulp_debounce_counter = 3;
  ulp_edges_wakeup = config.edges_wakeup_ > 0 ? config.edges_wakeup_ : std::numeric_limits<uint16_t>::max();
  ulp_debounce_max_count = config.debounce_;
  ulp_next_edge = static_cast<uint16_t>(!config.pin_->digital_read());
  ulp_io_number = rtcio_num; /* map from GPIO# to RTC_IO# */

  /* If pin is inverted, we need to swap activating detection of rising / falling edges */
  if (config.pin_->is_inverted()) {
    std::swap(ulp_rising_edge_en, ulp_falling_edge_en);
  }

  /* Initialize selected GPIO as RTC IO, enable input */
  rtc_gpio_init(gpio_num);
  rtc_gpio_set_direction(gpio_num, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_hold_en(gpio_num);

  /* Minimum pulse width is sleep_duration_ * (ulp_debounce_counter + 1). */
  ulp_set_wakeup_period(0, config.sleep_duration_ / std::chrono::microseconds{1});

  /* Start the program */
  error = ulp_run(&ulp_entry - RTC_SLOW_MEM);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "Starting ULP program failed: %s", esp_err_to_name(error));
    return nullptr;
  }

  return make_unique<UlpProgram>();
}

UlpProgram::State UlpProgram::pop_state() {
  State state = UlpProgram::peek_state();
  ulp_rising_edge_count = 0;
  ulp_falling_edge_count = 0;
  return state;
}

UlpProgram::State UlpProgram::peek_state() const {
  auto rising_edge_count = static_cast<uint16_t>(ulp_rising_edge_count);
  auto falling_edge_count = static_cast<uint16_t>(ulp_falling_edge_count);
  return {.rising_edge_count_ = rising_edge_count, .falling_edge_count_ = falling_edge_count};
}

RTC_DATA_ATTR int PulseCounterUlpSensor::pulse_count_persist = 0;
RTC_DATA_ATTR clock::time_point PulseCounterUlpSensor::last_time{};

float PulseCounterUlpSensor::get_setup_priority() const {
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) {
    return setup_priority::DATA;
  } else {
    /* In case of deep sleep wakup, we need to delay sending new values until we have a connection */
    return setup_priority::AFTER_CONNECTION;
  }
}

void PulseCounterUlpSensor::setup() {
  ESP_LOGCONFIG(TAG, "Running setup for '%s'", this->get_name().c_str());

  this->config_.pin_->setup();

  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) {
    ESP_LOGD(TAG, "Did not wake up from sleep, assuming restart or first boot and setting up ULP program");
    this->storage_ = UlpProgram::start(this->config_);
    last_time = clock::now();
  } else {
    ESP_LOGD(TAG, "Woke up from sleep, skipping set-up of ULP program");
    this->storage_ = make_unique<UlpProgram>();
  }

  /* Enable wakeup from ulp */
  if (this->config_.edges_wakeup_ > 0) {
    ESP_ERROR_CHECK(esp_sleep_enable_ulp_wakeup());
  }

  if (!this->storage_) {
    this->mark_failed();
    return;
  }
}

void PulseCounterUlpSensor::set_total_pulses(uint32_t pulses) {
  if (this->total_sensor_ != nullptr) {
    ESP_LOGD(TAG, "'%s': Reset pulses of total count to pulses: %d", this->get_name().c_str(), pulses);

    pulse_count_persist = std::max(static_cast<int>(pulses), 0);
    this->total_sensor_->publish_state(pulses);
  }
}

void PulseCounterUlpSensor::dump_config() {
  LOG_SENSOR("", "Pulse Counter", this);
  LOG_PIN("  Pin: ", this->config_.pin_);
  ESP_LOGCONFIG(TAG,
                "  Rising Edge: %s\n"
                "  Falling Edge: %s\n"
                "  Sleep Duration: %" PRIu32 " µs\n"
                "  Debounce: %" PRIu16 "\n"
                "  Edges Wakeup: %" PRIu16,
                countmode_to_string(this->config_.rising_edge_mode_),
                countmode_to_string(this->config_.falling_edge_mode_), this->config_.sleep_duration_ / microseconds{1},
                this->config_.debounce_, this->config_.edges_wakeup_);
  LOG_UPDATE_INTERVAL(this);
}

void PulseCounterUlpSensor::update() {
  // Can't update if ulp program hasn't been initialised
  if (!this->storage_) {
    return;
  }
  UlpProgram::State raw = this->storage_->pop_state();
  // Since ULP can't use the GPIOPin abstraction, pin inversion needs to be
  // manually implemented.
  int32_t pulse_count_ulp;
  if (this->config_.pin_->is_inverted()) {
    pulse_count_ulp = static_cast<int32_t>(config_.rising_edge_mode_) * raw.falling_edge_count_ +
                      static_cast<int32_t>(config_.falling_edge_mode_) * raw.rising_edge_count_;
  } else {
    pulse_count_ulp = static_cast<int32_t>(config_.rising_edge_mode_) * raw.rising_edge_count_ +
                      static_cast<int32_t>(config_.falling_edge_mode_) * raw.falling_edge_count_;
  }

  clock::time_point now = clock::now();
  clock::duration interval = now - last_time;
  last_time = now;

  if (interval > clock::duration::zero()) {
    float value = std::chrono::minutes{1} * static_cast<float>(pulse_count_ulp) / interval;  // pulses per minute
    ESP_LOGD(TAG, "'%s': Retrieved counter: %d pulses at %0.2f pulses/min", this->get_name().c_str(), pulse_count_ulp,
             value);
    this->publish_state(value);
  }

  if (this->total_sensor_ != nullptr) {
    // Get new overall value
    const int32_t pulse_count = pulse_count_persist + pulse_count_ulp;

    // Update persistent counter
    pulse_count_persist = (int) pulse_count;

    // publish new state
    ESP_LOGD(TAG, "'%s': Pulses from ULP: %d; Overall pulses: %d", this->get_name().c_str(), pulse_count_ulp,
             pulse_count);

    this->total_sensor_->publish_state(pulse_count);
  }
}

}  // namespace pulse_counter_ulp
}  // namespace esphome
