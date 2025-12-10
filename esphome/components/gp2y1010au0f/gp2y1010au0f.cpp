#include "gp2y1010au0f.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome {
namespace gp2y1010au0f {

static const char *const TAG = "gp2y1010au0f";
static const float MIN_VOLTAGE = 0.0f;
static const float MAX_VOLTAGE = 4.0f;

void GP2Y1010AU0FSensor::dump_config() {
  LOG_SENSOR("", "Sharp GP2Y1010AU0F PM2.5 Sensor", this);
  ESP_LOGCONFIG(TAG,
                "  Sampling duration: %" PRId32 " ms\n"
                "  ADC voltage multiplier: %.3f\n"
                "  Wait before sampling: %" PRId32 " µs\n"
                "  Wait after sampling: %" PRId32 " µs\n"
                "  Wait after LED off: %" PRId32 " µs",
                this->sample_duration_, 
                this->voltage_multiplier_,
                this->sample_wait_before_,
                this->sample_wait_after_,
                this->sample_wait_off_);
  LOG_UPDATE_INTERVAL(this);
}

void GP2Y1010AU0FSensor::update() {
  is_sampling_ = true;

  this->set_timeout("read", this->sample_duration_, [this]() {
    this->is_sampling_ = false;
    if (this->num_samples_ == 0)
      return;

    float mean = this->sample_sum_ / float(this->num_samples_);
    ESP_LOGD(TAG, "ADC read voltage: %.3f V (mean from %" PRId32 " samples)", mean, this->num_samples_);

    // PM2.5 calculation
    // ref: https://www.howmuchsnow.com/arduino/airquality/
    int16_t pm_2_5_value = 170 * mean;
    this->publish_state(pm_2_5_value);
  });

  // reset readings
  this->num_samples_ = 0;
  this->sample_sum_ = 0.0f;
}

void GP2Y1010AU0FSensor::loop() {
  if (!this->is_sampling_)
    return;

  // Sharp GP2Y1010AU0F Datasheet Timing Sequence:
  // 1. LED ON
  // 2. Wait 280µs (sensor stabilization)
  // 3. Sample ADC
  // 4. Wait 40µs (reading stabilization)
  // 5. LED OFF
  // 6. Wait 9680µs (complete pulse cycle = 10ms total)

  // Step 1: Enable the internal IR LED
  this->led_output_->turn_on();
  
  // Step 2: Wait for the sensor to stabilize (280µs per datasheet)
  delayMicroseconds(this->sample_wait_before_);
  
  // Step 3: Perform a single ADC sample
  float read_voltage = this->source_->sample();
  
  // Step 4: Wait for reading to stabilize (40µs per datasheet)
  delayMicroseconds(this->sample_wait_after_);
  
  // Step 5: Disable the internal IR LED
  this->led_output_->turn_off();
  
  // Step 6: Wait for LED to fully turn off and complete pulse cycle (9680µs per datasheet)
  // Total pulse cycle: 280 + 40 + 9680 = 10000µs = 10ms
  delayMicroseconds(this->sample_wait_off_);

  // Validate and accumulate reading
  if (std::isnan(read_voltage))
    return;
  
  read_voltage = read_voltage * this->voltage_multiplier_ - this->voltage_offset_;
  
  if (read_voltage < MIN_VOLTAGE || read_voltage > MAX_VOLTAGE)
    return;

  this->num_samples_++;
  this->sample_sum_ += read_voltage;
}

}  // namespace gp2y1010au0f
}  // namespace esphome
