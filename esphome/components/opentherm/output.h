#pragma once

#include "esphome/components/output/float_output.h"
#include "input.h"

namespace esphome {
namespace opentherm {

class OpenthermOutput : public output::FloatOutput, public Component, public OpenthermInput {
protected:
    bool has_state_ = false;
    const char* id = nullptr;

    float min_value, max_value;

public:
    float state;

    void set_id(const char* id) { this->id = id; }

    void write_state(float state) override { 
        ESP_LOGD("opentherm.output", "Received state: %.2f. Min value: %.2f, max value: %.2f", state, min_value, max_value);
        this->state = state < 0.003 && this->zero_means_zero_ ? 0.0 : min_value + state * (max_value - min_value);
        this->has_state_ = true;
        ESP_LOGD("opentherm.output", "Output %s set to %.2f", this->id, this->state);
    };

    bool has_state() { return this->has_state_; };

    void set_min_value(float min_value) override { this->min_value = min_value; }
    void set_max_value(float max_value) override { this->max_value = max_value; }
};

} // namespace opentherm
} // namespace esphome
