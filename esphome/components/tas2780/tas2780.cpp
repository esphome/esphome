#include "tas2780.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::tas2780 {

static const char *const TAG = "tas2780";

static const uint8_t TAS2780_PAGE_SELECT = 0x00;  // Page Select
static const uint8_t TAS2780_PAGE_0 = 0x00;       // Page 0
static const uint8_t TAS2780_PAGE_1 = 0x01;       // Page 1
static const uint8_t TAS2780_PAGE_FD = 0xFD;      // Page 0xFD

/* PAGE 0 */
static const uint8_t TAS2780_SW_RESET = 0x01;      // Software Reset
static const uint8_t TAS2780_SW_RESET_CMD = 0x01;  // Trigger software reset
static const uint8_t TAS2780_MODE_CTRL = 0x02;     // Device operational mode
static const uint8_t TAS2780_MODE_CTRL_MODE_MASK = 0x07;
static const uint8_t TAS2780_MODE_CTRL_MODE_ACTIVE = 0x00;
static const uint8_t TAS2780_MODE_CTRL_MODE_ACTIVE_MUTED = 0x01;
static const uint8_t TAS2780_MODE_CTRL_MODE_SFTW_SHTDWN = 0x02;

static const uint8_t TAS2780_CHNL_0 = 0x03;  // Y Bridge and Channel settings
static const uint8_t TAS2780_CHNL_0_CDS_MODE_SHIFT = 6;
static const uint8_t TAS2780_CHNL_0_CDS_MODE_MASK = (0x03 << TAS2780_CHNL_0_CDS_MODE_SHIFT);
static const uint8_t TAS2780_CHNL_0_AMP_LEVEL_SHIFT = 1;
static const uint8_t TAS2780_CHNL_0_AMP_LEVEL_MASK = (0x1F) << TAS2780_CHNL_0_AMP_LEVEL_SHIFT;
static const uint8_t TAS2780_AMP_LEVEL_MAX = 0x14;  // Codes above 20 are reserved

static const uint8_t TAS2780_DC_BLK0 = 0x04;  // SAR Filter and DC Path Blocker
static const uint8_t TAS2780_DC_BLK0_VBAT1S_MODE_SHIFT = 7;
static const uint8_t TAS2780_DC_BLK1 = 0x05;  // Record DC Blocker

static const uint8_t TAS2780_TDM_CFG2 = 0x0A;  // TDM Configuration 2
static const uint8_t TAS2780_TDM_CFG2_RX_SCFG_SHIFT = 4;
static const uint8_t TAS2780_TDM_CFG2_RX_SCFG_MASK = (3 << TAS2780_TDM_CFG2_RX_SCFG_SHIFT);
static const uint8_t TAS2780_TDM_CFG2_RX_SCFG_STEREO_DWN_MIX = (3 << TAS2780_TDM_CFG2_RX_SCFG_SHIFT);
static const uint8_t TAS2780_TDM_CFG2_RX_SCFG_MONO_LEFT = (1 << TAS2780_TDM_CFG2_RX_SCFG_SHIFT);
static const uint8_t TAS2780_TDM_CFG2_RX_SCFG_MONO_RIGHT = (2 << TAS2780_TDM_CFG2_RX_SCFG_SHIFT);
static const uint8_t TAS2780_TDM_CFG2_RX_WLEN_SHIFT = 2;
static const uint8_t TAS2780_TDM_CFG2_RX_WLEN_MASK = (3 << TAS2780_TDM_CFG2_RX_WLEN_SHIFT);
static const uint8_t TAS2780_TDM_CFG2_RX_WLEN_32BIT = (3 << TAS2780_TDM_CFG2_RX_WLEN_SHIFT);
static const uint8_t TAS2780_TDM_CFG2_RX_SLEN_MASK = (3 << 0);
static const uint8_t TAS2780_TDM_CFG2_RX_SLEN_32BIT = 2;

static const uint8_t TAS2780_TDM_CFG5 = 0x0E;                   // TDM Configuration 5
static const uint8_t TAS2780_TDM_CFG5_TX_VSNS_EN_SLOT4 = 0x44;  // vsns TX enable, slot 4
static const uint8_t TAS2780_TDM_CFG6 = 0x0F;                   // TDM Configuration 6
static const uint8_t TAS2780_TDM_CFG6_TX_ISNS_EN_SLOT0 = 0x40;  // isns TX enable, slot 0

static const uint8_t TAS2780_DVC = 0x1A;  // Digital Volume Control

/* Interrupts */
static const uint8_t TAS2780_INT_MASK_ALL = 0xFF;  // Mask all interrupts
static const uint8_t TAS2780_INT_MASK0 = 0x3B;     // Interrupt Mask 0
static const uint8_t TAS2780_INT_MASK1 = 0x3C;     // Interrupt Mask 1
static const uint8_t TAS2780_INT_MASK1_0 = 0x3D;   // Interrupt Mask 1_0 (INT_LTCH1_0 group)
static const uint8_t TAS2780_INT_MASK2 = 0x40;     // Interrupt Mask 2
static const uint8_t TAS2780_INT_MASK3 = 0x41;     // Interrupt Mask 3
static const uint8_t TAS2780_INT_LTCH0 = 0x49;     // Latched Interrupt Read-back 0
static const uint8_t TAS2780_INT_LTCH1 = 0x4A;     // Latched Interrupt Read-back 1
static const uint8_t TAS2780_INT_LTCH1_0 = 0x4B;   // Latched Interrupt Read-back 1_0
static const uint8_t TAS2780_INT_LTCH2 = 0x4F;     // Latched Interrupt Read-back 2

static const uint8_t TAS2780_INT_CLK_CFG = 0x5C;                // Clock Setting and IRQZ
static const uint8_t TAS2780_INT_CLK_CFG_CLR_LATCH = (1 << 2);  // Clear interrupt latches
static const uint8_t TAS2780_INT_CLK_CFG_MODE_MASK = 0x03;      // Trigger mode field mask
static const uint8_t TAS2780_INT_CLK_CFG_MODE_LIVE = 0x00;      // Trigger on any unmasked live interrupt
static const uint8_t TAS2780_PVDD_UVLO = 0x71;                  // UVLO Threshold
static const uint8_t TAS2780_PVDD_UVLO_2V76 = 0x03;             // PVDD UVLO threshold = 2.76V

/* PAGE 0x01 */
static const uint8_t TAS2780_INIT_0 = 0x17;        // Initialization
static const uint8_t TAS2780_INIT_0_VAL = 0xC8;    // SARBurstMask=0, CMP_HYST_LP=1
static const uint8_t TAS2780_LSR = 0x19;           // Modulation
static const uint8_t TAS2780_LSR_PWM_MODE = 0x00;  // PWM modulation mode
static const uint8_t TAS2780_INIT_1 = 0x21;        // Initialization
static const uint8_t TAS2780_INIT_1_VAL = 0x00;    // Disable comparator hysteresis
static const uint8_t TAS2780_INIT_2 = 0x35;        // Initialization
static const uint8_t TAS2780_INIT_2_VAL = 0x74;    // Noise minimized

/* PAGE 0xFD */
static const uint8_t TAS2780_PAGE_FD_ACCESS = 0x0D;         // Page 0xFD access unlock/lock register
static const uint8_t TAS2780_PAGE_FD_ACCESS_UNLOCK = 0x0D;  // Unlock page 0xFD access
static const uint8_t TAS2780_PAGE_FD_ACCESS_LOCK = 0x00;    // Lock page 0xFD access
static const uint8_t TAS2780_INIT_3 = 0x3E;                 // Initialization
static const uint8_t TAS2780_INIT_3_VAL = 0x4A;             // Optimal Dmin

/* Latched interrupt bits */
static const uint8_t TAS2780_INT_LTCH0_IR_OT = (1 << 0);     // over temp error
static const uint8_t TAS2780_INT_LTCH0_IR_OC = (1 << 1);     // over current error
static const uint8_t TAS2780_INT_LTCH0_IR_TDMCE = (1 << 2);  // TDM_CLOCK_ERROR
static const uint8_t TAS2780_INT_LTCH0_IR_LIMA = (1 << 3);   // limiter active
static const uint8_t TAS2780_INT_LTCH0_IR_PBIP = (1 << 4);   // PVDD below limiter inflection point
static const uint8_t TAS2780_INT_LTCH0_IR_LIMMA = (1 << 5);  // limiter max attenuation
static const uint8_t TAS2780_INT_LTCH0_IR_BOPIH = (1 << 6);  // BOP infinite hold
static const uint8_t TAS2780_INT_LTCH0_IR_BOPM = (1 << 7);   // due to bop mute

static const uint8_t TAS2780_INT_LTCH1_IR_VBATLIM = (1 << 0);  // Gain Limiter interrupt
static const uint8_t TAS2780_INT_LTCH1_IR_LDMODE = (3 << 3);   // Load Diagnostic mode fault status
static const uint8_t TAS2780_INT_LTCH1_IR_LDC = (1 << 5);      // Load diagnostic completion
static const uint8_t TAS2780_INT_LTCH1_IR_OTPCRC = (1 << 6);   // OTP CRC error flag

static const uint8_t TAS2780_INT_LTCH1_0_IR_VBAT1S_UVLO = (1 << 5);  // VBAT1S Under Voltage
static const uint8_t TAS2780_INT_LTCH1_0_IR_PLL_CLK = (1 << 7);      // Internal PLL Clock Error

static const uint8_t TAS2780_INT_LTCH2_IR_PUVLO = (1 << 0);   // PVDD UVLO
static const uint8_t TAS2780_INT_LTCH2_IR_LDO_OL = (1 << 1);  // Internal VBAT1S LDO Over Load
static const uint8_t TAS2780_INT_LTCH2_IR_LDO_OV = (1 << 2);  // Internal VBAT1S LDO Over Voltage
static const uint8_t TAS2780_INT_LTCH2_IR_LDO_UV = (1 << 3);  // Internal VBAT1S LDO Under Voltage

static const uint8_t POWER_MODES[4][2] = {
    {2, 0},  // PWR_MODE0: CDS_MODE=10, VBAT1S_MODE=0
    {0, 0},  // PWR_MODE1: CDS_MODE=00, VBAT1S_MODE=0
    {3, 1},  // PWR_MODE2: CDS_MODE=11, VBAT1S_MODE=1
    {1, 0},  // PWR_MODE3: CDS_MODE=01, VBAT1S_MODE=0
};

static uint8_t get_channel_select_reg_val(ChannelSelect channel) {
  switch (channel) {
    case MONO_DWN_MIX:
      return TAS2780_TDM_CFG2_RX_SCFG_STEREO_DWN_MIX;
    case LEFT_CHANNEL:
      return TAS2780_TDM_CFG2_RX_SCFG_MONO_LEFT;
    case RIGHT_CHANNEL:
      return TAS2780_TDM_CFG2_RX_SCFG_MONO_RIGHT;
  }
  return TAS2780_TDM_CFG2_RX_SCFG_STEREO_DWN_MIX;
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
    this->current_page_ = -1;
    return false;
  }
  this->current_page_ = page;
  return true;
}

bool TAS2780::init_() {
  // Software reset (must select page 0 first; reset invalidates page cache)
  if (!this->select_page_(TAS2780_PAGE_0)) {
    ESP_LOGE(TAG, "I2C write failed during init");
    return false;
  }
  this->current_page_ = -1;
  this->reg(TAS2780_SW_RESET) = TAS2780_SW_RESET_CMD;
  delay(1);

  // DC_BLK1 (0x05) reads 0x41 after reset; used as chip presence check
  // since TAS2780 has no dedicated WHO_AM_I register
  static const uint8_t TAS2780_DC_BLK1_RESET_VAL = 0x41;
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

  // Mask all interrupt groups on the IRQZ pin — events are polled via update()
  this->reg(TAS2780_INT_MASK0) = TAS2780_INT_MASK_ALL;
  this->reg(TAS2780_INT_MASK1) = TAS2780_INT_MASK_ALL;
  this->reg(TAS2780_INT_MASK1_0) = TAS2780_INT_MASK_ALL;
  this->reg(TAS2780_INT_MASK2) = TAS2780_INT_MASK_ALL;
  this->reg(TAS2780_INT_MASK3) = TAS2780_INT_MASK_ALL;

  // set interrupt to trigger on any unmasked live interrupts
  uint8_t int_clk_cfg;
  if (!this->read_byte(TAS2780_INT_CLK_CFG, &int_clk_cfg)) {
    ESP_LOGE(TAG, "Failed to read INT_CLK_CFG");
    return false;
  }
  this->reg(TAS2780_INT_CLK_CFG) = (int_clk_cfg & ~TAS2780_INT_CLK_CFG_MODE_MASK) | TAS2780_INT_CLK_CFG_MODE_LIVE;

  if (!this->apply_amp_and_channel_config())
    return false;

  // Software reset sets DVC back to 0 dB (full volume)
  if (!this->write_volume_()) {
    ESP_LOGE(TAG, "Failed to write volume");
    return false;
  }
  return true;
}

void TAS2780::activate(uint8_t power_mode) {
  if (power_mode == POWER_MODE_KEEP)
    power_mode = this->power_mode_;
  if (power_mode >= 4) {
    ESP_LOGE(TAG, "Invalid power mode %u, must be 0-3", power_mode);
    return;
  }
  ESP_LOGD(TAG, "Activating (PWR_MODE:%d)", power_mode);
  this->clear_latches_();
  if (power_mode != this->power_mode_) {
    this->power_mode_ = power_mode;
    if (!this->reinit_())
      return;
  }
  uint8_t mode = this->is_muted_ ? TAS2780_MODE_CTRL_MODE_ACTIVE_MUTED : TAS2780_MODE_CTRL_MODE_ACTIVE;
  this->write_mode_ctrl_(mode);
}

void TAS2780::deactivate() {
  ESP_LOGD(TAG, "Deactivating");
  this->write_mode_ctrl_(TAS2780_MODE_CTRL_MODE_SFTW_SHTDWN);
}

void TAS2780::reset() {
  if (!this->reinit_())
    return;
  this->activate(this->power_mode_);
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

bool TAS2780::set_power_mode_(uint8_t power_mode) {
  if (power_mode >= 4) {
    ESP_LOGE(TAG, "Invalid power mode %u, must be 0-3", power_mode);
    return false;
  }
  uint8_t chnl_0;
  if (!this->read_byte(TAS2780_CHNL_0, &chnl_0)) {
    ESP_LOGE(TAG, "Failed to read CHNL_0");
    return false;
  }
  this->reg(TAS2780_CHNL_0) =
      (chnl_0 & ~TAS2780_CHNL_0_CDS_MODE_MASK) | (POWER_MODES[power_mode][0] << TAS2780_CHNL_0_CDS_MODE_SHIFT);
  uint8_t dc_blk0;
  if (!this->read_byte(TAS2780_DC_BLK0, &dc_blk0)) {
    ESP_LOGE(TAG, "Failed to read DC_BLK0");
    return false;
  }
  this->reg(TAS2780_DC_BLK0) = (dc_blk0 & ~(1 << TAS2780_DC_BLK0_VBAT1S_MODE_SHIFT)) |
                               (POWER_MODES[power_mode][1] << TAS2780_DC_BLK0_VBAT1S_MODE_SHIFT);
  return true;
}

void TAS2780::clear_latches_() {
  // Clear interrupt latches without disturbing other INT_CLK_CFG bits
  uint8_t int_clk_cfg;
  if (this->select_page_(TAS2780_PAGE_0) && this->read_byte(TAS2780_INT_CLK_CFG, &int_clk_cfg)) {
    this->reg(TAS2780_INT_CLK_CFG) = int_clk_cfg | TAS2780_INT_CLK_CFG_CLR_LATCH;
  }
}

// Returns true if any latched interrupt flag is set
bool TAS2780::log_error_states_() {
  uint8_t latched_its;
  if (!this->select_page_(TAS2780_PAGE_0) || !this->read_byte(TAS2780_INT_LTCH0, &latched_its))
    return false;

  if (latched_its & TAS2780_INT_LTCH0_IR_OT) {
    ESP_LOGE(TAG, "Over temperature error");
  }
  if (latched_its & TAS2780_INT_LTCH0_IR_OC) {
    ESP_LOGE(TAG, "Over current error");
  }
  if (latched_its & TAS2780_INT_LTCH0_IR_TDMCE) {
    ESP_LOGE(TAG, "TDM Clock Error");
  }
  if (latched_its & TAS2780_INT_LTCH0_IR_LIMA) {
    ESP_LOGD(TAG, "Limiter active");
  }
  if (latched_its & TAS2780_INT_LTCH0_IR_PBIP) {
    ESP_LOGD(TAG, "PVDD below limiter inflection point");
  }
  if (latched_its & TAS2780_INT_LTCH0_IR_LIMMA) {
    ESP_LOGD(TAG, "Limiter max attenuation");
  }
  if (latched_its & TAS2780_INT_LTCH0_IR_BOPIH) {
    ESP_LOGE(TAG, "BOP infinite hold");
  }
  if (latched_its & TAS2780_INT_LTCH0_IR_BOPM) {
    ESP_LOGE(TAG, "BOP Mute");
  }

  uint8_t latched1_its;
  if (!this->read_byte(TAS2780_INT_LTCH1, &latched1_its))
    return latched_its != 0;

  if (latched1_its & TAS2780_INT_LTCH1_IR_VBATLIM) {
    ESP_LOGD(TAG, "Gain limiter active");
  }
  if (latched1_its & TAS2780_INT_LTCH1_IR_LDMODE) {
    ESP_LOGE(TAG, "Load Diagnostic mode fault status");
  }
  if (latched1_its & TAS2780_INT_LTCH1_IR_LDC) {
    ESP_LOGD(TAG, "Load diagnostic complete");
  }
  if (latched1_its & TAS2780_INT_LTCH1_IR_OTPCRC) {
    ESP_LOGE(TAG, "OTP CRC error flag");
  }

  uint8_t latched1_0_its;
  if (!this->read_byte(TAS2780_INT_LTCH1_0, &latched1_0_its))
    return (latched_its | latched1_its) != 0;

  if (latched1_0_its & TAS2780_INT_LTCH1_0_IR_VBAT1S_UVLO) {
    ESP_LOGE(TAG, "VBAT1S Under Voltage");
  }
  if (latched1_0_its & TAS2780_INT_LTCH1_0_IR_PLL_CLK) {
    ESP_LOGE(TAG, "Internal PLL Clock Error");
  }

  uint8_t latched2_its;
  if (!this->read_byte(TAS2780_INT_LTCH2, &latched2_its))
    return (latched_its | latched1_its | latched1_0_its) != 0;

  if (latched2_its & TAS2780_INT_LTCH2_IR_PUVLO) {
    ESP_LOGE(TAG, "PVDD UVLO");
  }
  if (latched2_its & TAS2780_INT_LTCH2_IR_LDO_OL) {
    ESP_LOGE(TAG, "Internal VBAT1S LDO Over Load");
  }
  if (latched2_its & TAS2780_INT_LTCH2_IR_LDO_OV) {
    ESP_LOGE(TAG, "Internal VBAT1S LDO Over Voltage");
  }
  if (latched2_its & TAS2780_INT_LTCH2_IR_LDO_UV) {
    ESP_LOGE(TAG, "Internal VBAT1S LDO Under Voltage");
  }
  return (latched_its | latched1_its | latched1_0_its | latched2_its) != 0;
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
  uint8_t mode_ctrl;
  if (!this->select_page_(TAS2780_PAGE_0) || !this->read_byte(TAS2780_MODE_CTRL, &mode_ctrl)) {
    ESP_LOGE(TAG, "Failed to read MODE_CTRL");
    return false;
  }
  this->reg(TAS2780_MODE_CTRL) = (mode_ctrl & ~TAS2780_MODE_CTRL_MODE_MASK) | mode;
  return true;
}

bool TAS2780::set_mute_off() {
  bool previous = this->is_muted_;
  this->is_muted_ = false;
  if (!this->write_mute_()) {
    this->is_muted_ = previous;
    return false;
  }
  return true;
}

bool TAS2780::set_mute_on() {
  bool previous = this->is_muted_;
  this->is_muted_ = true;
  if (!this->write_mute_()) {
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

bool TAS2780::is_muted() { return this->is_muted_; }

float TAS2780::volume() { return this->volume_; }

bool TAS2780::write_mute_() {
  uint8_t mode_ctrl;
  if (!this->select_page_(TAS2780_PAGE_0) || !this->read_byte(TAS2780_MODE_CTRL, &mode_ctrl)) {
    ESP_LOGE(TAG, "Failed to read MODE_CTRL");
    return false;
  }
  uint8_t current_mode = mode_ctrl & TAS2780_MODE_CTRL_MODE_MASK;
  // Only switch between active/muted if device is active; don't wake from shutdown
  if (current_mode == TAS2780_MODE_CTRL_MODE_ACTIVE || current_mode == TAS2780_MODE_CTRL_MODE_ACTIVE_MUTED) {
    uint8_t new_mode = this->is_muted_ ? TAS2780_MODE_CTRL_MODE_ACTIVE_MUTED : TAS2780_MODE_CTRL_MODE_ACTIVE;
    this->reg(TAS2780_MODE_CTRL) = (mode_ctrl & ~TAS2780_MODE_CTRL_MODE_MASK) | new_mode;
  }
  return true;
}

bool TAS2780::write_volume_() {
  /*
  V_{AMP} = INPUT + A_{DVC} + A_{AMP}

  V_{AMP} is the amplifier output voltage in dBV ()
  INPUT: digital input amplitude as a number of dB with respect to 0 dBFS
  A_{DVC}: is the digital volume control setting as a number of dB (default 0 dB)
  A_{AMP}: the amplifier output level setting as a number of dBV

  DVC_LVL[7:0] :            0dB to -100dB [0x00, 0xC8] c8 = 200
  AMP_LEVEL[4:0] : @48ksps 11dBV - 21dBV  [0x00, 0x14]
  */
  float range_min = std::min(this->vol_range_min_, this->vol_range_max_);
  float range_max = std::max(this->vol_range_min_, this->vol_range_max_);
  float volume = this->volume_ * (range_max - range_min) + range_min;
  float attenuation = (1.0f - volume) * 200.0f;
  ESP_LOGD(TAG, "Setting attenuation to: %4.2f", attenuation);
  // Clamp before converting; lambda-supplied volume ranges are not bounded
  auto dvc = static_cast<uint8_t>(clamp(attenuation, 0.0f, 200.0f));
  return this->select_page_(TAS2780_PAGE_0) && this->write_byte(TAS2780_DVC, dvc);
}

bool TAS2780::apply_amp_and_channel_config() {
  // Lambda-supplied values bypass schema validation
  if (this->amp_level_ > TAS2780_AMP_LEVEL_MAX) {
    ESP_LOGW(TAG, "Amp level %u out of range, using %u", this->amp_level_, TAS2780_AMP_LEVEL_MAX);
    this->amp_level_ = TAS2780_AMP_LEVEL_MAX;
  }

  // AMP_LEVEL
  uint8_t chnl_0;
  if (!this->select_page_(TAS2780_PAGE_0) || !this->read_byte(TAS2780_CHNL_0, &chnl_0)) {
    ESP_LOGE(TAG, "Failed to read CHNL_0");
    return false;
  }
  chnl_0 = (chnl_0 & ~TAS2780_CHNL_0_AMP_LEVEL_MASK) |
           ((this->amp_level_ << TAS2780_CHNL_0_AMP_LEVEL_SHIFT) & TAS2780_CHNL_0_AMP_LEVEL_MASK);
  this->reg(TAS2780_CHNL_0) = chnl_0;
  ESP_LOGD(TAG, "Update amp to level idx: %d", this->amp_level_);

  // CHANNEL_SELECT — read-modify-write to preserve other bits
  uint8_t tdm_cfg2;
  if (!this->read_byte(TAS2780_TDM_CFG2, &tdm_cfg2)) {
    ESP_LOGE(TAG, "Failed to read TDM_CFG2");
    return false;
  }
  tdm_cfg2 &= ~(TAS2780_TDM_CFG2_RX_SCFG_MASK | TAS2780_TDM_CFG2_RX_WLEN_MASK | TAS2780_TDM_CFG2_RX_SLEN_MASK);
  tdm_cfg2 |= get_channel_select_reg_val(this->selected_channel_) | TAS2780_TDM_CFG2_RX_WLEN_32BIT |
              TAS2780_TDM_CFG2_RX_SLEN_32BIT;
  this->reg(TAS2780_TDM_CFG2) = tdm_cfg2;
  return true;
}

}  // namespace esphome::tas2780
