#include "gt9895_touchscreen.h"

#include <memory>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::gt9895 {

ESPHOME_LOG_TAG(TAG, "gt9895.touchscreen");

static constexpr uint32_t INFO_TIMEOUT_ID = 0;
// The controller answers about 100 ms after its reset is released
static constexpr uint32_t RESET_STARTUP_MS = 100;

// Offsets in the runtime information block
static constexpr size_t RUNTIME_ARRAYS_OFFSET = 32;  // length, version, features and fixed parameters
static constexpr size_t RUNTIME_ARRAY_COUNT = 5;
static constexpr size_t RUNTIME_MISC_SIZE = 48;
static constexpr size_t RUNTIME_TOUCH_ADDRESS_OFFSET = 44;  // in the miscellaneous part after the arrays

void GT9895Touchscreen::setup() {
  if (this->reset_pin_ != nullptr) {
    pulse_reset(this->reset_pin_, {.high_before_ms = 1, .low_ms = 10});
    this->set_timeout(INFO_TIMEOUT_ID, RESET_STARTUP_MS, [this]() { this->read_info_(); });
  } else {
    this->read_info_();
  }
}

void GT9895Touchscreen::read_info_() {
  uint8_t fw[GT9895_FW_VERSION_SIZE];
  if (!this->read_(GT9895_REG_FW_VERSION, fw, sizeof(fw)) || !checksum_ok(fw, sizeof(fw))) {
    this->mark_failed(LOG_STR("Failed to read the firmware version"));
    return;
  }
  memcpy(this->product_id_, fw + 10, 8);
  this->firmware_version_ = encode_uint32(fw[21], fw[20], fw[19], fw[18]);

  // The runtime information gives the address of the touch reports
  uint8_t len_bytes[2];
  if (!this->read_(GT9895_REG_RUNTIME_INFO, len_bytes, sizeof(len_bytes))) {
    this->mark_failed(LOG_STR("Failed to read the runtime information"));
    return;
  }
  const size_t len = encode_uint16(len_bytes[1], len_bytes[0]);
  if (len < RUNTIME_ARRAYS_OFFSET + RUNTIME_ARRAY_COUNT + RUNTIME_MISC_SIZE + GT9895_CHECKSUM_SIZE ||
      len > GT9895_RUNTIME_INFO_MAX_SIZE) {
    ESP_LOGE(TAG, "Runtime information length %u", (unsigned) len);
    this->mark_failed(LOG_STR("Invalid runtime information"));
    return;
  }
  auto info = std::make_unique<uint8_t[]>(len);
  if (!this->read_(GT9895_REG_RUNTIME_INFO, info.get(), len) || !checksum_ok(info.get(), len)) {
    this->mark_failed(LOG_STR("Failed to read the runtime information"));
    return;
  }
  const size_t end = len - GT9895_CHECKSUM_SIZE;
  size_t offset = RUNTIME_ARRAYS_OFFSET;
  for (size_t i = 0; i != RUNTIME_ARRAY_COUNT && offset < end; i++)
    offset += 1 + info[offset] * 2;
  if (offset + RUNTIME_MISC_SIZE > end) {
    this->mark_failed(LOG_STR("Invalid runtime information"));
    return;
  }
  const uint8_t *addr = &info[offset + RUNTIME_TOUCH_ADDRESS_OFFSET];
  this->touch_data_address_ = encode_uint32(addr[3], addr[2], addr[1], addr[0]);
  if (this->touch_data_address_ == 0) {
    this->mark_failed(LOG_STR("Invalid runtime information"));
    return;
  }

  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);
  }
}

void GT9895Touchscreen::update_touches() {
  if (this->touch_data_address_ == 0) {
    this->skip_update_ = true;
    return;
  }
  uint8_t data[GT9895_HEADER_SIZE + GT9895_MAX_TOUCHES * GT9895_CONTACT_SIZE + GT9895_CHECKSUM_SIZE];
  size_t len = GT9895_HEADER_SIZE + GT9895_FIRST_READ_CONTACTS * GT9895_CONTACT_SIZE + GT9895_CHECKSUM_SIZE;
  if (!this->status_comm_result(this->read_(this->touch_data_address_, data, len))) {
    this->skip_update_ = true;
    return;
  }
  // No new report since the last one was cleared: keep the current touches
  if (data[0] == 0 || !checksum_ok(data, GT9895_HEADER_SIZE)) {
    this->skip_update_ = true;
    return;
  }

  const uint8_t count = std::min<uint8_t>(data[2] & 0x0F, GT9895_MAX_TOUCHES);
  if ((data[0] & GT9895_EVENT_TOUCH) != 0 && count != 0) {
    const size_t needed = GT9895_HEADER_SIZE + count * GT9895_CONTACT_SIZE + GT9895_CHECKSUM_SIZE;
    if (needed > len) {
      if (!this->status_comm_result(this->read_(this->touch_data_address_ + len, data + len, needed - len))) {
        this->skip_update_ = true;
        return;
      }
    }
    const uint8_t *contacts = data + GT9895_HEADER_SIZE;
    if (!checksum_ok(contacts, count * GT9895_CONTACT_SIZE + GT9895_CHECKSUM_SIZE)) {
      ESP_LOGV(TAG, "Bad contact checksum");
      this->skip_update_ = true;
    } else {
      for (uint8_t i = 0; i != count; i++) {
        const uint8_t *p = contacts + i * GT9895_CONTACT_SIZE;
        const uint16_t x = encode_uint16(p[3], p[2]);
        const uint16_t y = encode_uint16(p[5], p[4]);
        const uint16_t pressure = encode_uint16(p[7], p[6]);
        ESP_LOGV(TAG, "Touch %u: x=%u, y=%u, pressure=%u", p[0] >> 4, x, y, pressure);
        this->add_raw_touch_position_(p[0] >> 4, x, y, pressure);
      }
    }
  }

  // Clear the report so the controller can write the next one
  uint8_t clear[5];
  put_be32(clear, this->touch_data_address_);
  clear[4] = 0;
  this->status_comm_result(this->write(clear, sizeof(clear)) == i2c::ERROR_OK);
}

bool GT9895Touchscreen::read_(uint32_t address, uint8_t *data, size_t len) {
  uint8_t addr[4];
  put_be32(addr, address);
  return this->write_read(addr, sizeof(addr), data, len) == i2c::ERROR_OK;
}

bool GT9895Touchscreen::checksum_ok(const uint8_t *data, size_t len) {
  const size_t body = len - GT9895_CHECKSUM_SIZE;
  return sum16_bytes(data, body) == encode_uint16(data[body + 1], data[body]);
}

void GT9895Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG,
                "GT9895 Touchscreen:\n"
                "  Product ID: %s\n"
                "  Firmware version: 0x%08" PRIX32 "\n"
                "  X Raw Min: %d, X Raw Max: %d\n"
                "  Y Raw Min: %d, Y Raw Max: %d",
                this->product_id_, this->firmware_version_, this->x_raw_min_, this->x_raw_max_, this->y_raw_min_,
                this->y_raw_max_);
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
}

}  // namespace esphome::gt9895
