#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include "lis3dh_reg.h"

namespace esphome {
namespace lis3dh {

enum class LIS3DHRange : uint8_t {
  RANGE_2G = 0,
  RANGE_4G = 1,
  RANGE_8G = 2,
  RANGE_16G = 3,
};

enum class LIS3DHMode : uint8_t {
  MODE_LOW_POWER = 0,
  MODE_NORMAL = 1,
  MODE_HIGH_RESOLUTION = 2,
};

enum class LIS3DHDataRate : uint8_t {
  ODR_1HZ = 0x01,
  ODR_10HZ = 0x02,
  ODR_25HZ = 0x03,
  ODR_50HZ = 0x04,
  ODR_100HZ = 0x05,
  ODR_200HZ = 0x06,
  ODR_400HZ = 0x07,
  ODR_1600HZ = 0x08,
  ODR_5376HZ = 0x09,
};

enum class LIS3DHFifoMode : uint8_t {
  FIFO_BYPASS = 0,
  FIFO_MODE = 1,
  FIFO_STREAM = 2,
  FIFO_STREAM_TO_FIFO = 3,
};

// Matches the AOI/6D encoding in INT1_CFG/INT2_CFG (AOI is the high bit, 6D the low bit) so it
// can be written directly: see ST LIS3DH datasheet Table 64 "Interrupt mode".
enum class LIS3DHInterruptMode : uint8_t {
  OR = 0,           // AOI=0, 6D=0: OR combination of interrupt events
  MOVEMENT_6D = 1,  // AOI=0, 6D=1: 6-direction movement recognition
  AND = 2,          // AOI=1, 6D=0: AND combination of interrupt events
  POSITION_6D = 3,  // AOI=1, 6D=1: 6-direction position recognition
};

enum class LIS3DHOrientationXY : uint8_t {
  PORTRAIT_UPRIGHT,
  PORTRAIT_UPSIDE_DOWN,
  LANDSCAPE_LEFT,
  LANDSCAPE_RIGHT,
  FLAT,
};

enum class LIS3DHOrientationZ : uint8_t {
  FACE_UP,
  FACE_DOWN,
};

// ISR-side store, kept POD so it lives outside the vtable-bearing class.
struct LIS3DHStore {
  ISRInternalGPIOPin int1_pin;
  ISRInternalGPIOPin int2_pin;
  volatile bool int1_triggered{false};
  volatile bool int2_triggered{false};
  Component *parent{nullptr};

  static void IRAM_ATTR int1_gpio_intr(LIS3DHStore *arg);
  static void IRAM_ATTR int2_gpio_intr(LIS3DHStore *arg);
};

class LIS3DHComponent : public PollingComponent {
 public:
  LIS3DHComponent() = default;

  void setup() override;
  void update() override;
  void loop() override;
  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::BUS; }

  void set_accel_range(LIS3DHRange range) { this->range_ = range; }
  void set_operating_mode(LIS3DHMode mode) { this->mode_ = mode; }
  void set_odr(LIS3DHDataRate rate) { this->data_rate_ = rate; }

  void set_fifo_enabled(bool enabled) { this->fifo_enabled_ = enabled; }
  void set_fifo_mode(LIS3DHFifoMode mode) { this->fifo_mode_ = mode; }
  void set_fifo_watermark(uint8_t watermark) { this->fifo_watermark_ = watermark; }

  void set_int1_pin(InternalGPIOPin *pin) { this->int1_pin_ = pin; }
  void set_int2_pin(InternalGPIOPin *pin) { this->int2_pin_ = pin; }

  void set_tap_enabled(bool enabled) { this->tap_enabled_ = enabled; }
  void set_tap_threshold(uint8_t threshold) { this->tap_threshold_ = threshold; }
  void set_tap_shock_duration(uint8_t duration) { this->tap_shock_duration_ = duration; }
  void set_tap_quiet_duration(uint8_t duration) { this->tap_quiet_duration_ = duration; }
  void set_tap_double_tap_timeout(uint8_t timeout) { this->tap_double_tap_timeout_ = timeout; }

  void set_activity_enabled(bool enabled) { this->activity_enabled_ = enabled; }
  void set_activity_threshold(uint8_t threshold) { this->activity_threshold_ = threshold; }
  void set_activity_duration(uint8_t duration) { this->activity_duration_ = duration; }
  void set_activity_interrupt_mode(LIS3DHInterruptMode mode) { this->activity_interrupt_mode_ = mode; }

  void set_freefall_enabled(bool enabled) { this->freefall_enabled_ = enabled; }
  void set_freefall_threshold(uint8_t threshold) { this->freefall_threshold_ = threshold; }
  void set_freefall_duration(uint8_t duration) { this->freefall_duration_ = duration; }
  void set_freefall_interrupt_mode(LIS3DHInterruptMode mode) { this->freefall_interrupt_mode_ = mode; }

  void set_enable_deep_sleep_wakeup(bool enabled) { this->enable_deep_sleep_wakeup_ = enabled; }

  // "Sleep-to-wake" / "return-to-sleep": hardware-only ODR throttling, distinct from the
  // INT1 activity generator (which raises an interrupt) above. When enabled, the chip drops
  // itself to 10 Hz ODR after acceleration stays below auto_low_power_threshold_ for
  // auto_low_power_duration_, then restores the configured ODR/mode as soon as acceleration
  // exceeds the threshold again -- with no CPU involvement.
  void set_auto_low_power_enabled(bool enabled) { this->auto_low_power_enabled_ = enabled; }
  void set_auto_low_power_threshold(uint8_t threshold) { this->auto_low_power_threshold_ = threshold; }
  void set_auto_low_power_duration(uint8_t duration) { this->auto_low_power_duration_ = duration; }

  void set_high_pass_filter_enabled(bool enabled) { this->high_pass_filter_enabled_ = enabled; }
  void set_high_pass_filter_mode(uint8_t mode) { this->high_pass_filter_mode_ = mode; }
  void set_high_pass_filter_cutoff(uint8_t cutoff) { this->high_pass_filter_cutoff_ = cutoff; }

  void set_temperature_enabled(bool enabled) { this->temperature_enabled_ = enabled; }

#ifdef USE_SENSOR
  SUB_SENSOR(acceleration_x)
  SUB_SENSOR(acceleration_y)
  SUB_SENSOR(acceleration_z)
  SUB_SENSOR(temperature)
#endif

#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(tap)
  SUB_BINARY_SENSOR(double_tap)
  SUB_BINARY_SENSOR(activity)
  SUB_BINARY_SENSOR(freefall)
#endif

#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(orientation_xy)
  SUB_TEXT_SENSOR(orientation_z)
#endif

  template<typename F> void add_on_tap_callback(F &&callback) { this->tap_callback_.add(std::forward<F>(callback)); }
  template<typename F> void add_on_double_tap_callback(F &&callback) {
    this->double_tap_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_activity_callback(F &&callback) {
    this->activity_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_freefall_callback(F &&callback) {
    this->freefall_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_orientation_change_callback(F &&callback) {
    this->orientation_change_callback_.add(std::forward<F>(callback));
  }

  // Public for STMems driver C-callback bridge.
  virtual bool read_register(uint8_t reg, uint8_t *data, uint16_t len) = 0;
  virtual bool write_register(uint8_t reg, const uint8_t *data, uint16_t len) = 0;

 protected:
  // Errata: the LIS3DH's first write to INT1_CFG/INT2_CFG after power-up can be silently
  // dropped. Writes, reads back to confirm, and retries once more if the readback mismatches.
  bool write_register_verified_(uint8_t reg, uint8_t value);

  bool verify_device_id_();
  bool configure_device_();
  bool configure_interrupt_routing_();
  bool configure_fifo_();
  bool configure_tap_detection_();
  bool configure_motion_detection_();
  bool configure_freefall_detection_();
  bool configure_auto_low_power_();

  void read_acceleration_data_();
  void read_temperature_data_();
  void read_fifo_data_();
  void process_event_sources_();
  void update_orientation_();

  float convert_acceleration_to_ms2_(int16_t raw);
  float convert_temperature_(int16_t raw);

  // Whether ISR pins drive the loop. False means we always poll from update().
  bool interrupt_driven_() const { return this->int1_pin_ != nullptr || this->int2_pin_ != nullptr; }

  stmdev_ctx_t dev_ctx_{};

  LIS3DHRange range_;
  LIS3DHMode mode_;
  LIS3DHDataRate data_rate_;

  bool fifo_enabled_;
  LIS3DHFifoMode fifo_mode_;
  uint8_t fifo_watermark_;

  InternalGPIOPin *int1_pin_{nullptr};
  InternalGPIOPin *int2_pin_{nullptr};

  bool tap_enabled_;
  uint8_t tap_threshold_;
  uint8_t tap_shock_duration_;
  uint8_t tap_quiet_duration_;
  uint8_t tap_double_tap_timeout_;

  bool activity_enabled_;
  uint8_t activity_threshold_;
  uint8_t activity_duration_;
  LIS3DHInterruptMode activity_interrupt_mode_{LIS3DHInterruptMode::OR};

  bool freefall_enabled_;
  uint8_t freefall_threshold_;
  uint8_t freefall_duration_;
  LIS3DHInterruptMode freefall_interrupt_mode_{LIS3DHInterruptMode::AND};

  bool enable_deep_sleep_wakeup_;

  bool auto_low_power_enabled_;
  uint8_t auto_low_power_threshold_;
  uint8_t auto_low_power_duration_;

  bool high_pass_filter_enabled_;
  uint8_t high_pass_filter_mode_;
  uint8_t high_pass_filter_cutoff_;

  bool temperature_enabled_;

  uint32_t last_tap_event_{0};
  uint32_t last_double_tap_event_{0};
  bool last_activity_state_{false};
  bool last_freefall_state_{false};

  LIS3DHOrientationXY last_orientation_xy_{LIS3DHOrientationXY::FLAT};
  LIS3DHOrientationZ last_orientation_z_{LIS3DHOrientationZ::FACE_UP};
  bool orientation_published_{false};

  CallbackManager<void()> tap_callback_;
  CallbackManager<void()> double_tap_callback_;
  CallbackManager<void()> activity_callback_;
  CallbackManager<void()> freefall_callback_;
  CallbackManager<void()> orientation_change_callback_;

  float accel_x_ms2_{0};
  float accel_y_ms2_{0};
  float accel_z_ms2_{0};

  LIS3DHStore store_;
};

class TapTrigger : public Trigger<> {
 public:
  explicit TapTrigger(LIS3DHComponent *parent) {
    parent->add_on_tap_callback([this]() { this->trigger(); });
  }
};

class DoubleTapTrigger : public Trigger<> {
 public:
  explicit DoubleTapTrigger(LIS3DHComponent *parent) {
    parent->add_on_double_tap_callback([this]() { this->trigger(); });
  }
};

class ActivityTrigger : public Trigger<> {
 public:
  explicit ActivityTrigger(LIS3DHComponent *parent) {
    parent->add_on_activity_callback([this]() { this->trigger(); });
  }
};

class FreefallTrigger : public Trigger<> {
 public:
  explicit FreefallTrigger(LIS3DHComponent *parent) {
    parent->add_on_freefall_callback([this]() { this->trigger(); });
  }
};

class OrientationChangeTrigger : public Trigger<> {
 public:
  explicit OrientationChangeTrigger(LIS3DHComponent *parent) {
    parent->add_on_orientation_change_callback([this]() { this->trigger(); });
  }
};

}  // namespace lis3dh
}  // namespace esphome
