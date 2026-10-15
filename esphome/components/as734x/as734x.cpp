#include "as734x.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>

namespace esphome::as734x {

ESPHOME_LOG_TAG(TAG, "as734x");

// Registers and bits shared by both chips
static constexpr uint8_t REG_ENABLE = 0x80;
static constexpr uint8_t REG_ATIME = 0x81;
static constexpr uint8_t REG_ASTATUS = 0x94;
static constexpr uint8_t ENABLE_PON = 0x01;
static constexpr uint8_t ENABLE_SP_EN = 0x02;
static constexpr uint8_t ENABLE_SMUX_EN = 0x10;
static constexpr uint8_t CFG0_REG_BANK = 0x10;
static constexpr uint8_t BANK_SPLIT = 0x80;  // registers below this need REG_BANK set
static constexpr uint8_t STATUS2_AVALID = 0x40;
static constexpr uint8_t ASTATUS_ASAT = 0x80;

static constexpr uint8_t AS7341_CFG6 = 0xAF;
static constexpr uint8_t AS7341_SMUX_CMD_WRITE = 0x10;  // copy the SMUX image from RAM to the chain
static constexpr uint8_t AS7341_SMUX_RAM = 0x00;
static constexpr uint8_t AS7343_CHAIN_CMD = 0xE4;
static constexpr uint8_t AS7343_CHAIN_SMUX = 0xE7;
static constexpr uint8_t AS7343_CHAINS = 3;

// Read one field of a ChipInfo kept in flash
template<typename T> static T pgm(const T &field) {
  T value;
  progmem_memcpy(&value, &field, sizeof(T));
  return value;
}

static constexpr uint8_t NO_SLOT = 0xFF;
static constexpr uint8_t CLEAR_BAND = NUM_BANDS - 1;
static constexpr uint8_t MAX_SMUX_LEN = 20;
static constexpr uint8_t MAX_ADC_SLOTS = 18;
static constexpr uint32_t MIN_COLLECTION_TIMEOUT_MS = 30 * 1000;

#ifdef USE_AS7341
static const char AS7341_NAME[] PROGMEM = "AS7341";
static constexpr uint8_t AS7341_ADC_SLOTS = 6;
static constexpr uint8_t AS7341_SMUX_LEN = 20;
static constexpr uint8_t AS7341_SMUX_STEPS = 2;
static const RegValue AS7341_INIT[] PROGMEM = {{0x70, 0x00}};  // CONFIG: SPM mode, INT_SEL and LED_SEL cleared
static const uint8_t AS7341_SMUX[] PROGMEM = {
    0x30, 0x01, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x50, 0x00,  // step 0: F1 to F4, NIR, Clear
    0x00, 0x00, 0x20, 0x04, 0x00, 0x30, 0x01, 0x50, 0x00, 0x06,  // step 0, continued
    0x00, 0x00, 0x00, 0x40, 0x02, 0x00, 0x10, 0x03, 0x50, 0x10,  // step 1: F5 to F8, NIR, Clear
    0x03, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x50, 0x00, 0x06,  // step 1, continued
};
static_assert(sizeof(AS7341_SMUX) == AS7341_SMUX_STEPS * AS7341_SMUX_LEN && AS7341_SMUX_LEN <= MAX_SMUX_LEN);
static constexpr uint8_t N = NO_SLOT;
static const uint8_t AS7341_READOUT[] PROGMEM = {
    0, 1, N, 2, 3, N, N, N, N, N, N, N, N,  // step 0
    N, N, N, N, N, N, 0, N, 1, 2, 3, 5, 4,  // step 1: SMUX routes clear to ADC4 and NIR to ADC5
};
static_assert(sizeof(AS7341_READOUT) == AS7341_SMUX_STEPS * NUM_BANDS && AS7341_ADC_SLOTS <= MAX_ADC_SLOTS);
const ChipInfo AS7341_CHIP PROGMEM = {
    .name = AS7341_NAME,
    .init = AS7341_INIT,
    .smux = AS7341_SMUX,
    .readout = AS7341_READOUT,
    .init_count = 1,
    .astep_reg = 0xCA,
    .cfg0_reg = 0xA9,
    .cfg1_reg = 0xAA,
    .status2_reg = 0xA3,
    .id_reg = 0x92,
    .id_mask = 0xFC,
    .id_value = 0x09 << 2,
    .adc_slots = AS7341_ADC_SLOTS,
    .smux_steps = AS7341_SMUX_STEPS,
    .integration_cycles = 1,
    .smux_len = AS7341_SMUX_LEN,
    .auto_smux = false,
    .extra_clear_slot = NO_SLOT,
};
#endif

#ifdef USE_AS7343
static const char AS7343_NAME[] PROGMEM = "AS7343";
static constexpr uint8_t AS7343_ADC_SLOTS = 18;
static constexpr uint8_t AS7343_SMUX_LEN = 10;
static const char TCS3448_NAME[] PROGMEM = "TCS3448";
// CFG6, FD_CFG0, CFG10, CFG8, CFG20 (auto_smux, 18 channels in 3 cycles), AGC_GAIN_MAX, FD_TIME_1, FD_TIME_2
static const RegValue AS7343_INIT[] PROGMEM = {{0xF5, 0x00}, {0xDF, 0xA1}, {0x65, 0xF2}, {0xC9, 0xC8},
                                               {0xD6, 0x62}, {0xD7, 0x99}, {0xE0, 0x64}, {0xE2, 0x21}};
static const uint8_t AS7343_SMUX[] PROGMEM = {0x00, 0x04, 0x65, 0x02, 0x00, 0x05, 0x00, 0x01, 0x00, 0x30,
                                              0x00, 0x00, 0x60, 0x20, 0x04, 0x50, 0x03, 0x00, 0x01, 0x05,
                                              0x05, 0x00, 0x60, 0x00, 0x30, 0x00, 0x40, 0x10, 0x20, 0x00};
// Clear is the average of the cycle 1 (slot 4) and cycle 2 (slot 10) readings; cycle 3 always reads 0.
static const uint8_t AS7343_READOUT[] PROGMEM = {12, 6, 0, 7, 8, 1, 15, 2, 9, 13, 14, 3, 10};
static_assert(sizeof(AS7343_SMUX) == AS7343_CHAINS * AS7343_SMUX_LEN && AS7343_SMUX_LEN <= MAX_SMUX_LEN);
static_assert(sizeof(AS7343_READOUT) == NUM_BANDS && AS7343_ADC_SLOTS <= MAX_ADC_SLOTS);
static constexpr ChipInfo as7343_chip(const char *name) {
  return {
      .name = name,
      .init = AS7343_INIT,
      .smux = AS7343_SMUX,
      .readout = AS7343_READOUT,
      .init_count = sizeof(AS7343_INIT) / sizeof(RegValue),
      .astep_reg = 0xD4,
      .cfg0_reg = 0xBF,
      .cfg1_reg = 0xC6,
      .status2_reg = 0x90,
      .id_reg = 0x5A,
      .id_mask = 0xFF,
      .id_value = 0x81,
      .adc_slots = AS7343_ADC_SLOTS,
      .smux_steps = 1,
      .integration_cycles = 3,
      .smux_len = AS7343_SMUX_LEN,
      .auto_smux = true,
      .extra_clear_slot = 4,
  };
}
const ChipInfo AS7343_CHIP PROGMEM = as7343_chip(AS7343_NAME);
const ChipInfo TCS3448_CHIP PROGMEM = as7343_chip(TCS3448_NAME);  // AS7343 register map, address 0x59
#endif

// Registers below 0x80 are only reachable with the REG_BANK bit of CFG0 set; CFG0 itself always answers.
bool AS734XComponent::select_bank_(uint8_t reg) {
  const bool low = reg < BANK_SPLIT;
  if (low == this->bank_low_) {
    return true;
  }
  this->bank_low_ = low;
  return this->write_byte(pgm(this->chip_->cfg0_reg), low ? CFG0_REG_BANK : 0);
}

void AS734XComponent::setup() {
  const ChipInfo *chip = this->chip_;
  const uint8_t id_reg = pgm(chip->id_reg);
  uint8_t id = 0;
  // The sensor keeps its bank across an MCU reset, so the first access always writes CFG0
  bool ok = this->select_bank_(REG_ENABLE) && this->write_byte(REG_ENABLE, ENABLE_PON);
  delayMicroseconds(200);  // oscillator start-up
  ok = ok && this->select_bank_(id_reg) && this->read_byte(id_reg, &id);
  if (ok && (id & pgm(chip->id_mask)) != pgm(chip->id_value)) {
    this->mark_failed(LOG_STR("Wrong chip ID"));
    return;
  }
  const RegValue *init = pgm(chip->init);
  for (uint8_t i = 0, n = pgm(chip->init_count); ok && i < n; i++) {
    RegValue rv;
    progmem_memcpy(&rv, &init[i], sizeof(rv));
    ok = this->select_bank_(rv.reg) && this->write_byte(rv.reg, rv.value);
  }
  // Everything from here on, including the measurement registers, is in the high bank.
  ok = ok && this->select_bank_(REG_ATIME) && this->write_byte(REG_ATIME, this->atime_) &&
       this->write_byte_16(pgm(chip->astep_reg), byteswap(this->astep_)) &&
       this->write_byte(pgm(chip->cfg1_reg), this->gain_);
  if (pgm(chip->auto_smux)) {
    const uint8_t *smux = pgm(chip->smux);
    const uint8_t len = pgm(chip->smux_len);
    for (uint8_t i = 0; ok && i < AS7343_CHAINS * len; i++) {
      ok = this->write_byte(AS7343_CHAIN_SMUX, progmem_read_byte(&smux[i])) &&
           this->write_byte(AS7343_CHAIN_CMD, 0x46 + (i / len) * 0x10);
    }
  }
  if (!ok) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  // One integration cycle lasts (ATIME + 1) * (ASTEP + 1) * 2.78 us.
  const uint32_t cycle_ms = (this->atime_ + 1UL) * (this->astep_ + 1UL) * 139 / 50000;
  this->integration_ms_ = cycle_ms * pgm(chip->integration_cycles);
  this->timeout_ms_ = std::max(MIN_COLLECTION_TIMEOUT_MS, 2 * this->integration_ms_);
  this->disable_loop();
}

void AS734XComponent::dump_config() {
  const float gain = this->gain_ == GAIN_0_5X ? 0.5f : static_cast<float>(1 << (this->gain_ - 1));
  ESP_LOGCONFIG(TAG,
                "AS734x:\n"
                "  Chip: %s\n"
                "  Gain: %gx\n"
                "  ATIME: %u\n"
                "  ASTEP: %u",
                LOG_STR_ARG(pgm(this->chip_->name)), gain, this->atime_, this->astep_);
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  for (auto *sens : this->sensors_) {
    LOG_SENSOR("  ", "Counts", sens);
  }
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}

void AS734XComponent::update() {
  if (this->state_ != State::STATE_IDLE) {
    ESP_LOGW(TAG, "Skipping update, previous measurement still running");
    return;
  }
  this->step_ = 0;
  this->enable_loop();
  if (!this->start_step_()) {
    this->abort_measurement_(LOG_STR("Failed to start measurement"));
  }
}

bool AS734XComponent::start_step_() {
  this->start_ms_ = App.get_loop_component_start_time();
  if (pgm(this->chip_->auto_smux)) {
    this->state_ = State::STATE_READ_DATA;
    return this->write_byte(REG_ENABLE, ENABLE_PON | ENABLE_SP_EN);
  }
  // Load this step's image into SMUX RAM; SMUX_EN copies it to the chain and clears itself when done.
  std::array<uint8_t, MAX_SMUX_LEN> image{};
  const uint8_t len = pgm(this->chip_->smux_len);
  progmem_memcpy(image.data(), pgm(this->chip_->smux) + this->step_ * len, len);
  this->state_ = State::STATE_WAIT_SMUX;
  return this->write_byte(AS7341_CFG6, AS7341_SMUX_CMD_WRITE) &&
         this->write_bytes(AS7341_SMUX_RAM, image.data(), len) &&
         this->write_byte(REG_ENABLE, ENABLE_PON | ENABLE_SMUX_EN);
}

void AS734XComponent::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  uint8_t value = 0;
  switch (this->state_) {
    case State::STATE_WAIT_SMUX:
      // A failed read counts as busy, so the timeout below catches a chip that stops answering.
      if (this->read_byte(REG_ENABLE, &value) && !(value & ENABLE_SMUX_EN)) {
        this->start_ms_ = now;
        this->state_ = State::STATE_READ_DATA;
        if (!this->write_byte(REG_ENABLE, ENABLE_PON | ENABLE_SP_EN)) {
          this->abort_measurement_(LOG_STR("Failed to start integration"));
        }
        return;
      }
      break;
    case State::STATE_READ_DATA:
      if (now - this->start_ms_ < this->integration_ms_) {
        return;
      }
      if (this->read_byte(pgm(this->chip_->status2_reg), &value) && (value & STATUS2_AVALID)) {
        this->read_step_();
        return;
      }
      break;
    case State::STATE_IDLE:
      this->disable_loop();
      return;
  }
  if (now - this->start_ms_ > this->timeout_ms_) {
    this->abort_measurement_(LOG_STR("Measurement timeout"));
  }
}

void AS734XComponent::read_step_() {
  // Reading ASTATUS latches the data, which is only consistent within one burst starting there.
  std::array<uint8_t, 1 + 2 * MAX_ADC_SLOTS> frame{};
  if (!this->read_bytes(REG_ASTATUS, frame.data(), 1 + 2 * pgm(this->chip_->adc_slots)) ||
      !this->write_byte(REG_ENABLE, ENABLE_PON)) {
    this->abort_measurement_(LOG_STR("Failed to read channel data"));
    return;
  }
  if (frame[0] & ASTATUS_ASAT) {
    ESP_LOGV(TAG, "Step %u saturated", this->step_);
  }
  auto slot_value = [&frame](uint8_t slot) { return encode_uint16(frame[2 + 2 * slot], frame[1 + 2 * slot]); };
  const uint8_t *readout = pgm(this->chip_->readout) + this->step_ * NUM_BANDS;
  const uint8_t extra_clear = pgm(this->chip_->extra_clear_slot);
  for (uint8_t band = 0; band < NUM_BANDS; band++) {
    const uint8_t slot = progmem_read_byte(&readout[band]);
    if (slot == NO_SLOT || this->sensors_[band] == nullptr) {
      continue;
    }
    uint32_t counts = slot_value(slot);
    if (band == CLEAR_BAND && extra_clear != NO_SLOT) {
      counts = (counts + slot_value(extra_clear)) / 2;
    }
    this->sensors_[band]->publish_state(counts);
  }
  if (++this->step_ < pgm(this->chip_->smux_steps)) {
    if (!this->start_step_()) {
      this->abort_measurement_(LOG_STR("Failed to configure SMUX"));
    }
    return;
  }
  this->status_clear_warning();
  this->state_ = State::STATE_IDLE;
  this->disable_loop();
}

void AS734XComponent::abort_measurement_(const LogString *reason) {
  ESP_LOGW(TAG, "%s", LOG_STR_ARG(reason));
  this->write_byte(REG_ENABLE, ENABLE_PON);
  this->status_set_warning(reason);
  this->state_ = State::STATE_IDLE;
  this->disable_loop();
}

}  // namespace esphome::as734x
