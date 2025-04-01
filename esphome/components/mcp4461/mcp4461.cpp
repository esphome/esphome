#include "mcp4461.h"

#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace mcp4461 {

static const char *const TAG = "mcp4461";
constexpr uint8_t EEPROM_WRITE_TIMEOUT_MS = 10;

void Mcp4461Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up mcp4461 using address (0x%02X)...", this->address_);
  auto err = this->write(nullptr, 0);
  if (err != i2c::ERROR_OK) {
    this->error_code_ = MCP4461_STATUS_I2C_ERROR;
    this->mark_failed();
    return;
  }
  // save WP/WL status
  this->update_write_protection_status_();
  for (uint8_t i = 0; i < 8; i++) {
    if (this->reg_[i].initial_value.has_value()) {
      uint16_t initial_state = static_cast<uint16_t>(*this->reg_[i].initial_value * 256.0f);
      this->write_wiper_level_(i, initial_state);
    }
    if (this->reg_[i].enabled) {
      this->reg_[i].state = this->read_wiper_level_(i);
    } else {
      // only volatile wipers can be set disabled on hw level
      if (i < 4) {
        this->reg_[i].state = 0;
        Mcp4461WiperIdx wiper_idx = static_cast<Mcp4461WiperIdx>(i);
        this->disable_wiper_(wiper_idx);
      }
    }
  }
}

void Mcp4461Component::set_initial_value(Mcp4461WiperIdx wiper, float initial_value) {
  uint8_t wiper_idx = static_cast<uint8_t>(wiper);
  this->reg_[wiper_idx].initial_value = initial_value;
}

void Mcp4461Component::initialize_terminal_disabled(Mcp4461WiperIdx wiper, char terminal) {
  uint8_t wiper_idx = static_cast<uint8_t>(wiper);
  switch (terminal) {
    case 'a':
      this->reg_[wiper_idx].terminal_a = false;
      break;
    case 'b':
      this->reg_[wiper_idx].terminal_b = false;
      break;
    case 'w':
      this->reg_[wiper_idx].terminal_w = false;
      break;
  }
}

void Mcp4461Component::update_write_protection_status_() {
  uint8_t status_register_value = this->get_status_register_();
  this->write_protected_ = static_cast<bool>((status_register_value >> 0) & 0x01);
  this->reg_[0].wiper_lock_active = static_cast<bool>((status_register_value >> 2) & 0x01);
  this->reg_[1].wiper_lock_active = static_cast<bool>((status_register_value >> 3) & 0x01);
  this->reg_[2].wiper_lock_active = static_cast<bool>((status_register_value >> 5) & 0x01);
  this->reg_[3].wiper_lock_active = static_cast<bool>((status_register_value >> 6) & 0x01);
}

void Mcp4461Component::dump_config() {
  ESP_LOGCONFIG(TAG, "mcp4461:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "%s", LOG_STR_ARG(this->get_message_string(this->error_code_)));
  }
  // log wiper status
  for (uint8_t i = 0; i < 8; ++i) {
    // terminals only valid for volatile wipers 0-3 - enable/disable is terminal hw
    // so also invalid for nonvolatile. For these, only print current level.
    // reworked to be a one-line intentionally, as output would not be in order
    if (i < 4) {
      ESP_LOGCONFIG(TAG, "  ├── Volatile wiper [%u] level: %u, Status: %s, HW: %s, A: %s, B: %s, W: %s", i,
                    this->reg_[i].state, ONOFF(this->reg_[i].terminal_hw), ONOFF(this->reg_[i].terminal_a),
                    ONOFF(this->reg_[i].terminal_b), ONOFF(this->reg_[i].terminal_w), ONOFF(this->reg_[i].enabled));
    } else {
      ESP_LOGCONFIG(TAG, "  ├── Nonvolatile wiper [%u] level: %u", i, this->reg_[i].state);
    }
  }
}

void Mcp4461Component::loop() {
  if (this->status_has_warning()) {
    this->get_status_register_();
  }
  for (uint8_t i = 0; i < 8; i++) {
    if (this->reg_[i].update_level) {
      // set wiper i state if changed
      if (this->reg_[i].state != this->read_wiper_level_(i)) {
        this->write_wiper_level_(i, this->reg_[i].state);
      }
    }
    this->reg_[i].update_level = false;
    // can be true only for wipers 0-3
    // setting changes for terminals of nonvolatile wipers
    // is prohibited in public methods
    if (this->reg_[i].update_terminal) {
      // set terminal register changes
      Mcp4461TerminalIdx terminal_connector =
          i < 2 ? Mcp4461TerminalIdx::MCP4461_TERMINAL_0 : Mcp4461TerminalIdx::MCP4461_TERMINAL_1;
      uint8_t new_terminal_value = this->calc_terminal_connector_byte_(terminal_connector);
      ESP_LOGV(TAG, "updating terminal %u to new value %u", static_cast<uint8_t>(terminal_connector),
               new_terminal_value);
      this->set_terminal_register_(terminal_connector, new_terminal_value);
    }
    this->reg_[i].update_terminal = false;
  }
}

uint8_t Mcp4461Component::get_status_register_() {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "%s", LOG_STR_ARG(this->get_message_string(this->error_code_)));
    return 0;
  }
  uint8_t addr = static_cast<uint8_t>(Mcp4461Addresses::MCP4461_STATUS);
  uint8_t reg = addr | static_cast<uint8_t>(Mcp4461Commands::READ);
  uint16_t buf;
  if (!this->read_byte_16(reg, &buf)) {
    this->error_code_ = MCP4461_STATUS_REGISTER_ERROR;
    this->mark_failed();
    return 0;
  }
  uint8_t msb = buf >> 8;
  uint8_t lsb = static_cast<uint8_t>(buf & 0x00ff);
  if (msb != 1 || ((lsb >> 7) & 0x01) != 1 || ((lsb >> 1) & 0x01) != 1) {
    // D8, D7 and R1 bits are hardlocked to 1 -> a status msb bit 0 (bit 9 of status register) of 0 or lsb bit 1/7 = 0
    // indicate device/communication issues, therefore mark component failed
    this->error_code_ = MCP4461_STATUS_REGISTER_INVALID;
    this->mark_failed();
    return 0;
  }
  this->status_clear_warning();
  return lsb;
}

void Mcp4461Component::read_status_register_to_log() {
  uint8_t status_register_value = this->get_status_register_();
  ESP_LOGI(TAG, "D7:  %u, WL3: %u, WL2: %u, EEWA: %u, WL1: %u, WL0: %u, R1: %u, WP: %u",
           ((status_register_value >> 7) & 0x01), ((status_register_value >> 6) & 0x01),
           ((status_register_value >> 5) & 0x01), ((status_register_value >> 4) & 0x01),
           ((status_register_value >> 3) & 0x01), ((status_register_value >> 2) & 0x01),
           ((status_register_value >> 1) & 0x01), ((status_register_value >> 0) & 0x01));
}

uint8_t Mcp4461Component::get_wiper_address_(uint8_t wiper) {
  uint8_t addr;
  bool nonvolatile = false;
  if (wiper > 3) {
    nonvolatile = true;
    wiper = wiper - 4;
  }
  switch (wiper) {
    case 0:
      addr = static_cast<uint8_t>(Mcp4461Addresses::MCP4461_VW0);
      break;
    case 1:
      addr = static_cast<uint8_t>(Mcp4461Addresses::MCP4461_VW1);
      break;
    case 2:
      addr = static_cast<uint8_t>(Mcp4461Addresses::MCP4461_VW2);
      break;
    case 3:
      addr = static_cast<uint8_t>(Mcp4461Addresses::MCP4461_VW3);
      break;
    default:
      ESP_LOGW(TAG, "unknown wiper specified");
      return 0;
  }
  if (nonvolatile) {
    addr = addr + 0x20;
  }
  return addr;
}

uint16_t Mcp4461Component::get_wiper_level_(Mcp4461WiperIdx wiper) {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "%s", LOG_STR_ARG(this->get_message_string(this->error_code_)));
    return 0;
  }
  uint8_t wiper_idx = static_cast<uint8_t>(wiper);
  if (!(this->reg_[wiper_idx].enabled)) {
    ESP_LOGW(TAG, "%s", LOG_STR_ARG(this->get_message_string(MCP4461_WIPER_DISABLED)));
    return 0;
  }
  if (!(this->reg_[wiper_idx].enabled)) {
    ESP_LOGW(TAG, "reading from disabled volatile wiper %u, returning 0", wiper_idx);
    return 0;
  }
  return this->read_wiper_level_(wiper_idx);
}

uint16_t Mcp4461Component::read_wiper_level_(uint8_t wiper_idx) {
  uint8_t addr = this->get_wiper_address_(wiper_idx);
  uint8_t reg = addr | static_cast<uint8_t>(Mcp4461Commands::INCREMENT);
  if (wiper_idx > 3) {
    if (!this->is_eeprom_ready_for_writing_(true)) {
      return 0;
    }
  }
  uint16_t buf = 0;
  if (!(this->read_byte_16(reg, &buf))) {
    this->error_code_ = MCP4461_STATUS_I2C_ERROR;
    this->status_set_warning();
    ESP_LOGW(TAG, "Error fetching %swiper %u value", (wiper_idx > 3) ? "nonvolatile " : "", wiper_idx);
    return 0;
  }
  return buf;
}

bool Mcp4461Component::update_wiper_level_(Mcp4461WiperIdx wiper) {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "%s", LOG_STR_ARG(this->get_message_string(this->error_code_)));
    return false;
  }
  uint8_t wiper_idx = static_cast<uint8_t>(wiper);
  if (!(this->reg_[wiper_idx].enabled)) {
    ESP_LOGW(TAG, "%s", LOG_STR_ARG(this->get_message_string(MCP4461_WIPER_DISABLED)));
    return false;
  }
  uint16_t data = this->get_wiper_level_(wiper);
  ESP_LOGV(TAG, "Got value %u from wiper %u", data, wiper_idx);
  this->reg_[wiper_idx].state = data;
  return true;
}

bool Mcp4461Component::set_wiper_level_(Mcp4461WiperIdx wiper, uint16_t value) {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "%s", LOG_STR_ARG(this->get_message_string(this->error_code_)));
    return false;
  }
  uint8_t wiper_idx = static_cast<uint8_t>(wiper);
  if (value > 0x100) {
    ESP_LOGW(TAG, "%s", LOG_STR_ARG(this->get_message_string(MCP4461_VALUE_INVALID)));
    return false;
  }
  if (!(this->reg_[wiper_idx].enabled)) {
    ESP_LOGW(TAG, "%s", LOG_STR_ARG(this->get_message_string(MCP4461_WIPER_DISABLED)));
    return false;
  }
  if (this->reg_[wiper_idx].wiper_lock_active) {
    ESP_LOGW(TAG, "%s", LOG_STR_ARG(this->get_message_string(MCP4461_WIPER_LOCKED)));
    return false;
  }
  ESP_LOGV(TAG, "Setting MCP4461 wiper %u to %u", wiper_idx, value);
  this->reg_[wiper_idx].state = value;
  this->reg_[wiper_idx].update_level = true;
  return true;
}

void Mcp4461Component::write_wiper_level_(uint8_t wiper, uint16_t value) {
  bool nonvolatile = wiper > 3;
  if (!(this->mcp4461_write_(this->get_wiper_address_(wiper), value, nonvolatile))) {
    this->error_code_ = MCP4461_STATUS_I2C_ERROR;
    this->status_set_warning();
    ESP_LOGW(TAG, "Error writing %swiper %u level %u", (wiper > 3) ? "nonvolatile " : "", wiper, value);
  }
}

void Mcp4461Component::enable_wiper_(Mcp4461WiperIdx wiper) {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "%s", LOG_STR_ARG(this->get_message_string(this->error_code_)));
    return;
  }
  uint8_t wiper_idx = static_cast<uint8_t>(wiper);
  if ((this->reg_[wiper_idx].enabled)) {
    ESP_LOGW(TAG, "%s", LOG_STR_ARG(this->get_message_string(MCP4461_WIPER_ENABLED)));
    return;
  }
  if (this->reg_[wiper_idx].wiper_lock_active) {
    ESP_LOGW(TAG, "%s", LOG_STR_ARG(this->get_message_string(MCP4461_WIPER_LOCKED)));
    return;
  }
  ESP_LOGV(TAG, "Enabling wiper %u", wiper_idx);
  this->reg_[wiper_idx].enabled = true;
  if (wiper_idx < 4) {
    this->reg_[wiper_idx].terminal_hw = true;
    this->reg_[wiper_idx].update_terminal = true;
  }
}

void Mcp4461Component::disable_wiper_(Mcp4461WiperIdx wiper) {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "%s", LOG_STR_ARG(this->get_message_string(this->error_code_)));
    return;
  }
  uint8_t wiper_idx = static_cast<uint8_t>(wiper);
  if (!(this->reg_[wiper_idx].enabled)) {
    ESP_LOGW(TAG, "%s", LOG_STR_ARG(this->get_message_string(MCP4461_WIPER_DISABLED)));
    return;
  }
  if (this->reg_[wiper_idx].wiper_lock_active) {
    ESP_LOGW(TAG, "%s", LOG_STR_ARG(this->get_message_string(MCP4461_WIPER_LOCKED)));
    return;
  }
  ESP_LOGV(TAG, "Disabling wiper %u", wiper_idx);
  this->reg_[wiper_idx].enabled = true;
  if (wiper_idx < 4) {
    this->reg_[wiper_idx].terminal_hw = true;
    this->reg_[wiper_idx].update_terminal = true;
  }
}

bool Mcp4461Component::increase_wiper_(Mcp4461WiperIdx wiper) {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "%s", LOG_STR_ARG(this->get_message_string(this->error_code_)));
    return false;
  }
  uint8_t wiper_idx = static_cast<uint8_t>(wiper);
  if (!(this->reg_[wiper_idx].enabled)) {
    ESP_LOGW(TAG, "%s", LOG_STR_ARG(this->get_message_string(MCP4461_WIPER_DISABLED)));
    return false;
  }
  if (this->reg_[wiper_idx].wiper_lock_active) {
    ESP_LOGW(TAG, "%s", LOG_STR_ARG(this->get_message_string(MCP4461_WIPER_LOCKED)));
    return false;
  }
  if (this->reg_[wiper_idx].state == 256) {
    ESP_LOGV(TAG, "Maximum wiper level reached, further increase of wiper %u prohibited", wiper_idx);
    return false;
  }
  ESP_LOGV(TAG, "Increasing wiper %u", wiper_idx);
  uint8_t addr = this->get_wiper_address_(wiper_idx);
  uint8_t reg = addr | static_cast<uint8_t>(Mcp4461Commands::INCREMENT);
  auto err = this->write(&this->address_, reg, sizeof(reg));
  if (err != i2c::ERROR_OK) {
    this->error_code_ = MCP4461_STATUS_I2C_ERROR;
    this->status_set_warning();
    return false;
  }
  this->reg_[wiper_idx].state++;
  return true;
}

bool Mcp4461Component::decrease_wiper_(Mcp4461WiperIdx wiper) {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "%s", LOG_STR_ARG(this->get_message_string(this->error_code_)));
    return false;
  }
  uint8_t wiper_idx = static_cast<uint8_t>(wiper);
  if (!(this->reg_[wiper_idx].enabled)) {
    ESP_LOGW(TAG, "%s", LOG_STR_ARG(this->get_message_string(MCP4461_WIPER_DISABLED)));
    return false;
  }
  if (this->reg_[wiper_idx].wiper_lock_active) {
    ESP_LOGW(TAG, "%s", LOG_STR_ARG(this->get_message_string(MCP4461_WIPER_LOCKED)));
    return false;
  }
  if (this->reg_[wiper_idx].state == 0) {
    ESP_LOGV(TAG, "Minimum wiper level reached, further decrease of wiper %u prohibited", wiper_idx);
    return false;
  }
  ESP_LOGV(TAG, "Decreasing wiper %u", wiper_idx);
  uint8_t addr = this->get_wiper_address_(wiper_idx);
  uint8_t reg = addr | static_cast<uint8_t>(Mcp4461Commands::DECREMENT);
  auto err = this->write(&this->address_, reg, sizeof(reg));
  if (err != i2c::ERROR_OK) {
    this->error_code_ = MCP4461_STATUS_I2C_ERROR;
    this->status_set_warning();
    return false;
  }
  this->reg_[wiper_idx].state--;
  return true;
}

uint8_t Mcp4461Component::calc_terminal_connector_byte_(Mcp4461TerminalIdx terminal_connector) {
  uint8_t i = static_cast<uint8_t>(terminal_connector) <= 1 ? 0 : 2;
  uint8_t new_value_byte = 0;
  new_value_byte += static_cast<uint8_t>(this->reg_[i].terminal_b);
  new_value_byte += static_cast<uint8_t>(this->reg_[i].terminal_w) << 1;
  new_value_byte += static_cast<uint8_t>(this->reg_[i].terminal_a) << 2;
  new_value_byte += static_cast<uint8_t>(this->reg_[i].terminal_hw) << 3;
  new_value_byte += static_cast<uint8_t>(this->reg_[(i + 1)].terminal_b) << 4;
  new_value_byte += static_cast<uint8_t>(this->reg_[(i + 1)].terminal_w) << 5;
  new_value_byte += static_cast<uint8_t>(this->reg_[(i + 1)].terminal_a) << 6;
  new_value_byte += static_cast<uint8_t>(this->reg_[(i + 1)].terminal_hw) << 7;
  return new_value_byte;
}

uint8_t Mcp4461Component::get_terminal_register_(Mcp4461TerminalIdx terminal_connector) {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "%s", LOG_STR_ARG(this->get_message_string(this->error_code_)));
    return 0;
  }
  uint8_t reg = static_cast<uint8_t>(terminal_connector) == 0 ? static_cast<uint8_t>(Mcp4461Addresses::MCP4461_TCON0)
                                                              : static_cast<uint8_t>(Mcp4461Addresses::MCP4461_TCON1);
  reg |= static_cast<uint8_t>(Mcp4461Commands::READ);
  uint16_t buf;
  if (this->read_byte_16(reg, &buf)) {
    return static_cast<uint8_t>(buf & 0x00ff);
  } else {
    this->error_code_ = MCP4461_STATUS_I2C_ERROR;
    this->status_set_warning();
    ESP_LOGW(TAG, "Error fetching terminal register value");
    return 0;
  }
}

void Mcp4461Component::update_terminal_register_(Mcp4461TerminalIdx terminal_connector) {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "%s", LOG_STR_ARG(this->get_message_string(this->error_code_)));
    return;
  }
  if ((static_cast<uint8_t>(terminal_connector) != 0 && static_cast<uint8_t>(terminal_connector) != 1)) {
    return;
  }
  uint8_t terminal_data = this->get_terminal_register_(terminal_connector);
  if (terminal_data == 0) {
    return;
  }
  ESP_LOGV(TAG, "Got terminal register %u data 0x%02X", static_cast<uint8_t>(terminal_connector), terminal_data);
  uint8_t wiper_index = 0;
  if (static_cast<uint8_t>(terminal_connector) == 1) {
    wiper_index = 2;
  }
  this->reg_[wiper_index].terminal_b = ((terminal_data >> 0) & 0x01);
  this->reg_[wiper_index].terminal_w = ((terminal_data >> 1) & 0x01);
  this->reg_[wiper_index].terminal_a = ((terminal_data >> 2) & 0x01);
  this->reg_[wiper_index].terminal_hw = ((terminal_data >> 3) & 0x01);
  this->reg_[(wiper_index + 1)].terminal_b = ((terminal_data >> 4) & 0x01);
  this->reg_[(wiper_index + 1)].terminal_w = ((terminal_data >> 5) & 0x01);
  this->reg_[(wiper_index + 1)].terminal_a = ((terminal_data >> 6) & 0x01);
  this->reg_[(wiper_index + 1)].terminal_hw = ((terminal_data >> 7) & 0x01);
}

bool Mcp4461Component::set_terminal_register_(Mcp4461TerminalIdx terminal_connector, uint8_t data) {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "%s", LOG_STR_ARG(this->get_message_string(this->error_code_)));
    return false;
  }
  uint8_t addr;
  if (static_cast<uint8_t>(terminal_connector) == 0) {
    addr = static_cast<uint8_t>(Mcp4461Addresses::MCP4461_TCON0);
  } else if (static_cast<uint8_t>(terminal_connector) == 1) {
    addr = static_cast<uint8_t>(Mcp4461Addresses::MCP4461_TCON1);
  } else {
    ESP_LOGW(TAG, "Invalid terminal connector id %u specified", static_cast<uint8_t>(terminal_connector));
    return false;
  }
  if (!(this->mcp4461_write_(addr, data))) {
    this->error_code_ = MCP4461_STATUS_I2C_ERROR;
    this->status_set_warning();
    return false;
  }
  return true;
}

void Mcp4461Component::enable_terminal_(Mcp4461WiperIdx wiper, char terminal) {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "%s", LOG_STR_ARG(this->get_message_string(this->error_code_)));
    return;
  }
  uint8_t wiper_idx = static_cast<uint8_t>(wiper);
  ESP_LOGV(TAG, "Enabling terminal %c of wiper %u", terminal, wiper_idx);
  switch (terminal) {
    case 'h':
      this->reg_[wiper_idx].terminal_hw = true;
      break;
    case 'a':
      this->reg_[wiper_idx].terminal_a = true;
      break;
    case 'b':
      this->reg_[wiper_idx].terminal_b = true;
      break;
    case 'w':
      this->reg_[wiper_idx].terminal_w = true;
      break;
    default:
      ESP_LOGW(TAG, "Unknown terminal %c specified", terminal);
      return;
  }
  this->reg_[wiper_idx].update_terminal = false;
}

void Mcp4461Component::disable_terminal_(Mcp4461WiperIdx wiper, char terminal) {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "%s", LOG_STR_ARG(this->get_message_string(this->error_code_)));
    return;
  }
  uint8_t wiper_idx = static_cast<uint8_t>(wiper);
  ESP_LOGV(TAG, "Disabling terminal %c of wiper %u", terminal, wiper_idx);
  switch (terminal) {
    case 'h':
      this->reg_[wiper_idx].terminal_hw = false;
      break;
    case 'a':
      this->reg_[wiper_idx].terminal_a = false;
      break;
    case 'b':
      this->reg_[wiper_idx].terminal_b = false;
      break;
    case 'w':
      this->reg_[wiper_idx].terminal_w = false;
      break;
    default:
      ESP_LOGW(TAG, "Unknown terminal %c specified", terminal);
      return;
  }
  this->reg_[wiper_idx].update_terminal = false;
}

uint16_t Mcp4461Component::get_eeprom_value(Mcp4461EepromLocation location) {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "%s", LOG_STR_ARG(this->get_message_string(this->error_code_)));
    return 0;
  }
  uint8_t reg = 0;
  reg |= static_cast<uint8_t>(Mcp4461EepromLocation::MCP4461_EEPROM_1) + (static_cast<uint8_t>(location) * 0x10);
  reg |= static_cast<uint8_t>(Mcp4461Commands::READ);
  uint16_t buf;
  if (!this->is_eeprom_ready_for_writing_(true)) {
    return 0;
  }
  if (!this->read_byte_16(reg, &buf)) {
    this->error_code_ = MCP4461_STATUS_I2C_ERROR;
    this->status_set_warning();
    ESP_LOGW(TAG, "Error fetching EEPROM location value");
    return 0;
  }
  return buf;
}

bool Mcp4461Component::set_eeprom_value(Mcp4461EepromLocation location, uint16_t value) {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "%s", LOG_STR_ARG(this->get_message_string(this->error_code_)));
    return false;
  }
  uint8_t addr = 0;
  if (value > 511) {
    return false;
  }
  if (value > 256) {
    addr = 1;
  }
  addr |= static_cast<uint8_t>(Mcp4461EepromLocation::MCP4461_EEPROM_1) + (static_cast<uint8_t>(location) * 0x10);
  if (!(this->mcp4461_write_(addr, value, true))) {
    this->error_code_ = MCP4461_STATUS_I2C_ERROR;
    this->status_set_warning();
    ESP_LOGW(TAG, "Error writing EEPROM value");
    return false;
  }
  return true;
}

bool Mcp4461Component::is_writing_() {
  /* Read the EEPROM write-active status from the status register */
  bool writing = static_cast<bool>((this->get_status_register_() >> 4) & 0x01);

  /* If EEPROM is no longer writing, reset the timeout flag */
  if (!writing) {
    /* This is protected boolean flag in Mcp4461Component class */
    this->last_eeprom_write_timed_out_ = false;
  }

  return writing;
}

bool Mcp4461Component::is_eeprom_ready_for_writing_(bool wait_if_not_ready) {
  /* Check initial write status */
  bool ready_for_write = !this->is_writing_();

  /* Return early if no waiting is required or EEPROM is already ready */
  if (ready_for_write || !wait_if_not_ready || this->last_eeprom_write_timed_out_) {
    return ready_for_write;
  }

  /* Timestamp before starting the loop */
  const uint32_t start_millis = millis();

  ESP_LOGV(TAG, "Waiting until EEPROM is ready for write, start_millis = %" PRIu32, start_millis);

  /* Loop until EEPROM is ready or timeout is reached */
  while (!ready_for_write && ((millis() - start_millis) < EEPROM_WRITE_TIMEOUT_MS)) {
    ready_for_write = !this->is_writing_();

    /* If ready, exit early */
    if (ready_for_write) {
      ESP_LOGV(TAG, "EEPROM is ready for new write, elapsed_millis = %" PRIu32, millis() - start_millis);
      return true;
    }

    /* Not ready yet, yield before checking again */
    yield();
  }

  /* If still not ready after timeout, log error and mark the timeout */
  ESP_LOGE(TAG, "EEPROM write timeout exceeded (%u ms)", EEPROM_WRITE_TIMEOUT_MS);
  this->last_eeprom_write_timed_out_ = true;

  return false;
}

bool Mcp4461Component::mcp4461_write_(uint8_t addr, uint16_t data, bool nonvolatile) {
  uint8_t reg = data > 0xff ? 1 : 0;
  uint8_t value_byte = static_cast<uint8_t>(data & 0x00ff);
  ESP_LOGV(TAG, "Writing value %u to address %u", data, addr);
  reg |= addr;
  reg |= static_cast<uint8_t>(Mcp4461Commands::WRITE);
  if (nonvolatile) {
    if (this->write_protected_) {
      ESP_LOGW(TAG, "%s", LOG_STR_ARG(this->get_message_string(MCP4461_WRITE_PROTECTED)));
      return false;
    }
    if (!this->is_eeprom_ready_for_writing_(true)) {
      return false;
    }
  }
  return this->write_byte(reg, value_byte);
}
}  // namespace mcp4461
}  // namespace esphome
