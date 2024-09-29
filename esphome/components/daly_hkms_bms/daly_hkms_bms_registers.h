#pragma once
namespace esphome {
namespace daly_hkms_bms {

// 0x00 - 0x2F
static const uint16_t DALY_MODBUS_ADDR_CELL_VOLT_1 = 0x00;

// 0x30 - 0x37
static const uint16_t DALY_MODBUS_ADDR_CELL_TEMP_1 = 0x30;

static const uint16_t DALY_MODBUS_ADDR_VOLT = 0x38;
static const uint16_t DALY_MODBUS_ADDR_CURR = 0x39;
static const uint16_t DALY_MODBUS_ADDR_SOC = 0x3A;
static const uint16_t DALY_MODBUS_ADDR_SOH = 0x3B;

static const uint16_t DALY_MODBUS_ADDR_CELL_COUNT = 0x3C;
static const uint16_t DALY_MODBUS_ADDR_CELL_TEMP_COUNT = 0x3D;
static const uint16_t DALY_MODBUS_ADDR_CELL_VOLT_MAX = 0x3E;
static const uint16_t DALY_MODBUS_ADDR_CELL_VOLT_MAX_NUM = 0x3F;
static const uint16_t DALY_MODBUS_ADDR_CELL_VOLT_MIN = 0x40;
static const uint16_t DALY_MODBUS_ADDR_CELL_VOLT_MIN_NUM = 0x41;
static const uint16_t DALY_MODBUS_ADDR_CELL_VOLT_DIFF = 0x42;
static const uint16_t DALY_MODBUS_ADDR_CELL_TEMP_MAX = 0x43;
static const uint16_t DALY_MODBUS_ADDR_CELL_TEMP_MAX_NUM = 0x44;
static const uint16_t DALY_MODBUS_ADDR_CELL_TEMP_MIN = 0x45;
static const uint16_t DALY_MODBUS_ADDR_CELL_TEMP_MIN_NUM = 0x46;
static const uint16_t DALY_MODBUS_ADDR_CELL_TEMP_DIFF = 0x47;

static const uint16_t DALY_MODBUS_ADDR_CHG_DSCHG_STATUS = 0x48;
static const uint16_t DALY_MODBUS_ADDR_REMAINING_CAPACITY = 0x4B;
static const uint16_t DALY_MODBUS_ADDR_CYCLES = 0x4C;
static const uint16_t DALY_MODBUS_ADDR_BALANCE_STATUS = 0x4D;
static const uint16_t DALY_MODBUS_ADDR_BALANCE_STATUS_PER_CELL_1_TO_16 = 0x4F;
static const uint16_t DALY_MODBUS_ADDR_BALANCE_STATUS_PER_CELL_17_TO_32 = 0x50;
static const uint16_t DALY_MODBUS_ADDR_BALANCE_STATUS_PER_CELL_33_TO_48 = 0x51;

static const uint16_t DALY_MODBUS_ADDR_CHG_MOS_ACTIVE = 0x52;
static const uint16_t DALY_MODBUS_ADDR_DSCHG_MOS_ACTIVE = 0x53;
static const uint16_t DALY_MODBUS_ADDR_PRECHG_MOS_ACTIVE = 0x54;
static const uint16_t DALY_MODBUS_ADDR_HEATING_MOS_ACTIVE = 0x55;
static const uint16_t DALY_MODBUS_ADDR_FAN_MOS_ACTIVE = 0x56;

static const uint16_t DALY_MODBUS_ADDR_POWER = 0x58;
static const uint16_t DALY_MODBUS_ADDR_ENERGY = 0x59;

static const uint16_t DALY_MODBUS_ADDR_MOS_TEMP = 0x5A;
static const uint16_t DALY_MODBUS_ADDR_BOARD_TEMP = 0x5B;
static const uint16_t DALY_MODBUS_ADDR_HEATING_TEMP = 0x5C;

static const uint16_t DALY_MODBUS_REGISTER_COUNT = 0x5D;

}  // namespace daly_hkms_bms
}  // namespace esphome
