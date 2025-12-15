#ifndef DPS3xx_CONFIG_H_
#define DPS3xx_CONFIG_H_

namespace esphome {
namespace xensiv_dps3xx_base {

static const int DPS3xx_NUM_OF_REGMASKS = 16;

enum Interrupt_source_3xx_e {
  DPS3xx_NO_INTR = 0,
  DPS3xx_PRS_INTR = 1,
  DPS3xx_TEMP_INTR = 2,
  DPS3xx_BOTH_INTR = 3,
  DPS3xx_FIFO_FULL_INTR = 4,
};

namespace dps3xx {

enum Registers_e {
  PROD_ID = 0,
  REV_ID,
  TEMP_SENSOR,     // internal vs external
  TEMP_SENSORREC,  // temperature sensor recommendation
  TEMP_SE,         // temperature shift enable (if temp_osr>3)
  PRS_SE,          // pressure shift enable (if prs_osr>3)
  FIFO_FL,         // FIFO flush
  FIFO_EMPTY,      // FIFO empty
  FIFO_FULL,       // FIFO full
  INT_HL,
  INT_SEL,  // interrupt select
};

const RegMask_t registers[DPS3xx_NUM_OF_REGMASKS] = {
    {0x0D, 0x0F, 0},  // PROD_ID
    {0x0D, 0xF0, 4},  // REV_ID
    {0x07, 0x80, 7},  // TEMP_SENSOR
    {0x28, 0x80, 7},  // TEMP_SENSORREC
    {0x09, 0x08, 3},  // TEMP_SE
    {0x09, 0x04, 2},  // PRS_SE
    {0x0C, 0x80, 7},  // FIFO_FL
    {0x0B, 0x01, 0},  // FIFO_EMPTY
    {0x0B, 0x02, 1},  // FIFO_FULL
    {0x09, 0x80, 7},  // INT_HL
    {0x09, 0x70, 4},  // INT_SEL
};

const RegBlock_t coeffBlock = {0x10, 18};
}  // namespace dps3xx

}  // namespace xensiv_dps3xx_base
}  // namespace esphome

#endif
