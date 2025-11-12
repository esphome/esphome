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

// Helper for checking enums
template<typename T> constexpr bool is_enum_valid(T value) {
  // Assumes your enums all have a ::LAST member
  return value < T::LAST;
}

// Helper for checking float ranges
constexpr bool is_float_in_range(float value, float min, float max) { return value >= min && value <= max; }

// Helper for checking int ranges
constexpr bool is_int_in_range(int value, int min, int max) { return value >= min && value <= max; }

CC1101Component::CC1101Component() {
  this->gdo0_ = nullptr;
  this->reset_ = false;
  this->output_power_requested_ = 11.0f;
  this->output_power_effective_ = 11.0f;
  memset(this->pa_table_, 0, sizeof(pa_table_));
  memset(&this->state_, 0, sizeof(this->state_));

  // datasheet defaults (non-listed fields are zero)
  this->state_.GDO2_CFG = 0x29;
  this->state_.GDO1_CFG = 0x2E;
  this->state_.GDO0_CFG = 0x3F;
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

  // MDMCFG0
  this->state_.FS_AUTOCAL = 1;

  this->set_tuner_frequency(433920);
  this->set_tuner_if_frequency(153);
  this->set_tuner_filter_bandwidth(203);
  this->set_tuner_channel(0);
  this->set_tuner_channel_spacing(200);
  this->set_tuner_symbol_rate(5000);
  this->set_tuner_sync_mode(SyncMode::SYNC_MODE_NONE);
  this->set_tuner_carrier_sense_above_threshold(true);
  this->set_tuner_modulation_type(Modulation::MODULATION_ASK_OOK);
  this->set_agc_magn_target(MagnTarget::MAGN_TARGET_42DB);
  this->set_agc_max_lna_gain(MaxLnaGain::MAX_LNA_GAIN_DEFAULT);
  this->set_agc_max_dvga_gain(MaxDvgaGain::MAX_DVGA_GAIN_MINUS_3);
  this->set_agc_lna_priority(false);
  this->set_agc_wait_time(WaitTime::WAIT_TIME_32_SAMPLES);
}

void CC1101Component::setup() {
  if (this->gdo0_ != nullptr) {
#ifdef USE_ESP8266
    this->gdo0_->setup();
    this->gdo0_->pin_mode(gpio::FLAG_INPUT);
#endif
  }

  // datasheet 19.1.2 - Hardware CS strobe
  this->cs_->digital_write(true);
  delayMicroseconds(1);
  this->cs_->digital_write(false);
  delayMicroseconds(1);
  this->cs_->digital_write(true);
  delayMicroseconds(41);
  this->cs_->digital_write(false);
  // We cannot use a long delay here
  // 5ms is acceptable, but the CHIP_RDYn poll in the loop
  // will handle the real waiting.
  delayMicroseconds(5000);

  this->spi_setup();

  // Kick-start the state machine in loop()
  // We do NOT send commands or read registers here.
  this->component_state_ = ComponentState::SETUP_START;
}

void CC1101Component::dump_config() {
  ESP_LOGCONFIG(TAG, "CC1101:");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  GDO0: ", this->gdo0_);
}

void CC1101Component::loop() {
  if (this->is_waiting_) {
    Command cmd = this->cmd_queue_.front();
    bool ready = false;

    if (cmd == Command::RES) {
      // We are waiting for the SRES command to complete.
      // We poll by sending NOP strobes and checking the
      // CHIP_RDYn bit (bit 7) of the returned status byte.
      uint8_t status = this->strobe_(Command::NOP);

      // CHIP_RDYn is bit 7. If bit 7 is 0, the chip is ready.
      if ((status & 0x80) == 0) {
        ready = true;
      }
      // The 1500ms timeout below will catch this if it never becomes ready
    } else {
      // For all other commands, we poll the MARCSTATE register
      this->read_(Register::MARCSTATE);
      State s = (State) this->state_.MARC_STATE;
      if (cmd == Command::IDLE) {
        if (s == State::IDLE)
          ready = true;
      } else if (cmd == Command::RX) {
        if (s == State::RX || s == State::RX_END || s == State::RXTX_SWITCH)
          ready = true;
      } else if (cmd == Command::TX) {
        if (s == State::TX || s == State::TX_END || s == State::TXRX_SWITCH)
          ready = true;
      } else {
        ready = true;
      }
    }

    if (ready) {
      this->is_waiting_ = false;
      this->cmd_queue_.pop_front();
      if (cmd == Command::RES) {
        this->reset_ = true;
      }
    } else if (millis() - this->wait_start_time_ > 1500) {
      ESP_LOGE(TAG, "Wait for command %02X to complete timed out.", (uint8_t) cmd);
      this->is_waiting_ = false;
      this->cmd_queue_.clear();
      this->component_state_ = ComponentState::IDLE;
    }
    return;
  }

  if (!this->cmd_queue_.empty()) {
    Command cmd = this->cmd_queue_.front();
    ESP_LOGV(TAG, "Strobing %02X", (uint8_t) cmd);
    this->strobe_(cmd);        // This just sends the command
    this->is_waiting_ = true;  // The block above will handle the wait
    this->wait_start_time_ = millis();
    return;
  }

  switch (this->component_state_) {
    case ComponentState::IDLE:
      if (this->freq_request_pending_) {
        this->freq_request_pending_ = false;
        this->set_tuner_frequency(this->requested_freq_);
      }
      break;

    // This is the main setup sequence, starting from setup()
    case ComponentState::SETUP_START:
      this->send_(Command::RES);
      this->component_state_ = ComponentState::SETUP_WAIT_RESET;
      break;
    case ComponentState::SETUP_WAIT_RESET:
      // Wait for the RES command to be processed by the is_waiting_ block
      if (this->reset_) {
        this->read_(Register::PARTNUM);
        this->read_(Register::VERSION);
        if (this->state_.VERSION == 0 || this->state_.PARTNUM == 0xFF) {
          ESP_LOGE(TAG, "Failed to read CC1101 version. Check connection.");
          this->mark_failed();
          this->component_state_ = ComponentState::IDLE;  // Stop setup
          return;
        }
        char buff[32] = {0};
        snprintf(buff, sizeof(buff), "%02X%02X", this->state_.PARTNUM, this->state_.VERSION);
        this->chip_id_ = buff;
        ESP_LOGD(TAG, "%s was found", this->chip_id_.c_str());
        this->component_state_ = ComponentState::SETUP_WRITE_REGS;
      }
      break;
    case ComponentState::SETUP_WRITE_REGS:
      for (uint8_t i = 0; i <= 0x2E; i++) {
        if (i == 0x29 || i == 0x2B) {
          continue;
        }
        this->write_((Register) i);
      }
      this->write_(Register::PATABLE, this->pa_table_, sizeof(this->pa_table_));
      this->component_state_ = ComponentState::SETUP_WAIT_RX;
      this->send_(Command::RX);
      break;
    case ComponentState::SETUP_WAIT_RX:
      if (this->cmd_queue_.empty()) {
        ESP_LOGD(TAG, "CC1101 setup complete.");
        this->component_state_ = ComponentState::IDLE;
      }
      break;

    // This is the frequency change state machine
    case ComponentState::SET_FREQ_START:
      this->component_state_ = ComponentState::SET_FREQ_WAIT_IDLE;
      this->send_(Command::IDLE);
      break;
    case ComponentState::SET_FREQ_WAIT_IDLE:
      if (this->cmd_queue_.empty()) {
        this->component_state_ = ComponentState::SET_FREQ_WRITE_REGS;
      }
      break;
    case ComponentState::SET_FREQ_WRITE_REGS:
      this->write_(Register::FREQ2);
      this->write_(Register::FREQ1);
      this->write_(Register::FREQ0);
      this->component_state_ = ComponentState::SET_FREQ_WAIT_RX;
      this->send_(Command::RX);
      break;
    case ComponentState::SET_FREQ_WAIT_RX:
      if (this->cmd_queue_.empty()) {
        this->component_state_ = ComponentState::IDLE;
      }
      break;
  }
}

void CC1101Component::begin_tx() {
  this->send_(Command::TX);

  if (this->gdo0_ != nullptr) {
#ifdef USE_ESP8266
    // On ESP8266, we explicitly switch the pin to output for TX
    this->gdo0_->pin_mode(gpio::FLAG_OUTPUT);
#endif
  }
}

void CC1101Component::end_tx() {
  if (this->gdo0_ != nullptr) {
#ifdef USE_ESP8266
    // On ESP8266, switch back to input for RX
    this->gdo0_->pin_mode(gpio::FLAG_INPUT);
#endif
  }

  this->send_(Command::RX);
}

void CC1101Component::reset() { this->component_state_ = ComponentState::SETUP_START; }

uint8_t CC1101Component::strobe_(Command cmd) {
  uint8_t index = (uint8_t) cmd;
  if (cmd < Command::RES || cmd > Command::NOP) {
    ESP_LOGE(TAG, "%s(0x%02X) invalid register address", __func__, index);
    return 0xFF;  // Return an error status
  }

  this->enable();
  // Send the command and read the status byte returned on MISO
  uint8_t status_byte = this->transfer_byte(index);
  this->disable();

  ESP_LOGV(TAG, "%s(0x%02X) status=0x%02X", __func__, index, status_byte);
  return status_byte;
}

void CC1101Component::write_(Register reg) {
  uint8_t index = (uint8_t) reg;
  if (reg > Register::TEST0 || reg == Register::FSTEST || reg == Register::AGCTEST) {
    ESP_LOGE(TAG, "%s(0x%02X) invalid register address", __func__, index);
    return;
  }

  uint8_t value = this->regs_[index];

  this->enable();
  this->write_byte(index);
  this->transfer_array(&value, 1);
  this->disable();

  ESP_LOGV(TAG, "%s(0x%02X) = 0x%02X", __func__, index, this->regs_[index]);
}

void CC1101Component::write_(Register reg, uint8_t value) {
  uint8_t index = (uint8_t) reg;
  if (reg > Register::TEST0 || reg == Register::FSTEST || reg == Register::AGCTEST) {
    ESP_LOGE(TAG, "%s(0x%02X) invalid register address", __func__, index);
    return;
  }

  this->regs_[index] = value;
  this->write_(reg);
}

void CC1101Component::write_(Register reg, uint8_t *buffer, size_t length) {
  uint8_t index = (uint8_t) reg;
  if (reg != Register::PATABLE && reg != Register::FIFO) {
    ESP_LOGE(TAG, "%s(0x%02X) invalid register address", __func__, index);
    return;
  }

  this->enable();
  this->write_byte(index | BUS_WRITE | BUS_BURST);
  this->transfer_array(buffer, length);
  this->disable();

  ESP_LOGV(TAG, "%s(0x%02X) %zu", __func__, index, length);
}

bool CC1101Component::read_(Register reg) {
  uint8_t index = (uint8_t) reg;
  if (reg > Register::RCCTRL0_STATUS) {
    ESP_LOGE(TAG, "%s(0x%02X) invalid register address", __func__, index);
    return false;
  }

  this->enable();
  this->write_byte(index | BUS_READ | BUS_BURST);
  uint8_t value = this->transfer_byte(0);
  this->regs_[index] = value;
  this->disable();

  return true;
}

bool CC1101Component::read_(Register reg, uint8_t *buffer, size_t length) {
  uint8_t index = (uint8_t) reg;
  if (reg != Register::PATABLE && reg != Register::FIFO) {
    ESP_LOGE(TAG, "%s(0x%02X) invalid register address", __func__, index);
    return false;
  }

  this->enable();
  this->write_byte(index | BUS_READ | BUS_BURST);
  this->read_array(buffer, length);
  this->disable();

  return true;
}

void CC1101Component::send_(Command cmd) {
  if (cmd == Command::TX || cmd == Command::RX || cmd == Command::PWD) {
    this->cmd_queue_.push_back(Command::IDLE);
  }
  this->cmd_queue_.push_back(cmd);
}

template<typename T> static T get_enum_last(T value) { return T::LAST; }

// #define CHECK_ENUM(value) \
//   if ((value) >= get_enum_last(value)) { \
//     ESP_LOGE(TAG, "%s(%d) invalid", __func__, (int) (value)); \
//     return; \
//   }

// #define CHECK_FLOAT_RANGE(value, min_value, max_value) \
//   if ((value) < (min_value) || (value) > (max_value)) { \
//     ESP_LOGE(TAG, "%s(%.2f) invalid (%.2f - %.2f)", __func__, value, min_value, max_value); \
//     return; \
//   }

// #define CHECK_INT_RANGE(value, min_value, max_value) \
//   if ((value) < (min_value) || (value) > (max_value)) { \
//     ESP_LOGE(TAG, "%s(%d) invalid (%d - %d)", __func__, (int) (value), (int) (min_value), (int) (max_value)); \
//     return; \
//   }

static void split_float(float value, int mbits, uint8_t &e, uint32_t &m) {
  if (value < 0) {
    ESP_LOGE(TAG, "split_float(%f, %d): positive values only", value, mbits);
  }

  int e_tmp;
  float m_tmp = std::frexp(value, &e_tmp);

  if (e_tmp <= mbits) {
    ESP_LOGW(TAG, "split_float(%f, %d): exponent would be negative, set to minimum", value, mbits);
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

void CC1101Component::set_output_power(float value) {
  // CHECK_FLOAT_RANGE(value, OUTPUT_POWER_MIN, OUTPUT_POWER_MAX);
  if (!is_float_in_range(value, OUTPUT_POWER_MIN, OUTPUT_POWER_MAX)) {
    ESP_LOGE(TAG, "Invalid Output Power: %f", value);
    return;
  }

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
  } else {
    ESP_LOGE(TAG, "frequency out of range: %d", freq);
  }

  if ((Modulation) this->state_.MOD_FORMAT == Modulation::MODULATION_ASK_OOK) {
    this->pa_table_[0] = 0;
    this->pa_table_[1] = a;
  } else {
    this->pa_table_[0] = a;
    this->pa_table_[1] = 0;
  }

  this->output_power_effective_ = value;

  ESP_LOGD(TAG, "set_output_power(%.1f) %d", value, a);

  if (!this->reset_) {
    return;
  }

  this->write_(Register::PATABLE, this->pa_table_, sizeof(this->pa_table_));
}
void CC1101Component::set_rx_attenuation(RxAttenuation value) {
  // CHECK_ENUM(value);
  if (!is_enum_valid(value)) {
    ESP_LOGE(TAG, "Invalid RX Attenuation: %d", (int) value);
    return;
  }

  this->state_.CLOSE_IN_RX = (uint8_t) value;
  if (!this->reset_) {
    return;
  }
  this->write_(Register::FIFOTHR);
}
void CC1101Component::set_dc_blocking_filter(bool value) {
  this->state_.DEM_DCFILT_OFF = value ? 0 : 1;
  if (!this->reset_) {
    return;
  }
  this->write_(Register::MDMCFG2);
}
void CC1101Component::set_tuner_frequency(float value) {
  // CHECK_FLOAT_RANGE(value, FREQUENCY_MIN, FREQUENCY_MAX);
  if (!is_float_in_range(value, FREQUENCY_MIN, FREQUENCY_MAX)) {
    ESP_LOGE(TAG, "Invalid frequency: %f", value);
    return;
  }
  if (this->component_state_ != ComponentState::IDLE) {
    this->requested_freq_ = value;
    this->freq_request_pending_ = true;
    return;
  }
  int freq = (int) (value * (1 << 16) / XTAL_FREQUENCY);
  this->state_.FREQ2 = (uint8_t) (freq >> 16);
  this->state_.FREQ1 = (uint8_t) (freq >> 8);
  this->state_.FREQ0 = (uint8_t) freq;
  if (!this->reset_) {
    return;
  }
  this->component_state_ = ComponentState::SET_FREQ_START;
}
void CC1101Component::set_tuner_if_frequency(float value) {
  // CHECK_FLOAT_RANGE(value, IF_FREQUENCY_MIN, IF_FREQUENCY_MAX);
  if (!is_float_in_range(value, IF_FREQUENCY_MIN, IF_FREQUENCY_MAX)) {
    ESP_LOGE(TAG, "Invalid IF Frequency: %f", value);
    return;
  }
  this->state_.FREQ_IF = value * (1 << 10) / XTAL_FREQUENCY;
  if (!this->reset_) {
    return;
  }
  this->write_(Register::FSCTRL1);
}
void CC1101Component::set_tuner_filter_bandwidth(float value) {
  // CHECK_FLOAT_RANGE(value, BANDWIDTH_MIN, BANDWIDTH_MAX);
  if (!is_float_in_range(value, BANDWIDTH_MIN, BANDWIDTH_MAX)) {
    ESP_LOGE(TAG, "Invalid Filter Bandwidth: %f", value);
    return;
  }
  uint8_t e;
  uint32_t m;
  split_float(XTAL_FREQUENCY / (value * 8), 2, e, m);
  this->state_.CHANBW_E = (uint8_t) e;
  this->state_.CHANBW_M = (uint8_t) m;
  if (!this->reset_) {
    return;
  }
  this->write_(Register::MDMCFG4);
}
void CC1101Component::set_tuner_channel(uint8_t value) {
  // CHECK_INT_RANGE(value, CHANNEL_MIN, CHANNEL_MAX);
  if (!is_int_in_range(value, CHANNEL_MIN, CHANNEL_MAX)) {
    ESP_LOGE(TAG, "Invalid Channel: %d", (int) value);
    return;
  }
  this->state_.CHANNR = value;
  if (!this->reset_) {
    return;
  }
  this->write_(Register::CHANNR);
}
void CC1101Component::set_tuner_channel_spacing(float value) {
  // CHECK_FLOAT_RANGE(value, CHANNEL_SPACING_MIN, CHANNEL_SPACING_MAX);
  if (!is_float_in_range(value, CHANNEL_SPACING_MIN, CHANNEL_SPACING_MAX)) {
    ESP_LOGE(TAG, "Invalid Channel Spacing: %f", value);
    return;
  }
  uint8_t e;
  uint32_t m;
  split_float(value * (1 << 18) / XTAL_FREQUENCY, 8, e, m);
  this->state_.CHANSPC_E = (uint8_t) e;
  this->state_.CHANSPC_M = (uint8_t) m;
  if (!this->reset_) {
    return;
  }
  this->write_(Register::MDMCFG1);
  this->write_(Register::MDMCFG0);
}
void CC1101Component::set_tuner_fsk_deviation(float value) {
  // CHECK_FLOAT_RANGE(value, FSK_DEVIATION_MIN, FSK_DEVIATION_MAX);
  if (!is_float_in_range(value, FSK_DEVIATION_MIN, FSK_DEVIATION_MAX)) {
    ESP_LOGE(TAG, "Invalid FSK Deviation: %f", value);
    return;
  }
  uint8_t e;
  uint32_t m;
  split_float(value * (1 << 17) / XTAL_FREQUENCY, 3, e, m);
  this->state_.DEVIATION_E = (uint8_t) e;
  this->state_.DEVIATION_M = (uint8_t) m;
  if (!this->reset_) {
    return;
  }
  this->write_(Register::DEVIATN);
}
void CC1101Component::set_tuner_msk_deviation(uint8_t value) {
  // CHECK_INT_RANGE(value, MSK_DEVIATION_MIN, MSK_DEVIATION_MAX);
  if (!is_int_in_range(value, MSK_DEVIATION_MIN, MSK_DEVIATION_MAX)) {
    ESP_LOGE(TAG, "Invalid MSK Deviation: %d", (int) value);
    return;
  }
  this->state_.DEVIATION_E = 0;
  this->state_.DEVIATION_M = value - 1;
  if (!this->reset_) {
    return;
  }
  this->write_(Register::DEVIATN);
}
void CC1101Component::set_tuner_symbol_rate(float value) {
  // CHECK_FLOAT_RANGE(value, SYMBOL_RATE_MIN, SYMBOL_RATE_MAX);
  if (!is_float_in_range(value, SYMBOL_RATE_MIN, SYMBOL_RATE_MAX)) {
    ESP_LOGE(TAG, "Invalid Symbol Rate: %f", value);
    return;
  }
  uint8_t e;
  uint32_t m;
  split_float(value * (1 << 28) / (XTAL_FREQUENCY * 1000), 8, e, m);
  this->state_.DRATE_E = (uint8_t) e;
  this->state_.DRATE_M = (uint8_t) m;
  if (!this->reset_) {
    return;
  }
  this->write_(Register::MDMCFG4);
  this->write_(Register::MDMCFG3);
}
void CC1101Component::set_tuner_sync_mode(SyncMode value) {
  // CHECK_ENUM(value);
  if (!is_enum_valid(value)) {
    ESP_LOGE(TAG, "Invalid Sync Mode: %d", (int) value);
    return;
  }
  this->state_.SYNC_MODE = (uint8_t) value;
  if (!this->reset_) {
    return;
  }
  this->write_(Register::MDMCFG2);
}
void CC1101Component::set_tuner_carrier_sense_above_threshold(bool value) {
  this->state_.CARRIER_SENSE_ABOVE_THRESHOLD = value ? 1 : 0;
  if (!this->reset_) {
    return;
  }
  this->write_(Register::MDMCFG2);
}
void CC1101Component::set_tuner_modulation_type(Modulation value) {
  // CHECK_ENUM(value);
  if (!is_enum_valid(value)) {
    ESP_LOGE(TAG, "Invalid Modulation Type: %d", (int) value);
    return;
  }
  this->state_.MOD_FORMAT = (uint8_t) value;
  this->state_.PA_POWER = value == Modulation::MODULATION_ASK_OOK ? 1 : 0;
  if (!this->reset_) {
    return;
  }
  this->write_(Register::MDMCFG2);
  this->write_(Register::FREND0);
}
void CC1101Component::set_tuner_manchester(bool value) {
  this->state_.MANCHESTER_EN = value ? 1 : 0;
  if (!this->reset_) {
    return;
  }
  this->write_(Register::MDMCFG2);
}
void CC1101Component::set_tuner_num_preamble(uint8_t value) {
  this->state_.NUM_PREAMBLE = value;
  if (!this->reset_) {
    return;
  }
  this->write_(Register::MDMCFG1);
}
void CC1101Component::set_tuner_sync1(uint8_t value) {
  this->state_.SYNC1 = value;
  if (!this->reset_) {
    return;
  }
  this->write_(Register::SYNC1);
}
void CC1101Component::set_tuner_sync0(uint8_t value) {
  this->state_.SYNC0 = value;
  if (!this->reset_) {
    return;
  }
  this->write_(Register::SYNC0);
}
void CC1101Component::set_tuner_pktlen(uint8_t value) {
  this->state_.PKTLEN = value;
  if (!this->reset_) {
    return;
  }
  this->write_(Register::PKTLEN);
}
void CC1101Component::set_agc_magn_target(MagnTarget value) {
  // CHECK_ENUM(value);
  if (!is_enum_valid(value)) {
    ESP_LOGE(TAG, "Invalid MAGN Target: %d", (int) value);
    return;
  }
  this->state_.MAGN_TARGET = (uint8_t) value;
  if (!this->reset_) {
    return;
  }
  this->write_(Register::AGCCTRL2);
}
void CC1101Component::set_agc_max_lna_gain(MaxLnaGain value) {
  // CHECK_ENUM(value);
  if (!is_enum_valid(value)) {
    ESP_LOGE(TAG, "Invalid LNA Gain: %d", (int) value);
    return;
  }
  this->state_.MAX_LNA_GAIN = (uint8_t) value;
  if (!this->reset_) {
    return;
  }
  this->write_(Register::AGCCTRL2);
}
void CC1101Component::set_agc_max_dvga_gain(MaxDvgaGain value) {
  // CHECK_ENUM(value);
  if (!is_enum_valid(value)) {
    ESP_LOGE(TAG, "Invalid DVGA Gain: %d", (int) value);
    return;
  }
  this->state_.MAX_DVGA_GAIN = (uint8_t) value;
  if (!this->reset_) {
    return;
  }
  this->write_(Register::AGCCTRL2);
}
void CC1101Component::set_agc_carrier_sense_abs_thr(int8_t value) {
  // CHECK_INT_RANGE(value, CARRIER_SENSE_ABS_THR_MIN, CARRIER_SENSE_ABS_THR_MAX);
  if (!is_int_in_range(value, CARRIER_SENSE_ABS_THR_MIN, CARRIER_SENSE_ABS_THR_MAX)) {
    ESP_LOGE(TAG, "Invalid CS ABS Threshold: %d", (int) value);
    return;
  }
  this->state_.CARRIER_SENSE_ABS_THR = (uint8_t) (value & 0b1111);
  if (!this->reset_) {
    return;
  }
  this->write_(Register::AGCCTRL1);
}
void CC1101Component::set_agc_carrier_sense_rel_thr(CarrierSenseRelThr value) {
  // CHECK_ENUM(value);
  if (!is_enum_valid(value)) {
    ESP_LOGE(TAG, "Invalid CS Rel Threshold: %d", (int) value);
    return;
  }
  this->state_.CARRIER_SENSE_REL_THR = (uint8_t) value;
  if (!this->reset_) {
    return;
  }
  this->write_(Register::AGCCTRL1);
}
void CC1101Component::set_agc_lna_priority(bool value) {
  this->state_.AGC_LNA_PRIORITY = value ? 1 : 0;
  if (!this->reset_) {
    return;
  }
  this->write_(Register::AGCCTRL1);
}
void CC1101Component::set_agc_filter_length_fsk_msk(FilterLengthFskMsk value) {
  // CHECK_ENUM(value);
  if (!is_enum_valid(value)) {
    ESP_LOGE(TAG, "Invalid Filter Length: %d", (int) value);
    return;
  }
  this->state_.FILTER_LENGTH = (uint8_t) value;
  if (!this->reset_) {
    return;
  }
  this->write_(Register::AGCCTRL0);
}
void CC1101Component::set_agc_filter_length_ask_ook(FilterLengthAskOok value) {
  // CHECK_ENUM(value);
  if (!is_enum_valid(value)) {
    ESP_LOGE(TAG, "Invalid Filter Length: %d", (int) value);
    return;
  }
  this->state_.FILTER_LENGTH = (uint8_t) value;
  if (!this->reset_) {
    return;
  }
  this->write_(Register::AGCCTRL0);
}
void CC1101Component::set_agc_freeze(Freeze value) {
  // CHECK_ENUM(value);
  if (!is_enum_valid(value)) {
    ESP_LOGE(TAG, "Invalid AGC Freeze Setting: %d", (int) value);
    return;
  }
  this->state_.AGC_FREEZE = (uint8_t) value;
  if (!this->reset_) {
    return;
  }
  this->write_(Register::AGCCTRL0);
}
void CC1101Component::set_agc_wait_time(WaitTime value) {
  // CHECK_ENUM(value);
  if (!is_enum_valid(value)) {
    ESP_LOGE(TAG, "Invalid AGC Wait Time: %d", (int) value);
    return;
  }
  this->state_.WAIT_TIME = (uint8_t) value;
  if (!this->reset_) {
    return;
  }
  this->write_(Register::AGCCTRL0);
}
void CC1101Component::set_agc_hyst_level(HystLevel value) {
  // CHECK_ENUM(value);
  if (!is_enum_valid(value)) {
    ESP_LOGE(TAG, "Invalid AGC Hyst Level: %d", (int) value);
    return;
  }
  this->state_.HYST_LEVEL = (uint8_t) value;
  if (!this->reset_) {
    return;
  }
  this->write_(Register::AGCCTRL0);
}

}  // namespace cc1101
}  // namespace esphome
