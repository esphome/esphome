#include "insignia.h"
#include "esphome/core/log.h"

namespace esphome {
namespace insignia {

static const char *const TAG = "insignia.climate";

static const int INSIGNIA_CARRIER_FREQ = 38000;

// Pulse timings
static const int INSIGNIA_INIT_HIGH_US =   4400;
static const int INSIGNIA_INIT_LOW_US =    4400;
static const int INSIGNIA_ONE_LOW_US =     600;
static const int INSIGNIA_ZERO_LOW_US =    1600; 
static const int INSIGNIA_HIGH_US =        550;

static const uint8_t INSIGNIA_PACKET_LENGTH = 6;

// Packet Type - Byte 0
static const uint8_t INSIGNIA_PKT_STATE =       0xA1;
static const uint8_t INSIGNIA_PKT_CMD =         0xA2;
static const uint8_t INSIGNIA_PKT_FM =          0xA4;

// Power Modes - State Byte 1, Bit 7
static const uint8_t INSIGNIA_POWER_ON =        0x80;
static const uint8_t INSIGNIA_POWER_OFF =       0x00;

// Fan Speeds - State Byte 1, Bits 5,4,3
static const uint8_t INSIGNIA_FAN_AUTO =        0x20;
static const uint8_t INSIGNIA_FAN_HIGH =        0x18;
static const uint8_t INSIGNIA_FAN_MED =         0x10;
static const uint8_t INSIGNIA_FAN_MIN =         0x08;

// HVAC Modes - State Byte 1, Bits 2,1,0
static const uint8_t INSIGNIA_COOL =            0x00;
static const uint8_t INSIGNIA_DRY =             0x01;
static const uint8_t INSIGNIA_AUTO =            0x02;
static const uint8_t INSIGNIA_HEAT =            0x03;
static const uint8_t INSIGNIA_FAN =             0x04;

// LED Control - Command Byte 1
static const uint32_t INSIGNIA_LED =            0x08;

// Swing Control - Command Byte 1
static const uint32_t INSIGNIA_SWING_ON =       0x02;
static const uint32_t INSIGNIA_SWING_OFF =      0x01;

void InsigniaClimate::transmit_state() {
  uint8_t remote_state[INSIGNIA_PACKET_LENGTH] = {0};
  uint8_t inverted_remote_state[INSIGNIA_PACKET_LENGTH] = {0};

  if (send_swing_cmd_) {
    // Handle swing command
    send_swing_cmd_ = false;
    remote_state[0] = INSIGNIA_PKT_CMD;
    if (this->swing_mode == climate::CLIMATE_SWING_OFF) {
      remote_state[1] = INSIGNIA_SWING_OFF;
    } else {
      remote_state[1] = INSIGNIA_SWING_ON;
    }
    remote_state[2] = 0xff;
    remote_state[3] = 0xff;
    remote_state[4] = 0xff;
  // Frontend doesn't curently have a way to pass in LED state
  // } else if (send_led_cmd_) {
  //   // Handle LED command
  //   remote_state[0] = INSIGNIA_PKT_CMD;
  //   remote_state[1] = INSIGNIA_LED;
  //   remote_state[2] = 0xff;
  //   remote_state[3] = 0xff;
  //   remote_state[4] = 0xff;
  } else {
    // Handle all other state changes
    remote_state[0] = INSIGNIA_PKT_STATE;

    // HVAC Mode
    switch (this->mode) {
      case climate::CLIMATE_MODE_HEAT:
        remote_state[1] |= INSIGNIA_HEAT;
        break;
      case climate::CLIMATE_MODE_HEAT_COOL:
        remote_state[1] |= INSIGNIA_AUTO;
        break;
      case climate::CLIMATE_MODE_FAN_ONLY:
        remote_state[1] |= INSIGNIA_FAN;
        break;
      case climate::CLIMATE_MODE_DRY:
        remote_state[1] |= INSIGNIA_DRY;
        break;
      default:
      case climate::CLIMATE_MODE_COOL:
        remote_state[1] |= INSIGNIA_COOL;
        break;
    }

    // Fan Mode
    switch (this->fan_mode.value()) {
      case climate::CLIMATE_FAN_HIGH:
        remote_state[1] |= INSIGNIA_FAN_HIGH;
        break;
      case climate::CLIMATE_FAN_MEDIUM:
        remote_state[1] |= INSIGNIA_FAN_MED;
        break;
      case climate::CLIMATE_FAN_LOW:
        remote_state[1] |= INSIGNIA_FAN_MIN;
        break;
      default:
      case climate::CLIMATE_FAN_AUTO:
        remote_state[1] |= INSIGNIA_FAN_AUTO;
        break;
    }

    // Power
    if (this->mode == climate::CLIMATE_MODE_OFF) {
      remote_state[1] |= INSIGNIA_POWER_OFF;
    } else {
      remote_state[1] |= INSIGNIA_POWER_ON;
    }

    // Temperature Set Point

    // The factory remote always sends a set point, even in modes where its not used.
    // ESPHome always presents this number in celcius, regardless of frontend settings.
    // This AC unit only supports farhenheit, so we need to convert it back.
    float target_celcius = clamp<float>(this->target_temperature, INSIGNIA_TEMP_MIN, INSIGNIA_TEMP_MAX);
    uint8_t target_farhenheit = roundf( target_celcius * 1.8 + 32.0 );
    remote_state[2] = 34 + target_farhenheit;

    // Packets 3 and 4 are not used for state packets
    remote_state[3] = 0xff;
    remote_state[4] = 0xff;
  }

  // Calculate checksum
  for (int i = 0; i < 5; i++) {
    remote_state[5] += reverse_bits(remote_state[i]);
  }
  remote_state[5] += 0xff;
  remote_state[5] = reverse_bits(remote_state[5]) ^ 0xff;

  for (uint8_t i = 0; i < INSIGNIA_PACKET_LENGTH; i++) {
    inverted_remote_state[i] = (remote_state[i] ^ 0xff);
  }

  ESP_LOGV(TAG, "Sending insignia code: 0x%2X %2X %2X %2X %2X %2X", remote_state[0],remote_state[1],remote_state[2],remote_state[3],remote_state[4],remote_state[5] );

  this->send_packet(inverted_remote_state, INSIGNIA_PACKET_LENGTH);
  this->send_packet(remote_state, INSIGNIA_PACKET_LENGTH);

}

uint8_t InsigniaClimate::reverse_bits(uint8_t inbyte) {
  uint8_t reversed = 0x00;
  for (uint8_t i = 0; i < 8; i++) {
    if (inbyte & (0x80 >> i)) {
      reversed |= (0x01 << i);
    }
  }
  return reversed;
}

void InsigniaClimate::send_packet(uint8_t const *message, uint8_t length) {
  ESP_LOGV(TAG, "Transmit message length %d", length);

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  data->set_carrier_frequency(INSIGNIA_CARRIER_FREQ);

  // Start signal
  data->mark(INSIGNIA_INIT_HIGH_US);
  data->space(INSIGNIA_INIT_LOW_US);

  // Data
  for (uint8_t msgbyte = 0; msgbyte < length; msgbyte++) {
    for (uint8_t bit = 0; bit < 8; bit++) {
      data->mark(INSIGNIA_HIGH_US);
      if (message[msgbyte] & (0x80 >> bit)) {   // shift bits out left to right
        data->space(INSIGNIA_ONE_LOW_US);
      } else {
        data->space(INSIGNIA_ZERO_LOW_US);
      }
    }
  }
  // End the last bit
  data->mark(INSIGNIA_HIGH_US);

  // Stop signal
  data->space(INSIGNIA_INIT_LOW_US);

  transmit.perform();
}

}  // namespace insignia
}  // namespace esphome
