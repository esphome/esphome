#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_CAMERA
#include "esphome/components/camera/camera.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

namespace esphome {
namespace amg8833 {

enum FPS : uint8_t { FPS_10 = 0, FPS_1 };

enum Mode : uint8_t { MOTION = 0, PRESENCE };

inline const char *to_string(FPS fps) {
  switch (fps) {
    case FPS_10:
      return "FPS_10";
    case FPS_1:
      return "FPS_1";
  }
  return "FPS_10";
}

inline const char *to_string(Mode mode) {
  switch (mode) {
    case MOTION:
      return "MOTION";
    case PRESENCE:
      return "PRESENCE";
  }
  return "MOTION";
}

class AMG8833 : public PollingComponent, public i2c::I2CDevice {
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(motion)
  SUB_BINARY_SENSOR(presence)
#endif
#ifdef USE_CAMERA
  SUB_CAMERA(thermal)
#endif
#ifdef USE_NUMBER
  SUB_NUMBER(presence_hysteresis)
  SUB_NUMBER(presence_upper)
  SUB_NUMBER(presence_lower)
  SUB_NUMBER(motion_hysteresis)
  SUB_NUMBER(motion_maximum)
  SUB_NUMBER(motion_minimum)
#endif
#ifdef USE_SELECT
  SUB_SELECT(fps)
  SUB_SELECT(mode)
#endif
#ifdef USE_SENSOR
  SUB_SENSOR(ambient)
  SUB_SENSOR(maximum)
  SUB_SENSOR(minimum)
#endif
#ifdef USE_SWITCH
  SUB_SWITCH(filter)
  SUB_SWITCH(interrupt_pin)
#endif

 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  uint32_t get_update_interval() const override { return this->fps_ == FPS_1 ? 1000 : 100; }

  void set_fps(FPS fps) { this->fps_ = fps; }
  void set_filter(bool enable) { this->filter_ = enable; }
  void set_software_output(bool enable) { this->software_output_ = enable; }
  void set_interrupt_pin(bool enable) { this->interrupt_pin_ = enable; }
  void set_mode(Mode mode) { this->mode_ = mode; };
  void set_presence_thresholds(float lower, float upper, float hysteresis) {
    this->presence_lower_ = lower;
    this->presence_upper_ = upper;
    this->presence_hysteresis_ = hysteresis;
  }
  void set_motion_thresholds(float minimum, float maximum, float hysteresis) {
    this->motion_minimum_ = minimum;
    this->motion_maximum_ = maximum;
    this->motion_hysteresis_ = hysteresis;
  }

  void number_presence_hysteresis(float value);
  void number_presence_upper(float value);
  void number_presence_lower(float value);
  void number_motion_hysteresis(float value);
  void number_motion_maximum(float value);
  void number_motion_minimum(float value);

  void switch_filter(bool enable);
  void switch_interrupt_pin(bool enable);

  void select_fps(const std::string &fps);
  void select_mode(const std::string &mode);

  void add_measurement_callback(std::function<void(std::array<std::array<float, 8>, 8> &)> &&callback) {
    this->measurement_callback_.add(std::move(callback));
  }

  std::array<std::array<float, 8>, 8> &get_measurement() { return measurement_; }

 protected:
  bool write_fps_();
  bool write_filter_();
  bool write_mode_();
  bool write_presence_thresholds_();
  bool write_motion_thresholds_();
  bool write_threshold_(uint8_t a_register, float temperature);
  bool is_temperature_interrupt();
  float thermistor_to_temperature_();

  uint8_t pixels_[128];
  uint8_t thermistor_[2];
  uint8_t interrupts_[8];
  uint8_t status_{};
  FPS fps_{FPS_1};
  bool filter_{false};
  bool interrupt_pin_{false};
  bool software_output_{false};
  Mode mode_{PRESENCE};
  float presence_lower_{};
  float presence_upper_{};
  float presence_hysteresis_{};
  float motion_minimum_{};
  float motion_maximum_{};
  float motion_hysteresis_{};
  std::array<std::array<float, 8>, 8> measurement_;
  CallbackManager<void(std::array<std::array<float, 8>, 8> &)> measurement_callback_{};
};

}  // namespace amg8833
}  // namespace esphome
