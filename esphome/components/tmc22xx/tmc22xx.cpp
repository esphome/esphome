#include "tmc22xx.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cmath>
#include <cstring>
#include <numbers>

namespace esphome::tmc22xx {

ESPHOME_LOG_TAG(TAG, "tmc22xx");

static constexpr uint8_t SYNC_BYTE = 0x05;
static constexpr uint8_t MASTER_ADDRESS = 0xFF;
static constexpr uint8_t WRITE_BIT = 0x80;
static constexpr size_t READ_REQUEST_SIZE = 4;
static constexpr size_t DATAGRAM_SIZE = 8;
static constexpr uint8_t DEFAULT_TOFF = 3;
static constexpr float SENSE_RESISTOR_OFFSET = 0.02f;  // Ohm, added to RSENSE in the datasheet current formula
static constexpr float INTERNAL_RSENSE_OHM = 0.17f;    // Equivalent sense resistance when using internal sensing
// INDEX pulses are counted once per loop, so near the target the speed is limited to cover the remaining
// distance in no less than this time. A slow loop pass then cannot run far past the target.
static constexpr float APPROACH_TIME_S = 0.02f;
// Skip VACTUAL writes that change the speed by less than 1/16, each write blocks the loop for the UART transfer
static constexpr int32_t VACTUAL_CHANGE_DIVISOR = 16;
static constexpr uint32_t STATUS_POLL_INTERVAL_MS = 1000;
// While the driver does not answer, for example without motor supply, only poll every 10 s; each failed read
// blocks the loop for the UART timeout
static constexpr uint8_t UNREACHABLE_POLL_SKIP = 9;
static constexpr uint32_t GSTAT_FLAGS = 0x07;  // reset, drv_err, uv_cp
static constexpr uint32_t DRV_STATUS_FLAGS = 0x0FFF;

static const LogString *driver_event_to_string(DriverEvent event) {
  switch (event) {
    case DRIVER_EVENT_RESET:
      return LOG_STR("Driver reset");
    case DRIVER_EVENT_DRIVER_ERROR:
      return LOG_STR("Driver error");
    case DRIVER_EVENT_CHARGE_PUMP_UNDERVOLTAGE:
      return LOG_STR("Charge pump undervoltage");
    case DRIVER_EVENT_OVERTEMPERATURE_PREWARNING:
      return LOG_STR("Overtemperature prewarning");
    case DRIVER_EVENT_OVERTEMPERATURE:
      return LOG_STR("Overtemperature shutdown");
    case DRIVER_EVENT_SHORT_TO_GROUND_A:
      return LOG_STR("Short to ground on phase A");
    case DRIVER_EVENT_SHORT_TO_GROUND_B:
      return LOG_STR("Short to ground on phase B");
    case DRIVER_EVENT_LOW_SIDE_SHORT_A:
      return LOG_STR("Low side short on phase A");
    case DRIVER_EVENT_LOW_SIDE_SHORT_B:
      return LOG_STR("Low side short on phase B");
    case DRIVER_EVENT_OPEN_LOAD_A:
      return LOG_STR("Open load on phase A");
    case DRIVER_EVENT_OPEN_LOAD_B:
      return LOG_STR("Open load on phase B");
    case DRIVER_EVENT_TEMPERATURE_120C:
      return LOG_STR("Temperature above 120C");
    case DRIVER_EVENT_TEMPERATURE_143C:
      return LOG_STR("Temperature above 143C");
    case DRIVER_EVENT_TEMPERATURE_150C:
      return LOG_STR("Temperature above 150C");
    case DRIVER_EVENT_TEMPERATURE_157C:
      return LOG_STR("Temperature above 157C");
    default:
      return LOG_STR("Unknown");
  }
}

// CRC-8 with polynomial 0x07 over the datagram bits in the order they go out on the wire, LSB first
static uint8_t datagram_crc(const uint8_t *data, size_t len) {
  uint8_t crc = 0;
  for (size_t i = 0; i < len; i++) {
    const uint8_t reflected = reverse_bits(data[i]);
    crc = crc8(&reflected, 1, crc, 0x07, true);
  }
  return crc;
}

// Index into RESTORED_REGISTERS, or -1 when the register is not restored after a driver reset
static int restored_index(uint8_t reg) {
  for (size_t i = 0; i < RESTORED_REGISTERS.size(); i++) {
    if (RESTORED_REGISTERS[i] == reg)
      return static_cast<int>(i);
  }
  return -1;
}

void IRAM_ATTR IndexPulseStore::gpio_intr(IndexPulseStore *arg) { arg->pulses = arg->pulses + arg->direction; }

void IRAM_ATTR DiagPinStore::gpio_intr(DiagPinStore *arg) { arg->triggered = true; }

void TMC22XXStepper::setup() {
  if (this->enn_pin_ != nullptr) {
    this->enn_pin_->setup();
    this->enn_pin_->digital_write(true);
  }

  auto version = this->read_field(VERSION);
  if (!version.has_value()) {
    ESP_LOGE(TAG, "No response from driver at address %u", this->address_);
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  this->version_ = *version;
  if (this->version_ != this->expected_version()) {
    ESP_LOGW(TAG, "Unexpected IC version 0x%02X", this->version_);
  }

  this->restore_registers();
  this->write_register(REG_GSTAT, GSTAT_FLAGS);
  this->apply_config_();
  // Currents live in a write-only register, so later changes survive a driver reset through its cached value
  if (!std::isnan(this->initial_run_current_))
    this->set_run_current(this->initial_run_current_);
  if (!std::isnan(this->initial_hold_current_))
    this->set_hold_current(this->initial_hold_current_);

  if (this->index_pin_ != nullptr) {
    this->index_pin_->setup();
    this->index_pin_->attach_interrupt(IndexPulseStore::gpio_intr, &this->index_store_, gpio::INTERRUPT_ANY_EDGE);
  } else {
    this->step_pin_->setup();
    this->step_pin_->digital_write(false);
    this->dir_pin_->setup();
    this->dir_pin_->digital_write(false);
  }

  if (this->diag_pin_ != nullptr) {
    this->diag_pin_->setup();
    this->diag_pin_->attach_interrupt(DiagPinStore::gpio_intr, &this->diag_store_, gpio::INTERRUPT_RISING_EDGE);
  }
  // Polling notices a driver reset, and conditions that clear without a DIAG edge
  this->set_interval(STATUS_POLL_INTERVAL_MS, [this]() {
    if (this->status_poll_skip_ > 0) {
      this->status_poll_skip_--;
      return;
    }
    if (!this->update_status_())
      this->status_poll_skip_ = UNREACHABLE_POLL_SKIP;
  });

  this->set_enabled(true);
}

void TMC22XXStepper::restore_registers() {
  this->write_register(REG_IHOLD_IRUN, this->ihold_irun_);
  this->write_register(REG_TPOWERDOWN, this->tpowerdown_);
  this->write_register(REG_TPWMTHRS, this->tpwmthrs_);
  this->write_register(REG_VACTUAL, 0);
  for (size_t i = 0; i < RESTORED_REGISTERS.size(); i++) {
    if ((this->restored_written_ & (1u << i)) != 0)
      this->write_register(RESTORED_REGISTERS[i], this->restored_values_[i]);
  }
}

void TMC22XXStepper::apply_config_() {
  this->write_field(PDN_DISABLE, true);
  this->write_field(MSTEP_REG_SELECT, true);
  this->write_field(INTERNAL_RSENSE, std::isnan(this->rsense_));
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

  this->set_microsteps(this->initial_microsteps_);

  if (this->index_pin_ != nullptr) {
    // INDEX toggles on every step of the internal pulse generator that VACTUAL drives
    this->write_field(INDEX_OTPW, false);
    this->write_field(INDEX_STEP, true);
  } else {
    // Step on both edges, so every step pin toggle is one step
    this->write_field(DEDGE, true);
    this->write_field(MULTISTEP_FILT, false);
  }
}

bool TMC22XXStepper::update_status_() {
  auto gstat = this->read_register(REG_GSTAT);
  if (!gstat.has_value())
    return false;
  uint32_t drv_status = 0;
  // DRV_STATUS only feeds on_status and the DIAG check, skip the transfer when neither is used
  if (this->diag_pin_ != nullptr || !this->status_callback_.empty()) {
    auto value = this->read_register(REG_DRV_STATUS);
    if (!value.has_value())
      return false;
    drv_status = *value;
  }
  // GSTAT flags stay set until written with 1; they are set again while the condition lasts
  if ((*gstat & GSTAT_FLAGS) != 0)
    this->write_register(REG_GSTAT, *gstat & GSTAT_FLAGS);

  uint16_t status = (*gstat & GSTAT_FLAGS) | ((drv_status & DRV_STATUS_FLAGS) << 3);
  // Restore before reporting, so an on_status automation for the reset sees and can change the restored settings
  if ((status & (1u << DRIVER_EVENT_RESET)) != 0) {
    ESP_LOGW(TAG, "Restoring settings after driver reset");
    this->restore_registers();
    // The motor stopped with the reset, ramp up again from standstill
    this->current_speed_ = 0.0f;
    this->set_enabled(this->enabled_);
  }
  uint16_t changed = status ^ this->status_;
  this->status_ = status;
  for (uint8_t event = 0; event < DRIVER_EVENT_COUNT; event++) {
    if ((changed & (1u << event)) == 0)
      continue;
    bool active = (status & (1u << event)) != 0;
    // Open load and temperature levels are informational, the driver keeps running
    if (active && event < DRIVER_EVENT_OPEN_LOAD_A) {
      ESP_LOGW(TAG, "%s", LOG_STR_ARG(driver_event_to_string(static_cast<DriverEvent>(event))));
    } else {
      ESP_LOGD(TAG, "%s: %s", LOG_STR_ARG(driver_event_to_string(static_cast<DriverEvent>(event))),
               active ? LOG_STR_LITERAL("active") : LOG_STR_LITERAL("cleared"));
    }
    this->status_callback_.call(static_cast<DriverEvent>(event), active);
  }
  return true;
}

void TMC22XXStepper::dump_config() {
  ESP_LOGCONFIG(TAG,
                "  Address: %u\n"
                "  IC version: 0x%02X",
                this->address_, this->version_);
  // Register reads on a failed driver would only wait for the UART timeout
  if (!this->is_failed()) {
    uint8_t irun = this->read_field(IRUN).value_or(0);
    uint8_t ihold = this->read_field(IHOLD).value_or(0);
    ESP_LOGCONFIG(TAG, "  Microsteps: %u", this->get_microsteps());
    if (this->analog_current_scale_) {
      ESP_LOGCONFIG(TAG,
                    "  Run current scale: %u\n"
                    "  Hold current scale: %u\n"
                    "  Current scaled by VREF",
                    irun, ihold);
    } else {
      ESP_LOGCONFIG(TAG,
                    "  Run current: %.2f A\n"
                    "  Hold current: %.2f A\n"
                    "  Max current: %.2f A",
                    this->scale_to_current(irun), this->scale_to_current(ihold), this->scale_to_current(31));
    }
  }
  LOG_PIN("  ENN Pin: ", this->enn_pin_);
  LOG_PIN("  STEP Pin: ", this->step_pin_);
  LOG_PIN("  DIR Pin: ", this->dir_pin_);
  LOG_PIN("  INDEX Pin: ", this->index_pin_);
  LOG_PIN("  DIAG Pin: ", this->diag_pin_);
  LOG_STEPPER(this);
}

void TMC22XXStepper::loop() {
  if (this->index_pin_ != nullptr)
    this->add_index_pulses_();
  if (this->diag_store_.triggered) {
    this->diag_store_.triggered = false;
    // DIAG rises on a driver error, any other rise is chip specific
    if (!this->update_status_()) {
      ESP_LOGW(TAG, "DIAG rose, but the driver status could not be read");
    } else if ((this->status_ & GSTAT_FLAGS) == 0) {
      this->on_diag_without_error();
    }
  }

  if (this->has_reached_target()) {
    this->high_freq_.stop();
    this->moving_ = false;
  } else {
    this->high_freq_.start();
    if (!this->enabled_)
      this->set_enabled(true);
    // An unreachable driver is polled only every 10 s. Read it before the first step, so settings lost when the
    // motor supply dropped are restored before the move runs on the chip defaults.
    if (!this->moving_ && this->status_poll_skip_ > 0) {
      this->status_poll_skip_ = this->update_status_() ? 0 : UNREACHABLE_POLL_SKIP;
    }
    this->moving_ = true;
  }

  if (this->index_pin_ != nullptr) {
    this->loop_serial_();
  } else {
    this->loop_step_dir_();
  }
}

void TMC22XXStepper::add_index_pulses_() {
  InterruptLock lock;
  this->current_position += this->index_store_.pulses;
  this->index_store_.pulses = 0;
}

void TMC22XXStepper::loop_serial_() {
  int32_t remaining = this->target_position - this->current_position;
  int8_t direction = (remaining > 0) - (remaining < 0);
  // Ran past the target, restart the ramp from standstill in the other direction
  if (direction != 0 && direction == -this->direction_)
    this->current_speed_ = 0.0f;
  this->calculate_speed_(micros());
  this->current_speed_ = std::min(this->current_speed_, static_cast<float>(std::abs(remaining)) / APPROACH_TIME_S);

  int32_t vactual = direction * this->speed_to_vactual_(this->current_speed_);
  int32_t last = this->get_vactual_();
  if (vactual == last)
    return;
  if (vactual != 0 && (vactual > 0) == (last > 0) && std::abs(vactual - last) < std::abs(last) / VACTUAL_CHANGE_DIVISOR)
    return;
  // Pulses that arrive while VACTUAL changes direction are counted in the new direction
  this->index_store_.direction = direction;
  this->direction_ = direction;
  this->write_field(VACTUAL, static_cast<uint32_t>(vactual));
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
  if (this->get_vactual_() != 0)
    this->write_field(VACTUAL, 0);
}

void TMC22XXStepper::set_enabled(bool enabled) {
  if (!enabled) {
    if (this->get_vactual_() != 0)
      this->write_field(VACTUAL, 0);
    if (this->index_pin_ != nullptr) {
      // Count the pulses up to the stop, so the position is current before the target is set
      this->add_index_pulses_();
      this->index_store_.direction = 0;
      this->direction_ = 0;
    }
    // Stop at the current position, so the motor does not run off again when re-enabled
    this->target_position = this->current_position;
  }
  if (this->enn_pin_ != nullptr) {
    this->enn_pin_->digital_write(!enabled);
  } else {
    this->write_field(TOFF, enabled ? this->toff_ : 0);
  }
  this->enabled_ = enabled;
}

void TMC22XXStepper::set_toff(uint8_t toff) {
  this->toff_ = toff;
  // Without an ENN pin, TOFF=0 is what keeps the outputs disabled
  if (this->enabled_ || this->enn_pin_ != nullptr)
    this->write_field(TOFF, toff);
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

void TMC22XXStepper::set_run_current(float current) {
  if (auto scale = this->current_to_scale_(current))
    this->write_field(IRUN, *scale);
}

void TMC22XXStepper::set_hold_current(float current) {
  if (auto scale = this->current_to_scale_(current))
    this->write_field(IHOLD, *scale);
}

float TMC22XXStepper::full_scale_current_() {
  return (this->vsense_active_ ? 0.180f : 0.325f) /
         ((std::isnan(this->rsense_) ? INTERNAL_RSENSE_OHM : this->rsense_) + SENSE_RESISTOR_OFFSET);
}

float TMC22XXStepper::scale_to_current(uint8_t scale) {
  return (std::min<uint8_t>(scale, 31) + 1) / 32.0f * this->full_scale_current_() / std::numbers::sqrt2_v<float>;
}

optional<uint8_t> TMC22XXStepper::current_to_scale_(float current) {
  if (this->analog_current_scale_) {
    ESP_LOGW(TAG, "Current in A is not supported with analog current scale, use irun/ihold");
    return {};
  }
  float scale = 32.0f * std::numbers::sqrt2_v<float> * current / this->full_scale_current_() - 1.0f;
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

uint32_t *TMC22XXStepper::shadow_register(uint8_t reg) {
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
  datagram[7] = datagram_crc(datagram, 7);

  while (this->available())
    this->read();
  this->write_array(datagram, DATAGRAM_SIZE);
  this->flush();
  // On the single wire bus every byte sent is also received, drop that echo
  uint8_t echo[DATAGRAM_SIZE];
  if (!this->read_array(echo, DATAGRAM_SIZE) || memcmp(echo, datagram, DATAGRAM_SIZE) != 0) {
    ESP_LOGV(TAG, "No echo writing register 0x%02X", reg);
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return false;
  }
  this->status_clear_warning();

  if (uint32_t *shadow = this->shadow_register(reg)) {
    *shadow = value;
  } else if (int index = restored_index(reg); index >= 0) {
    this->restored_values_[index] = value;
    this->restored_written_ |= 1u << index;
  }
  return true;
}

optional<uint32_t> TMC22XXStepper::read_register(uint8_t reg) {
  if (uint32_t *shadow = this->shadow_register(reg))
    return *shadow;

  uint8_t request[READ_REQUEST_SIZE] = {SYNC_BYTE, this->address_, reg, 0};
  request[3] = datagram_crc(request, 3);

  while (this->available())
    this->read();
  this->write_array(request, READ_REQUEST_SIZE);
  this->flush();
  uint8_t reply[DATAGRAM_SIZE];
  if (!this->read_array(reply, READ_REQUEST_SIZE) || !this->read_array(reply, DATAGRAM_SIZE)) {
    ESP_LOGV(TAG, "No reply reading register 0x%02X", reg);
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return {};
  }
  if (reply[0] != SYNC_BYTE || reply[1] != MASTER_ADDRESS || reply[2] != reg || reply[7] != datagram_crc(reply, 7)) {
    ESP_LOGV(TAG, "Invalid reply reading register 0x%02X", reg);
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return {};
  }
  this->status_clear_warning();
  return encode_uint32(reply[3], reply[4], reply[5], reply[6]);
}

bool TMC22XXStepper::write_field(const RegisterField &field, uint32_t value) {
  // Skip the UART timeout when the driver never answered during setup
  if (this->is_failed())
    return false;
  optional<uint32_t> data;
  int index = restored_index(field.reg);
  if (index >= 0 && (this->restored_written_ & (1u << index)) != 0) {
    // Modify the last written value, not the chip: after a driver reset that the poll has not seen yet, the chip
    // holds its defaults, and those would replace the values to restore
    data = this->restored_values_[index];
  } else {
    data = this->read_register(field.reg);
  }
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
