#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"

#include <cinttypes>

namespace esphome {
namespace hx711 {

/// @brief Available HX711 gain settings.
enum class HX711Gain : uint8_t {
  HX711_GAIN_128 = 1,  ///< 128x gain, channel A (Default after power on)
  HX711_GAIN_32 = 2,   ///< 32x gain, channel B
  HX711_GAIN_64 = 3,   ///< 64x gain, channel A
};

class HX711Sensor : public sensor::Sensor, public PollingComponent {
 public:
  void set_dout_pin(GPIOPin *dout_pin) { this->dout_pin_ = dout_pin; }
  void set_sck_pin(GPIOPin *sck_pin) { this->sck_pin_ = sck_pin; }
  void set_gain(HX711Gain gain) { this->gain_ = gain; }
  void set_measurement_ready_timeout(uint16_t measurement_ready_timeout_ms) {
    this->measurement_ready_timeout_ms_ = measurement_ready_timeout_ms;
  }
  void set_settling_time(uint16_t settling_time_ms) { this->settling_time_ms_ = settling_time_ms; }
  void set_power_down_after_reading(bool power_down_after_reading) {
    this->power_down_after_reading_ = power_down_after_reading;
  }
#ifdef USE_HX711_CHANNEL_B_SENSOR
  void set_channel_b_sensor(sensor::Sensor *channel_b_sensor) { this->channel_b_sensor_ = channel_b_sensor; }
#endif

  /// @brief Overriden to prevent automatic poller start
  void call_setup() override { this->setup(); };
  /// @brief Setups pins and starts power up sequence
  void setup() override;
  /// @brief Handles HX711 power-up and polling
  void loop() override;
  /// @brief Initiates a sensor reading cycle if no update is in progress.
  ///
  /// Handles HX711 power-up if powered down, and resets gain if needed.
  /// Logs warnings if update collisions or power issues are detected.
  void update() override;
  void dump_config() override;
  void on_powerdown() override { this->power_down_internal_(); }
  /// @brief Logs the new gain setting and sets internal gain variable
  ///
  /// This function is called by set gain automation. It serves as a helper function for logging
  ///
  /// @note Gain will not be applied right away, it's applied during normal component operation if needed
  ///
  /// @param[in] gain New gain setting
  void set_new_gain(HX711Gain gain);

  /// @brief Powers up the HX711 sensor and optionally starts the polling process.
  ///
  /// This function checks if the HX711 sensor is currently powered down. If it is, it powers
  /// up the sensor (Channel A, gain 128 is the default setting in HX711 as per datasheet),
  /// If the desired gain is different from the default, a log message is generated.
  /// It also prepares for the power-up sequence and optionally sets the flag to
  /// start the poller if it was previously stopped.
  ///
  /// @note Actual power-up sequence (like setting the gain and settling) is handled in the loop() function.
  ///
  /// @param[in] should_start_poller A boolean indicating whether to start the polling process after powering up.
  /// @return `true` if the sensor was successfully powered up, `false` if it was already powered up.
  bool power_up(bool should_start_poller = false);
  /// @brief Powers down the HX711 sensor and optionally stops the polling process.
  ///
  /// This function powers down the HX711 sensor if it is not already powered down.
  /// If the sensor is already powered down but automatic power-down after reading was enabled,
  /// it disables wakeup behavior. It also cancels any active timeouts related to settling or
  /// measurement readiness. If requested, it stops the update poller.
  ///
  /// @param[in] stop_poller A boolean indicating whether to stop the polling process during power down.
  /// @return `true` if the sensor was successfully powered down or already in a managed power-down state,
  ///         `false` if the sensor was already powered down and no further action was needed.
  bool power_down(bool stop_poller = true);

  /// @brief Returns whether the HX711 ADC has reached a stable state.
  ///
  /// Taken from the datasheet: "Settling time refers to the time from power up,
  /// reset, input channel change and gain change to valid stable output data."
  ///
  /// @note Delta Sigma ADC conversion needs priming, and first four ADC readings
  ///       after the start (400ms at 10 Hz) are usually containing errors and must be discarded.
  ///
  /// @return True if the HX711 ADC has reached a stable state, false otherwise.
  bool is_settled() const { return this->hx711_state_flags_.settled; }
  /// @brief Returns whether the poller is stopped.
  /// @return True if the polling interval is stopped, false otherwise.
  bool is_poller_stopped() const { return this->hx711_state_flags_.poller_stopped; }
  /// @brief Returns whether an update cycle is currently in progress.
  /// @return True if an update is in progress, false otherwise.
  bool is_update_in_progress() const { return this->hx711_state_flags_.update_in_progress; }
  /// @brief Returns whether the HX711 power-up sequence is currently running.
  /// @return True if the power-up sequence is running, false otherwise.
  bool is_power_up_sequence_running() const { return this->hx711_state_flags_.power_up_sequence_running; }
  /// @brief Returns whether the HX711 ADC is powered down.
  /// @return True if the HX711 ADC is powered down, false otherwise.
  bool is_powered_down() const { return !this->hx711_state_flags_.powered_on_state; };
  /// @brief Returns whether the HX711 ADC measurement is ready to be read. (DOUT pin is low).
  /// @return True if the HX711 ADC conversion is done, false otherwise.
  bool is_measurement_ready();

 protected:
  /// @brief Logs error, powers down the HX711 and marks component as failed.
  /// @param[in] message Pointer to LogString error message.
  void mark_failed_internal_(const LogString *message);

  /// @brief Starts the measurement-ready timeout for the HX711 sensor.
  ///
  /// This function sets a timeout to wait for the HX711 sensor's data output (DOUT) pin to indicate
  /// that a measurement is ready. If the timeout expires before the sensor signals readiness,
  /// the sensor is powered down and marked as failed with an appropriate error message.
  /// This prevents the system from hanging indefinitely in case of sensor failure or disconnection.
  ///
  /// @note This timeout is not restarted by this function if it's already active.
  ///
  /// @warning `is_measurement_ready()` must be called to check sensor's data output and cancel timeout.
  ///
  /// @return `true` if the timeout was successfully started, `false` if it was already running.
  bool start_measurement_ready_timeout_();
  /// @brief Starts the settling timeout for the HX711 ADC after power-up.
  ///
  /// This function begins a timeout to allow the HX711 ADC to settle after being powered up or
  /// after gain change.
  /// During this period, the sensor is considered not yet ready to produce accurate measurements.
  /// Once the timeout completes:
  /// - The sensor is marked as settled,
  /// - Any warning status is cleared,
  /// - And if polling was deferred until after settling, the poller is started.
  void start_settle_timeout_();

  /// @brief Powers down the HX711 sensor by setting the PD_SCK pin low and resets settled state.
  void power_down_internal_();
  /// @brief Powers up the HX711 sensor by setting the PD_SCK pin high and resets settled state.
  void power_up_internal_();
  /// @brief Powers down the HX711 sensor and then powers it back up.
  /// @param[in] use_internal_powerdown If true `power_down_internal_()` will be called instead of `power_down()`.
  void power_cycle_restart_(bool use_internal_powerdown = false);

  /// @brief Reads raw data from the HX711 sensor, handling gain cycling and optional settling.
  ///
  /// This function performs a raw 24-bit data read from the HX711 sensor using bit-banging,
  /// and cycles the clock pin to set the desired gain for the next measurement. It validates
  /// the sensor's readiness, power state, and settling status before proceeding with the read.
  ///
  /// If the gain setting has changed or a forced read is requested, the sensor will be marked as
  /// unsettled and a new settling timeout can optionally be started.
  ///
  /// @warning Settling timeout will not be started if function returns `false`.
  ///
  /// @param[out] result Pointer to a `uint32_t` where the read value will be stored. Can be `nullptr`
  ///                    if the caller doesn't need the raw result.
  /// @param[in] start_settle_timeout If `true`, triggers a settling timeout if conditions require it
  ///                             (e.g., gain changed or forced read).
  /// @param[in] force If `true`,forces a read and gain set, starts settling timeout.
  ///
  /// @return `true` if the read was successful and the sensor is in a valid state,
  ///         `false` if any preconditions (e.g., power state, readiness, or settling) were not met
  ///         or if the data read indicated a potential fault (e.g., DOUT not high after read).
  bool read_sensor_(uint32_t *result, bool start_settle_timeout, bool force = false);

#ifdef USE_HX711_CHANNEL_B_SENSOR
  /// @brief Logs the value for Channel B sensor and publishes it.
  ///
  /// Helper function to reduce flash usage.
  ///
  /// @param[in] value Channel B sensor value.
  void log_and_publish_channel_b_value_(float value);
#endif

  /// @brief Settling time in milliseconds.
  uint16_t settling_time_ms_;
  /// @brief Timeout in milliseconds to wait before marking component as failed.
  uint16_t measurement_ready_timeout_ms_;
  /// @brief Flag to indicate whether to power down the sensor after reading.
  bool power_down_after_reading_;

  /// @brief Represents internal state flags for the HX711 sensor component.
  ///
  /// This struct holds multiple state-related flags that are packed into a bitfield
  /// to minimize RAM usage. Each bit represents a specific condition or control flag
  /// relevant to the operation and timing of the HX711 ADC and polling system.
  struct HX711StateFlags {
    /// @brief Flag to indicate whether to start the poller after settling.
    uint8_t should_start_poller : 1;
    /// @brief Flag to indicate whether the poller is currently stopped.
    uint8_t poller_stopped : 1;
    /// @brief Flag to indicate whether the HX711 ADC has reached a stable state.
    uint8_t settled : 1;
    /// @brief Flag to indicate whether an update cycle is currently in progress.
    ///
    /// Gets set in update(), reset in loop()
    uint8_t update_in_progress : 1;
    /// @brief Flag to indicate that the power-up sequence is currently running.
    ///
    /// Gets set in power_up(), reset in loop().
    uint8_t power_up_sequence_running : 1;
    /// @brief Flag indicating whether the measurement-ready timeout is currently active.
    ///
    /// This flag is set to `true` when `start_measurement_ready_timeout_()` initiates the timeout
    /// waiting for the HX711's DOUT pin to signal readiness. It is reset to `false` when the timeout
    /// expires or is explicitly cleared in the processing loop. Used to prevent timeout
    /// from restarting.
    uint8_t measurement_ready_timeout_active : 1;
    /// @brief Flag indicating whether the HX711 is currently powered on.
    uint8_t powered_on_state : 1;

#if defined(USE_HX711_CHANNEL_B_SENSOR)
    /// @brief Flag to indicate that a reading from channel B is pending.
    ///
    /// If this is true, that means that in next loop() we need to read channel B.
    ///
    /// @note This flag must be reset after reading channel b in loop().
    uint8_t channel_b_sensor_read_pending : 1;
#endif
  };
  /// @brief Internal state flags for the HX711 sensor component.
  HX711StateFlags hx711_state_flags_ {
    .should_start_poller = false, .poller_stopped = true, .settled = false, .update_in_progress = false,
    .power_up_sequence_running = false, .measurement_ready_timeout_active = false, .powered_on_state = false,
#if defined(USE_HX711_CHANNEL_B_SENSOR)
    .channel_b_sensor_read_pending = false,
#endif
  };

#if defined(USE_HX711_CHANNEL_B_SENSOR)
  /// @brief Sensor for additional simultaneous channel B readings
  sensor::Sensor *channel_b_sensor_{nullptr};
#endif

  /// @brief Data output pin (DOUT) for HX711
  ///
  /// Used for reading data and checking measurement readiness.
  GPIOPin *dout_pin_;
  /// @brief Clock pin (PD_SCK) for HX711
  ///
  /// Used for both clocking data out and power control.
  GPIOPin *sck_pin_;

  /// @brief Gain to set after new measurement.
  HX711Gain gain_{HX711Gain::HX711_GAIN_128};
  /// @brief The last gain that was set.
  ///
  /// After a reset or power-down event, input selection defaults to Channel A with a gain of 128.
  HX711Gain last_gain_{HX711Gain::HX711_GAIN_128};
};

template<typename... Ts> class HX711SensorActionBase : public Action<Ts...> {
 public:
  void set_parent(HX711Sensor *parent) { this->parent_ = parent; }

 protected:
  HX711Sensor *parent_;
};

template<typename... Ts> class PowerUpAction : public HX711SensorActionBase<Ts...> {
 public:
  void play(Ts... x) override {
    if (!this->parent_->is_ready()) {
      return;
    }

    // Start poller after settling
    this->parent_->power_up(true);
  }
};

template<typename... Ts> class PowerDownAction : public HX711SensorActionBase<Ts...> {
 public:
  void play(Ts... x) override {
    if (!this->parent_->is_ready()) {
      return;
    }

    // This stops the poller also
    this->parent_->power_down(true);
  }
};

template<typename... Ts> class SetGainAction : public HX711SensorActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(HX711Gain, gain)

  void play(Ts... x) override {
    if (!this->parent_->is_ready()) {
      return;
    }
    this->parent_->set_new_gain(this->gain_.value(x...));
  }
};

}  // namespace hx711
}  // namespace esphome
