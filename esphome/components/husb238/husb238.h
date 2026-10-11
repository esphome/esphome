#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome::husb238 {

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
constexpr uint8_t REG_NUM = 10;

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
  uint8_t raw;
};

union RegGoCommand {
  struct {
    uint8_t function : 5;
    uint8_t reserved : 3;
  };
  uint8_t raw;
};

static_assert(sizeof(PdStatus0) == 1, "PdStatus0 must be 1 byte");
static_assert(sizeof(RegPdStatus1) == 1, "RegPdStatus1 must be 1 byte");
static_assert(sizeof(RegSrcPdo) == 1, "RegSrcPdo must be 1 byte");
static_assert(sizeof(RegSrcPdoSelect) == 1, "RegSrcPdoSelect must be 1 byte");
static_assert(sizeof(RegGoCommand) == 1, "RegGoCommand must be 1 byte");

class Husb238Component : public PollingComponent, public i2c::I2CDevice {
 public:
  Husb238Component() = default;

  void setup() override;
  void update() override;
  void dump_config() override;

  bool is_attached();

#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(attached)
  SUB_BINARY_SENSOR(cc_direction)
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
    uint8_t raw[REG_NUM];
  } registers_;
  static_assert(sizeof(registers_) == REG_NUM, "registers_ layout must match REG_NUM bytes");

  bool read_all_(bool &is_changed);
};

}  // namespace esphome::husb238
