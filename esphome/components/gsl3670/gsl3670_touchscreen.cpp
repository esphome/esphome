#include "gsl3670_touchscreen.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace gsl3670 {

static const char *const TAG = "gsl3670.touchscreen";

// ---------------------------------------------------------------------------
// setup() – mirrors esp_lcd_touch_gsl3670_init() in the Seeed BSP:
//   clear_reg → reset → load_fw → startup_chip → reset → startup_chip
// ---------------------------------------------------------------------------
void GSL3670Touchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up GSL3670 touchscreen...");

  if (reset_pin_ != nullptr) {
    reset_pin_->setup();
    reset_pin_->digital_write(true);
    delay(20);
  }

  if (interrupt_pin_ != nullptr) {
    interrupt_pin_->setup();
    this->attach_interrupt_(interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);
  }

  // Initialisation sequence from Seeed BSP esp_lcd_touch_gsl3670_init()
  clear_reg_();
  reset_();
  load_firmware_();
  startup_chip_();
  reset_();
  startup_chip_();

  ESP_LOGCONFIG(TAG, "GSL3670 initialised OK");
}

void GSL3670Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "GSL3670 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Reset Pin:     ", reset_pin_);
  LOG_PIN("  Interrupt Pin: ", interrupt_pin_);
  ESP_LOGCONFIG(TAG, "  Firmware records: %zu", firmware_len_);
}

// ---------------------------------------------------------------------------
// update_touches() – mirrors esp_lcd_touch_gsl3670_read_data() in Seeed BSP
// ---------------------------------------------------------------------------
void GSL3670Touchscreen::update_touches() {
  uint8_t buf[44] = {};
  if (!read_reg_(0x80, buf, 44)) {
    ESP_LOGW(TAG, "I2C read failed");
    return;
  }

  uint8_t finger_num = buf[0];

  // Build gsl_touch_info exactly as the Seeed driver does
  struct gsl_touch_info cinfo = {};
  cinfo.finger_num = finger_num;

  for (int j = 0; j < finger_num && j < 10; j++) {
    // buf[(j+1)*4 + 0..3]:  byte0=y_lo, byte1=y_hi, byte2=x_lo, byte3=id|x_hi
    cinfo.x[j] = (int) (((buf[(j + 1) * 4 + 3] & 0x0f) << 8) | buf[(j + 1) * 4 + 2]);
    cinfo.y[j] = (int) ((buf[(j + 1) * 4 + 1] << 8) | buf[(j + 1) * 4 + 0]);
    cinfo.id[j] = (buf[(j + 1) * 4 + 3] >> 4) & 0x0f;
  }

  // Run the Silead point-ID algorithm (mandatory for correct multi-touch)
  gsl_alg_id_main(&cinfo);

  // Handle tiaoping (screen-reset signal from the algorithm)
  uint32_t mask = gsl_mask_tiaoping();
  if (mask > 0 && mask < 0xffffffff) {
    uint8_t tmp[4];
    tmp[0] = 0x0a;
    tmp[1] = 0;
    tmp[2] = 0;
    tmp[3] = 0;
    write_reg_(0xf0, tmp, 4);
    tmp[0] = (uint8_t) (mask & 0xff);
    tmp[1] = (uint8_t) ((mask >> 8) & 0xff);
    tmp[2] = (uint8_t) ((mask >> 16) & 0xff);
    tmp[3] = (uint8_t) ((mask >> 24) & 0xff);
    write_reg_(0x08, tmp, 4);
  }

  finger_num = (uint8_t) cinfo.finger_num;
  for (int j = 0; j < finger_num; j++) {
    ESP_LOGV(TAG, "Touch[%d] id=%d x=%d y=%d", j, cinfo.id[j], cinfo.x[j], cinfo.y[j]);
    this->add_raw_touch_position_((uint8_t) cinfo.id[j], (uint16_t) cinfo.x[j], (uint16_t) cinfo.y[j]);
  }
}

// ---------------------------------------------------------------------------
// clear_reg_() – mirrors esp_lcd_touch_gsl3670_clear_reg()
//   GPIO reset → write 0x01 to 0x88 → write 0x04 to 0xe4 → write 0x00 to 0xe0
// ---------------------------------------------------------------------------
void GSL3670Touchscreen::clear_reg_() {
  ESP_LOGD(TAG, "clear_reg");

  // GPIO reset pulse
  if (reset_pin_ != nullptr) {
    reset_pin_->digital_write(false);
    delay(20);
    reset_pin_->digital_write(true);
    delay(20);
  }

  write_reg8_(0x88, 0x01);
  delay(5);
  write_reg8_(0xe4, 0x04);
  delay(5);
  write_reg8_(0xe0, 0x00);
  delay(20);
}

// ---------------------------------------------------------------------------
// reset_() – mirrors touch_gsl3670_reset()
//   GPIO reset → write 0x04 to 0xe4 → write 4×0x00 to 0xbc
// ---------------------------------------------------------------------------
void GSL3670Touchscreen::reset_() {
  ESP_LOGD(TAG, "reset");

  if (reset_pin_ != nullptr) {
    reset_pin_->digital_write(false);
    delay(20);
    reset_pin_->digital_write(true);
    delay(20);
  }

  write_reg8_(0xe4, 0x04);
  delay(10);

  uint8_t zeros[4] = {0, 0, 0, 0};
  write_reg_(0xbc, zeros, 4);
  delay(10);
}

// ---------------------------------------------------------------------------
// load_firmware_() – mirrors esp_lcd_touch_gsl3670_load_fw()
//   Iterates the {offset, val} table:
//   - offset == 0xf0 → write only the low byte to reg 0xf0 (page select)
//   - otherwise      → write all 4 bytes (LE) to the register
// ---------------------------------------------------------------------------
void GSL3670Touchscreen::load_firmware_() {
  if (firmware_ == nullptr || firmware_len_ == 0) {
    ESP_LOGW(TAG, "No firmware supplied – skipping");
    return;
  }

  ESP_LOGD(TAG, "Loading firmware (%zu records)...", firmware_len_);

  for (size_t i = 0; i < firmware_len_; i++) {
    uint8_t reg = (uint8_t) firmware_[i].offset;
    uint32_t val = firmware_[i].val;

    if (reg == 0xf0) {
      // Page select: write only 1 byte
      write_reg8_(0xf0, (uint8_t) (val & 0xff));
    } else {
      // Data write: 4 bytes little-endian
      write_reg32_(reg, val);
    }

    // Yield periodically to avoid WDT
    if ((i & 0x3f) == 0x3f)
      delay(1);
  }

  ESP_LOGD(TAG, "Firmware load complete");
}

// ---------------------------------------------------------------------------
// startup_chip_() – mirrors esp_lcd_touch_gsl3670_startup_chip()
//   write 0x00 to 0xe0 → call gsl_DataInit with the config blob
// ---------------------------------------------------------------------------
void GSL3670Touchscreen::startup_chip_() {
  ESP_LOGD(TAG, "startup_chip");
  write_reg8_(0xe0, 0x00);
  delay(10);

  if (config_ != nullptr && config_len_ > 0) {
    // gsl_DataInit takes a non-const pointer; the data is read-only in practice
    gsl_DataInit(const_cast<uint32_t *>(this->config_));
  } else {
    ESP_LOGW(TAG, "No config data for gsl_DataInit");
  }
}

// ---------------------------------------------------------------------------
// I2C helpers
// ---------------------------------------------------------------------------

bool GSL3670Touchscreen::write_reg_(uint8_t reg, const uint8_t *data, size_t len) {
  // ESPHome I2CDevice::write_register writes [reg][data...]
  auto err = this->write_register(reg, data, len);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C write reg 0x%02X failed (%d)", reg, err);
    return false;
  }
  return true;
}

bool GSL3670Touchscreen::write_reg32_(uint8_t reg, uint32_t val) {
  uint8_t buf[4] = {
      (uint8_t) (val & 0xff),
      (uint8_t) ((val >> 8) & 0xff),
      (uint8_t) ((val >> 16) & 0xff),
      (uint8_t) ((val >> 24) & 0xff),
  };
  return write_reg_(reg, buf, 4);
}

bool GSL3670Touchscreen::write_reg8_(uint8_t reg, uint8_t val) { return write_reg_(reg, &val, 1); }

bool GSL3670Touchscreen::read_reg_(uint8_t reg, uint8_t *data, size_t len) {
  auto err = this->read_register(reg, data, len);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C read reg 0x%02X failed (%d)", reg, err);
    return false;
  }
  return true;
}

}  // namespace gsl3670
}  // namespace esphome
