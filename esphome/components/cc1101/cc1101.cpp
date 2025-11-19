#include "cc1101.h"
#include "cc1101pa.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <climits>
#include <cmath>
#include <cstdio>

namespace esphome {
namespace cc1101 {

static const char *const TAG = "cc1101";

template<typename T> constexpr bool is_enum_valid(T value) { return value < T::LAST; }
constexpr bool is_float_in_range(float value, float min, float max) { return value >= min && value <= max; }
constexpr bool is_int_in_range(int value, int min, int max) { return value >= min && value <= max; }

CC1101Component::CC1101Component() {
  this->reset_ = false;
  this->output_power_requested_ = 10.0f;  // Default 10dBm
  this->output_power_effective_ = 10.0f;
  memset(this->pa_table_, 0, sizeof(pa_table_));
  memset(&this->state_, 0, sizeof(this->state_));

  // datasheet defaults
  this->state_.GDO2_CFG = 0x0D;  // Serial Data (for RX on GDO2)
  this->state_.GDO1_CFG = 0x2E;
  this->state_.GDO0_CFG = 0x0D;  // Serial Data (for RX on GDO0 / TX Input)
  this->state_.FIFO_THR = 7;
  this->state_.SYNC1 = 0xD3;
  this->state_.SYNC0 = 0x91;
  this->state_.PKTLEN = 0xFF;
  this->state_.APPEND_STATUS = 1;
  this->state_.LENGTH_CONFIG = 1;
  this->state_.CRC_EN = 1;
  this->state_.WHITE_DATA = 1;
  this->state_.FREQ_IF = 0x0F;
  this->state_.FREQ2 = 0x1E;
  this->state_.FREQ1 = 0xC4;
  this->state_.FREQ0 = 0xEC;
  this->state_.DRATE_E = 0x0C;
  this->state_.CHANBW_E = 0x02;
  this->state_.DRATE_M = 0x22;
  this->state_.SYNC_MODE = 2;
  this->state_.CHANSPC_E = 2;
  this->state_.NUM_PREAMBLE = 2;
  this->state_.CHANSPC_M = 0xF8;
  this->state_.DEVIATION_M = 7;
  this->state_.DEVIATION_E = 4;
  this->state_.RX_TIME = 7;
  this->state_.CCA_MODE = 3;
  this->state_.PO_TIMEOUT = 1;
  this->state_.FOC_LIMIT = 2;
  this->state_.FOC_POST_K = 1;
  this->state_.FOC_PRE_K = 2;
  this->state_.FOC_BS_CS_GATE = 1;
  this->state_.BS_POST_KP = 1;
  this->state_.BS_POST_KI = 1;
  this->state_.BS_PRE_KP = 2;
  this->state_.BS_PRE_KI = 1;
  this->state_.MAGN_TARGET = 3;
  this->state_.AGC_LNA_PRIORITY = 1;
  this->state_.FILTER_LENGTH = 1;
  this->state_.WAIT_TIME = 1;
  this->state_.HYST_LEVEL = 2;
  this->state_.WOREVT1 = 0x87;
  this->state_.WOREVT0 = 0x6B;
  this->state_.RC_CAL = 1;
  this->state_.EVENT1 = 7;
  this->state_.RC_PD = 1;
  this->state_.MIX_CURRENT = 2;
  this->state_.LODIV_BUF_CURRENT_RX = 1;
  this->state_.LNA2MIX_CURRENT = 1;
  this->state_.LNA_CURRENT = 1;
  this->state_.LODIV_BUF_CURRENT_TX = 1;
  this->state_.FSCAL3_LO = 9;
  this->state_.CHP_CURR_CAL_EN = 2;
  this->state_.FSCAL3_HI = 2;
  this->state_.FSCAL2 = 0x0A;
  this->state_.FSCAL1 = 0x20;
  this->state_.FSCAL0 = 0x0D;
  this->state_.RCCTRL1 = 0x41;
  this->state_.FSTEST = 0x59;
  this->state_.PTEST = 0x7F;
  this->state_.AGCTEST = 0x3F;
  this->state_.TEST2 = 0x88;
  this->state_.TEST1 = 0x31;
  this->state_.TEST0_LO = 1;
  this->state_.VCO_SEL_CAL_EN = 1;
  this->state_.TEST0_HI = 2;

  // IOCFGx
  this->state_.GDO2_CFG = 0x0D;  // Async serial output (TODO: enum)
  this->state_.GDO0_CFG = 0x0D;

  // PKTCTRL0
  this->state_.PKT_FORMAT = 3;
  this->state_.LENGTH_CONFIG = 2;
  this->state_.FS_AUTOCAL = 1;

  // Default Settings
  this->set_frequency(433920);
  this->set_if_frequency(153);
  this->set_filter_bandwidth(203);
  this->set_channel(0);
  this->set_channel_spacing(200);
  this->set_symbol_rate(5000);
  this->set_sync_mode(SyncMode::SYNC_MODE_NONE);
  this->set_carrier_sense_above_threshold(true);
  this->set_modulation_type(Modulation::MODULATION_ASK_OOK);
  this->set_magn_target(MagnTarget::MAGN_TARGET_42DB);
  this->set_max_lna_gain(MaxLnaGain::MAX_LNA_GAIN_DEFAULT);
  this->set_max_dvga_gain(MaxDvgaGain::MAX_DVGA_GAIN_MINUS_3);
  this->set_lna_priority(false);
  this->set_wait_time(WaitTime::WAIT_TIME_32_SAMPLES);

  // CRITICAL: Initialize PA Table to avoid transmitting 0 power (Silence)
  this->set_output_power(10.0f);
}

void CC1101Component::setup() {
  this->spi_setup();
  this->cs_->digital_write(true);
  delayMicroseconds(1);
  this->cs_->digital_write(false);
  delayMicroseconds(1);
  this->cs_->digital_write(true);
  delayMicroseconds(41);
  this->cs_->digital_write(false);
  delay(5);

  this->strobe_(Command::RES);
  delay(5);

  this->read_(Register::PARTNUM);
  this->read_(Register::VERSION);

  char buff[32] = {0};
  snprintf(buff, sizeof(buff), "%02X%02X", this->state_.PARTNUM, this->state_.VERSION);
  this->chip_id_ = buff;

  if (this->state_.VERSION == 0 || this->state_.PARTNUM == 0xFF) {
    ESP_LOGE(TAG, "Failed to verify CC1101.");
    this->mark_failed();
    return;
  }

  ESP_LOGD(TAG, "CC1101 found! %s", this->chip_id_.c_str());
  this->reset_ = true;

  for (uint8_t i = 0; i <= 0x2E; i++) {
    if (i == 0x29 || i == 0x2B)
      continue;
    this->write_((Register) i);
  }
  this->write_(Register::PATABLE, this->pa_table_, sizeof(this->pa_table_));
  this->enter_rx_();
}

void CC1101Component::dump_config() {
  ESP_LOGCONFIG(TAG, "CC1101:");
  LOG_PIN("  CS Pin: ", this->cs_);
  ESP_LOGCONFIG(TAG, "  Chip ID: %s", this->chip_id_.c_str());
}

void CC1101Component::loop() {}

void CC1101Component::begin_tx() {
  ESP_LOGV(TAG, "Beginning TX sequence");

  // 1. Ensure Packet Format is 3 (Async Serial)
  // This triggers the hardware to use GDO0 as input during TX.
  this->write_(Register::PKTCTRL0, 0x32);

  // 2. Turn on RF Synthesizer
  this->strobe_(Command::TX);

  // 3. Wait for TX state
  if (!this->wait_for_state_(State::TX, 50)) {
    ESP_LOGW(TAG, "Timed out waiting for TX state!");
  }
}

void CC1101Component::end_tx() {
  ESP_LOGV(TAG, "Ending TX sequence");

  // 1. Return to RX Mode
  this->enter_rx_();
}

void CC1101Component::reset() {
  this->strobe_(Command::RES);
  this->setup();
}

void CC1101Component::set_idle() {
  // The private helper function handles the strobe to Command::IDLE
  this->enter_idle_();
  ESP_LOGV(TAG, "Transitioned to IDLE state.");
}

void CC1101Component::set_gdo0_config(uint8_t value) {
  this->state_.GDO0_CFG = value;
  if (!this->reset_)
    return;
  this->write_(Register::IOCFG0);
}

void CC1101Component::set_gdo2_config(uint8_t value) {
  this->state_.GDO2_CFG = value;
  if (!this->reset_)
    return;
  this->write_(Register::IOCFG2);
}

bool CC1101Component::wait_for_state_(State target_state, uint32_t timeout_ms) {
  uint32_t start = millis();
  while (millis() - start < timeout_ms) {
    this->read_(Register::MARCSTATE);
    State s = (State) (this->state_.MARC_STATE & 0x1F);
    if (s == target_state)
      return true;
    delayMicroseconds(100);
  }
  return false;
}

void CC1101Component::enter_idle_() {
  this->strobe_(Command::IDLE);
  this->wait_for_state_(State::IDLE);
}

void CC1101Component::enter_rx_() { this->strobe_(Command::RX); }

uint8_t CC1101Component::strobe_(Command cmd) {
  uint8_t index = (uint8_t) cmd;
  if (cmd < Command::RES || cmd > Command::NOP)
    return 0xFF;
  this->enable();
  uint8_t status_byte = this->transfer_byte(index);
  this->disable();
  return status_byte;
}

void CC1101Component::write_(Register reg) {
  uint8_t index = (uint8_t) reg;
  uint8_t value = this->regs_[index];
  this->enable();
  this->write_byte(index);
  this->transfer_array(&value, 1);
  this->disable();
}

void CC1101Component::write_(Register reg, uint8_t value) {
  uint8_t index = (uint8_t) reg;
  this->regs_[index] = value;
  this->write_(reg);
}

void CC1101Component::write_(Register reg, uint8_t *buffer, size_t length) {
  uint8_t index = (uint8_t) reg;
  this->enable();
  this->write_byte(index | BUS_WRITE | BUS_BURST);
  this->transfer_array(buffer, length);
  this->disable();
}

bool CC1101Component::read_(Register reg) {
  uint8_t index = (uint8_t) reg;
  this->enable();
  this->write_byte(index | BUS_READ | BUS_BURST);
  uint8_t value = this->transfer_byte(0);
  this->regs_[index] = value;
  this->disable();
  return true;
}

bool CC1101Component::read_(Register reg, uint8_t *buffer, size_t length) {
  uint8_t index = (uint8_t) reg;
  this->enable();
  this->write_byte(index | BUS_READ | BUS_BURST);
  this->read_array(buffer, length);
  this->disable();
  return true;
}

static void split_float(float value, int mbits, uint8_t &e, uint32_t &m) {
  int e_tmp;
  float m_tmp = std::frexp(value, &e_tmp);
  if (e_tmp <= mbits) {
    e = 0;
    m = 0;
    return;
  }
  e = (uint8_t) (e_tmp - mbits - 1);
  m = (uint32_t) (((m_tmp * 2 - 1) * (1 << (mbits + 1))) + 1) >> 1;
  if (m == (1UL << mbits)) {
    e = e + 1;
    m = 0;
  }
}

// Setters
void CC1101Component::set_output_power(float value) {
  if (!is_float_in_range(value, OUTPUT_POWER_MIN, OUTPUT_POWER_MAX))
    return;
  this->output_power_requested_ = value;
  int freq =
      (int) (this->state_.FREQ2 << 16 | this->state_.FREQ1 << 8 | this->state_.FREQ0) * XTAL_FREQUENCY / (1 << 16);
  uint8_t a = 0xC0;
  if (freq >= 300000 && freq <= 348000) {
    a = PowerTable::find(PA_TABLE_315, sizeof(PA_TABLE_315) / sizeof(PA_TABLE_315[0]), value);
  } else if (freq >= 378000 && freq <= 464000) {
    a = PowerTable::find(PA_TABLE_433, sizeof(PA_TABLE_433) / sizeof(PA_TABLE_433[0]), value);
  } else if (freq >= 779000 && freq < 900000) {
    a = PowerTable::find(PA_TABLE_868, sizeof(PA_TABLE_868) / sizeof(PA_TABLE_868[0]), value);
  } else if (freq >= 900000 && freq <= 928000) {
    a = PowerTable::find(PA_TABLE_915, sizeof(PA_TABLE_915) / sizeof(PA_TABLE_915[0]), value);
  }

  if ((Modulation) this->state_.MOD_FORMAT == Modulation::MODULATION_ASK_OOK) {
    this->pa_table_[0] = 0;
    this->pa_table_[1] = a;
  } else {
    this->pa_table_[0] = a;
    this->pa_table_[1] = 0;
  }
  this->output_power_effective_ = value;
  if (!this->reset_)
    return;
  this->write_(Register::PATABLE, this->pa_table_, sizeof(this->pa_table_));
}

void CC1101Component::set_rx_attenuation(RxAttenuation value) {
  if (!is_enum_valid(value))
    return;
  this->state_.CLOSE_IN_RX = (uint8_t) value;
  if (!this->reset_)
    return;
  this->write_(Register::FIFOTHR);
}

void CC1101Component::set_dc_blocking_filter(bool value) {
  this->state_.DEM_DCFILT_OFF = value ? 0 : 1;
  if (!this->reset_)
    return;
  this->write_(Register::MDMCFG2);
}

void CC1101Component::set_frequency(float value) {
  if (!is_float_in_range(value, FREQUENCY_MIN, FREQUENCY_MAX))
    return;
  int freq = (int) (value * (1 << 16) / XTAL_FREQUENCY);
  this->state_.FREQ2 = (uint8_t) (freq >> 16);
  this->state_.FREQ1 = (uint8_t) (freq >> 8);
  this->state_.FREQ0 = (uint8_t) freq;
  if (!this->reset_)
    return;
  this->enter_idle_();
  this->write_(Register::FREQ2);
  this->write_(Register::FREQ1);
  this->write_(Register::FREQ0);
  this->enter_rx_();
}

void CC1101Component::set_if_frequency(float value) {
  if (!is_float_in_range(value, IF_FREQUENCY_MIN, IF_FREQUENCY_MAX))
    return;
  this->state_.FREQ_IF = value * (1 << 10) / XTAL_FREQUENCY;
  if (!this->reset_)
    return;
  this->write_(Register::FSCTRL1);
}

void CC1101Component::set_filter_bandwidth(float value) {
  if (!is_float_in_range(value, BANDWIDTH_MIN, BANDWIDTH_MAX))
    return;
  uint8_t e;
  uint32_t m;
  split_float(XTAL_FREQUENCY / (value * 8), 2, e, m);
  this->state_.CHANBW_E = (uint8_t) e;
  this->state_.CHANBW_M = (uint8_t) m;
  if (!this->reset_)
    return;
  this->write_(Register::MDMCFG4);
}

void CC1101Component::set_channel(uint8_t value) {
  if (!is_int_in_range(value, CHANNEL_MIN, CHANNEL_MAX))
    return;
  this->state_.CHANNR = value;
  if (!this->reset_)
    return;
  this->enter_idle_();
  this->write_(Register::CHANNR);
  this->enter_rx_();
}

void CC1101Component::set_channel_spacing(float value) {
  if (!is_float_in_range(value, CHANNEL_SPACING_MIN, CHANNEL_SPACING_MAX))
    return;
  uint8_t e;
  uint32_t m;
  split_float(value * (1 << 18) / XTAL_FREQUENCY, 8, e, m);
  this->state_.CHANSPC_E = (uint8_t) e;
  this->state_.CHANSPC_M = (uint8_t) m;
  if (!this->reset_)
    return;
  this->write_(Register::MDMCFG1);
  this->write_(Register::MDMCFG0);
}

void CC1101Component::set_fsk_deviation(float value) {
  if (!is_float_in_range(value, FSK_DEVIATION_MIN, FSK_DEVIATION_MAX))
    return;
  uint8_t e;
  uint32_t m;
  split_float(value * (1 << 17) / XTAL_FREQUENCY, 3, e, m);
  this->state_.DEVIATION_E = (uint8_t) e;
  this->state_.DEVIATION_M = (uint8_t) m;
  if (!this->reset_)
    return;
  this->write_(Register::DEVIATN);
}

void CC1101Component::set_msk_deviation(uint8_t value) {
  if (!is_int_in_range(value, MSK_DEVIATION_MIN, MSK_DEVIATION_MAX))
    return;
  this->state_.DEVIATION_E = 0;
  this->state_.DEVIATION_M = value - 1;
  if (!this->reset_)
    return;
  this->write_(Register::DEVIATN);
}

void CC1101Component::set_symbol_rate(float value) {
  if (!is_float_in_range(value, SYMBOL_RATE_MIN, SYMBOL_RATE_MAX))
    return;
  uint8_t e;
  uint32_t m;
  split_float(value * (1 << 28) / (XTAL_FREQUENCY * 1000), 8, e, m);
  this->state_.DRATE_E = (uint8_t) e;
  this->state_.DRATE_M = (uint8_t) m;
  if (!this->reset_)
    return;
  this->write_(Register::MDMCFG4);
  this->write_(Register::MDMCFG3);
}

void CC1101Component::set_sync_mode(SyncMode value) {
  if (!is_enum_valid(value))
    return;
  this->state_.SYNC_MODE = (uint8_t) value;
  if (!this->reset_)
    return;
  this->write_(Register::MDMCFG2);
}

void CC1101Component::set_carrier_sense_above_threshold(bool value) {
  this->state_.CARRIER_SENSE_ABOVE_THRESHOLD = value ? 1 : 0;
  if (!this->reset_)
    return;
  this->write_(Register::MDMCFG2);
}

void CC1101Component::set_modulation_type(Modulation value) {
  if (!is_enum_valid(value))
    return;
  this->state_.MOD_FORMAT = (uint8_t) value;
  this->state_.PA_POWER = value == Modulation::MODULATION_ASK_OOK ? 1 : 0;
  if (!this->reset_)
    return;
  this->enter_idle_();
  this->write_(Register::MDMCFG2);
  this->write_(Register::FREND0);
  this->enter_rx_();
}

void CC1101Component::set_manchester(bool value) {
  this->state_.MANCHESTER_EN = value ? 1 : 0;
  if (!this->reset_)
    return;
  this->write_(Register::MDMCFG2);
}

void CC1101Component::set_num_preamble(uint8_t value) {
  this->state_.NUM_PREAMBLE = value;
  if (!this->reset_)
    return;
  this->write_(Register::MDMCFG1);
}

void CC1101Component::set_sync1(uint8_t value) {
  this->state_.SYNC1 = value;
  if (!this->reset_)
    return;
  this->write_(Register::SYNC1);
}

void CC1101Component::set_sync0(uint8_t value) {
  this->state_.SYNC0 = value;
  if (!this->reset_)
    return;
  this->write_(Register::SYNC0);
}

void CC1101Component::set_pktlen(uint8_t value) {
  this->state_.PKTLEN = value;
  if (!this->reset_)
    return;
  this->write_(Register::PKTLEN);
}

void CC1101Component::set_magn_target(MagnTarget value) {
  if (!is_enum_valid(value))
    return;
  this->state_.MAGN_TARGET = (uint8_t) value;
  if (!this->reset_)
    return;
  this->write_(Register::AGCCTRL2);
}

void CC1101Component::set_max_lna_gain(MaxLnaGain value) {
  if (!is_enum_valid(value))
    return;
  this->state_.MAX_LNA_GAIN = (uint8_t) value;
  if (!this->reset_)
    return;
  this->write_(Register::AGCCTRL2);
}

void CC1101Component::set_max_dvga_gain(MaxDvgaGain value) {
  if (!is_enum_valid(value))
    return;
  this->state_.MAX_DVGA_GAIN = (uint8_t) value;
  if (!this->reset_)
    return;
  this->write_(Register::AGCCTRL2);
}

void CC1101Component::set_carrier_sense_abs_thr(int8_t value) {
  if (!is_int_in_range(value, CARRIER_SENSE_ABS_THR_MIN, CARRIER_SENSE_ABS_THR_MAX))
    return;
  this->state_.CARRIER_SENSE_ABS_THR = (uint8_t) (value & 0b1111);
  if (!this->reset_)
    return;
  this->write_(Register::AGCCTRL1);
}

void CC1101Component::set_carrier_sense_rel_thr(CarrierSenseRelThr value) {
  if (!is_enum_valid(value))
    return;
  this->state_.CARRIER_SENSE_REL_THR = (uint8_t) value;
  if (!this->reset_)
    return;
  this->write_(Register::AGCCTRL1);
}

void CC1101Component::set_lna_priority(bool value) {
  this->state_.AGC_LNA_PRIORITY = value ? 1 : 0;
  if (!this->reset_)
    return;
  this->write_(Register::AGCCTRL1);
}

void CC1101Component::set_filter_length_fsk_msk(FilterLengthFskMsk value) {
  if (!is_enum_valid(value))
    return;
  this->state_.FILTER_LENGTH = (uint8_t) value;
  if (!this->reset_)
    return;
  this->write_(Register::AGCCTRL0);
}

void CC1101Component::set_filter_length_ask_ook(FilterLengthAskOok value) {
  if (!is_enum_valid(value))
    return;
  this->state_.FILTER_LENGTH = (uint8_t) value;
  if (!this->reset_)
    return;
  this->write_(Register::AGCCTRL0);
}

void CC1101Component::set_freeze(Freeze value) {
  if (!is_enum_valid(value))
    return;
  this->state_.AGC_FREEZE = (uint8_t) value;
  if (!this->reset_)
    return;
  this->write_(Register::AGCCTRL0);
}

void CC1101Component::set_wait_time(WaitTime value) {
  if (!is_enum_valid(value))
    return;
  this->state_.WAIT_TIME = (uint8_t) value;
  if (!this->reset_)
    return;
  this->write_(Register::AGCCTRL0);
}

void CC1101Component::set_hyst_level(HystLevel value) {
  if (!is_enum_valid(value))
    return;
  this->state_.HYST_LEVEL = (uint8_t) value;
  if (!this->reset_)
    return;
  this->write_(Register::AGCCTRL0);
}

}  // namespace cc1101
}  // namespace esphome
