#include "mcp4728.h"
#include "esphome/core/log.h"
namespace esphome {
namespace mcp4728 {

const uint8_t MCP4728_MULTI_IR_CMD = 0x40;
const uint8_t MCP4728_MULTI_EEPROM_CMD = 0x50;
const uint8_t MCP4728_FAST_WRITE_CMD = 0xC0;

static const char *const TAG = "mcp4728.output";

void MCP4728OutputComponent::setup() { ESP_LOGCONFIG(TAG, "Setting up MCP4728 (0x%02X)...", this->address_); }

MCP4728Channel *MCP4728OutputComponent::create_channel(uint8_t channel) {
  auto *c = new MCP4728Channel(this, channel);
  return c;
}

bool MCP4728OutputComponent::set_channel_value_(MCP4728ChannelID channel, uint16_t new_value,
                                                MCP4728VRef new_vref = MCP4728_VREF_VDD,
                                                MCP4728Gain new_gain = MCP4728_GAIN_1X,
                                                MCP4728PDMode new_pd_mode = MCP4728_PD_MODE_NORMAL,
                                                bool udac = false) {
  uint8_t output_buffer[3];

  // build the setter header/ "address"
  // 0 1 0 0 0 DAC1 DAC0 UDAC[A]
  uint8_t sequential_write_cmd = MCP4728_MULTI_IR_CMD;
  sequential_write_cmd |= (channel << 1);
  sequential_write_cmd |= udac;

  output_buffer[0] = sequential_write_cmd;
  // VREF PD1 PD0 Gx D11 D10 D9 D8 [A] D7 D6 D5 D4 D3 D2 D1 D0 [A]
  new_value |= (new_vref << 15);
  new_value |= (new_pd_mode << 13);
  new_value |= (new_gain << 12);

  output_buffer[1] = new_value >> 8;
  output_buffer[2] = new_value & 0xFF;

  if (!this->write_bytes_raw(output_buffer, 3)) {
    return false;
  }
  return true;
}

void MCP4728Channel::write_state(float state) {
  MCP4728ChannelID channel_enum;
  uint16_t value = (uint16_t) round(state * (pow(2, 12) - 1));
  switch (this->channel_) {
    case 0:
      channel_enum = MCP4728_CHANNEL_A;
      break;
    case 1:
      channel_enum = MCP4728_CHANNEL_B;
      break;
    case 2:
      channel_enum = MCP4728_CHANNEL_C;
      break;
    case 3:
      channel_enum = MCP4728_CHANNEL_D;
      break;
    default:
      break;
  }
  this->parent_->set_channel_value_(channel_enum, value);
}

}  // namespace mcp4728
}  // namespace esphome
