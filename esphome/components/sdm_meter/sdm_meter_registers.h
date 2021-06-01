#pragma once

namespace esphome {
namespace sdm_meter {

/* PHASE STATUS REGISTERS */
static const uint16_t SDM_PHASE_1_VOLTAGE = 0x0000;
static const uint16_t SDM_PHASE_2_VOLTAGE = 0x0002;
static const uint16_t SDM_PHASE_3_VOLTAGE = 0x0004;
static const uint16_t SDM_PHASE_1_CURRENT = 0x0006;
static const uint16_t SDM_PHASE_2_CURRENT = 0x0008;
static const uint16_t SDM_PHASE_3_CURRENT = 0x000A;
static const uint16_t SDM_PHASE_1_ACTIVE_POWER = 0x000C;
static const uint16_t SDM_PHASE_2_ACTIVE_POWER = 0x000E;
static const uint16_t SDM_PHASE_3_ACTIVE_POWER = 0x0010;
static const uint16_t SDM_PHASE_1_APPARENT_POWER = 0x0012;
static const uint16_t SDM_PHASE_2_APPARENT_POWER = 0x0014;
static const uint16_t SDM_PHASE_3_APPARENT_POWER = 0x0016;
static const uint16_t SDM_PHASE_1_REACTIVE_POWER = 0x0018;
static const uint16_t SDM_PHASE_2_REACTIVE_POWER = 0x001A;
static const uint16_t SDM_PHASE_3_REACTIVE_POWER = 0x001C;
static const uint16_t SDM_PHASE_1_POWER_FACTOR = 0x001E;
static const uint16_t SDM_PHASE_2_POWER_FACTOR = 0x0020;
static const uint16_t SDM_PHASE_3_POWER_FACTOR = 0x0022;
static const uint16_t SDM_PHASE_1_ANGLE = 0x0024;
static const uint16_t SDM_PHASE_2_ANGLE = 0x0026;
static const uint16_t SDM_PHASE_3_ANGLE = 0x0028;

static const uint16_t SDM_AVERAGE_L_TO_N_VOLTS = 0x002A;
static const uint16_t SDM_AVERAGE_LINE_CURRENT = 0x002E;
static const uint16_t SDM_SUM_LINE_CURRENT = 0x0030;
static const uint16_t SDM_TOTAL_SYSTEM_POWER = 0x0034;
static const uint16_t SDM_TOTAL_SYSTEM_APPARENT_POWER = 0x0038;
static const uint16_t SDM_TOTAL_SYSTEM_REACTIVE_POWER = 0x003C;
static const uint16_t SDM_TOTAL_SYSTEM_POWER_FACTOR = 0x003E;
static const uint16_t SDM_TOTAL_SYSTEM_PHASE_ANGLE = 0x0042;

static const uint16_t SDM_FREQUENCY = 0x0046;

static const uint16_t SDM_IMPORT_ACTIVE_ENERGY = 0x0048;
static const uint16_t SDM_EXPORT_ACTIVE_ENERGY = 0x004A;
static const uint16_t SDM_IMPORT_REACTIVE_ENERGY = 0x004C;
static const uint16_t SDM_EXPORT_REACTIVE_ENERGY = 0x004E;

static const uint16_t SDM_VAH_SINCE_LAST_RESET = 0x0050;
static const uint16_t SDM_AH_SINCE_LAST_RESET = 0x0052;
static const uint16_t SDM_TOTAL_SYSTEM_POWER_DEMAND = 0x0054;
static const uint16_t SDM_MAXIMUM_TOTAL_SYSTEM_POWER_DEMAND = 0x0056;
static const uint16_t SDM_CURRENT_SYSTEM_POSITIVE_POWER_DEMAND = 0x0058;
static const uint16_t SDM_MAXIMUM_SYSTEM_POSITIVE_POWER_DEMAND = 0x005A;
static const uint16_t SDM_CURRENT_SYSTEM_REVERSE_POWER_DEMAND = 0x005C;
static const uint16_t SDM_MAXIMUM_SYSTEM_REVERSE_POWER_DEMAND = 0x005E;
static const uint16_t SDM_TOTAL_SYSTEM_VA_DEMAND = 0x0064;
static const uint16_t SDM_MAXIMUM_TOTAL_SYSTEM_VA_DEMAND = 0x0066;
static const uint16_t SDM_NEUTRAL_CURRENT_DEMAND = 0x0068;
static const uint16_t SDM_MAXIMUM_NEUTRAL_CURRENT = 0x006A;
static const uint16_t SDM_LINE_1_TO_LINE_2_VOLTS = 0x00C8;
static const uint16_t SDM_LINE_2_TO_LINE_3_VOLTS = 0x00CA;
static const uint16_t SDM_LINE_3_TO_LINE_1_VOLTS = 0x00CC;
static const uint16_t SDM_AVERAGE_LINE_TO_LINE_VOLTS = 0x00CE;
static const uint16_t SDM_NEUTRAL_CURRENT = 0x00E0;

static const uint16_t SDM_PHASE_1_LN_VOLTS_THD = 0x00EA;
static const uint16_t SDM_PHASE_2_LN_VOLTS_THD = 0x00EC;
static const uint16_t SDM_PHASE_3_LN_VOLTS_THD = 0x00EE;
static const uint16_t SDM_PHASE_1_CURRENT_THD = 0x00F0;
static const uint16_t SDM_PHASE_2_CURRENT_THD = 0x00F2;
static const uint16_t SDM_PHASE_3_CURRENT_THD = 0x00F4;

static const uint16_t SDM_AVERAGE_LINE_TO_NEUTRAL_VOLTS_THD = 0x00F8;
static const uint16_t SDM_AVERAGE_LINE_CURRENT_THD = 0x00FA;
static const uint16_t SDM_TOTAL_SYSTEM_POWER_FACTOR_INV = 0x00FE;
static const uint16_t SDM_PHASE_1_CURRENT_DEMAND = 0x0102;
static const uint16_t SDM_PHASE_2_CURRENT_DEMAND = 0x0104;
static const uint16_t SDM_PHASE_3_CURRENT_DEMAND = 0x0106;
static const uint16_t SDM_MAXIMUM_PHASE_1_CURRENT_DEMAND = 0x0108;
static const uint16_t SDM_MAXIMUM_PHASE_2_CURRENT_DEMAND = 0x010A;
static const uint16_t SDM_MAXIMUM_PHASE_3_CURRENT_DEMAND = 0x010C;
static const uint16_t SDM_LINE_1_TO_LINE_2_VOLTS_THD = 0x014E;
static const uint16_t SDM_LINE_2_TO_LINE_3_VOLTS_THD = 0x0150;
static const uint16_t SDM_LINE_3_TO_LINE_1_VOLTS_THD = 0x0152;
static const uint16_t SDM_AVERAGE_LINE_TO_LINE_VOLTS_THD = 0x0154;

static const uint16_t SDM_TOTAL_ACTIVE_ENERGY = 0x0156;
static const uint16_t SDM_TOTAL_REACTIVE_ENERGY = 0x0158;

static const uint16_t SDM_L1_IMPORT_ACTIVE_ENERGY = 0x015A;
static const uint16_t SDM_L2_IMPORT_ACTIVE_ENERGY = 0x015C;
static const uint16_t SDM_L3_IMPORT_ACTIVE_ENERGY = 0x015E;
static const uint16_t SDM_L1_EXPORT_ACTIVE_ENERGY = 0x0160;
static const uint16_t SDM_L2_EXPORT_ACTIVE_ENERGY = 0x0162;
static const uint16_t SDM_L3_EXPORT_ACTIVE_ENERGY = 0x0164;
static const uint16_t SDM_L1_TOTAL_ACTIVE_ENERGY = 0x0166;
static const uint16_t SDM_L2_TOTAL_ACTIVE_ENERGY = 0x0168;
static const uint16_t SDM_L3_TOTAL_ACTIVE_ENERGY = 0x016a;
static const uint16_t SDM_L1_IMPORT_REACTIVE_ENERGY = 0x016C;
static const uint16_t SDM_L2_IMPORT_REACTIVE_ENERGY = 0x016E;
static const uint16_t SDM_L3_IMPORT_REACTIVE_ENERGY = 0x0170;
static const uint16_t SDM_L1_EXPORT_REACTIVE_ENERGY = 0x0172;
static const uint16_t SDM_L2_EXPORT_REACTIVE_ENERGY = 0x0174;
static const uint16_t SDM_L3_EXPORT_REACTIVE_ENERGY = 0x0176;
static const uint16_t SDM_L1_TOTAL_REACTIVE_ENERGY = 0x0178;
static const uint16_t SDM_L2_TOTAL_REACTIVE_ENERGY = 0x017A;
static const uint16_t SDM_L3_TOTAL_REACTIVE_ENERGY = 0x017C;

static const uint16_t SDM_CURRENT_RESETTABLE_TOTAL_ACTIVE_ENERGY = 0x0180;
static const uint16_t SDM_CURRENT_RESETTABLE_TOTAL_REACTIVE_ENERGY = 0x0182;
static const uint16_t SDM_CURRENT_RESETTABLE_IMPORT_ENERGY = 0x0184;
static const uint16_t SDM_CURRENT_RESETTABLE_EXPORT_ENERGY = 0x0186;
static const uint16_t SDM_IMPORT_POWER = 0x0500;
static const uint16_t SDM_EXPORT_POWER = 0x0502;

}  // namespace sdm_meter
}  // namespace esphome
