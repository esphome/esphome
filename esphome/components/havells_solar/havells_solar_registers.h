#pragma once
namespace esphome {
namespace havells_solar {

static const float TWO_DEC_UNIT = 0.01;
static const float ONE_DEC_UNIT = 0.1;
static const float NO_DEC_UNIT = 1;
static const float MULTIPLY_TEN_UNIT = 10;

/* PV Input Message */
static const uint16_t HAVELLS_PV_1_VOLTAGE = 0x0006;
static const uint16_t HAVELLS_PV_1_CURRENT = 0x0007;
static const uint16_t HAVELLS_PV_2_VOLTAGE = 0x0008;
static const uint16_t HAVELLS_PV_2_CURRENT = 0x0009;
static const uint16_t HAVELLS_PV_1_POWER = 0x000A;
static const uint16_t HAVELLS_PV_2_POWER = 0x000B;

/* Output Grid Message */
static const uint16_t HAVELLS_SYSTEM_ACTIVE_POWER = 0x000C;
static const uint16_t HAVELLS_SYSTEM_REACTIVE_POWER = 0x000D;
static const uint16_t HAVELLS_GRID_FREQUENCY = 0x000E;
static const uint16_t HAVELLS_PHASE_1_VOLTAGE = 0x000F;
static const uint16_t HAVELLS_PHASE_1_CURRENT = 0x0010;
static const uint16_t HAVELLS_PHASE_2_VOLTAGE = 0x0011;
static const uint16_t HAVELLS_PHASE_2_CURRENT = 0x0012;
static const uint16_t HAVELLS_PHASE_3_VOLTAGE = 0x0013;
static const uint16_t HAVELLS_PHASE_3_CURRENT = 0x0014;

/* Inverter Generation message */
static const uint16_t HAVELLS_TOTAL_ENERGY_PRODUCTION = 0x0015;
static const uint16_t HAVELLS_TOTAL_GENERATION_TIME = 0x0017;
static const uint16_t HAVELLS_TODAY_PRODUCTION = 0x0019;
static const uint16_t HAVELLS_TODAY_GENERATION_TIME = 0x001A;

/* Inverter inner message */
static const uint16_t HAVELLS_INVERTER_MODULE_TEMP = 0x001B;
static const uint16_t HAVELLS_INVERTER_INNER_TEMP = 0x001C;
static const uint16_t HAVELLS_INVERTER_BUS_VOLTAGE = 0x001D;
static const uint16_t HAVELLS_PV1_VOLTAGE_SAMPLED_BY_SECONDARY_CPU = 0x001E;
static const uint16_t HAVELLS_PV2_VOLTAGE_SAMPLED_BY_SECONDARY_CPU = 0x001F;
static const uint16_t HAVELLS_PV1_INSULATION_OF_P_TO_GROUND = 0x0024;
static const uint16_t HAVELLS_PV2_INSULATION_OF_P_TO_GROUND = 0x0025;
static const uint16_t HAVELLS_INSULATION_OF_PV_N_TO_GROUND = 0x0026;
static const uint16_t HAVELLS_GFCI_VALUE = 0x002A;
static const uint16_t HAVELLS_DCI_OF_R = 0x002B;
static const uint16_t HAVELLS_DCI_OF_S = 0x002C;
static const uint16_t HAVELLS_DCI_OF_T = 0x002D;
}  // namespace havells_solar
}  // namespace esphome
