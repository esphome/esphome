#include "esphome/core/log.h"
#include "xensiv_pas_co2_base.h"

namespace esphome {
namespace xensiv_pas_co2_base {
static const char *const TAG = "xensiv_pas_co2.component";

void XensivPasCO2::setup() {
  ESP_LOGCONFIG(TAG, "Setting up XensivPasCO2 component");

  // Test I2C communication first using scratch register
  for (int i = 0; i < 3; i++) {
    if (this->test_scratch_register_()) {
      ESP_LOGCONFIG(TAG, "I2C communication test passed on attempt %d", i + 1);
      break;
    } else if (i < 2) {
      ESP_LOGW(TAG, "I2C communication test attempt %d failed, retrying...", i + 1);
    } else {
      ESP_LOGE(TAG, "I2C communication test failed");
      this->failure_reason_ += "I2C communication test failed";
      this->mark_failed();
      return;
    }
  }

  // Set up pressure compensation source callback early if configured
  if (this->pressure_compensation_source_ != nullptr) {
    this->pressure_compensation_source_->add_on_state_callback([this](float pressure_hpa) {
      ESP_LOGD(TAG, "Pressure compensation source updated: %.2f hPa", pressure_hpa);
      this->set_pressure_compensation((uint16_t) pressure_hpa);
    });
    ESP_LOGCONFIG(TAG, "Pressure compensation source callback registered");
  }

  // Perform full sensor reset (reset sticky bits, set to idle state)
  // Soft reset - use XENSIV_PAS_CO2_CMD_SOFT_RESET command
  if (this->write_byte(XENSIV_PAS_CO2_REG_SENS_RST, XENSIV_PAS_CO2_CMD_SOFT_RESET)) {
    ESP_LOGCONFIG(TAG, "Sensor soft reset");
  } else {
    ESP_LOGW(TAG, "Failed to perform sensor soft reset");
    this->failure_reason_ += "Failed to perform sensor soft reset";
    this->mark_failed();
  }

  // Schedule sensor initialization after a delay to avoid blocking setup
  this->set_timeout(XENSIV_PAS_CO2_SOFT_RESET_DELAY_MS, [this]() { XensivPasCO2::setup_sensor(this); });
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

    // Apply pending pressure compensation immediately after a DRDY cycle boundary
    if (this->pending_pressure_update_) {
      uint16_t ref = this->pending_pressure_ref_;
      this->pending_pressure_update_ = false;
      // Skip if default requested
      if (ref == 0) {
        ESP_LOGD(TAG, "Pressure compensation pending was 0 (default), skipping");
      } else {
        uint8_t press_h = (ref >> 8) & 0xFF;
        uint8_t press_l = ref & 0xFF;
        // ESP_LOGD(TAG, "Applying pending pressure compensation: %d hPa", ref);
        if (!this->write_with_retry_(XENSIV_PAS_CO2_REG_PRESS_REF_H, press_h)) {
          ESP_LOGW(TAG, "Retry write failed for PRESS_REF_H, re-queueing");
          // Re-queue once; avoid tight loops
          this->pending_pressure_update_ = true;
          this->pending_pressure_ref_ = ref;
        } else if (!this->write_with_retry_(XENSIV_PAS_CO2_REG_PRESS_REF_L, press_l)) {
          ESP_LOGW(TAG, "Retry write failed for PRESS_REF_L, re-queueing");
          this->pending_pressure_update_ = true;
          this->pending_pressure_ref_ = ref;
        } else {
          // ESP_LOGD(TAG, "Pending pressure compensation applied successfully");
        }
      }
    }
  }
}

void XensivPasCO2::setup_sensor(XensivPasCO2 *arg) {
  ESP_LOGCONFIG(TAG, "Starting sensor configuration...");

  if (!arg->update_sensor_rate_()) {
    ESP_LOGE(TAG, "Failed to set sensor rate");
  }

  // Apply pressure compensation using common setter (handles 0/default internally)
  arg->set_pressure_compensation(arg->pressure_ref_);

  // Configure sensor interrupt register and GPIO pin if configured
  if (!arg->setup_interrupt_()) {
    arg->failure_reason_ += "Failed to set up interrupt; ";
    arg->mark_failed();
  }

  if (!arg->update_operation_mode_()) {
    arg->failure_reason_ += "Failed to set operation mode; ";
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

void XensivPasCO2::gpio_intr(XensivPasCO2 *arg) { arg->data_ready_ = true; }

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
    this->interrupt_pin_->attach_interrupt(XensivPasCO2::gpio_intr, this,
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
        // Set to continuous measurement mode without baseline offset compensation
        xensiv_pas_co2_measurement_config_t meas_cfg;
        meas_cfg.u = 0;
        meas_cfg.b.op_mode = XENSIV_PAS_CO2_OP_MODE_CONTINUOUS;
        meas_cfg.b.boc_cfg = XENSIV_PAS_CO2_BOC_CFG_DISABLE;
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
  // If pressure_ref is 0, skip setting (use sensor default)
  if (pressure_ref == 0) {
    ESP_LOGD(TAG, "Pressure compensation set to 0, using sensor default");
    // Clear any pending
    this->pending_pressure_update_ = false;
    this->pending_pressure_ref_ = 0;
    return;
  }

  // Pressure reference is stored as 16-bit value in hPa units (1 bit = 1 hPa)
  uint8_t press_h = (pressure_ref >> 8) & 0xFF;  // Upper byte
  uint8_t press_l = pressure_ref & 0xFF;         // Lower byte

  ESP_LOGD(TAG, "Setting pressure compensation to %d hPa", pressure_ref);

  // If we're inside a measurement cycle, writes can sporadically fail.
  // Queue the update to be applied right after next DRDY handling.
  this->pending_pressure_update_ = true;
  this->pending_pressure_ref_ = pressure_ref;
}
bool XensivPasCO2::write_with_retry_(uint8_t reg, uint8_t value, int retries, uint32_t delay_ms) {
  for (int attempt = 0; attempt <= retries; attempt++) {
    if (this->write_byte(reg, value)) {
      return true;
    }
    if (attempt < retries) {
      ESP_LOGD(TAG, "I2C write 0x%02X failed (attempt %d), retrying after %d ms", reg, attempt + 1, (int) delay_ms);
    }
  }
  return false;
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
  // Start single-shot measurement without automatic baseline offset compensation
  xensiv_pas_co2_measurement_config_t meas_cfg;
  meas_cfg.u = 0;
  meas_cfg.b.op_mode = XENSIV_PAS_CO2_OP_MODE_SINGLE;
  meas_cfg.b.boc_cfg = XENSIV_PAS_CO2_BOC_CFG_DISABLE;
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

void XensivPasCO2::reset_ABOC() {
  // Reset Automatic Baseline Offset Compensation (ABOC)
  if (this->write_byte(XENSIV_PAS_CO2_REG_SENS_RST, XENSIV_PAS_CO2_CMD_RESET_ABOC)) {
    ESP_LOGD(TAG, "ABOC reset command sent");
  } else {
    ESP_LOGW(TAG, "Failed to send ABOC reset command");
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
    ESP_LOGE(TAG, "Failure Reason: %s", this->failure_reason_.c_str());
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
    ESP_LOGCONFIG(TAG, "  Pressure Compensation: %d hPa", this->pressure_ref_);
  } else {
    ESP_LOGCONFIG(TAG, "  Pressure Compensation: Using sensor default (1015 hPa)");
  }
}

}  // namespace xensiv_pas_co2_base
}  // namespace esphome
