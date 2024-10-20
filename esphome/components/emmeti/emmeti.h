#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace emmeti {

const uint8_t EMMETI_TEMP_MIN = 16;  // Celsius
const uint8_t EMMETI_TEMP_MAX = 30;  // Celsius

// Modes

enum EmmetiMode : uint8_t {
  EMMETI_MODE_HEAT_COOL = 0x00,
  EMMETI_MODE_COOL = 0x01,
  EMMETI_MODE_DRY = 0x02,
  EMMETI_MODE_FAN = 0x03,
  EMMETI_MODE_HEAT = 0x04,
};

// Fan Speed

enum EmmetiFanMode : uint8_t {
  EMMETI_FAN_AUTO = 0x00,
  EMMETI_FAN_1 = 0x01,
  EMMETI_FAN_2 = 0x02,
  EMMETI_FAN_3 = 0x03,
};

// Fan Position

enum EmmetiBlades : uint8_t {
  EMMETI_BLADES_STOP = 0x00,
  EMMETI_BLADES_FULL = 0x01,
  EMMETI_BLADES_1 = 0x02,
  EMMETI_BLADES_2 = 0x03,
  EMMETI_BLADES_3 = 0x04,
  EMMETI_BLADES_4 = 0x05,
  EMMETI_BLADES_5 = 0x06,
  EMMETI_BLADES_LOW = 0x07,
  EMMETI_BLADES_MID = 0x09,
  EMMETI_BLADES_HIGH = 0x11,
};

// IR Transmission
const uint32_t EMMETI_IR_FREQUENCY = 38000;
const uint32_t EMMETI_HEADER_MARK = 9076;
const uint32_t EMMETI_HEADER_SPACE = 4408;
const uint32_t EMMETI_BIT_MARK = 660;
const uint32_t EMMETI_ONE_SPACE = 1630;
const uint32_t EMMETI_ZERO_SPACE = 530;
const uint32_t EMMETI_MESSAGE_SPACE = 20000;

struct EmmetiState {
  uint8_t mode = 0;
  uint8_t bitmap = 0;
  uint8_t fan_speed = 0;
  uint8_t temp = 0;
  uint8_t fan_pos = 0;
  uint8_t th = 0;
  uint8_t checksum = 0;
};

class EmmetiClimate : public climate_ir::ClimateIR {
 public:
  EmmetiClimate()
      : climate_ir::ClimateIR(EMMETI_TEMP_MIN, EMMETI_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL}) {}

 protected:
  // Transmit via IR the state of this climate controller
  void transmit_state() override;
  // Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;
  bool parse_state_frame_(EmmetiState curr_state);

  // setters
  uint8_t set_mode_();
  uint8_t set_temp_();
  uint8_t set_fan_speed_();
  uint8_t gen_checksum_();
  uint8_t set_blades_();

  // getters
  climate::ClimateMode get_mode_(uint8_t mode);
  climate::ClimateFanMode get_fan_speed_(uint8_t fan);
  void get_blades_(uint8_t fanpos);
  // get swing
  climate::ClimateSwingMode get_swing_(uint8_t bitmap);
  float get_temp_(uint8_t temp);

  // check if the received frame is valid
  bool check_checksum_(uint8_t checksum);

  template<typename T> T reverse_(T val, size_t len);

  template<typename T> void add_(T val, size_t len, esphome::remote_base::RemoteTransmitData *ata);

  template<typename T> void add_(T val, esphome::remote_base::RemoteTransmitData *data);

  template<typename T> void reverse_add_(T val, size_t len, esphome::remote_base::RemoteTransmitData *data);

  uint8_t blades_ = EMMETI_BLADES_STOP;
};

}  // namespace emmeti
}  // namespace esphome
