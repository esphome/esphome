#pragma once

namespace esphome {
namespace selec_meter {

static const float TWO_DEC_UNIT = 0.01;
static const float ONE_DEC_UNIT = 0.1;
static const float NO_DEC_UNIT = 1;
static const float MULTIPLY_TEN_UNIT = 10;
static const float MULTIPLY_THOUSAND_UNIT = 1000;

/* PHASE STATUS REGISTERS */
static const uint16_t SELEC_TOTAL_ACTIVE_ENERGY = 0x0000;
static const uint16_t SELEC_IMPORT_ACTIVE_ENERGY = 0x0002;
static const uint16_t SELEC_EXPORT_ACTIVE_ENERGY = 0x0004;
static const uint16_t SELEC_TOTAL_REACTIVE_ENERGY = 0x0006;
static const uint16_t SELEC_IMPORT_REACTIVE_ENERGY = 0x0008;
static const uint16_t SELEC_EXPORT_REACTIVE_ENERGY = 0x000A;
static const uint16_t SELEC_APPARENT_ENERGY = 0x000C;
static const uint16_t SELEC_ACTIVE_POWER = 0x000E;
static const uint16_t SELEC_REACTIVE_POWER = 0x0010;
static const uint16_t SELEC_APPARENT_POWER = 0x0012;
static const uint16_t SELEC_VOLTAGE = 0x0014;
static const uint16_t SELEC_CURRENT = 0x0016;
static const uint16_t SELEC_POWER_FACTOR = 0x0018;
static const uint16_t SELEC_FREQUENCY = 0x001A;
static const uint16_t SELEC_MAXIMUM_DEMAND_ACTIVE_POWER = 0x001C;
static const uint16_t SELEC_MAXIMUM_DEMAND_REACTIVE_POWER = 0x001E;
static const uint16_t SELEC_MAXIMUM_DEMAND_APPARENT_POWER = 0x0020;

}  // namespace selec_meter
}  // namespace esphome
