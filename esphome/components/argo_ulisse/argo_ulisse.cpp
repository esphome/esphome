#include "argo_ulisse.h"

#include <cmath>  // std::isnan

#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/log.h"

namespace esphome {
namespace argo_ulisse {

static const char *const TAG = "argo.climate";

void ArgoUlisseClimate::setup() {
  climate_ir::ClimateIR::setup();

  // Never send nan to HA
  if (std::isnan(this->current_temperature))
    this->current_temperature = 0;

  // add a callback to handle the iFeel feature
  ESP_LOGCONFIG(TAG, "Setting up iFeel callback..");
  this->add_on_state_callback([this](climate::Climate &climate) {
    ESP_LOGV(TAG, "Received state callback for iFeel report..");
    if (this->ifeel_) {
      this->transmit_ifeel_();
    }
  });
}

uint8_t ArgoUlisseClimate::calc_checksum_(const ArgoProtocolWREM3 *data, size_t length) {
  ESP_LOGV(TAG, "Calculating checksum..");
  if (length < 1) {
    ESP_LOGW(TAG, "Nothing to calculate checksum on");
    return 0;  // Changed from -1 to 0 as the return type is uint8_t
  }

  size_t payload_size_bits = (length - 1) * 8;  // Last byte carries checksum

  uint8_t checksum = 0;            // Initialize checksum
  const uint8_t *ptr = data->raw;  // Point to the start of the raw data

  // Calculate checksum for all bytes except the last one
  for (size_t i = 0; i < length - 1; i++) {
    checksum += ptr[i];
  }

  // Add stray bits from last byte to the checksum (if any)
  const uint8_t mask_payload = 0xFF >> (8 - (payload_size_bits % 8));
  checksum += (ptr[length - 1] & mask_payload);

  const uint8_t mask_checksum = 0xFF >> (payload_size_bits % 8);
  return checksum & mask_checksum;
}

void ArgoUlisseClimate::transmit_state() {
  ESP_LOGD(TAG, "Transmitting state..");
  this->last_transmit_time_ = millis();
  ArgoProtocolWREM3 ac_packet;
  memset(&ac_packet, 0, sizeof(ac_packet));

  // Set up the header
  ac_packet.Pre1 = ARGO_PREAMBLE;
  ac_packet.IrChannel = 0;  // Assume channel 0, adjust if needed
  ac_packet.IrCommandType = ARGO_IR_MESSAGE_TYPE_AC_CONTROL;

  // Set default HA clima features
  ESP_LOGV(TAG, "  Setting default HA climate features..");
  ac_packet.Mode = this->operation_mode_();          // Set mode
  ac_packet.RoomTemp = this->sensor_temperature_();  // for iFeel
  ac_packet.Temp = this->temperature_();             // target temperature
  ac_packet.Fan = this->fan_speed_();
  ac_packet.Flap = this->swing_mode_();

  // Set other features (adjust as needed)
  ESP_LOGV(TAG, "  Setting up other features..");
  ac_packet.Power = this->mode != climate::CLIMATE_MODE_OFF;
  ac_packet.iFeel = this->ifeel_;
  ac_packet.Night = this->preset == climate::CLIMATE_PRESET_SLEEP;
  ac_packet.Eco = this->preset == climate::CLIMATE_PRESET_ECO;
  ac_packet.Boost = this->preset == climate::CLIMATE_PRESET_BOOST;
  ac_packet.Filter = this->filter_;
  ac_packet.Light =
      this->light_ && this->preset != climate::CLIMATE_PRESET_SLEEP && this->mode != climate::CLIMATE_MODE_OFF;

  ESP_LOGD(TAG, "  iFeel: %s", ac_packet.iFeel ? "true" : "false");
  ESP_LOGD(TAG, "  Light: %s", ac_packet.Light ? "true" : "false");
  ESP_LOGD(TAG, "  Filter: %s", ac_packet.Filter ? "true" : "false");

  ac_packet.Post1 = ARGO_POST1;

  // Calculate checksum
  ESP_LOGV(TAG, "  Calculating AC checksum..");
  ac_packet.Sum = this->calc_checksum_(&ac_packet, ARGO_IR_MESSAGE_LENGTH_AC_CONTROL);

  // Transmit the IR signal
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(ARGO_IR_FREQUENCY);  // Adjust if the frequency is different

  // Send the header
  data->mark(ARGO_HEADER_MARK);
  data->space(ARGO_HEADER_SPACE);

  // Transmit the data
  for (uint8_t i = 0; i < ARGO_IR_MESSAGE_LENGTH_AC_CONTROL; i++) {
    for (uint8_t mask = 1; mask > 0; mask <<= 1) {
      data->mark(ARGO_BIT_MARK);
      bool bit = ac_packet.raw[i] & mask;
      data->space(bit ? ARGO_ONE_SPACE : ARGO_ZERO_SPACE);
    }
  }
  data->mark(ARGO_BIT_MARK);
  data->space(0);

  transmit.perform();
}

void ArgoUlisseClimate::transmit_ifeel_() {
  // Send current room temperature for the iFeel feature as a silent IR
  // message (no acknowledgement from the device) (WREM3)
  ESP_LOGD(TAG, "Transmitting iFeel report..");
  this->last_transmit_time_ = millis();

  ArgoProtocolWREM3 ifeel_packet;

  // Set up the header
  ifeel_packet.Pre1 = ARGO_PREAMBLE;
  ifeel_packet.IrChannel = 0;  // Assume channel 0, adjust if needed
  ifeel_packet.IrCommandType = ARGO_IR_MESSAGE_TYPE_IFEEL_TEMP_REPORT;

  ifeel_packet.ifeelreport.SensorT = this->sensor_temperature_();

  // Calculate checksum
  ESP_LOGV(TAG, "  Calculating iFeel checksum..");
  ifeel_packet.ifeelreport.CheckHi = this->calc_checksum_(&ifeel_packet, ARGO_IR_MESSAGE_LENGTH_IFEEL_TEMP_REPORT);

  // Transmit the IR signal
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(ARGO_IR_FREQUENCY);  // Adjust if the frequency is different

  // Send the header
  data->mark(ARGO_HEADER_MARK);
  data->space(ARGO_HEADER_SPACE);

  // Transmit the data
  for (uint8_t i = 0; i < ARGO_IR_MESSAGE_LENGTH_IFEEL_TEMP_REPORT; i++) {
    for (uint8_t mask = 1; mask > 0; mask <<= 1) {
      data->mark(ARGO_BIT_MARK);
      bool bit = ifeel_packet.raw[i] & mask;
      data->space(bit ? ARGO_ONE_SPACE : ARGO_ZERO_SPACE);
    }
  }
  data->mark(ARGO_BIT_MARK);
  data->space(0);

  transmit.perform();
}

uint8_t ArgoUlisseClimate::operation_mode_() {
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      return ARGO_MODE_COOL;
    case climate::CLIMATE_MODE_DRY:
      return ARGO_MODE_DRY;
    case climate::CLIMATE_MODE_HEAT:
      return ARGO_MODE_HEAT;
    case climate::CLIMATE_MODE_HEAT_COOL:
      return ARGO_MODE_AUTO;
    case climate::CLIMATE_MODE_FAN_ONLY:
      return ARGO_MODE_FAN;
    case climate::CLIMATE_MODE_OFF:
    default:
      return ARGO_MODE_OFF;
  }
}

uint8_t ArgoUlisseClimate::fan_speed_() {
  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_LOW:
      return ARGO_FAN_SILENT;
    case climate::CLIMATE_FAN_MEDIUM:
      return ARGO_FAN_MEDIUM;
    case climate::CLIMATE_FAN_HIGH:
      return ARGO_FAN_HIGHEST;
    case climate::CLIMATE_FAN_AUTO:
    default:
      return ARGO_FAN_AUTO;
  }
}

uint8_t ArgoUlisseClimate::swing_mode_() {
  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_VERTICAL:
      return ARGO_FLAP_FULL_AUTO;
    case climate::CLIMATE_SWING_HORIZONTAL:
      return ARGO_FLAP_HALF_AUTO;  // hacky way to set half auto vertical swing
    default:
      return ARGO_FLAP_MIDDLE_HIGH;
  }
}

uint8_t ArgoUlisseClimate::temperature_() {
  // Clamp the temperature to the valid range
  float temp_valid =
      clamp<float>(this->target_temperature, ARGO_TEMP_MIN - ARGO_TEMP_OFFSET, ARGO_TEMP_MAX - ARGO_TEMP_OFFSET);
  return process_temperature_(temp_valid);
}

uint8_t ArgoUlisseClimate::sensor_temperature_() {
  float temp_valid = clamp<float>(this->current_temperature, ARGO_SENSOR_TEMP_MIN - ARGO_TEMP_OFFSET,
                                  ARGO_SENSOR_TEMP_MAX - ARGO_TEMP_OFFSET);
  return process_temperature_(temp_valid);
}

uint8_t ArgoUlisseClimate::process_temperature_(float temperature) {
  ESP_LOGV(TAG, "Processing temperature..");
  ESP_LOGV(TAG, "  Input Temperature: %.2f°C", temperature);

  // Sending 0 equals +4, e.g. "If I want 12 degrees, I need to send 8"
  temperature -= ARGO_TEMP_OFFSET;
  ESP_LOGV(TAG, "  Delta Temperature: %.2f°C", temperature);

  // floor the temperature
  uint8_t temp = (uint8_t) round(temperature);
  ESP_LOGV(TAG, "  Processed Temperature: %d°C", temp);
  return temp;
}

climate::ClimateTraits ArgoUlisseClimate::traits() {
  climate::ClimateTraits traits = climate_ir::ClimateIR::traits();
  traits.set_supports_current_temperature(true);
  return traits;
}

void ArgoUlisseClimate::control(const climate::ClimateCall &call) { climate_ir::ClimateIR::control(call); }

}  // namespace argo_ulisse
}  // namespace esphome
