#include "tas2780.h"

#include <cmath>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::tas2780 {

static const char *const TAG = "tas2780";

static constexpr uint8_t TAS2780_PAGE_SELECT = 0x00;  // Page Select
static constexpr uint8_t TAS2780_PAGE_0 = 0x00;       // Page 0
static constexpr uint8_t TAS2780_PAGE_1 = 0x01;       // Page 1
static constexpr uint8_t TAS2780_PAGE_FD = 0xFD;      // Page 0xFD

/* PAGE 0 */
static constexpr uint8_t TAS2780_SW_RESET = 0x01;      // Software Reset
static constexpr uint8_t TAS2780_SW_RESET_CMD = 0x01;  // Trigger software reset
static constexpr uint8_t TAS2780_MODE_CTRL = 0x02;     // Device operational mode
static constexpr uint8_t TAS2780_MODE_CTRL_MODE_MASK = 0x07;
static constexpr uint8_t TAS2780_MODE_CTRL_MODE_ACTIVE = 0x00;
static constexpr uint8_t TAS2780_MODE_CTRL_MODE_ACTIVE_MUTED = 0x01;
static constexpr uint8_t TAS2780_MODE_CTRL_MODE_SFTW_SHTDWN = 0x02;

static constexpr uint8_t TAS2780_CHNL_0 = 0x03;  // Y Bridge and Channel settings
static constexpr uint8_t TAS2780_CHNL_0_CDS_MODE_SHIFT = 6;
static constexpr uint8_t TAS2780_CHNL_0_CDS_MODE_MASK = (0x03 << TAS2780_CHNL_0_CDS_MODE_SHIFT);
static constexpr uint8_t TAS2780_CHNL_0_AMP_LEVEL_SHIFT = 1;
static constexpr uint8_t TAS2780_CHNL_0_AMP_LEVEL_MASK = (0x1F) << TAS2780_CHNL_0_AMP_LEVEL_SHIFT;
static constexpr uint8_t TAS2780_AMP_LEVEL_MAX = 0x14;  // Codes above 20 are reserved

static constexpr uint8_t TAS2780_DC_BLK0 = 0x04;  // SAR Filter and DC Path Blocker
static constexpr uint8_t TAS2780_DC_BLK0_VBAT1S_MODE_MASK = (1 << 7);
static constexpr uint8_t TAS2780_DC_BLK1 = 0x05;            // Record DC Blocker
static constexpr uint8_t TAS2780_DC_BLK1_RESET_VAL = 0x41;  // Presence check, there is no WHO_AM_I register

static constexpr uint8_t TAS2780_TDM_CFG2 = 0x0A;  // TDM Configuration 2
static constexpr uint8_t TAS2780_TDM_CFG2_RX_SCFG_SHIFT = 4;
static constexpr uint8_t TAS2780_TDM_CFG2_RX_SCFG_MASK = (3 << TAS2780_TDM_CFG2_RX_SCFG_SHIFT);
static constexpr uint8_t TAS2780_TDM_CFG2_RX_WLEN_SHIFT = 2;
static constexpr uint8_t TAS2780_TDM_CFG2_RX_WLEN_MASK = (3 << TAS2780_TDM_CFG2_RX_WLEN_SHIFT);
static constexpr uint8_t TAS2780_TDM_CFG2_RX_WLEN_32BIT = (3 << TAS2780_TDM_CFG2_RX_WLEN_SHIFT);
static constexpr uint8_t TAS2780_TDM_CFG2_RX_SLEN_MASK = (3 << 0);
static constexpr uint8_t TAS2780_TDM_CFG2_RX_SLEN_32BIT = 2;

static constexpr uint8_t TAS2780_TDM_CFG5 = 0x0E;                   // TDM Configuration 5
static constexpr uint8_t TAS2780_TDM_CFG5_TX_VSNS_EN_SLOT4 = 0x44;  // vsns TX enable, slot 4
static constexpr uint8_t TAS2780_TDM_CFG6 = 0x0F;                   // TDM Configuration 6
static constexpr uint8_t TAS2780_TDM_CFG6_TX_ISNS_EN_SLOT0 = 0x40;  // isns TX enable, slot 0

static constexpr uint8_t TAS2780_DVC = 0x1A;           // Digital Volume Control
static constexpr uint8_t TAS2780_DVC_MAX_ATTEN = 200;  // 0 dB (0x00) to -100 dB (0xC8) in 0.5 dB steps

/* Interrupts */
static constexpr uint8_t TAS2780_INT_MASK_ALL = 0xFF;  // Mask all interrupts
static constexpr uint8_t TAS2780_INT_MASK0 = 0x3B;     // Interrupt Mask 0
static constexpr uint8_t TAS2780_INT_MASK1 = 0x3C;     // Interrupt Mask 1
static constexpr uint8_t TAS2780_INT_MASK1_0 = 0x3D;   // Interrupt Mask 1_0 (INT_LTCH1_0 group)
static constexpr uint8_t TAS2780_INT_MASK2 = 0x40;     // Interrupt Mask 2
static constexpr uint8_t TAS2780_INT_MASK3 = 0x41;     // Interrupt Mask 3
static constexpr uint8_t TAS2780_INT_LTCH0 = 0x49;     // Latched Interrupt Read-back 0, 1 and 1_0 follow
static constexpr uint8_t TAS2780_INT_LTCH2 = 0x4F;     // Latched Interrupt Read-back 2

static constexpr uint8_t TAS2780_INT_CLK_CFG = 0x5C;                // Clock Setting and IRQZ
static constexpr uint8_t TAS2780_INT_CLK_CFG_CLR_LATCH = (1 << 2);  // Clear interrupt latches
static constexpr uint8_t TAS2780_INT_CLK_CFG_MODE_MASK = 0x03;      // Trigger mode field mask
static constexpr uint8_t TAS2780_INT_CLK_CFG_MODE_LIVE = 0x00;      // Trigger on any unmasked live interrupt
static constexpr uint8_t TAS2780_PVDD_UVLO = 0x71;                  // UVLO Threshold
static constexpr uint8_t TAS2780_PVDD_UVLO_2V76 = 0x03;             // PVDD UVLO threshold = 2.76V

/* PAGE 0x01 */
static constexpr uint8_t TAS2780_INIT_0 = 0x17;        // Initialization
static constexpr uint8_t TAS2780_INIT_0_VAL = 0xC8;    // SARBurstMask=0, CMP_HYST_LP=1
static constexpr uint8_t TAS2780_LSR = 0x19;           // Modulation
static constexpr uint8_t TAS2780_LSR_PWM_MODE = 0x00;  // PWM modulation mode
static constexpr uint8_t TAS2780_INIT_1 = 0x21;        // Initialization
static constexpr uint8_t TAS2780_INIT_1_VAL = 0x00;    // Disable comparator hysteresis
static constexpr uint8_t TAS2780_INIT_2 = 0x35;        // Initialization
static constexpr uint8_t TAS2780_INIT_2_VAL = 0x74;    // Noise minimized

/* PAGE 0xFD */
static constexpr uint8_t TAS2780_PAGE_FD_ACCESS = 0x0D;         // Page 0xFD access unlock/lock register
static constexpr uint8_t TAS2780_PAGE_FD_ACCESS_UNLOCK = 0x0D;  // Unlock page 0xFD access
static constexpr uint8_t TAS2780_PAGE_FD_ACCESS_LOCK = 0x00;    // Lock page 0xFD access
static constexpr uint8_t TAS2780_INIT_3 = 0x3E;                 // Initialization
static constexpr uint8_t TAS2780_INIT_3_VAL = 0x4A;             // Optimal Dmin

// CDS_MODE (2 bits) and VBAT1S_MODE (1 bit) per power mode 0..3, packed so nothing lands in RAM on ESP8266:
// PWR_MODE0: 2/0, PWR_MODE1: 0/0, PWR_MODE2: 3/1, PWR_MODE3: 1/0
static constexpr uint8_t POWER_MODE_CDS_MODES = 0x72;
static constexpr uint8_t POWER_MODE_VBAT1S_MODES = 0x04;

// Latched interrupt bits per register (INT_LTCH0, 1, 1_0, 2, one byte each, low to high), split into faults
// and informational events; bits outside both masks are undefined.
static constexpr uint32_t TAS2780_INT_LTCH_ERROR_MASKS = 0x0FA058C7;
static constexpr uint32_t TAS2780_INT_LTCH_INFO_MASKS = 0x00002138;

static const LogString *fault_name(uint8_t reg, uint8_t bit) {
  switch ((reg << 3) | bit) {
    case 0x00:
      return LOG_STR("Over temperature error");
    case 0x01:
      return LOG_STR("Over current error");
    case 0x02:
      return LOG_STR("TDM Clock Error");
    case 0x03:
      return LOG_STR("Limiter active");
    case 0x04:
      return LOG_STR("PVDD below limiter inflection point");
    case 0x05:
      return LOG_STR("Limiter max attenuation");
    case 0x06:
      return LOG_STR("BOP infinite hold");
    case 0x07:
      return LOG_STR("BOP Mute");
    case 0x08:
      return LOG_STR("Gain limiter active");
    case 0x0B:
      return LOG_STR("Load Diagnostic mode fault status");
    case 0x0D:
      return LOG_STR("Load diagnostic complete");
    case 0x0E:
      return LOG_STR("OTP CRC error flag");
    case 0x15:
      return LOG_STR("VBAT1S Under Voltage");
    case 0x17:
      return LOG_STR("Internal PLL Clock Error");
    case 0x18:
      return LOG_STR("PVDD UVLO");
    case 0x19:
      return LOG_STR("Internal VBAT1S LDO Over Load");
    case 0x1A:
      return LOG_STR("Internal VBAT1S LDO Over Voltage");
    case 0x1B:
      return LOG_STR("Internal VBAT1S LDO Under Voltage");
    default:
      return nullptr;
  }
}

void TAS2780::setup() {
  if (!this->init_()) {
    this->mark_failed();
    return;
  }
  this->write_mode_ctrl_(TAS2780_MODE_CTRL_MODE_SFTW_SHTDWN);
}

bool TAS2780::select_page_(uint8_t page) {
  if (this->current_page_ == page)
    return true;
  if (!this->write_byte(TAS2780_PAGE_SELECT, page)) {
    this->current_page_ = 0xFF;
    return false;
  }
  this->current_page_ = page;
  return true;
}

bool TAS2780::update_bits_(uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t current;
  if (!this->select_page_(TAS2780_PAGE_0) || !this->read_byte(reg, &current)) {
    ESP_LOGE(TAG, "Failed to read register 0x%02X", reg);
    return false;
  }
  return this->write_byte(reg, (current & ~mask) | (value & mask));
}

bool TAS2780::init_() {
  // Software reset (must select page 0 first; reset invalidates page cache)
  if (!this->select_page_(TAS2780_PAGE_0)) {
    ESP_LOGE(TAG, "I2C write failed during init");
    return false;
  }
  this->current_page_ = 0xFF;
  this->reg(TAS2780_SW_RESET) = TAS2780_SW_RESET_CMD;
  delay(1);

  uint8_t chd1;
  if (!this->read_byte(TAS2780_DC_BLK1, &chd1)) {
    ESP_LOGE(TAG, "I2C read failed during init");
    return false;
  }
  if (chd1 != TAS2780_DC_BLK1_RESET_VAL) {
    ESP_LOGE(TAG, "Init failed (DC_BLK1=0x%02X, expected 0x%02X)", chd1, TAS2780_DC_BLK1_RESET_VAL);
    return false;
  }

  if (!this->select_page_(TAS2780_PAGE_0)) {
    return false;
  }
  this->reg(TAS2780_TDM_CFG5) = TAS2780_TDM_CFG5_TX_VSNS_EN_SLOT4;
  this->reg(TAS2780_TDM_CFG6) = TAS2780_TDM_CFG6_TX_ISNS_EN_SLOT0;

  if (!this->select_page_(TAS2780_PAGE_1)) {
    return false;
  }
  this->reg(TAS2780_LSR) = TAS2780_LSR_PWM_MODE;
  this->reg(TAS2780_INIT_0) = TAS2780_INIT_0_VAL;
  this->reg(TAS2780_INIT_1) = TAS2780_INIT_1_VAL;
  this->reg(TAS2780_INIT_2) = TAS2780_INIT_2_VAL;

  if (!this->select_page_(TAS2780_PAGE_FD)) {
    return false;
  }
  this->reg(TAS2780_PAGE_FD_ACCESS) = TAS2780_PAGE_FD_ACCESS_UNLOCK;
  this->reg(TAS2780_INIT_3) = TAS2780_INIT_3_VAL;
  this->reg(TAS2780_PAGE_FD_ACCESS) = TAS2780_PAGE_FD_ACCESS_LOCK;

  if (!this->select_page_(TAS2780_PAGE_0)) {
    return false;
  }
  if (!this->set_power_mode_(this->power_mode_))
    return false;

  // When Y bridge is used (eg. PWR_MODE1) PVDD UVLO threshold needs to be set 2.5 V above VBAT1S level.
  //  UVLO = 1.753V + val * 0.332V
  this->reg(TAS2780_PVDD_UVLO) = TAS2780_PVDD_UVLO_2V76;

  // Mask all interrupt groups on the IRQZ pin, events are polled via update()
  this->reg(TAS2780_INT_MASK0) = TAS2780_INT_MASK_ALL;
  this->reg(TAS2780_INT_MASK1) = TAS2780_INT_MASK_ALL;
  this->reg(TAS2780_INT_MASK1_0) = TAS2780_INT_MASK_ALL;
  this->reg(TAS2780_INT_MASK2) = TAS2780_INT_MASK_ALL;
  this->reg(TAS2780_INT_MASK3) = TAS2780_INT_MASK_ALL;

  if (!this->update_bits_(TAS2780_INT_CLK_CFG, TAS2780_INT_CLK_CFG_MODE_MASK, TAS2780_INT_CLK_CFG_MODE_LIVE))
    return false;

  // Software reset sets DVC back to 0 dB (full volume)
  return this->apply_config();
}

void TAS2780::activate() {
  ESP_LOGD(TAG, "Activating (PWR_MODE:%u)", this->power_mode_);
  this->clear_latches_();
  if (this->power_mode_ != this->applied_power_mode_ && !this->reinit_())
    return;
  this->write_mode_ctrl_(this->active_mode_());
}

void TAS2780::deactivate() {
  ESP_LOGD(TAG, "Deactivating");
  this->write_mode_ctrl_(TAS2780_MODE_CTRL_MODE_SFTW_SHTDWN);
}

void TAS2780::reset() {
  if (this->reinit_())
    this->activate();
}

bool TAS2780::reinit_() {
  if (!this->init_()) {
    ESP_LOGE(TAG, "Re-initialization failed");
    this->status_set_error(LOG_STR("Init failed"));
    return false;
  }
  this->status_clear_error();
  return true;
}

void TAS2780::set_power_mode(uint8_t power_mode) {
  // Lambda-supplied values bypass schema validation; refuse before anything touches the chip
  if (power_mode >= 4) {
    ESP_LOGE(TAG, "Invalid power mode %u, must be 0-3", power_mode);
    return;
  }
  this->power_mode_ = power_mode;
}

bool TAS2780::set_power_mode_(uint8_t power_mode) {
  uint8_t cds_mode = (POWER_MODE_CDS_MODES >> (power_mode * 2)) & 0x03;
  uint8_t vbat1s_mode = (POWER_MODE_VBAT1S_MODES >> power_mode) & 0x01;
  if (!this->update_bits_(TAS2780_CHNL_0, TAS2780_CHNL_0_CDS_MODE_MASK, cds_mode << TAS2780_CHNL_0_CDS_MODE_SHIFT) ||
      !this->update_bits_(TAS2780_DC_BLK0, TAS2780_DC_BLK0_VBAT1S_MODE_MASK, vbat1s_mode ? 0xFF : 0)) {
    return false;
  }
  this->applied_power_mode_ = power_mode;
  return true;
}

void TAS2780::clear_latches_() {
  this->update_bits_(TAS2780_INT_CLK_CFG, TAS2780_INT_CLK_CFG_CLR_LATCH, TAS2780_INT_CLK_CFG_CLR_LATCH);
}

// Returns true if any latched interrupt flag is set
bool TAS2780::log_error_states_() {
  uint8_t latched[4];
  if (!this->select_page_(TAS2780_PAGE_0) || !this->read_bytes(TAS2780_INT_LTCH0, latched, 3) ||
      !this->read_byte(TAS2780_INT_LTCH2, &latched[3])) {
    return false;
  }
  // LDMODE is a two-bit field in INT_LTCH1; report it once
  if (latched[1] & (1 << 4))
    latched[1] = (latched[1] & ~(1 << 4)) | (1 << 3);
  for (uint8_t reg = 0; reg < 4; reg++) {
    uint8_t errors = latched[reg] & (TAS2780_INT_LTCH_ERROR_MASKS >> (reg * 8));
    uint8_t infos = latched[reg] & (TAS2780_INT_LTCH_INFO_MASKS >> (reg * 8));
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (errors & (1 << bit)) {
        ESP_LOGE(TAG, "%s", LOG_STR_ARG(fault_name(reg, bit)));
      } else if (infos & (1 << bit)) {
        ESP_LOGD(TAG, "%s", LOG_STR_ARG(fault_name(reg, bit)));
      }
    }
  }
  return (latched[0] | latched[1] | latched[2] | latched[3]) != 0;
}

void TAS2780::update() {
  // Latches hold until cleared; without this the same events are logged on every update
  if (this->log_error_states_())
    this->clear_latches_();
}

void TAS2780::dump_config() {
  ESP_LOGCONFIG(TAG, "Audio Amplifier:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  const char *channel_str = LOG_STR_LITERAL("Mono Downmix");
  if (this->selected_channel_ == LEFT_CHANNEL) {
    channel_str = LOG_STR_LITERAL("Left");
  } else if (this->selected_channel_ == RIGHT_CHANNEL) {
    channel_str = LOG_STR_LITERAL("Right");
  }
  ESP_LOGCONFIG(TAG,
                "  Power Mode: %u\n"
                "  Amp Level: %u\n"
                "  Volume Range: %.2f - %.2f\n"
                "  Channel: %s",
                this->power_mode_, this->amp_level_, this->vol_range_min_, this->vol_range_max_, channel_str);
}

bool TAS2780::write_mode_ctrl_(uint8_t mode) {
  return this->update_bits_(TAS2780_MODE_CTRL, TAS2780_MODE_CTRL_MODE_MASK, mode);
}

uint8_t TAS2780::active_mode_() const {
  return this->is_muted_ ? TAS2780_MODE_CTRL_MODE_ACTIVE_MUTED : TAS2780_MODE_CTRL_MODE_ACTIVE;
}

bool TAS2780::set_mute_(bool muted) {
  bool previous = this->is_muted_;
  this->is_muted_ = muted;
  uint8_t mode_ctrl;
  if (!this->select_page_(TAS2780_PAGE_0) || !this->read_byte(TAS2780_MODE_CTRL, &mode_ctrl)) {
    ESP_LOGE(TAG, "Failed to read MODE_CTRL");
    this->is_muted_ = previous;
    return false;
  }
  uint8_t current_mode = mode_ctrl & TAS2780_MODE_CTRL_MODE_MASK;
  // Only switch between active/muted if device is active; don't wake from shutdown
  if ((current_mode == TAS2780_MODE_CTRL_MODE_ACTIVE || current_mode == TAS2780_MODE_CTRL_MODE_ACTIVE_MUTED) &&
      !this->write_byte(TAS2780_MODE_CTRL, (mode_ctrl & ~TAS2780_MODE_CTRL_MODE_MASK) | this->active_mode_())) {
    ESP_LOGE(TAG, "Failed to write MODE_CTRL");
    this->is_muted_ = previous;
    return false;
  }
  return true;
}

bool TAS2780::set_volume(float volume) {
  float previous = this->volume_;
  this->volume_ = clamp(volume, 0.0f, 1.0f);
  if (!this->write_volume_()) {
    this->volume_ = previous;
    return false;
  }
  return true;
}

bool TAS2780::write_volume_() {
  // Lambda-supplied volume ranges are not bounded or ordered
  float range_min = std::min(this->vol_range_min_, this->vol_range_max_);
  float range_max = std::max(this->vol_range_min_, this->vol_range_max_);
  float volume = clamp(std::lerp(range_min, range_max, this->volume_), 0.0f, 1.0f);
  uint8_t dvc = remap<uint8_t, float>(volume, 0.0f, 1.0f, TAS2780_DVC_MAX_ATTEN, 0);
  ESP_LOGD(TAG, "Setting attenuation to: %u", dvc);
  return this->select_page_(TAS2780_PAGE_0) && this->write_byte(TAS2780_DVC, dvc);
}

bool TAS2780::apply_config() { return this->apply_amp_and_channel_config_() && this->write_volume_(); }

bool TAS2780::apply_amp_and_channel_config_() {
  // Lambda-supplied values bypass schema validation
  if (this->amp_level_ > TAS2780_AMP_LEVEL_MAX) {
    ESP_LOGW(TAG, "Amp level %u out of range, using %u", this->amp_level_, TAS2780_AMP_LEVEL_MAX);
    this->amp_level_ = TAS2780_AMP_LEVEL_MAX;
  }
  ESP_LOGD(TAG, "Update amp to level idx: %u", this->amp_level_);
  if (!this->update_bits_(TAS2780_CHNL_0, TAS2780_CHNL_0_AMP_LEVEL_MASK,
                          this->amp_level_ << TAS2780_CHNL_0_AMP_LEVEL_SHIFT)) {
    return false;
  }
  return this->update_bits_(
      TAS2780_TDM_CFG2, TAS2780_TDM_CFG2_RX_SCFG_MASK | TAS2780_TDM_CFG2_RX_WLEN_MASK | TAS2780_TDM_CFG2_RX_SLEN_MASK,
      (this->selected_channel_ << TAS2780_TDM_CFG2_RX_SCFG_SHIFT) | TAS2780_TDM_CFG2_RX_WLEN_32BIT |
          TAS2780_TDM_CFG2_RX_SLEN_32BIT);
}

}  // namespace esphome::tas2780
