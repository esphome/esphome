#include "hx711.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hx711 {

static const char *const TAG = "hx711";

/// Names for timeouts used in HX711 sensor component.

static const char *const TIMEOUT_NAME_SETTLE = "settle";
static const char *const TIMEOUT_NAME_MEASUREMENT_READY = "dout_ready";

/// Log messages

static const char *const LOG_STR_NOT_SETTLED = "not settled";
static const char *const LOG_STR_NOT_READY = "not ready";
static const char *const LOG_STR_POWERED_DOWN = "powered down";
static const char *const LOG_STR_POWERED_UP = "powered up";

/// @brief Converts HX711 gain enum to its corresponding numeric gain value.
/// @param[in] gain Gain setting as an HX711Gain enum.
/// @return Numeric gain value (128, 64, 32, or 0 if invalid).
constexpr uint8_t hx711_gain_to_linear_gain(const HX711Gain gain) {
  return (gain == HX711Gain::HX711_GAIN_128)  ? 128U
         : (gain == HX711Gain::HX711_GAIN_64) ? 64U
         : (gain == HX711Gain::HX711_GAIN_32) ? 32U
                                              : 0U;
}

void HX711Sensor::setup() {
  this->sck_pin_->setup();
  this->dout_pin_->setup();

  // No timeouts are running, using `power_down_internal_()`
  this->power_cycle_restart_(true);
}

void HX711Sensor::loop() {
  if (this->is_power_up_sequence_running()) {
    // Wait for HX711 to be ready
    if (!this->is_measurement_ready()) {
      this->start_measurement_ready_timeout_();
      return;
    }

    // Gain defaults to x128 after power on
    // If requested gain is not x128
    // Force a read to set the gain
    // Force read will start settle timeout if function returns true
    if (this->gain_ == HX711Gain::HX711_GAIN_128) {
      this->start_settle_timeout_();
    } else if (!this->read_sensor_(nullptr, true, true)) {
      this->mark_failed_internal_(LOG_STR("power-up can't set gain"));
    }

    this->hx711_state_flags_.power_up_sequence_running = false;
    return;
  }
  if (!this->is_update_in_progress() || this->is_powered_down() || !this->is_settled()) {
    return;
  }
  if (!this->is_measurement_ready()) {
    this->start_measurement_ready_timeout_();
    return;
  }

#if defined(USE_HX711_CHANNEL_B_SENSOR)
  if (this->hx711_state_flags_.channel_b_sensor_read_pending) {
    this->hx711_state_flags_.channel_b_sensor_read_pending = false;
    uint32_t result = 0;

    // Gain will be restored by read_sensor_
    // Do not start settle timeout
    const bool read_operation_result = this->read_sensor_(&result, false);

    if (this->power_down_after_reading_) {
      // Power-down after channel B read if needed
      this->power_down(false);
    } else {
      // Start settle timeout for next measurement
      this->start_settle_timeout_();
    }

    if (!read_operation_result) {
      // On failed read, publish nan and do power cycle restart
      this->log_and_publish_channel_b_value_(NAN);
      this->power_cycle_restart_();
      return;
    }

    const float result_f = static_cast<float>(static_cast<int32_t>(result));
    // Publish value on succesful read
    this->log_and_publish_channel_b_value_(result_f);

    this->hx711_state_flags_.update_in_progress = false;
    return;
  }
#endif

  bool start_settle_timeout_after_read = !this->power_down_after_reading_;
  const HX711Gain expected_gain_for_current_reading = this->last_gain_;

#if defined(USE_HX711_CHANNEL_B_SENSOR)
  bool current_measurement_is_channel_b = expected_gain_for_current_reading == HX711Gain::HX711_GAIN_32;
  const HX711Gain &gain_to_restore = expected_gain_for_current_reading;

  // If needed, after reading the value, set the channel to channel B
  if (!current_measurement_is_channel_b) {
    start_settle_timeout_after_read = true;
    this->gain_ = HX711Gain::HX711_GAIN_32;
    this->hx711_state_flags_.channel_b_sensor_read_pending = true;
  }
#endif

  // Read the sensor
  uint32_t result = 0;
  if (this->read_sensor_(&result, start_settle_timeout_after_read)) {
    if (this->power_down_after_reading_) {
#if defined(USE_HX711_CHANNEL_B_SENSOR)
      // Power down only if there is no pending reading
      if (!this->hx711_state_flags_.channel_b_sensor_read_pending) {
        this->power_down(false);
      }
#else
      // Power down but don't stop the poller
      this->power_down(false);
#endif
    }

    const int32_t value = static_cast<int32_t>(result);
    ESP_LOGV(TAG, "'%s': Got value %" PRId32 " (gain x%u)", this->name_.c_str(), value,
             hx711_gain_to_linear_gain(expected_gain_for_current_reading));
    this->publish_state(value);

#if defined(USE_HX711_CHANNEL_B_SENSOR)
    if (!current_measurement_is_channel_b) {
      this->gain_ = gain_to_restore;

      // early return to prevent update_in_progress_ from being set to false
      return;
    }

    // Since current measurement is from channel b, publish it.
    this->log_and_publish_channel_b_value_(static_cast<float>(value));
#endif
  } else {
    // Failed to read the sensor, user can filter out NAN if needed.
    this->publish_state(NAN);
#if defined(USE_HX711_CHANNEL_B_SENSOR)
    if (current_measurement_is_channel_b) {
      this->log_and_publish_channel_b_value_(NAN);
    } else {
      this->gain_ = gain_to_restore;
    }
#endif
    // Restart the HX711 sensor, this will start powerup sequence.
    this->power_cycle_restart_();
  }

  this->hx711_state_flags_.update_in_progress = false;
}

void HX711Sensor::update() {
  if (this->is_update_in_progress()) {
    ESP_LOGW(TAG, "'%s': Previous update in progress", this->name_.c_str());
    return;
  }

  ESP_LOGV(TAG, "'%s': Updating", this->name_.c_str());

  bool power_up_started = false;

  if (this->is_powered_down()) {
    if (!this->power_down_after_reading_) {
      ESP_LOGW(TAG, "'%s': Currently %s, cannot update", this->name_.c_str(), LOG_STR_POWERED_DOWN);
      return;
    }

    this->power_up(false);
    power_up_started = true;
  }

  if (!power_up_started && (this->gain_ != this->last_gain_)) {
    this->power_down(false);
    this->power_up(false);
  }

  this->hx711_state_flags_.update_in_progress = true;
}

void HX711Sensor::dump_config() {
  LOG_SENSOR("", "HX711", this);
  LOG_PIN("  DOUT Pin: ", this->dout_pin_);
  LOG_PIN("  SCK Pin: ", this->sck_pin_);
  ESP_LOGCONFIG(TAG,
                "  Gain: x%u\n"
                "  Last gain: x%u\n"
                "  Settling time: %u ms\n"
                "  Power-down after reading: %s\n"
                "  Measurement ready timeout: %u ms\n",
                hx711_gain_to_linear_gain(this->gain_), hx711_gain_to_linear_gain(this->last_gain_),
                this->settling_time_ms_, YESNO(this->power_down_after_reading_), this->measurement_ready_timeout_ms_);
#if defined(USE_HX711_CHANNEL_B_SENSOR)
  LOG_SENSOR("  ", "Channel B Sensor", this->channel_b_sensor_);
#endif
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL_FOR, this->name_.c_str());
  }
  LOG_UPDATE_INTERVAL(this);
}

bool HX711Sensor::is_measurement_ready() {
  if (this->is_failed()) {
    return false;
  }

  const bool ready = !this->dout_pin_->digital_read();
  if (ready) {
    this->cancel_timeout(TIMEOUT_NAME_MEASUREMENT_READY);
    this->hx711_state_flags_.measurement_ready_timeout_active = false;
  }
  return ready;
}

void HX711Sensor::power_cycle_restart_(const bool use_internal_powerdown) {
  ESP_LOGD(TAG, "Restarting");
  // Reset state machine flags
#if defined(USE_HX711_CHANNEL_B_SENSOR)
  this->hx711_state_flags_.channel_b_sensor_read_pending = false;
#endif
  this->hx711_state_flags_.update_in_progress = false;
  this->hx711_state_flags_.power_up_sequence_running = false;
  // Reset the HX711
  if (use_internal_powerdown) {
    this->power_down_internal_();
  } else {
    this->power_down();
  }
  // At least 60 microseconds is needed for powerdown.
  delayMicroseconds(80);
  // Start poller after settling
  this->power_up(true);
}

bool HX711Sensor::power_up(const bool should_start_poller) {
  if (this->is_failed()) {
    return false;
  }

  if (!this->is_powered_down()) {
    ESP_LOGW(TAG, "'%s': already %s", this->name_.c_str(), LOG_STR_POWERED_UP);
    return false;
  }

  this->power_up_internal_();
  ESP_LOGD(TAG, "'%s': %s", this->name_.c_str(), LOG_STR_POWERED_UP);

  // After a reset or power-down event, input selection is default to Channel A with a gain of 128.
  this->last_gain_ = HX711Gain::HX711_GAIN_128;

  if (this->gain_ != this->last_gain_) {
    ESP_LOGV(TAG, "'%s': setting gain to x%u", this->name_.c_str(), hx711_gain_to_linear_gain(this->gain_));
  }

  this->hx711_state_flags_.power_up_sequence_running = true;

  if (this->hx711_state_flags_.poller_stopped) {
    this->hx711_state_flags_.should_start_poller = should_start_poller;
  }

  return true;
}

bool HX711Sensor::power_down(const bool stop_poller) {
  if (this->is_failed()) {
    return false;
  }

  if (this->is_powered_down() && !this->power_down_after_reading_) {
    ESP_LOGW(TAG, "'%s': already %s", this->name_.c_str(), LOG_STR_POWERED_DOWN);
    return false;
  }

  this->cancel_timeout(TIMEOUT_NAME_SETTLE);
  this->cancel_timeout(TIMEOUT_NAME_MEASUREMENT_READY);
  if (stop_poller) {
    ESP_LOGW(TAG, "'%s': Stopping poller", this->name_.c_str());
    this->stop_poller();
    this->hx711_state_flags_.poller_stopped = true;
  }
  this->power_down_internal_();
  delayMicroseconds(80);
  ESP_LOGD(TAG, "'%s': %s", this->name_.c_str(), LOG_STR_POWERED_DOWN);
  return true;
}

void HX711Sensor::set_new_gain(HX711Gain gain) {
  const char *const gain_operation_str = this->gain_ == gain ? "already" : "will be";
  ESP_LOGV(TAG, "'%s': Gain %s set to x%u", this->name_.c_str(), gain_operation_str, hx711_gain_to_linear_gain(gain));
  this->set_gain(gain);
}

void HX711Sensor::start_settle_timeout_() {
  this->hx711_state_flags_.settled = false;
  ESP_LOGV(TAG, "'%s': Settling", this->name_.c_str());
  this->set_timeout(TIMEOUT_NAME_SETTLE, this->settling_time_ms_, [this]() {
    this->hx711_state_flags_.settled = true;
    this->status_clear_warning();
    ESP_LOGV(TAG, "'%s': %sd", this->name_.c_str(), TIMEOUT_NAME_SETTLE);
    if (this->hx711_state_flags_.should_start_poller) {
      this->hx711_state_flags_.should_start_poller = false;
      this->hx711_state_flags_.poller_stopped = false;
      ESP_LOGD(TAG, "'%s': Starting poller", this->name_.c_str());
      this->start_poller();
    }
  });
}

bool HX711Sensor::start_measurement_ready_timeout_() {
  if (!this->hx711_state_flags_.measurement_ready_timeout_active) {
    this->hx711_state_flags_.measurement_ready_timeout_active = true;
    this->set_timeout(TIMEOUT_NAME_MEASUREMENT_READY, this->measurement_ready_timeout_ms_, [this]() {
      this->hx711_state_flags_.measurement_ready_timeout_active = false;
      this->power_down_internal_();
      this->mark_failed(LOG_STR("ready timeout"));
    });
    return true;
  }

  return false;
}

void HX711Sensor::mark_failed_internal_(const LogString *message) {
  if (this->is_failed())
    return;
  this->power_down_internal_();
  this->mark_failed(message);
  ESP_LOGE(TAG, "'%s' failed, automatically %s", this->name_.c_str(), LOG_STR_POWERED_DOWN);
}

void HX711Sensor::power_down_internal_() {
  // When PD_SCK pin changes from low to high and stays at high for longer than 60µs, HX711 enters power down mode.
  this->sck_pin_->digital_write(true);
  this->hx711_state_flags_.settled = false;
  this->hx711_state_flags_.powered_on_state = false;
}

void HX711Sensor::power_up_internal_() {
  if (this->is_failed())
    return;
  // When PD_SCK pin changes from high to low and stays at low, HX711 exits power down mode.
  this->sck_pin_->digital_write(false);
  this->hx711_state_flags_.settled = false;
  this->hx711_state_flags_.powered_on_state = true;
}

#if defined(USE_HX711_CHANNEL_B_SENSOR)
void HX711Sensor::log_and_publish_channel_b_value_(const float value) {
  if (this->channel_b_sensor_ == nullptr) {
    ESP_LOGE(TAG, "Channel B sensor not set");
    return;
  }
  ESP_LOGD(TAG, "'%s': Got Channel B value %.0f (gain x32)", this->channel_b_sensor_->get_name().c_str(), value);
  this->channel_b_sensor_->publish_state(value);
}
#endif

bool HX711Sensor::read_sensor_(uint32_t *result, const bool start_settle_timeout, const bool force) {
  if (this->is_failed()) {
    return false;
  }
  if (this->is_powered_down()) {
    ESP_LOGE(TAG, "%s", LOG_STR_POWERED_DOWN);
    return false;
  }
  if (!this->is_measurement_ready()) {
    ESP_LOGW(TAG, "%s", LOG_STR_NOT_READY);
    this->status_set_warning(LOG_STR_NOT_READY);
    return false;
  }
  if (!this->is_settled() && !force) {
    ESP_LOGW(TAG, "%s", LOG_STR_NOT_SETTLED);
    this->status_set_warning(LOG_STR_NOT_SETTLED);
    return false;
  }

  uint32_t data = 0;
  bool final_dout;
  ESP_LOGV(TAG, "'%s': last_gain=x%u; force=%s", this->name_.c_str(), hx711_gain_to_linear_gain(this->last_gain_),
           YESNO(force));
  {
    InterruptLock lock;
    for (uint8_t i = 0; i < 24; i++) {
      this->sck_pin_->digital_write(true);
      delayMicroseconds(1);
      data |= uint32_t(this->dout_pin_->digital_read()) << (23 - i);
      this->sck_pin_->digital_write(false);
      delayMicroseconds(1);
    }

    // Cycle clock pin for gain setting
    for (uint8_t i = 0; i < static_cast<uint8_t>(this->gain_); i++) {
      this->sck_pin_->digital_write(true);
      delayMicroseconds(1);
      this->sck_pin_->digital_write(false);
      delayMicroseconds(1);
    }
    final_dout = this->dout_pin_->digital_read();
  }

  bool should_start_settle_timeout = false;

  if ((this->last_gain_ != this->gain_) || force) {
    ESP_LOGV(TAG, "'%s': gain (x%u) changed to x%u", this->name_.c_str(), hx711_gain_to_linear_gain(this->last_gain_),
             hx711_gain_to_linear_gain(this->gain_));
    this->last_gain_ = this->gain_;
    this->hx711_state_flags_.settled = false;
    should_start_settle_timeout = start_settle_timeout || force;
  }

  if (!final_dout) {
    ESP_LOGW(TAG, "'%s': Final dout err; 0x%08" PRIx32, this->name_.c_str(), data);
    this->status_set_warning("final_dout not high");
    return false;
  }

  if (should_start_settle_timeout) {
    this->start_settle_timeout_();
  }

  if (start_settle_timeout && this->is_settled()) {
    this->status_clear_warning();
  }

  if (result != nullptr) {
    if (data & 0x800000ULL) {
      data |= 0xFF000000ULL;
    }
    *result = data;
  }

  return true;
}

}  // namespace hx711
}  // namespace esphome
