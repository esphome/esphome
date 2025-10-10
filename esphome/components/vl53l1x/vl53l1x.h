#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

#include <list>

namespace esphome {
  namespace i2c {
    class I2CBus;
  }
namespace vl53l1x {
    void register_sensor(::esphome::i2c::I2CDevice* dev);
    void set_bootstrap_device(::esphome::i2c::I2CDevice* dev);
    void clear_bootstrap_device();

enum DistanceMode : uint8_t { SHORT = 0, LONG = 2 };
enum InterruptWhenMode : uint8_t {
    NOT_SET = 0xff,
    BELOW_MIN = 0,
    ABOVE_MAX = 1,
    OUTSIDE_WINDOW = 2,
    INSIDE_WINDOW = 3
};

class VL53L1xSensor : public sensor::Sensor, public Component, public i2c::I2CDevice {
 public:
  VL53L1xSensor();
  void setup() override;
  void loop() override;
  void dump_config() override;
  RetryResult update();

  void set_enable_pin(GPIOPin *enable) { this->enable_pin_ = enable; }
  void set_interrupt_pin(InternalGPIOPin *interrupt_pin) { this->interrupt_pin_ = interrupt_pin; }
  void set_timing_budget(uint32_t timing_budget) { this->measurement_timing_budget_ms_ = timing_budget; }
  void set_distance_mode(DistanceMode mode) { this->distance_mode_ = mode; }
  void set_update_interval(uint32_t update_interval_ms) { this->update_interval_ms_ = update_interval_ms; }
  void set_distance_threshold(uint16_t min, uint16_t max, InterruptWhenMode interrupt_when) {
    this->distance_threshold.min = min != 0xff ? min : 0;
    this->distance_threshold.max = max != 0xff ? max : 0;
    this->distance_threshold.interrupt_when = interrupt_when;
  }
  void set_offset(int16_t offset) {this->offset = offset; }
  void set_xtalk_correction(uint16_t xtalk_correction) {this->xtalk_correction = xtalk_correction;}
  void set_sigma_threshold(uint16_t sigma_threshold){ this->sigma_threshold = sigma_threshold;}
  void set_signal_threshold(uint16_t signal_threshold){ this->signal_threshold = signal_threshold;}
  /**
   * Origo is upper left corner, region is coordinates 0-15.
   */
  void set_roi(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    this->roi.x = x;
    this->roi.y = y;
    this->roi.w = w;
    this->roi.h = h;
    this->roi.isSet = true;
  }

  static void schedule_update_from_isr(VL53L1xSensor *sensor) {
    sensor->enable_loop_soon_any_context();
  }

 protected:
  
  /** Enable the device. Will pull the enable pin high, if configured. */
  bool enable();
  /** Disable the device. Will pull the enable pin low, if configured. */
  void disable();

  bool init_sensor_();
  void setup_enable_pin();

  enum ReadResult {
    SUCCESS, FAILURE, RETRY
  };
  ReadResult read_distance_mm_(uint16_t &distance_mm);

  bool apply_distance_mode();
  bool apply_timing_budget();
  bool apply_update_interval();
  bool apply_distance_threshold();
  bool apply_roi();
  bool apply_offset();
  bool apply_xtalk_correction();
  bool apply_sigma_threshold();
  bool apply_signal_threshold();
  


  GPIOPin *enable_pin_{nullptr};
  InternalGPIOPin *interrupt_pin_{nullptr};
  uint32_t measurement_timing_budget_ms_{50};
  uint32_t update_interval_ms_{60000};
  DistanceMode distance_mode_{DistanceMode::SHORT};
  int16_t offset{0};
  uint16_t xtalk_correction{0};
  uint16_t sigma_threshold{0xffff};
  uint16_t signal_threshold{0xffff};
  struct { uint16_t min{0xff}, max{0xff}; InterruptWhenMode interrupt_when{NOT_SET}; } distance_threshold; 
  struct { uint8_t x{0}, y{0}, w{0}, h{0}; bool isSet{false};} roi;
  bool initialized_{false};

  static std::list<VL53L1xSensor*> all_sensors;
  static bool pin_setup_complete;
};

}  // namespace vl53l1
}  // namespace esphome


