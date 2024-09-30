#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif

namespace esphome {
namespace husb238 {

enum class CommandRegister : uint8_t {
  PD_STATUS0 = 0x00,
  PD_STATUS1 = 0x01,
  SRC_PDO_5V = 0x02,
  SRC_PDO_9V = 0x03,
  SRC_PDO_12V = 0x04,
  SRC_PDO_15V = 0x05,
  SRC_PDO_18V = 0x06,
  SRC_PDO_20V = 0x07,
  SRC_PDO = 0x08,
  GO_COMMAND = 0x09,
};
const uint8_t REG_NUM = 10;

enum class SrcVoltage : uint8_t {
  PD_UNATTACHED = 0b000,
  PD_5V = 0b001,
  PD_9V = 0b010,
  PD_12V = 0b011,
  PD_15V = 0b100,
  PD_18V = 0b101,
  PD_20V = 0b110,
};

enum class SrcCurrent : uint8_t {
  I_0_5_A = 0b000,
  I_0_7_A = 0b001,
  I_1_0_A = 0b010,
  I_1_25_A = 0b011,
  I_1_5_A = 0b100,
  I_1_75_A = 0b101,
  I_2_0_A = 0b110,
  I_2_25_A = 0b111,
  I_2_5_A = 0b1000,
  I_2_75_A = 0b1001,
  I_3_0_A = 0b1010,
  I_3_25_A = 0b1011,
  I_3_5_A = 0b1100,
  I_4_0_A = 0b1101,
  I_4_5_A = 0b1110,
  I_5_0_A = 0b1111,
};

enum class SrcCurrent5V : uint8_t {
  I_0_5_A = 0b00,
  I_1_5_A = 0b01,
  I_2_4_A = 0b10,
  I_3_0_A = 0b11,
};

enum class PdResponse : uint8_t {
  NO_RESPONSE = 0b000,
  SUCCESS = 0b001,
  INVALID_COMMAND = 0b011,
  COMMAND_NOT_SUPPORTED = 0b100,
  TRANSACTION_FAIL = 0b101,
};

enum class SrcVoltageSelection : uint8_t {
  NOT_SELECTED = 0b0000,
  SRC_PDO_5V = 0b0001,
  SRC_PDO_9V = 0b0010,
  SRC_PDO_12V = 0b0011,
  SRC_PDO_15V = 0b1000,
  SRC_PDO_18V = 0b1001,
  SRC_PDO_20V = 0b1010,
};

enum class CommandFunction : uint8_t {
  REQUEST_PDO = 0b00001,
  GET_SRC_CAP = 0b00100,
  HARD_RESET = 0b10000,
};

union PdStatus0 {
  struct {
    SrcCurrent current : 4;
    SrcVoltage voltage : 4;
  };
  uint8_t raw;
};

union RegPdStatus1 {
  struct {
    SrcCurrent5V current_5v : 2;
    bool voltage_5v : 1;
    PdResponse response : 3;
    bool attached : 1;
    bool cc_dir : 1;
  };
  uint8_t raw;
};

union RegSrcPdo {
  struct {
    SrcCurrent current : 4;
    uint8_t reserved : 3;
    bool detected : 1;
  };
  uint8_t raw;
};

union RegSrcPdoSelect {
  struct {
    uint8_t reserved : 4;
    SrcVoltageSelection voltage : 4;
  };
  u_int8_t raw;
};

union RegGoCommand {
  struct {
    CommandFunction function : 5;
    uint8_t reserved : 3;
  };
  uint8_t raw;
};

class Husb238Component : public PollingComponent, public i2c::I2CDevice {
 public:
  Husb238Component() = default;

  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  bool command_request_voltage(int volt);
  bool command_request_voltage(const std::string &select_state);
  bool command_request_pdo(SrcVoltageSelection voltage);
  bool command_get_src_cap() { return this->send_command_(CommandFunction::GET_SRC_CAP); };
  bool command_reset() { return this->send_command_(CommandFunction::HARD_RESET); };

  bool is_attached();

#ifdef USE_SENSOR
  SUB_SENSOR(voltage)
  SUB_SENSOR(current)
  SUB_SENSOR(selected_voltage)
#endif

#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(status)
  SUB_TEXT_SENSOR(capabilities)
#endif

#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(attached)
  SUB_BINARY_SENSOR(cc_direction)
#endif

#ifdef USE_SELECT
  SUB_SELECT(voltage)
#endif

 private:
  union {
    struct {
      PdStatus0 pd_status0;
      RegPdStatus1 pd_status1;
      RegSrcPdo src_pdo_5v;
      RegSrcPdo src_pdo_9v;
      RegSrcPdo src_pdo_12v;
      RegSrcPdo src_pdo_15v;
      RegSrcPdo src_pdo_18v;
      RegSrcPdo src_pdo_20v;
      RegSrcPdoSelect src_pdo_sel;
      RegGoCommand go_command;
    };
    uint8_t raw[10];
  } registers_;

  bool read_all_();
  bool read_status_();
  bool send_command_(CommandFunction function);

  // RegSrcPdo get_detected_current_();

  bool select_pdo_voltage_(SrcVoltageSelection voltage);
  std::string get_capabilities_();
};

#ifdef USE_SELECT
class Husb238VoltageSelect : public select::Select, public Parented<Husb238Component> {
 public:
  Husb238VoltageSelect() = default;

 protected:
  void control(const std::string &value) override {
    this->publish_state(value);
    this->parent_->command_request_voltage(state);
  };
};
#endif

}  // namespace husb238
}  // namespace esphome
