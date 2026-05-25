#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include <array>
#include <cmath>
#include <functional>
#include <numbers>

namespace esphome::motion {

// ---Data class

class MotionData {
 public:
  float acceleration[3]{NAN, NAN, NAN};
  float angular_rate[3]{NAN, NAN, NAN};
  // TODO - compass
};

// indices into data arrays
static constexpr uint8_t X_AXIS = 0;
static constexpr uint8_t Y_AXIS = 1;
static constexpr uint8_t Z_AXIS = 2;

// Main component class
class MotionComponent : public PollingComponent {
 public:
  // Lifecycle
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_matrix(const std::array<float, 9> &m) { memcpy(this->matrix_, m.data(), sizeof(this->matrix_)); }
  void set_calibration_key(uint32_t key) { this->pref_key_ = key; }

  /// Calibrate the matrix so the current reading maps to [0, 0, 1] (device flat).
  bool calibrate_level();
  /// Assuming Y-axis rotation only, correct the heading so X/Y align correctly.
  bool calibrate_heading();
  /// Save the current matrix to NVS.
  void save_calibration();

  template<typename F> void add_listener(F &&cb) { this->motion_data_callback_.add(std::forward<F>(cb)); }

 protected:
  // platforms must implement this method to update raw data.
  virtual bool update_data(MotionData &data) = 0;

  // for mapping axes
  float matrix_[9]{
      1, 0, 0, 0, 1, 0, 0, 0, 1,
  };

  void map_axes_(float output[3], const float input[3]) const {
    output[0] = input[X_AXIS] * this->matrix_[0] + input[Y_AXIS] * this->matrix_[1] + input[Z_AXIS] * this->matrix_[2];
    output[1] = input[X_AXIS] * this->matrix_[3] + input[Y_AXIS] * this->matrix_[4] + input[Z_AXIS] * this->matrix_[5];
    output[2] = input[X_AXIS] * this->matrix_[6] + input[Y_AXIS] * this->matrix_[7] + input[Z_AXIS] * this->matrix_[8];
  }

  LazyCallbackManager<void(MotionData &)> motion_data_callback_{};
  uint32_t pref_key_{0};
  ESPPreferenceObject pref_{};
};

// --- Actions ---

template<typename... Ts> class CalibrateLevelAction : public Action<Ts...> {
 public:
  explicit CalibrateLevelAction(MotionComponent *parent) : parent_(parent) {}
  void set_save(bool save) { this->save_ = save; }
  Trigger<> *get_success_trigger() { return &this->success_trigger_; }
  Trigger<> *get_error_trigger() { return &this->error_trigger_; }
  void play(const Ts &...) override {
    if (this->parent_->calibrate_level()) {
      if (this->save_)
        this->parent_->save_calibration();
      this->success_trigger_.trigger();
    } else {
      this->error_trigger_.trigger();
    }
  }

 protected:
  MotionComponent *parent_;
  Trigger<> success_trigger_;
  Trigger<> error_trigger_;
  bool save_{false};
};

template<typename... Ts> class CalibrateHeadingAction : public Action<Ts...> {
 public:
  explicit CalibrateHeadingAction(MotionComponent *parent) : parent_(parent) {}
  void set_save(bool save) { this->save_ = save; }
  Trigger<> *get_success_trigger() { return &this->success_trigger_; }
  Trigger<> *get_error_trigger() { return &this->error_trigger_; }
  void play(const Ts &...) override {
    if (this->parent_->calibrate_heading()) {
      if (this->save_)
        this->parent_->save_calibration();
      this->success_trigger_.trigger();
    } else {
      this->error_trigger_.trigger();
    }
  }

 protected:
  MotionComponent *parent_;
  Trigger<> success_trigger_;
  Trigger<> error_trigger_;
  bool save_{false};
};

}  // namespace esphome::motion
