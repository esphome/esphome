#include "esphome/core/log.h"
#include "xensiv_pas_co2_base.h"

namespace esphome {
namespace xensiv_pas_co2_base {
static const char *const TAG = "xensiv_pas_co2.component";

void XensivPasCO2::setup() {
  ESP_LOGCONFIG(TAG, "Setting up XensivPasCO2 component");

  // Test I2C communication first using scratch register
  if (!this->test_scratch_register_()) {
    ESP_LOGE(TAG, "I2C communication test failed");
    this->mark_failed();
    return;
  }

  // Perform full sensor reset (reset sticky bits, set to idle state)

  // Soft reset - use XENSIV_PAS_CO2_CMD_SOFT_RESET command
  if (this->write_byte(XENSIV_PAS_CO2_REG_SENS_RST, XENSIV_PAS_CO2_CMD_SOFT_RESET)) {
    ESP_LOGCONFIG(TAG, "Sensor soft reset");
  } else {
    ESP_LOGW(TAG, "Failed to perform sensor soft reset");
    this->mark_failed();
  }

  // Schedule sensor initialization after a delay to avoid blocking setup
  this->set_timeout(XENSIV_PAS_CO2_SOFT_RESET_DELAY_MS, [this]() { XensivPasCO2::setup_sensor_(this); });
}

void XensivPasCO2::loop() {
  // Only process data if sensor is initialized
  if (!this->initialized_) {
    return;
  }

  // Check if data is ready via interrupt
  if (this->data_ready_) {
    this->data_ready_ = false;  // Clear flag

    // Read CO2 data
    this->read_co2_ppm();

    // Clear MEAS_STS INT_STS_CLR bit
    this->write_byte(XENSIV_PAS_CO2_REG_MEAS_STS, XENSIV_PAS_CO2_REG_MEAS_STS_INT_STS_CLR_MSK);

    // Update operation mode if needed
    this->update_operation_mode_();
  }
}

void XensivPasCO2::setup_sensor_(XensivPasCO2 *arg) {
  ESP_LOGCONFIG(TAG, "Starting sensor configuration...");

  if (!arg->update_sensor_rate_()) {
    ESP_LOGE(TAG, "Failed to set sensor rate");
  }

  // Set pressure compensation if configured
  if (arg->pressure_ref_ > 0) {
    uint8_t press_h = (arg->pressure_ref_ >> 8) & 0xFF;
    uint8_t press_l = arg->pressure_ref_ & 0xFF;

    ESP_LOGD(TAG, "Setting pressure compensation to %d Pa", arg->pressure_ref_);

    if (!arg->write_byte(XENSIV_PAS_CO2_REG_PRESS_REF_H, press_h)) {
      ESP_LOGE(TAG, "Failed to write PRESS_REF_H");
    }
    if (!arg->write_byte(XENSIV_PAS_CO2_REG_PRESS_REF_L, press_l)) {
      ESP_LOGE(TAG, "Failed to write PRESS_REF_L");
    }
  } else {
    ESP_LOGD(TAG, "Pressure compensation not configured, using sensor default");
  }

  // Configure sensor interrupt register and GPIO pin if configured
  if (!arg->setup_interrupt_()) {
    ESP_LOGE(TAG, "Failed to setup interrupt");
    arg->mark_failed();
  }

  if (!arg->update_operation_mode_()) {
    ESP_LOGE(TAG, "Failed to set operation mode");
    arg->mark_failed();
  }

  // Testing single shot measurement to finalize initialization
  arg->set_timeout(XENSIV_PAS_CO2_SINGLE_SHOT_DELAY_MS, [arg]() {
    if (!arg->check_sensor_ready_()) {
      ESP_LOGW(TAG, "Sensor not ready after single shot");
    } else {
      arg->initialized_ = true;
      ESP_LOGD(TAG, "Sensor initialized");
    }
  });
}

bool XensivPasCO2::test_scratch_register_() {
  uint8_t read_val = 0;

  // Write test pattern to scratch register
  if (!this->write_byte(XENSIV_PAS_CO2_REG_SCRATCH_PAD, XENSIV_PAS_CO2_COMM_TEST_VAL)) {
    ESP_LOGE(TAG, "Failed to write to scratch register");
    return false;
  }

  // Read back the value
  if (!this->read_byte(XENSIV_PAS_CO2_REG_SCRATCH_PAD, &read_val)) {
    ESP_LOGE(TAG, "Failed to read from scratch register");
    return false;
  }

  // Verify the value matches
  if (read_val != XENSIV_PAS_CO2_COMM_TEST_VAL) {
    ESP_LOGE(TAG, "Scratch register test failed: expected 0x%02X, got 0x%02X", XENSIV_PAS_CO2_COMM_TEST_VAL, read_val);
    return false;
  }

  ESP_LOGD(TAG, "Scratch register test passed");
  return true;
}

void XensivPasCO2::gpio_intr_(XensivPasCO2 *arg) { arg->data_ready_ = true; }

bool XensivPasCO2::setup_interrupt_() {
  // Configure interrupt: DRDY function (data ready), active low
  xensiv_pas_co2_interrupt_config_t int_cfg;
  int_cfg.u = 0;
  int_cfg.b.int_func = XENSIV_PAS_CO2_INTERRUPT_FUNCTION_DRDY;
  int_cfg.b.int_typ = XENSIV_PAS_CO2_INTERRUPT_TYPE_LOW_ACTIVE;
  int_cfg.b.alarm_typ = XENSIV_PAS_CO2_ALARM_TYPE_LOW_TO_HIGH;

  if (!this->write_byte(XENSIV_PAS_CO2_REG_INT_CFG, int_cfg.u)) {
    ESP_LOGW(TAG, "Failed to configure interrupt register");
    return false;
  }

  ESP_LOGCONFIG(TAG, "Interrupt register configured (active low, data ready)");

  // Set up interrupt pin if configured
  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    // Input only - sensor has push-pull output (active low)
    this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT);
    this->interrupt_pin_->attach_interrupt(XensivPasCO2::gpio_intr_, this,
                                           gpio::INTERRUPT_FALLING_EDGE  // Active low interrupt
    );
    ESP_LOGCONFIG(TAG, "  Interrupt GPIO pin configured (active low)");
  }

  return true;
}

bool XensivPasCO2::update_operation_mode_() {
  if (this->continuous_operation_mode_) {
    // Read current measurement config
    xensiv_pas_co2_measurement_config_t current_meas_cfg;
    if (this->read_bytes(XENSIV_PAS_CO2_REG_MEAS_CFG, &current_meas_cfg.u, 1)) {
      if (current_meas_cfg.b.op_mode != XENSIV_PAS_CO2_OP_MODE_CONTINUOUS) {
        // Set to continuous measurement mode with automatic baseline offset compensation
        xensiv_pas_co2_measurement_config_t meas_cfg;
        meas_cfg.u = 0;
        meas_cfg.b.op_mode = XENSIV_PAS_CO2_OP_MODE_CONTINUOUS;
        meas_cfg.b.boc_cfg = XENSIV_PAS_CO2_BOC_CFG_AUTOMATIC;
        meas_cfg.b.pwm_mode = XENSIV_PAS_CO2_PWM_MODE_SINGLE_PULSE;
        meas_cfg.b.pwm_outen = 0;  // PWM output disabled

        bool success = this->write_byte(XENSIV_PAS_CO2_REG_MEAS_CFG, meas_cfg.u);

        if (success) {
          ESP_LOGD(TAG, "Sensor reverted to continuous measurement mode");
          return true;
        } else {
          ESP_LOGW(TAG, "Failed to set sensor to continuous measurement mode");
          return false;
        }
      }
      // Already in continuous mode, nothing to do
      return true;
    } else {
      ESP_LOGW(TAG, "Failed to read MEAS_CFG register");
      return false;
    }
  }
  // this->continuous_operation_mode_ is false, nothing to do
  return true;
}

bool XensivPasCO2::update_sensor_rate_() {
  // Rate validation is done in sensor.py (5-4095 seconds)
  // Rate is stored as 12-bit value across MEAS_RATE_H and MEAS_RATE_L registers
  uint16_t rate = this->sensor_rate_;
  uint8_t rate_h = (rate >> 8) & 0xFF;  // Upper byte
  uint8_t rate_l = rate & 0xFF;         // Lower byte

  ESP_LOGD(TAG, "Setting sensor rate to %d seconds", rate);

  if (!this->write_byte(XENSIV_PAS_CO2_REG_MEAS_RATE_H, rate_h)) {
    ESP_LOGE(TAG, "Failed to write MEAS_RATE_H");
    return false;
  }
  if (!this->write_byte(XENSIV_PAS_CO2_REG_MEAS_RATE_L, rate_l)) {
    ESP_LOGE(TAG, "Failed to write MEAS_RATE_L");
    return false;
  }
  return true;
}

void XensivPasCO2::set_pressure_compensation(uint16_t pressure_ref) {
  this->pressure_ref_ = pressure_ref;

  // If sensor is already initialized, write the value immediately
  if (this->initialized_) {
    // If pressure_ref is 0, skip setting (use sensor default)
    if (pressure_ref == 0) {
      ESP_LOGD(TAG, "Pressure compensation set to 0, using sensor default");
      return;
    }

    // Pressure reference is stored as 16-bit value in Pascal units
    uint8_t press_h = (pressure_ref >> 8) & 0xFF;  // Upper byte
    uint8_t press_l = pressure_ref & 0xFF;         // Lower byte

    ESP_LOGD(TAG, "Setting pressure compensation to %d Pa", pressure_ref);

    if (!this->write_byte(XENSIV_PAS_CO2_REG_PRESS_REF_H, press_h)) {
      ESP_LOGE(TAG, "Failed to write PRESS_REF_H");
      return;
    }
    if (!this->write_byte(XENSIV_PAS_CO2_REG_PRESS_REF_L, press_l)) {
      ESP_LOGE(TAG, "Failed to write PRESS_REF_L");
      return;
    }
  }
  // If not initialized yet, value will be written during setup_sensor_()
}

bool XensivPasCO2::check_sensor_ready_() {
  xensiv_pas_co2_status_t sens_sts;

  // Read sensor status register
  if (!this->read_byte(XENSIV_PAS_CO2_REG_SENS_STS, &sens_sts.u)) {
    ESP_LOGE(TAG, "Failed to read SENS_STS register");
    return false;
  }

  // Check if sensor is ready
  if (!sens_sts.b.sen_rdy) {
    ESP_LOGW(TAG, "Sensor not ready (SEN_RDY bit is 0)");
    return false;
  }

  // Check for errors
  if (sens_sts.b.iccerr) {
    ESP_LOGW(TAG, "Communication error detected (ICCERR)");
  }
  if (sens_sts.b.orvs) {
    ESP_LOGW(TAG, "Out-of-range VDD12V error (ORVS)");
  }
  if (sens_sts.b.ortmp) {
    ESP_LOGW(TAG, "Out-of-range temperature error (ORTMP)");
  }

  ESP_LOGD(TAG, "Sensor is ready (SEN_RDY=1)");
  return true;
}

bool XensivPasCO2::measure_now() {
  // Start single-shot measurement with automatic baseline offset compensation
  xensiv_pas_co2_measurement_config_t meas_cfg;
  meas_cfg.u = 0;
  meas_cfg.b.op_mode = XENSIV_PAS_CO2_OP_MODE_SINGLE;
  meas_cfg.b.boc_cfg = XENSIV_PAS_CO2_BOC_CFG_AUTOMATIC;
  meas_cfg.b.pwm_mode = XENSIV_PAS_CO2_PWM_MODE_SINGLE_PULSE;
  meas_cfg.b.pwm_outen = 0;  // PWM output disabled

  if (this->write_byte(XENSIV_PAS_CO2_REG_MEAS_CFG, meas_cfg.u)) {
    ESP_LOGD(TAG, "Starting single-shot measurement");
    return true;
  } else {
    ESP_LOGW(TAG, "Failed to start single-shot measurement");
    return false;
  }
}

void XensivPasCO2::read_co2_ppm() {
  ESP_LOGD(TAG, "Reading CO2 data...");

  uint8_t co2_ppm_val[2] = {0};
  xensiv_pas_co2_meas_status_t meas_sts;

  // Check DRDY flag
  if (this->read_bytes(XENSIV_PAS_CO2_REG_MEAS_STS, &meas_sts.u, 1)) {
    ESP_LOGD(TAG, "MEAS_STS: 0x%02X, DRDY: %s, INT_STS: %s, ALARM: %s", meas_sts.u, meas_sts.b.drdy ? "SET" : "NOT SET",
             meas_sts.b.int_sts ? "SET" : "NOT SET", meas_sts.b.alarm ? "SET" : "NOT SET");

    if (meas_sts.b.drdy) {
      if (this->read_bytes(XENSIV_PAS_CO2_REG_CO2PPM_H, co2_ppm_val, 2)) {
        // Read CO2PPM_H and CO2PPM_L
        uint8_t co2ppm_h = co2_ppm_val[0];
        uint8_t co2ppm_l = co2_ppm_val[1];
        int16_t co2_raw = (static_cast<int16_t>(co2ppm_h) << 8) | co2ppm_l;
        this->co2_ppm_ = static_cast<float>(co2_raw);

        if (this->co2_sensor_ != nullptr) {
          this->co2_sensor_->publish_state(this->co2_ppm_);
        }
      } else {
        ESP_LOGW(TAG, "Failed to read CO2 concentration registers");
      }
    } else {
      ESP_LOGD(TAG, "DRDY not set, CO2 value not ready");
    }
  } else {
    ESP_LOGW(TAG, "Failed to read MEAS_STS register for DRDY check");
  }
}

void XensivPasCO2::dump_config() {
  ESP_LOGCONFIG(TAG, "XENSIV PASCO2 CO2 Sensor:");

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with PASCO2 failed!");
  }

  if (this->co2_sensor_ != nullptr) {
    LOG_SENSOR("  ", "CO2 Sensor", this->co2_sensor_);
  }

  if (this->interrupt_pin_ != nullptr) {
    LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  } else {
    ESP_LOGCONFIG(TAG, "  Interrupt Pin: Not configured");
  }

  ESP_LOGCONFIG(TAG, "  Operation Mode: %s", this->continuous_operation_mode_ ? "Continuous" : "Single-shot");

  ESP_LOGCONFIG(TAG, "  Measurement Rate: %d seconds", this->sensor_rate_);

  if (this->pressure_ref_ > 0) {
    ESP_LOGCONFIG(TAG, "  Pressure Compensation: %d Pa (%.2f hPa)", this->pressure_ref_, this->pressure_ref_ / 100.0f);
  } else {
    ESP_LOGCONFIG(TAG, "  Pressure Compensation: Using sensor default : 1015 hPa");
  }
}
}  // namespace xensiv_pas_co2_base
}  // namespace esphome
