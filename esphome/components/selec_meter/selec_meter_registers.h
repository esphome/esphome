#pragma once

namespace esphome::selec_meter {

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

/* EM4M REGISTERS -- offsets are register indices from input register 30000 */
static const uint8_t EM4M_REGISTER_COUNT = 114;  // 114 x 16-bit registers

/* EM4M -- common quantities shared with the EM2M sensor keys */
static const uint16_t EM4M_VOLTAGE = 0x0006;         // Average Voltage LN
static const uint16_t EM4M_CURRENT = 0x0016;         // Average Current
static const uint16_t EM4M_ACTIVE_POWER = 0x002A;    // Total kW
static const uint16_t EM4M_REACTIVE_POWER = 0x002C;  // Total kVAr
static const uint16_t EM4M_APPARENT_POWER = 0x002E;  // Total kVA
static const uint16_t EM4M_POWER_FACTOR = 0x0036;    // Average PF
static const uint16_t EM4M_FREQUENCY = 0x0038;
static const uint16_t EM4M_MAXIMUM_DEMAND_ACTIVE_POWER = 0x0046;
static const uint16_t EM4M_MAXIMUM_DEMAND_REACTIVE_POWER = 0x0048;
static const uint16_t EM4M_MAXIMUM_DEMAND_APPARENT_POWER = 0x004A;
static const uint16_t EM4M_IMPORT_ACTIVE_ENERGY = 0x0058;    // Total kWh - Import
static const uint16_t EM4M_EXPORT_ACTIVE_ENERGY = 0x005A;    // Total kWh - Export
static const uint16_t EM4M_IMPORT_REACTIVE_ENERGY = 0x0068;  // Total kVArh - Import
static const uint16_t EM4M_EXPORT_REACTIVE_ENERGY = 0x006A;  // Total kVArh - Export
static const uint16_t EM4M_AVERAGE_VOLTAGE_LL = 0x000E;      // Average Voltage LL

// Net (import - export) energy totals, split by source. Only meaningful when the
// meter's "Dual Source" setting is Yes; DG registers read 0 on single-source installs.
static const uint16_t EM4M_NET_ACTIVE_ENERGY_MAINS = 0x003A;    // Total Net kWh (MAINS)
static const uint16_t EM4M_NET_REACTIVE_ENERGY_MAINS = 0x003C;  // Total Net kVArh (MAINS)
static const uint16_t EM4M_NET_APPARENT_ENERGY_MAINS = 0x003E;  // Total Net kVAh (MAINS)
static const uint16_t EM4M_NET_ACTIVE_ENERGY_DG = 0x0040;       // Total Net kWh (DG)
static const uint16_t EM4M_NET_REACTIVE_ENERGY_DG = 0x0042;     // Total Net kVArh (DG)
static const uint16_t EM4M_NET_APPARENT_ENERGY_DG = 0x0044;     // Total Net kVAh (DG)

/* EM4M -- per-phase quantities, no EM2M equivalent */
static const uint16_t EM4M_VOLTAGE_L1 = 0x0000;   // Voltage V1N
static const uint16_t EM4M_VOLTAGE_L2 = 0x0002;   // Voltage V2N
static const uint16_t EM4M_VOLTAGE_L3 = 0x0004;   // Voltage V3N
static const uint16_t EM4M_VOLTAGE_L12 = 0x0008;  // Voltage V12
static const uint16_t EM4M_VOLTAGE_L23 = 0x000A;  // Voltage V23
static const uint16_t EM4M_VOLTAGE_L31 = 0x000C;  // Voltage V31
static const uint16_t EM4M_CURRENT_L1 = 0x0010;
static const uint16_t EM4M_CURRENT_L2 = 0x0012;
static const uint16_t EM4M_CURRENT_L3 = 0x0014;
static const uint16_t EM4M_ACTIVE_POWER_L1 = 0x0018;    // kW1
static const uint16_t EM4M_ACTIVE_POWER_L2 = 0x001A;    // kW2
static const uint16_t EM4M_ACTIVE_POWER_L3 = 0x001C;    // kW3
static const uint16_t EM4M_REACTIVE_POWER_L1 = 0x001E;  // kVAr1
static const uint16_t EM4M_REACTIVE_POWER_L2 = 0x0020;  // kVAr2
static const uint16_t EM4M_REACTIVE_POWER_L3 = 0x0022;  // kVAr3
static const uint16_t EM4M_APPARENT_POWER_L1 = 0x0024;  // kVA1
static const uint16_t EM4M_APPARENT_POWER_L2 = 0x0026;  // kVA2
static const uint16_t EM4M_APPARENT_POWER_L3 = 0x0028;  // kVA3
static const uint16_t EM4M_POWER_FACTOR_L1 = 0x0030;
static const uint16_t EM4M_POWER_FACTOR_L2 = 0x0032;
static const uint16_t EM4M_POWER_FACTOR_L3 = 0x0034;
static const uint16_t EM4M_IMPORT_ACTIVE_ENERGY_L1 = 0x004C;    // kWh1 - Import
static const uint16_t EM4M_IMPORT_ACTIVE_ENERGY_L2 = 0x004E;    // kWh2 - Import
static const uint16_t EM4M_IMPORT_ACTIVE_ENERGY_L3 = 0x0050;    // kWh3 - Import
static const uint16_t EM4M_EXPORT_ACTIVE_ENERGY_L1 = 0x0052;    // kWh1 - Export
static const uint16_t EM4M_EXPORT_ACTIVE_ENERGY_L2 = 0x0054;    // kWh2 - Export
static const uint16_t EM4M_EXPORT_ACTIVE_ENERGY_L3 = 0x0056;    // kWh3 - Export
static const uint16_t EM4M_IMPORT_REACTIVE_ENERGY_L1 = 0x005C;  // kVArh1 - Import
static const uint16_t EM4M_IMPORT_REACTIVE_ENERGY_L2 = 0x005E;  // kVArh2 - Import
static const uint16_t EM4M_IMPORT_REACTIVE_ENERGY_L3 = 0x0060;  // kVArh3 - Import
static const uint16_t EM4M_EXPORT_REACTIVE_ENERGY_L1 = 0x0062;  // kVArh1 - Export
static const uint16_t EM4M_EXPORT_REACTIVE_ENERGY_L2 = 0x0064;  // kVArh2 - Export
static const uint16_t EM4M_EXPORT_REACTIVE_ENERGY_L3 = 0x0066;  // kVArh3 - Export
static const uint16_t EM4M_APPARENT_ENERGY_L1 = 0x006C;         // kVAh-1
static const uint16_t EM4M_APPARENT_ENERGY_L2 = 0x006E;         // kVAh-2
static const uint16_t EM4M_APPARENT_ENERGY_L3 = 0x0070;         // kVAh-3

// Far outside the main 0-113 register block; read via a separate Modbus transaction.
static const uint16_t EM4M_SERIAL_NUMBER = 0x02AC;  // Serial No. (Hex), 30684
static const uint16_t EM4M_DG_SENSING = 0x02C6;     // DG Sensing (1: Pass, 0: Fail), 30710

}  // namespace esphome::selec_meter
