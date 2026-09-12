#pragma once

#include "esphome/components/number/number.h"
#include "../rs485_frame.h"

#include <functional>
#include <vector>

namespace esphome::rs485_frame {

class RS485FrameNumber : public number::Number {
 public:
  using encode_lambda_t = std::function<optional<std::vector<uint8_t>>(float)>;
  explicit RS485FrameNumber(RS485FrameHub *parent) : parent_(parent) {}
  void set_template(encode_lambda_t &&lambda) { this->lambda_ = std::move(lambda); }

 protected:
  void control(float value) override;
  RS485FrameHub *parent_;
  encode_lambda_t lambda_{nullptr};
};

}  // namespace esphome::rs485_frame
