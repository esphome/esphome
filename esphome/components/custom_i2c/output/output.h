#pragma once

#include "../custom_i2c.h"
#include "esphome/components/output/float_output.h"

namespace esphome::custom_i2c {

template<uint8_t bytes> class CustomI2COutput : public output::FloatOutput, public Component {
 public:
  // mind you, I'll eat my hat if anyone so much as has a need for 4 bytes in the first place
  static_assert(bytes > 0 && bytes <= 4,
                "custom_i2c float outputs only support writing up to 4 bytes for now. If you need "
                "support for more, please open an issue.");

  void setup() override {
    // TODO: remove before merging, currently here to troubleshoot a setup order annoyance
    ESP_LOGV(TAG, "Setting up custom_i2c output");  // NOLINT
    this->turn_off();
  }
  void dump_config() override {}
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void set_register(CustomI2CRegister *register__) { this->register_ = register__; }
  void write_state(float state) override {
    float max = static_cast<float>(std::numeric_limits<uint32_t>::max());
    float multiplied_state = state * max;

    uint32_t result;
    if (std::nextafter(multiplied_state, INFINITY) >= max) {
      result = std::numeric_limits<uint32_t>::max();
    } else {
      result = static_cast<uint32_t>(multiplied_state);
    }

    ESP_LOGVV(FLOAT_OUTPUT_TAG, "converted state %.6f to float value %.6f to uint32_t value 0x%08x", state,
              multiplied_state, result);  // NOLINT

    result = convert_big_endian(result);
    std::array<uint8_t, bytes> data;
    memcpy(data.data(), reinterpret_cast<uint8_t *>(&result), bytes);

    this->register_->write_bytes(data);
  }

 protected:
  CustomI2CRegister *register_{};
};

}  // namespace esphome::custom_i2c
