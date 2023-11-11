#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/output/float_output.h"


const uint16_t I2C_ADDRESS = 0x27;
uint16_t REGISTER_ADDRESS = 0x80;
const char* TAG = "krida_4ch_dimmer_c";
uint8_t temp = 100; //Initial value of the register

namespace esphome {
  class Krida4chDimmer : public Component, public i2c::I2CDevice, public output::FloatOutput {
  public:
    Krida4chDimmer() {}
    float get_setup_priority() const override { return esphome::setup_priority::BUS; } //Access I2C bus

    void setup() override {
      ESP_LOGCONFIG(TAG, "Setting up KridaDimmer (0x%02X)...", this->address_);
      auto err = this->write(nullptr, 0);
      if (err != i2c::ERROR_OK) {
        this->error_code_ = COMMUNICATION_FAILED;
        this->mark_failed();
        return;
      }      //Add code here as needed}
    }

    void dump_config() {
      LOG_I2C_DEVICE(this);

      if (this->error_code_ == COMMUNICATION_FAILED) {
        ESP_LOGE(TAG, "Communication with KridaDimmer failed!");
      }
    }

    void write_state(float state) {
      ESP_LOGI(TAG, "Updating dimmer value %3.2f", state);
      const uint16_t value = trunc(state);
      //Did the user change the input?
      this->write_register(REGISTER_ADDRESS, value, 1);
  }
}