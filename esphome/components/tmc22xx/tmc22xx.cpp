#include "tmc22xx.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome::tmc22xx {

static const char *const TAG = "tmc22xx";

static constexpr uint8_t SYNC_BYTE = 0x05;
static constexpr uint8_t MASTER_ADDRESS = 0xFF;
static constexpr uint8_t WRITE_BIT = 0x80;
static constexpr size_t READ_REQUEST_SIZE = 4;
static constexpr size_t DATAGRAM_SIZE = 8;
static constexpr uint8_t DEFAULT_TOFF = 3;
static constexpr float SENSE_RESISTOR_OFFSET = 0.02f;  // Ohm, added to RSENSE in the datasheet current formula
static constexpr float INTERNAL_RSENSE_OHM = 0.17f;    // Equivalent sense resistance when using internal sensing

static uint8_t crc8(const uint8_t *data, size_t len) {
  uint8_t crc = 0;
  for (size_t i = 0; i < len; i++) {
    uint8_t byte = data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      if ((crc >> 7) ^ (byte & 0x01)) {
        crc = (crc << 1) ^ 0x07;
      } else {
        crc = crc << 1;
      }
      byte >>= 1;
    }
  }
  return crc;
}

void IRAM_ATTR IndexPulseStore::gpio_intr(IndexPulseStore *arg) { arg->pulses = arg->pulses + arg->direction; }

void TMC22XXStepper::setup() {
  if (this->enn_pin_ != nullptr) {
    this->enn_pin_->setup();
    this->enn_pin_->digital_write(true);
  }

  auto version = this->read_field(VERSION);
  if (!version.has_value()) {
    ESP_LOGE(TAG, "No response from driver at address %u", this->address_);
    this->mark_failed();
    return;
  }
  this->version_ = *version;
  if (this->version_ != this->expected_version_()) {
    ESP_LOGW(TAG, "Unexpected IC version 0x%02X", this->version_);
  }

  // Bring write-only registers in line with their cached values and clear the reset flag
  this->write_register(REG_IHOLD_IRUN, this->ihold_irun_);
  this->write_register(REG_TPOWERDOWN, this->tpowerdown_);
  this->write_register(REG_TPWMTHRS, this->tpwmthrs_);
  this->write_register(REG_VACTUAL, 0);
  this->write_register(REG_GSTAT, 0x07);

  this->write_field(PDN_DISABLE, true);
  this->write_field(MSTEP_REG_SELECT, true);
  this->write_field(INTERNAL_RSENSE, !this->rsense_.has_value());
  this->write_field(I_SCALE_ANALOG, this->analog_current_scale_);
  if (this->vsense_.has_value())
    this->write_field(VSENSE, *this->vsense_);
  if (this->ottrim_.has_value())
    this->write_field(OTTRIM, *this->ottrim_);
  this->vsense_active_ = this->read_field(VSENSE).value_or(0);

  // TOFF=0 disables the outputs, keep the current value so it can be restored on enable
  auto toff = this->read_field(TOFF);
  if (toff.has_value() && *toff != 0)
    this->toff_ = *toff;

  if (this->initial_microsteps_.has_value())
    this->set_microsteps(*this->initial_microsteps_);
  if (this->initial_run_current_.has_value())
    this->set_run_current(*this->initial_run_current_);
  if (this->initial_hold_current_.has_value())
    this->set_hold_current(*this->initial_hold_current_);

  if (this->index_pin_ != nullptr) {
    // INDEX toggles on every step of the internal pulse generator that VACTUAL drives
    this->write_field(INDEX_OTPW, false);
    this->write_field(INDEX_STEP, true);
    this->index_pin_->setup();
    this->index_pin_->attach_interrupt(IndexPulseStore::gpio_intr, &this->index_store_, gpio::INTERRUPT_ANY_EDGE);
  } else {
    // Step on both edges, so every step pin toggle is one step
    this->write_field(DEDGE, true);
    this->write_field(MULTISTEP_FILT, false);
    this->step_pin_->setup();
    this->step_pin_->digital_write(false);
    this->dir_pin_->setup();
    this->dir_pin_->digital_write(false);
  }

  this->set_enabled(true);
}

void TMC22XXStepper::dump_config() {
  ESP_LOGCONFIG(TAG,
                "  Address: %u\n"
                "  IC version: 0x%02X\n"
                "  Microsteps: %u\n"
                "  Run current: %.2f A\n"
                "  Hold current: %.2f A\n"
                "  Max current: %.2f A",
                this->address_, this->version_, this->get_microsteps(),
                this->scale_to_current(this->read_field(IRUN).value_or(0)),
                this->scale_to_current(this->read_field(IHOLD).value_or(0)), this->scale_to_current(31));
  LOG_PIN("  ENN Pin: ", this->enn_pin_);
  LOG_PIN("  STEP Pin: ", this->step_pin_);
  LOG_PIN("  DIR Pin: ", this->dir_pin_);
  LOG_PIN("  INDEX Pin: ", this->index_pin_);
  LOG_STEPPER(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}

void TMC22XXStepper::loop() {
  if (this->has_reached_target()) {
    this->high_freq_.stop();
  } else {
    this->high_freq_.start();
    if (!this->enabled_)
      this->set_enabled(true);
  }

  if (this->index_pin_ != nullptr) {
    this->loop_serial_();
  } else {
    this->loop_step_dir_();
  }
}

void TMC22XXStepper::loop_serial_() {
  int32_t pulses;
  {
    InterruptLock lock;
    pulses = this->index_store_.pulses;
    this->index_store_.pulses = 0;
  }
  this->current_position += pulses;

  this->calculate_speed_(micros());
  int8_t direction = 0;
  if (this->target_position > this->current_position) {
    direction = 1;
  } else if (this->target_position < this->current_position) {
    direction = -1;
  }

  int32_t vactual = direction * this->speed_to_vactual_(this->current_speed_);
  if (vactual == this->vactual_)
    return;
  // Pulses that arrive while VACTUAL changes direction are counted in the new direction
  this->index_store_.direction = direction;
  if (this->write_field(VACTUAL, static_cast<uint32_t>(vactual)))
    this->vactual_ = vactual;
}

void TMC22XXStepper::loop_step_dir_() {
  int32_t direction = this->should_step_();
  if (direction == 0)
    return;
  if (direction != this->direction_) {
    this->dir_pin_->digital_write(direction > 0);
    this->direction_ = direction;
    delayMicroseconds(1);
  }
  this->step_state_ = !this->step_state_;
  this->step_pin_->digital_write(this->step_state_);
}

void TMC22XXStepper::on_shutdown() {
  if (this->vactual_ != 0)
    this->write_field(VACTUAL, 0);
}

void TMC22XXStepper::set_enabled(bool enabled) {
  if (!enabled) {
    // Stop at the current position, so the motor does not run off again when re-enabled
    this->target_position = this->current_position;
    if (this->vactual_ != 0 && this->write_field(VACTUAL, 0))
      this->vactual_ = 0;
  }
  if (this->enn_pin_ != nullptr) {
    this->enn_pin_->digital_write(!enabled);
  } else {
    this->write_field(TOFF, enabled ? this->toff_ : 0);
  }
  this->enabled_ = enabled;
}

void TMC22XXStepper::set_microsteps(uint16_t microsteps) {
  for (uint8_t mres = 0; mres <= 8; mres++) {
    if ((256u >> mres) == microsteps) {
      this->write_field(MRES, mres);
      return;
    }
  }
  ESP_LOGW(TAG, "Invalid microsteps %u", microsteps);
}

uint16_t TMC22XXStepper::get_microsteps() {
  auto mres = this->read_field(MRES);
  if (!mres.has_value() || *mres > 8)
    return 0;
  return 256u >> *mres;
}

float TMC22XXStepper::full_scale_voltage_() {
  return (this->vsense_active_ ? 0.180f : 0.325f) /
         (this->rsense_.value_or(INTERNAL_RSENSE_OHM) + SENSE_RESISTOR_OFFSET);
}

float TMC22XXStepper::scale_to_current(uint8_t scale) {
  return (std::min<uint8_t>(scale, 31) + 1) / 32.0f * this->full_scale_voltage_() / std::sqrt(2.0f);
}

uint8_t TMC22XXStepper::current_to_scale_(float current) {
  float scale = 32.0f * std::sqrt(2.0f) * current / this->full_scale_voltage_() - 1.0f;
  if (scale > 31.0f) {
    ESP_LOGW(TAG, "%.2f A is above the %.2f A limit, using the limit", current, this->scale_to_current(31));
    return 31;
  }
  return static_cast<uint8_t>(std::max(0.0f, std::round(scale)));
}

int32_t TMC22XXStepper::speed_to_vactual_(float speed) const {
  // VACTUAL is in microsteps per 2^24 clock cycles
  return static_cast<int32_t>(std::lround(speed * 16777216.0f / this->clock_frequency_));
}

uint32_t *TMC22XXStepper::shadow_register_(uint8_t reg) {
  switch (reg) {
    case REG_IHOLD_IRUN:
      return &this->ihold_irun_;
    case REG_TPOWERDOWN:
      return &this->tpowerdown_;
    case REG_TPWMTHRS:
      return &this->tpwmthrs_;
    case REG_VACTUAL:
      return &this->vactual_reg_;
    default:
      return nullptr;
  }
}

bool TMC22XXStepper::write_register(uint8_t reg, uint32_t value) {
  uint8_t datagram[DATAGRAM_SIZE] = {SYNC_BYTE,
                                     this->address_,
                                     static_cast<uint8_t>(reg | WRITE_BIT),
                                     static_cast<uint8_t>(value >> 24),
                                     static_cast<uint8_t>(value >> 16),
                                     static_cast<uint8_t>(value >> 8),
                                     static_cast<uint8_t>(value),
                                     0};
  datagram[7] = crc8(datagram, 7);

  while (this->available())
    this->read();
  this->write_array(datagram, DATAGRAM_SIZE);
  this->flush();
  // On the single wire bus every byte sent is also received, drop that echo
  uint8_t echo[DATAGRAM_SIZE];
  this->read_array(echo, DATAGRAM_SIZE);

  if (uint32_t *shadow = this->shadow_register_(reg))
    *shadow = value;
  return true;
}

optional<uint32_t> TMC22XXStepper::read_register(uint8_t reg) {
  if (uint32_t *shadow = this->shadow_register_(reg))
    return *shadow;

  uint8_t request[READ_REQUEST_SIZE] = {SYNC_BYTE, this->address_, reg, 0};
  request[3] = crc8(request, 3);

  while (this->available())
    this->read();
  this->write_array(request, READ_REQUEST_SIZE);
  this->flush();
  uint8_t reply[DATAGRAM_SIZE];
  if (!this->read_array(reply, READ_REQUEST_SIZE) || !this->read_array(reply, DATAGRAM_SIZE)) {
    ESP_LOGV(TAG, "No reply reading register 0x%02X", reg);
    return {};
  }
  if (reply[0] != SYNC_BYTE || reply[1] != MASTER_ADDRESS || reply[2] != reg || reply[7] != crc8(reply, 7)) {
    ESP_LOGV(TAG, "Invalid reply reading register 0x%02X", reg);
    return {};
  }
  return encode_uint32(reply[3], reply[4], reply[5], reply[6]);
}

bool TMC22XXStepper::write_field(const RegisterField &field, uint32_t value) {
  auto data = this->read_register(field.reg);
  if (!data.has_value())
    return false;
  uint32_t mask = field.mask();
  return this->write_register(field.reg, (*data & ~mask) | ((value << field.shift) & mask));
}

optional<uint32_t> TMC22XXStepper::read_field(const RegisterField &field) {
  auto data = this->read_register(field.reg);
  if (!data.has_value())
    return {};
  return extract_field(*data, field);
}

uint32_t TMC22XXStepper::extract_field(uint32_t data, const RegisterField &field) {
  uint32_t value = (data & field.mask()) >> field.shift;
  if (field.is_signed && field.width < 32) {
    uint32_t sign_bit = 1u << (field.width - 1);
    value = (value ^ sign_bit) - sign_bit;
  }
  return value;
}

}  // namespace esphome::tmc22xx
