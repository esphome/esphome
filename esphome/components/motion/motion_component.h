#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include <array>
#include <functional>
#include <numbers>

namespace esphome {
namespace motion {

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
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_matrix(const std::array<float, 9> &m) { memcpy(this->matrix_, m.data(), sizeof(this->matrix_)); }

  void add_listener(std::function<void(MotionData &)> &&cb) { motion_data_callback_.add(std::move(cb)); }

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
  };

  LazyCallbackManager<void(MotionData &)> motion_data_callback_{};
};

}  // namespace motion
}  // namespace esphome
