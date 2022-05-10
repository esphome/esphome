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
static const uint8_t INSIGNIA_LED =             0x08;
static const uint8_t INSIGNIA_LED_CSUM =        0x75;

// Swing Control - Command Byte 1
static const uint8_t INSIGNIA_SWING_ON =        0x02;
static const uint8_t INSIGNIA_SWING_OFF =       0x01;

// Follow Me Options - FM Byte 3
static const uint8_t INSIGNIA_FM_ON =           0xff;
static const uint8_t INSIGNIA_FM_OFF =          0x3f;
static const uint8_t INSIGNIA_FM_UPDATE =       0x7f;

void InsigniaClimate::setup() {
#ifdef USE_ESP8266
  ESP_LOGW(TAG, "This component is not reliable on the ESP8266 platform - an ESP32 is highly recommended");
#endif
  // If a sensor has been configured, use it report current temp in the frontend
  if (this->sensor_) {
    this->sensor_->add_on_state_callback([this](float state) {
      this->current_temperature = state;
      // current temperature changed, publish state
      this->publish_state();
    });
    this->current_temperature = this->sensor_->state;
  } else {
    this->current_temperature = NAN;
  }

  // restore set points
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  } else {
    // restore from defaults
    this->mode = climate::CLIMATE_MODE_OFF;
    // initialize target temperature to some value so that it's not NAN
    this->target_temperature =
        roundf(clamp(this->current_temperature, this->minimum_temperature_, this->maximum_temperature_));
    this->fan_mode = climate::CLIMATE_FAN_AUTO;
    this->swing_mode = climate::CLIMATE_SWING_OFF;
    this->preset = climate::CLIMATE_PRESET_NONE;
  }

  // Never send nan to HA
  if (std::isnan(this->target_temperature)) {
    this->target_temperature = 24;
  }

  // Setup Follow Me
  if (this->fm_configured_) {
    // add a callback so that whenever the Follow Me switch state changes 
    // we can update accordingly
    this->fm_switch_->add_on_state_callback([this](bool state) {
      this->fm_enabled_ = state;
      this->fm_state_changed_ = true;
      this->update();
    });
  }

  // Setup LED switch
  if (this->led_configured_) {
    this->led_switch_->add_on_state_callback([this](bool state) {
      this->led_enabled_ = state;
      this->toggle_led();
    });
  }
}

void InsigniaClimate::dump_config() {
  LOG_CLIMATE("", "IR Climate", this);
  ESP_LOGCONFIG(TAG, "  Min. Temperature: %.1f°C", this->minimum_temperature_);
  ESP_LOGCONFIG(TAG, "  Max. Temperature: %.1f°C", this->maximum_temperature_);
  ESP_LOGCONFIG(TAG, "  Supports HEAT: %s", YESNO(this->supports_heat_));
  ESP_LOGCONFIG(TAG, "  Supports COOL: %s", YESNO(this->supports_cool_));
  ESP_LOGCONFIG(TAG, "  Follow Me Configured: %s", YESNO(this->fm_configured_));
  ESP_LOGCONFIG(TAG, "  LED Switch Configured: %s", YESNO(this->led_configured_));
}

// We'll use the update loop to send Follow Me data if it's enabled.
// We need to send regular update packets to the AC unit even if the
// sensor state hasn't changed, or Follow Me mode will time out.
void InsigniaClimate::update() {
  if (this->fm_configured_) {
    // ESP_LOGVV(TAG, "FM Enabled State: %s", YESNO(this->fm_enabled_));
    // ESP_LOGVV(TAG, "FM Switch State: %s", YESNO(this->fm_switch_->state));
    if (this->fm_enabled_ or this->fm_state_changed_) {
      // Transition to any of these modes disables the FM feature
      if (this->mode == climate::CLIMATE_MODE_FAN_ONLY or 
          this->mode == climate::CLIMATE_MODE_DRY or 
          this->mode == climate::CLIMATE_MODE_OFF 
         ) {
        ESP_LOGV(TAG, "Disabling FM due to incompatible HVAC mode");
        this->fm_state_changed_ = false;
        this->fm_enabled_ = false;
        return;
      }

      // Assemble a Follow Me packet
      uint8_t remote_state[INSIGNIA_PACKET_LENGTH] = {0};

      remote_state[0] = INSIGNIA_PKT_FM;
      remote_state[1] = this->compile_hvac_byte();
      remote_state[2] = this->compile_set_point_byte();

      if (this->fm_state_changed_) {
        if (this->fm_enabled_) {
          ESP_LOGD(TAG, "Enabling Follow Me");
          remote_state[3] = INSIGNIA_FM_ON;
        } else {
          ESP_LOGD(TAG, "Disabling Follow Me");
          remote_state[3] = INSIGNIA_FM_OFF;
        }
      } else {
        remote_state[3] = INSIGNIA_FM_UPDATE;
      }
      this->fm_state_changed_ = false;

      // Report sensor data
      float target_celcius = (float)(this->sensor_->get_state());
      uint8_t target_farhenheit = roundf( target_celcius * 1.8 + 32.0 );
      remote_state[4] = target_farhenheit - 31;
      ESP_LOGD(TAG, "Sending FM Update with Temperature: %3u", target_farhenheit);

      remote_state[5] = this->calculate_checksum(remote_state, INSIGNIA_PACKET_LENGTH - 1);
      this->send_transmission(remote_state, INSIGNIA_PACKET_LENGTH);
    }
  }
}

void InsigniaClimate::toggle_led() {
  uint8_t remote_state[INSIGNIA_PACKET_LENGTH] = {
    INSIGNIA_PKT_CMD,
    INSIGNIA_LED,
    0xff,
    0xff,
    0xff,
    INSIGNIA_LED_CSUM
  };
  ESP_LOGD(TAG, "Sending LED Toggle");
  this->send_transmission(remote_state, INSIGNIA_PACKET_LENGTH);
}

uint8_t InsigniaClimate::compile_hvac_byte() {
  uint8_t hvac_byte = 0x00;
  // HVAC Mode
  switch (this->mode) {
    case climate::CLIMATE_MODE_HEAT:
      hvac_byte |= INSIGNIA_HEAT;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
      hvac_byte |= INSIGNIA_AUTO;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      hvac_byte |= INSIGNIA_FAN;
      break;
    case climate::CLIMATE_MODE_DRY:
      hvac_byte |= INSIGNIA_DRY;
      break;
    default:
    case climate::CLIMATE_MODE_COOL:
      hvac_byte |= INSIGNIA_COOL;
      break;
  }

  // Fan Mode
  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_HIGH:
      hvac_byte |= INSIGNIA_FAN_HIGH;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      hvac_byte |= INSIGNIA_FAN_MED;
      break;
    case climate::CLIMATE_FAN_LOW:
      hvac_byte |= INSIGNIA_FAN_MIN;
      break;
    default:
    case climate::CLIMATE_FAN_AUTO:
      hvac_byte |= INSIGNIA_FAN_AUTO;
      break;
  }

  // Power
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    hvac_byte |= INSIGNIA_POWER_OFF;
  } else {
    hvac_byte |= INSIGNIA_POWER_ON;
  }

  return hvac_byte;
}

uint8_t InsigniaClimate::compile_set_point_byte() {
  // The AC expects a valid set point, even in modes where its not used.
  // ESPHome always presents this number in celcius.
  // This AC unit only supports farhenheit, so we need to convert it.
  float target_celcius = clamp<float>(this->target_temperature, INSIGNIA_TEMP_MIN, INSIGNIA_TEMP_MAX);
  uint8_t target_farhenheit = roundf( target_celcius * 1.8 + 32.0 );
  return (34 + target_farhenheit);
}

uint8_t InsigniaClimate::calculate_checksum(uint8_t const *message, uint8_t length) {
  uint8_t csum = 0x00;
  for (int i = 0; i < length; i++) {
    csum += this->reverse_bits(message[i]);
  }
  csum += 0xff;
  csum = this->reverse_bits(csum) ^ 0xff;

  return csum;
}

void InsigniaClimate::transmit_state() {
  uint8_t remote_state[INSIGNIA_PACKET_LENGTH] = {0};
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
  } else {
    // Handle all other state changes
    remote_state[0] = INSIGNIA_PKT_STATE;
    remote_state[1] = this->compile_hvac_byte();

    // Temperature Set Point
    remote_state[2] = compile_set_point_byte();

    // Packets 3 and 4 are not used for state packets
    remote_state[3] = 0xff;
    remote_state[4] = 0xff;
  }

  // Calculate checksum
  remote_state[5] = this->calculate_checksum(remote_state, INSIGNIA_PACKET_LENGTH - 1);

  send_transmission(remote_state, INSIGNIA_PACKET_LENGTH);

  // Reconfigure Follow Me to current mode
  if (this->mode == climate::CLIMATE_MODE_OFF or 
      this->mode == climate::CLIMATE_MODE_DRY or 
      this->mode == climate::CLIMATE_MODE_FAN_ONLY) {
    // The head unit will automatically turn FM off in any of these modes
    this->fm_enabled_ = false;
  } else if (this->fm_switch_->state and not
             this->fm_enabled_) {
    // FM switch is on, and we just switched to a supported mode, so we need to
    // re-send an enable packet
    ESP_LOGV(TAG, "Sending delayed FM Enable packet to sync states");
    this->fm_enabled_ = true;
    this->fm_state_changed_ = true;
    delay(2000);
    this->update();
  }
}

void InsigniaClimate::send_transmission(uint8_t const *message, uint8_t length) {
  // This AC expects the same packet sent twice, one of them in one's compliment
  // of the other
  uint8_t inverted_message[INSIGNIA_PACKET_LENGTH] = {0};
  for (uint8_t i = 0; i < INSIGNIA_PACKET_LENGTH; i++) {
    inverted_message[i] = (message[i] ^ 0xff);
  }

  ESP_LOGV(TAG, "Sending insignia code: 0x%2X %2X %2X %2X %2X %2X", message[0],message[1],message[2],message[3],message[4],message[5] );

  this->send_packet(inverted_message, INSIGNIA_PACKET_LENGTH);
  this->send_packet(message, INSIGNIA_PACKET_LENGTH);
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
