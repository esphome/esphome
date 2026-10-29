#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"

#include <list>

namespace esphome::bq25820 {

// Register addresses, in datasheet Table 7-7 order.
// 16-bit registers: low byte at the listed address, high byte at address + 1.
static const uint8_t BQ25820_REG_CHARGE_VOLTAGE_LIMIT = 0x00;
static const uint8_t BQ25820_REG_CHARGE_CURRENT_LIMIT = 0x02;
static const uint8_t BQ25820_REG_INPUT_CURRENT_LIMIT = 0x06;
static const uint8_t BQ25820_REG_INPUT_VOLTAGE_LIMIT = 0x08;
static const uint8_t BQ25820_REG_REVERSE_MODE_CURRENT_LIMIT = 0x0A;
static const uint8_t BQ25820_REG_REVERSE_MODE_VOLTAGE_LIMIT = 0x0C;
static const uint8_t BQ25820_REG_PRECHARGE_CURRENT_LIMIT = 0x10;
static const uint8_t BQ25820_REG_TERMINATION_CURRENT_LIMIT = 0x12;
static const uint8_t BQ25820_REG_PRECHARGE_TERMINATION_CONTROL = 0x14;
static const uint8_t BQ25820_REG_TIMER_CONTROL = 0x15;
static const uint8_t BQ25820_REG_THREE_STAGE_CHARGE_CONTROL = 0x16;  // all reserved
static const uint8_t BQ25820_REG_CHARGER_CONTROL = 0x17;
static const uint8_t BQ25820_REG_PIN_CONTROL = 0x18;
static const uint8_t BQ25820_REG_POWER_PATH_AND_REVERSE_MODE_CONTROL = 0x19;
static const uint8_t BQ25820_REG_MPPT_CONTROL = 0x1A;
static const uint8_t BQ25820_REG_TS_CHARGING_THRESHOLD_CONTROL = 0x1B;
static const uint8_t BQ25820_REG_TS_CHARGING_REGION_BEHAVIOR_CONTROL = 0x1C;
static const uint8_t BQ25820_REG_TS_REVERSE_MODE_THRESHOLD_CONTROL = 0x1D;
static const uint8_t BQ25820_REG_REVERSE_UNDERVOLTAGE_CONTROL = 0x1E;
static const uint8_t BQ25820_REG_VAC_MPP = 0x1F;
static const uint8_t BQ25820_REG_CHARGER_STATUS_1 = 0x21;
static const uint8_t BQ25820_REG_CHARGER_STATUS_2 = 0x22;
static const uint8_t BQ25820_REG_CHARGER_STATUS_3 = 0x23;
static const uint8_t BQ25820_REG_FAULT_STATUS = 0x24;
static const uint8_t BQ25820_REG_CHARGER_FLAG_1 = 0x25;
static const uint8_t BQ25820_REG_CHARGER_FLAG_2 = 0x26;
static const uint8_t BQ25820_REG_FAULT_FLAG = 0x27;
static const uint8_t BQ25820_REG_CHARGER_MASK_1 = 0x28;
static const uint8_t BQ25820_REG_CHARGER_MASK_2 = 0x29;
static const uint8_t BQ25820_REG_FAULT_MASK = 0x2A;
static const uint8_t BQ25820_REG_ADC_CONTROL = 0x2B;
static const uint8_t BQ25820_REG_ADC_CHANNEL_CONTROL = 0x2C;
static const uint8_t BQ25820_REG_IAC_ADC = 0x2D;
static const uint8_t BQ25820_REG_IBAT_ADC = 0x2F;
static const uint8_t BQ25820_REG_VAC_ADC = 0x31;
static const uint8_t BQ25820_REG_VBAT_ADC = 0x33;
static const uint8_t BQ25820_REG_VSYS_ADC = 0x35;
static const uint8_t BQ25820_REG_TS_ADC = 0x37;
static const uint8_t BQ25820_REG_VFB_ADC = 0x39;
static const uint8_t BQ25820_REG_GATE_DRIVER_STRENGTH_CONTROL = 0x3B;
static const uint8_t BQ25820_REG_GATE_DRIVER_DEAD_TIME_CONTROL = 0x3C;
static const uint8_t BQ25820_REG_PART_INFORMATION = 0x3D;
static const uint8_t BQ25820_REG_REVERSE_MODE_BATTERY_DISCHARGE_CURRENT = 0x62;

// ADC resolution, from datasheet Tables 7-41 to 7-47.
// 16-bit registers are little-endian: low byte at the lower address.
static const uint16_t VBAT_ADC_STEP_MV = 2;  // VBAT_ADC: 2 mV per LSB
static const uint16_t VSYS_ADC_STEP_MV = 2;  // VSYS_ADC: 2 mV per LSB
static const uint16_t VAC_ADC_STEP_MV = 2;   // VAC_ADC: 2 mV per LSB
static const uint16_t VFB_ADC_STEP_MV = 1;   // VFB_ADC: 1 mV per LSB
// TS pin NTC network: 10k NTC from TS to PGND, 10k reference resistor.
static const uint16_t NTC_R0_OHM = 10000;
static const uint16_t NTC_B_CO = 3435;
static const uint16_t NTC_REF_OHM = 10000;
// Current ADC steps are set at runtime from the sense resistor values
// (datasheet reference: 5 mOhm RBAT_SNS, 2 mOhm RAC_SNS).

// Config register field scaling, from datasheet Tables 7-9 to 7-16.
static const uint16_t VFB_REG_BASE_MV = 1504;  // FB regulation limit: 1504-1566 mV
static const uint16_t VFB_REG_STEP_MV = 2;
static const uint16_t ICHG_REG_STEP_MA = 50;  // Fast charge current: 400-20000 mA
static const uint16_t IAC_DPM_STEP_MA = 125;  // Input current DPM: 1000-50000 mA
static const uint16_t VAC_DPM_STEP_MV = 20;   // Input voltage DPM: 4400-65000 mV
static const uint16_t IAC_REV_STEP_MA = 125;  // Reverse mode input current: 1000-50000 mA
static const uint16_t VSYS_REV_STEP_MV = 20;  // Reverse mode system voltage: 3300-65000 mV
static const uint16_t IPRECHG_STEP_MA = 50;   // Precharge current: 250-10000 mA
static const uint16_t ITERM_STEP_MA = 50;     // Termination current: 250-10000 mA

// Snapshot of converted readings and status, passed to listeners each update.
struct BQ25820Data {
  uint16_t vbat_mV{0};
  uint16_t vsys_mV{0};
  uint16_t vac_mV{0};
  uint16_t vfb_mV{0};
  uint16_t vac_mpp_mV{0};
  int16_t ibat_mA{0};  // positive = charging
  int16_t iac_mA{0};
  uint16_t ts_percent10{0};      // TS pin ADC reading, 0.1% units
  float ts_temp_C{0};            // TS pin NTC temperature
  uint8_t charge_stat{0};        // CHARGE_STAT[2:0]
  uint8_t battery_temp_stat{0};  // TS_STAT[2:0]
  bool pg{false};
  bool iac_dpm{false};
  bool vac_dpm{false};
  bool watchdog{false};
  bool acfet{false};
  bool batfet{false};
  bool reverse{false};
  bool vac_undervoltage{false};
  bool vac_overvoltage{false};
  bool ibat_overcurrent{false};
  bool vbat_overvoltage{false};
  bool thermal_shutdown{false};
  bool charge_safety_timer{false};
  bool drv_sup_invalid{false};
};

class BQ25820Listener {
 public:
  virtual ~BQ25820Listener() = default;
  virtual void on_data(const BQ25820Data &data) = 0;
};

class BQ25820Component : public PollingComponent, public i2c::I2CDevice {
 public:
  BQ25820Component();

  void setup() override;
  void dump_config() override;
  void update() override;

  // Listener registration
  void add_listener(BQ25820Listener *listener) { this->listeners_.push_back(listener); }

  // Register access
  uint8_t read_register(uint8_t reg);
  uint8_t read_register(uint8_t reg, uint8_t *out_value);
  uint8_t read_register(uint8_t reg, uint8_t *out_value, size_t len);
  void write_register(uint8_t reg, uint8_t value);
  uint16_t read_register_16bit(uint8_t reg);
  void write_register_16bit(uint8_t reg, uint16_t value);

  // CHARGE_STAT[2:0] values (datasheet Table 7-29)
  static const uint8_t CHARGE_STAT_NOT_CHARGING = 0x0;
  static const uint8_t CHARGE_STAT_TRICKLE = 0x1;
  static const uint8_t CHARGE_STAT_PRECHARGE = 0x2;
  static const uint8_t CHARGE_STAT_FAST = 0x3;
  static const uint8_t CHARGE_STAT_TAPER = 0x4;
  static const uint8_t CHARGE_STAT_TOP_OFF = 0x6;
  static const uint8_t CHARGE_STAT_DONE = 0x7;

  // Methods in sequence by register, e.g. in the same order as shown in the datasheet.

  // Charge Voltage Limit (in mV)
  // Address: 0x0
  // Bit 4:0
  // Reset: 0x10 (reset by: REG_RESET)
  // POR: 1536mV (10h)
  // Range: 1504mV-1566mV (0h to 1Fh)
  // Bit step: 2mV
  // Offset: 1504mV
  uint16_t charge_voltage_limit_mV();

  // Charge Current Limit (in mA)
  // Address: 0x2
  // Bit 10:2
  // Reset: 0x190 (reset by: REG_RESET, WATCHDOG)
  // POR: 20000mA (190h)
  // Range: 400mA-20000mA (8h-190h)
  // Clamped Low
  // Clamped High
  // Bit Step: 50mA
  uint16_t charge_current_limit_mA();

  // Input Current DPM Limit (in mA)
  // Address: 0x6
  // Bit: 10:2
  // Reset: 0x190 (reset by: REG_RESET)
  // POR: 50000mA (190h)
  // Range: 1000mA-50000mA (8h-190h)
  // Clamped Low
  // Clamped High
  // Bit Step: 125mA
  uint16_t input_current_regulation_limit_mA();

  // Input Voltage DPM Limit (in mV)
  // Address: 0x8
  // Bit 13:2
  // Reset: 0xD2 (reset by: REG_RESET)
  // POR: 4400mV (DCh)
  // Range: 4400mV-65000mV (DCh-CB2h)
  // Clamped Low
  // Clamped High
  // Bit Step: 20mV
  uint16_t input_voltage_regulation_limit_mV();

  // Reverse Mode Input Current Limit (in mA)
  // Address: 0xA
  // Bit 10:2
  // Reset: 0x190 (reset by: REG_RESET)
  // POR: 50000mA (190h)
  // Range: 1000mA-50000mA (8h-190h)
  // Clamped Low
  // Clamped High
  // Bit Step: 125mA
  uint16_t reverse_mode_current_limit_mA();

  // Reverse Mode System Voltage Limit (in mV)
  // Address: 0xC
  // Bit 13:2
  // Reset: 0xFA (reset by: REG_RESET)
  // POR: 5000mV (FAh)
  // Range: 3300mV-65000mV (A5h-CB2h)
  // Clamped Low
  // Clamped High
  // Bit Step: 20mV
  uint16_t reverse_mode_voltage_limit_mv();

  // Precharge Current Limit (in mA)
  // Address: 0x10
  // Bit 9:2
  // Reset: 0x50 (reset by: REG_RESET)
  // POR: 4000mA (50h)
  // Range: 250mA-10000mA (5h-C8h)
  // Clamped Low
  // Clamped High
  // Bit Step: 50mA
  // Actual pre-charge current is the lower of IPRECHG and ICHG pin
  uint16_t precharge_current_limit_mA();

  // Termination Current Limit (in mA)
  // Address: 0x12
  // Bit 9:2
  // Reset: 0x28 (reset by: REG_RESET)
  // POR: 2000mA (28h)
  // Range: 250mA-10000mA (5h-C8h)
  // Clamped Low
  // Clamped High
  // Bit Step: 50mA
  uint16_t termination_current_limit_mA();

  // Precharge and Termination Control
  // Address: 0x14

  // Bit 3 is EN_TERM (Enable termination control)
  // Reset: 0x1 (reset by: REG_RESET)
  // 0b = Disable, 1b = Enable
  bool precharge_termination_enable();

  // Bit 2:1 is VBAT_LOWV (Battery threshold for PRECHG to FASTCHG transition)
  // Reset: 0x3 (reset by: REG_RESET)
  // As percentage of VFB_REG:
  // 00b = 30% x VFB_REG
  // 01b = 55% x VFB_REG
  // 10b = 66.7% x VFB_REG
  // 11b = 71.4% x VFB_REG
  uint8_t precharge_vbat_percent();

  // Bit 0 is EN_PRECHG (Enable pre-charge and trickle charge functions)
  // Reset: 0x1 (reset by: REG_RESET)
  // 0b = Disable, 1b = Enable
  bool precharge_trickle_enable();

  // Timer Control
  // Address: 0x15

  // Bit 7:6 is TOPOFF_TMR (Top-off timer control)
  // Reset: 0x0 (reset by: REG_RESET)
  // 00b = Disable
  // 01b = 15 mins
  // 10b = 30 mins
  // 11b = 45 mins
  uint8_t timer_topoff_minutes();

  // Bit 5:4 is WATCHDOG (Watchdog timer control)
  // Reset: 0x1 (reset by: REG_RESET)
  // 00b = Disable
  // 01b = 40s
  // 10b = 80s
  // 11b = 160s
  uint8_t timer_watchdog_seconds();

  // Bit 3 is EN_CHG_TMR (Enable charge safety timer)
  // Reset is 0x1 (reset by: REG_RESET, WATCHDOG)
  // 0b = Disable
  // 1b = Enable
  bool timer_charge_enable();

  // Bit 2:1 is CHG_TMR (Charge safety timer setting)
  // Reset is 0x2 (reset by: REG_RESETT)
  // 00b = 5hr
  // 01b = 8hr
  // 10b = 12hr
  // 11b = 24hr
  uint8_t timer_charge_time_hrs();

  // Bit 0 is EN_TMR2X (Charge safety timer speed in DPM)
  // Reset is 0x1 (reset by: REG_RESET)
  // 0b = Timer always counts normally
  // 1b = Timer slowed by 2x during input DPM
  bool timer_counter_speed();

  // Charger Control
  // Address: 0x17

  // Bit 7:6 is VRECHG (Battery auto-recharge threshold)
  // Reset is 0x3 (reset by: REG_RESET)
  // As percentage of VFB_REG:
  // 00b = 93.0% x VFB_REG
  // 01b = 94.3% x VFB_REG
  // 10b = 95.2% x VFB_REG
  // 11b = 97.6% x VFB_REG
  uint8_t charger_recharge_threshold_percent();

  // Bit 5 is WD_RST (I2C Watchdog timer reset control)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = Normal
  // 1b = Reset (bit goes back to 0 after timer reset)

  // Bit 4 is DIS_CE_PIN (/CE pin function disable)
  // Reset is 0x0 (reset by: RERG_RESET)
  // 0b = /CE pin enabled
  // 1b = /CE pin disabled
  bool charger_disable_ce_pin();

  // Bit 3 is EN_CHG_BIT_RESET_BEHAVIOR (Controls the EN_CHG bit behavior when WATCHDOG expires)
  // Reset is 0x1 (reset by: REG_RESET)
  // 0b = EN_CHG bit resets to 0
  // 1b = EN_CHG bit resets to 1
  bool charger_bit_reset_behavior();

  // Bit 2 is EN_HIZ (HIZ mode enable)
  // Reset is 0x0 (reset by: REG_RESET, WATCHDOG, adapter plug in)
  // 0b = Disable
  // 1b = Enable
  bool charger_hiz_enable();

  // Bit 1 is EN_IBAT_LOAD (Battery Load (IBAT_LOAD) Enable)
  // Reset is 0x0 (reset by: REG_RESET, WATCHDOG)
  // 0b = Disabled
  // 1b = Enabled
  bool charger_enable_battery_load();

  // Bit 0 is EN_CHG (Charge enable)
  // Reset is 0x1 (reset by: REG_RESET, WATCHDOG)
  // 0b = Disable
  // 1b = Enable
  bool charger_charge_enable();

  // Pin Control
  // Address: 0x18

  // Bit 7 is EN_ICHG_PIN (ICHG pin function enable)
  // Reset is 0x1 (reset by: REG_RESET, WATCHDOG)
  // 0b = ICHG pin disabled
  // 1b = ICHG pin enabled
  bool pin_ichg_enable();

  // Bit 6 is EN_ILIM_HIZ_PIN (ILIM_HIZ pin function enable)
  // Reset is 0x1 (reset by: REG_RESET, WATCHDOG)
  // 0b = ILIM_HIZ pin disabled
  // 1b = ILIM_HIZ pin enabled
  bool pin_ilim_hiz_enable();

  // Bit 5 is DIS_PG_PIN (PG pin function disable)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = PG pin enabled
  // 1b = PG pin disabled
  bool pin_pg_disable();

  // Bit 4 is DIS_STAT_PINS (STAT1, STAT2 pin function disable)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = STAT pins enabled
  // 1b = STAT pins disabled
  bool pin_stats_disable();

  // Bit 3 is FORCE_STAT4_ON (CE_STAT4 pin override)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = CE_STAT4 open-drain off
  // 1b = CE_STAT4 pulls LOW
  bool pin_force_stat4_on();

  // Bit 2 is FORCE_STAT3_ON (PG_STAT3 pin override)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = PG_STAT3 open-drain off
  // 1b = PG_STAT3 pulls LOW
  bool pin_force_stat3_on();

  // Bit 1 is FORCE_STAT2_ON (STAT2 pin override)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = STAT2 open-drain off
  // 1b = STAT2 pulls LOW
  bool pin_force_stat2_on();

  // Bit 0 is FORCE_STAT1_ON (STAT1 pin override)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = STAT1 open-drain off
  // 1b = STAT1 pulls LOW
  bool pin_force_stat1_on();

  // Power Path and Reverse Mode Control
  // Address: 0x19

  // Bit 7 is REG_RST (Register reset to default values)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = Not reset
  // 1b = Reset (bit goes back to 0 after register reset)
  bool power_path_reset();

  // Bit 6 is EN_IAC_LOAD (VAC Load (IAC_LOAD) Enable)
  // Reset is 0x0 (reset by: REG_RESET, WATCHDOG)
  // 0b = Disabled
  // 1b = Enabled
  bool power_path_load_enable();

  // Bit 5 is EN_PFM (Enable PFM mode in light-load)
  // Reset is 0x1 (reset by: REG_RESET)
  // 0b = Disable (Fixed-frequency DCM operation)
  // 1b = Enable (PFM operation)
  bool power_path_pfm_enable();

  // Bit 4 is FORCE_BATFET_OFF (Force BATFET off)
  // Reset is 0x0 (reset by: REG_RESET, Adapter plug in)
  // 0b = Allow normal BATFET operation
  // 1b = Force BATFET off
  bool power_path_force_battery_off();

  // Bit 3 is PWRPATH_REDUCE_VDRV (Power-Path (ACFET, BATFET) Drive Voltage Select)
  // Reset is 0x0 (reset by: REG_RESET, WATCHDOG)
  // 0b = 10V
  // 1b = 7V
  bool power_path_reduce_drive_voltage();

  // Bit 2 is  EN_BATFET_IDEAL_DIODE (Enable BATFET ideal diode turn-on mode)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = Disable
  // 1b = Enable
  bool power_path_ideal_diode_enable();

  // Bit 1 is EN_AUTO_REV (Auto Reverse Mode to regulate SYS when VBAT < VSYS_REV)
  // Reset is 0x0 (reset by: REG_RESET, WATCHDOG)
  // 0b = Disable Auto Reverse
  // 1b = Enable Auto Reverse
  bool power_path_auto_reverse_enable();

  // Bit 0 is EN_REV (Reverse Mode control)
  // Reset is 0x0 (reset by: REG_RESET, WATCHDOG)
  // 0b = Disable
  // 1b = Enable
  bool power_path_reverse_mode_enable();

  // MPPT Control
  // Address: 0x1A

  // Bit 7 is FORCE_SWEEP (Force Full Panel Sweep and reset MPPT timers)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = Normal
  // 1b = Start Full Panel Sweep
  bool mppt_force_sweep();

  // Bit 2:1 is FULL_SWEEP_TMR (Full Panel Sweep timer control)
  // Reset is 0x0 (reset by: REG_RESET)
  // 00b = 3 min
  // 01b = 10 min
  // 10b = 15 min
  // 11b = 20 min
  uint8_t mppt_sweep_timer();

  // Bit 0 is EN_MPPT (MPPT algorithm control)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = Disable MPPT
  // 1b = Enable MPPT
  bool mppt_enable();

  // TS Charging Threshold Control
  // Address: 0x1B

  // Bit 7:6 is TS_T5 (Temp. sensor T5 (HOT) threshold control)
  // Reset is 0x2 (reset by: REG_RESET)
  // 00b = 41.2% (50C)
  // 01b = 37.7% (55C)
  // 10b = 34.375% (60C)
  // 11b = 31.25%(65C)
  uint8_t temp_threshold5_percent();

  // Bit 5:4 is TS_T3 (Temp. sensor T3 (WARM) threshold control)
  // Reset is 0x1 (reset by: REG_RESET)
  // 0b = 48.4% (40C)
  // 01b = 44.8% (45C)
  // 10b = 41.2% (50C)
  // 11b = 37.7% (55C)
  uint8_t temp_threshold3_percent();

  // Bit 3:2 is TS_T2 (Temp. sensor T2 (COOL) threshold control)
  // Reset is 0x1 (reset by: REG_RESET)
  // 00b = 71.1% (5C)
  // 01b = 68.4% (10C)
  // 10b = 65.5% (15C)
  // 11b = 62.4% (20C)
  uint8_t temp_threshold2_percent();

  // Bit 1:0 is TS_T1 (Temp. sensor T1 (COLD) threshold control)
  // Reset is 0x2 (reset by: REG_RESET)
  // 00b = 77.15% (-10C)
  // 01b = 75.32% (-5C)
  // 10b = 73.25% (0C)
  // 11b = 71.1% (5C)
  uint8_t temp_threshold1_percent();

  // TS Charging Region Behavior Control
  // Address: 0x1C

  // Bit 6:5 is JEITA_VSET (JEITA Warm (T3 < TS < T5) regulation voltage setting, as percentage of VFB_REG)
  // Reset is 0x2 (reset by: REG_RESET)
  // 00b = Charge Suspend
  // 01b = 94.3% x VFB_REG
  // 10b = 97.6% x VFB_REG
  // 11b = 100% x VFB_REG
  uint8_t temp_jeita_warm_percent();

  // Bit 4 is JEITA_ISETH (JEITA Warm (T3 < TS < T5) regulation current setting, as percentage of ICHG_REG:)
  // Reset is 0x1 (reset by: REG_RESET)
  // 0b = 40% x ICHG_REG
  // 1b = 100% x ICHG_REG
  bool temp_jeita_iset_warm();

  // Bit 3:2 is JEITA_ISETC (JEITA Cool (T1 < TS < T2) regulation current setting, as percentage of ICHG_REG)
  // Reset is 0x1 (reset by: REG_RESET)
  // 00b = Charge Suspend
  // 01b = 20% x ICHG_REG
  // 10b = 40% x ICHG_REG
  // 11b = 100% x ICHG_REG
  uint8_t temp_jeita_iset_cool();

  // Bit 1 is EN_JEITA (JEITA profile control)
  // Reset is 0x1 (reset by: REG_RESET)
  // 0b = Disabled (COLD/HOT control only)
  // 1b = Enabled (COLD/COOL/WARM/HOT control)
  bool temp_jeita_enable();

  // Bit 0 is EN_TS (Temp. sensor pin function control)
  // Reset is 0x1 (reset by: REG_RESET)
  // 0b = Disabled (ignore TS pin)
  // 1b = Enabled
  bool temp_jeita_sensor_pin_enable();

  // TS Reverse Mode Threshold Control
  // Address: 0x1D

  // Bit 7:6 is BHOT (Reverse Mode Temp. sensor HOT temperature threshold control)
  // Reset is 0x1 (reset by: REG_RESET)
  // 00b = 37.7% (55C)
  // 01b = 34.2% (60C)
  // 10b = 31.25%(65C)
  // 11b = Disable
  uint8_t temp_reverse_hot_percent();

  // Bit 5 is BCOLD (Reverse Mode TS COLD temperature threshold control)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = 77.15% (-10C)
  // 1b = 80% (-20C)
  bool temp_reverse_cold_threshold();

  // Reverse Undervoltage Control
  // Address: 0x1E
  // Bit 5 is SYSREV_UV (Reverse Mode System UVP)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = 80% of VSYS_REV target
  // 1b = Fixed at 3.3V
  bool reverse_sysrev_uv();

  // VAC Max Power Point Detected
  // Address: 0x1F
  // Bit 13:2 is VAC_MPP (Input Voltage for Max Power Point detected)
  // Reset is 0x0
  // POR: 0mV (0h)
  // Range: 0mV-60000mV (0h-BB8h)
  // Clamped High
  // Bit Step: 20mV
  uint16_t vac_mpp_mV();

  // Charger Status 1
  // Address: 0x21

  // Bit 7 is ADC_DONE_STAT (ADC conversion status)
  // 0b = Conversion not complete
  // 1b = Conversion complete
  bool status_adc_conversion();

  // Bit 6 is IAC_DPM_STAT (Input Current regulation status)
  // 0b = Normal
  // 1b = In Input Current regulation
  bool status_iac_dpm();

  // Bit 5 is VAC_DPM_STAT (Input Voltage regulation status)
  // 0b = Normal
  // 1b = In Input Voltage regulation
  bool status_vac_dpm();

  // Bit 3 is WD_STAT (I2C Watchdog timer status)
  // 0b = Normal
  // 1b = WD timer expired
  bool status_watchdog();

  // Bit 2:0 is CHARGE_STAT (Charge cycle status)
  // 000b = Not charging
  // 001b = Trickle Charge (VBAT < VBAT_SHORT)
  // 010b = Pre-Charge (VBAT < VBAT_LOWV)
  // 011b = Fast Charge (CC mode)
  // 100b = Taper Charge (CV mode)
  // 101b = Reserved
  // 110b = Top-off Timer Charge
  // 111b = Charge Termination Done
  uint8_t status_charge();

  // Charger Status 2
  // Address: 0x22

  // Bit 7 is PG_STAT (Input Power Good status)
  // 0b = Not Power Good
  // 1b = Power Good
  bool status_powergood();

  // Bit 6:4 is TS_STAT (Battery NTC temp. sensor status)
  // 000b = Normal
  // 001b = TS Warm
  // 010b = TS Cool
  // 011b = TS Cold
  // 100b = TS Hot
  uint8_t status_battery_temp();

  // Charger Status 3
  // Address: 0x23

  // Bit 5:4 is FSW_SYNC_STAT (FSW_SYNC pin status)
  // 00b = Normal, no external clock detected
  // 01b = Valid ext. clock detected
  // 10b = Pin fault (frequency out-of-range)
  // 11b = Reserved
  uint8_t status_fsync();

  // Bit 2 is REVERSE_STAT (Converter Reverse Mode status)
  // 0b = Reverse Mode off
  // 1b = Reverse Mode on
  bool status_reverse();

  // Bit 1 is ACFET_STAT (ACFET driver status)
  // 0b = ACFET off
  // 1b = ACFET on
  bool status_acfet();

  // Bit 0 is BATFET_STAT (BATFET driver status)
  // 0b = BATFET off
  // 1b = BATFET on
  bool status_batfet();

  // Fault Status
  // Address: 0x24

  // Bit 7 is VAC_UV_STAT (Input under-voltage status)
  // 0b = Input Normal
  // 1b = Device in Input under-voltage protection
  bool fault_input_undervolt_protect();

  // Bit 6 is VAC_OV_STAT (Input over-voltage status)
  // 0b = Input Normal
  // 1b = Device in Input over-voltage protection
  bool fault_input_overvolt_protect();

  // Bit 5 is IBAT_OCP_STAT (Battery over-current status)
  // 0b = Battery current normal
  // 1b = Battery over-current detected
  bool fault_battery_overcurrent_protect();

  // Bit 4 is VBAT_OV_STAT (Battery over-voltage status)
  // 0b = Normal
  // 1b = Device in Battery over-voltage protection
  bool fault_battery_overvolt_protect();

  // Bit 3 is TSHUT_STAT (Thermal shutdown status)
  // 0b = Normal
  // 1b = Device in thermal shutdown protection
  bool fault_overtemp_protect();

  // Bit 2 is CHG_TMR_STAT (Charge safety timer status)
  // 0b = Normal
  // 1b = Charge safety timer expired
  bool fault_charge_safety_timer_expire();

  // Bit 1 is DRV_OKZ_STAT (DRV_SUP pin voltage status)
  // 0b = Normal
  // 1b = DRV_SUP pin voltage is out of valid range
  bool fault_drv_sup_voltage_invalid();

  // Charger Flag 1
  // Address: 0x25

  // Bit 7 is ADC_DONE_FLAG (ADC conversion INT flag)
  // 0b = Conversion not complete
  // 1b = Conversion complete
  bool status_adc_done();

  // Bit 6 is IAC_DPM_FLAG (Input Current regulation INT flag)
  // 0b = Normal
  // 1b = Device entered Input Current regulation
  bool status_iac_dpm_active();

  // Bit 5 is VAC_DPM_FLAG (Input Voltage regulation INT flag)
  // 0b = Normal
  // 1b = Device entered Input Voltage regulation
  bool status_vac_dpm_active();

  // Bit 3 is WD_FLAG (I2C Watchdog timer INT flag)
  // 0b = Normal
  // 1b = WD_STAT rising edge detected
  bool status_watchdog_stat();

  // Bit 0 is CHARGE_FLAG (Charge cycle INT flag)
  // 0b = Not charging
  // 1b = CHARGE_STAT[2:0] bits changed (transition to any state)
  bool status_charge_flag();

  // Charger Flag 2
  // Address: 0x26

  // Bit 7 is PG_FLAG (Input Power Good INT flag)
  // 0b = Normal
  // 1b = PG signal toggle detected
  bool status_powergood_toggle_detect();

  // Bit 6 is ACFET_FLAG (ACFET driver INT flag)
  // 0b = Normal
  // 1b = ACFET signal toggle detected
  bool status_acfet_toggle_detect();

  // Bit 5 is BATFET_FLAG (BATFET driver INT flag)
  // 0b = Normal
  // 1b = BATFET signal toggle detected
  bool status_batfet_toggle_detect();

  // Bit 4 is TS_FLAG (Battery NTC temp. sensor INT flag)
  // 0b = Normal
  // 1b = TS_STAT[2:0] bits changed (transitioned to any state)
  bool status_battery_temp_detect();

  // Bit 3 is REVERSE_FLAG (Reverse Mode INT flag)
  // 0b = Normal
  // 1b = Reverse Mode toggle detected
  bool status_reverse_detect();

  // Bit 1 is FSW_SYNC_FLAG (FSW_SYNC pin signal INT flag)
  // 0b = Normal
  // 1b = FSW_SYNC status changed
  bool status_fsw_sync_detect();

  // Fault Flag
  // Address: 0x27

  // Bit 7 is VAC_UV_FLAG (Input under-voltage INT flag)
  // 0b = Normal
  // 1b = Entered input under-voltage fault
  bool fault_input_undervolt();

  // Bit 6 is VAC_OV_FLAG (Input over-voltage INT flag)
  // 0b = Normal
  // 1b = Entered Input over-voltage fault
  bool fault_input_overvolt();

  // Bit 5 is IBAT_OCP_FLAG (Battery over-current INT flag)
  // 0b = Normal
  // 1b = Entered Battery over-current fault
  bool fault_battery_overcurrent();

  // Bit 4 is VBAT_OV_FLAG (Battery over-voltage INT flag)
  // 0b = Normal
  // 1b = Entered battery over-voltage fault
  bool fault_battery_overvolt();

  // Bit 3 is TSHUT_FLAG (Thermal shutdown INT flag)
  // 0b = Normal
  // 1b = Entered TSHUT fault
  bool fault_battery_thermal_shutdown();

  // Bit 2 is CHG_TMR_FLAG (Charge safety timer INT flag)
  // 0b = Normal
  // 1b = Charge Safety timer expired rising edge detected
  bool fault_charge_safety_timer();

  // Bit 1 is DRV_OKZ_FLAG (DRV_SUP pin voltage INT flag)
  // 0b = Normal
  // 1b = DRV_SUP pin fault detected
  bool fault_drv_sup_voltage();

  // Charger Mask 1
  // Address: 0x28

  // Bit 7 is ADC_DONE_MASK (ADC conversion INT mask)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = ADC_DONE produces INT pulse
  // 1b = ADC_DONE does not produce INT pulse
  bool charger_mask_adc_done();

  // Bit 6 is IAC_DPM_MASK (Input Current regulation INT mask)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = IAC_DPM_FLAG produces INT pulse
  // 1b = IAC_DPM_FLAG does not produce INT pulse
  bool charger_mask_iac_dpm();

  // Bit 5 is VAC_DPM_MASK (Input Voltage regulation INT mask)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = VAC_DPM_FLAG produces INT pulse
  // 1b = VAC_DPM_FLAG does not produce INT pulse
  bool charger_mask_dpm_pulse_disable();

  // Bit 3 is WD_MASK (I2C Watchdog timer INT mask)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = WD expiration produces INT pulse
  // 1b = WD expiration does not produce INT pulse
  bool charger_mask_watchdog_pulse_disable();

  // Bit 0 is CHARGE_MASK (Charge cycle INT mask)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = CHARGE_STAT change produces INT pulse
  // 1b = CHARGE_STAT change does not produces INT pulse
  bool charger_mask_int_pulse_disable();

  // Charger Mask 2
  // Address: 0x29

  // Bit 7 is PG_MASK (Input Power Good INT mask)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = PG toggle produces INT pulse
  // 1b = PG toggle does not produce INT pulse
  bool charger_mask_powergood_pulse_disable();

  // Bit 6 is ACFET_MASK (ACFET driver INT mask)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = ACFET toggle produces INT pulse
  // 1b = ACFET toggle does not produce INT pulse
  bool charger_mask_acfet_pulse_disable();

  // Bit 5 is BATFET_MASK (BATFET driver INT mask)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = BATFET toggle produces INT pulse
  // 1b = BATFET toggle does not produce INT pulse
  bool charger_mask_batfet_pulse_disable();

  // Bit 4 is TS_MASK (Battery NTC temp. sensor INT mask)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = TS_STAT change produces INT pulse
  // 1b = TS_STAT change does not produce INT pulse
  bool charger_mask_tempsense_pulse_disable();

  // Bit 3 is REVERSE_MASK (Reverse Mode INT mask)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = REVERSE_STAT toggle produces INT pulse
  // 1b = REVERSE_STAT toggle does no produce INT pulse
  bool charger_mask_reverse_pulse_disable();

  // Bit 1 is FSW_SYNC_MASK (FSW_SYNC pin signal INT mask)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = FSW_SYNC status change produces INT pulse
  // 1b = FSW_SYNC status change does not produce INT pulse
  bool charger_mask_fsw_pulse_disable();

  // Fault Mask
  // Address: 0x2A

  // Bit 7 is VAC_UV_MASK (Input under-voltage INT mask)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = Input under-voltage event produces INT pulse
  // 1b = Input under-voltage event does not produce INT pulse
  bool fault_mask_undervolt_pulse_disable();

  // Bit 6 is VAC_OV_MASK (Input over-voltage INT mask)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = Input over-voltage event produces INT pulse
  // 1b = Input over-voltage event does not produce INT pulse
  bool fault_mask_overvolt_pulse_disable();

  // Bit 5 is IBAT_OCP_MASK (Battery over-current INT mask)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = Battery over-current event produces INT pulse
  // 1b = Battery over-current event does not produce INT pulse
  bool fault_mask_batt_overcurrent_pulse_disable();

  // Bit 4 is VBAT_OV_MASK (Battery over-voltage INT mask)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = Battery over-voltage event produces INT pulse
  // 1b = Battery over-voltage event does not produce INT pulse
  bool fault_mask_batt_overvolt_pulse_disable();

  // Bit 3 is TSHUT_MASK (Thermal shutdown INT mask)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = TSHUT event produces INT pulse
  // 1b = TSHUT event does not produce INT pulse
  bool fault_mask_thermal_shutdown_pulse_disable();

  // Bit 2 is CHG_TMR_MASK (Charge safety timer INT mask)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = Timer expired rising edge produces INT pulse
  // 1b = Timer expired rising edge does not produce INT pulse
  bool fault_mask_timer_pulse_disable();

  // Bit 1 is DRV_OKZ_MASK (DRV_SUP pin voltage INT mask)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = DRV_SUP pin fault produces INT pulse
  // 1b = DRV_SUP pin fault does not produce INT pulse
  bool fault_mask_drvsup_pulse_disable();

  // ADC Control
  // Address: 0x2B

  // Bit 7 is ADC_EN (ADC control)
  // Reset is 0x0 (reset by: REG_RESET, WATCHDOG)
  // 0b = Disable ADC
  // 1b = Enable ADC
  bool adc_enable();

  // Bit 6 is ADC_RATE (ADC conversion rate control)
  // Reset is 0x1 (reset by: REG_RESET)
  // 0b = Continuous conversion
  // 1b = One-shot conversion
  bool adc_rate();

  // Bit 5:4 is ADC_SAMPLE (ADC sample speed)
  // Reset is 0x2 (reset by: REG_RESET)
  // 00b = 15 bit effective resolution
  // 01b = 14 bit effective resolution
  // 10b = 13 bit effective resolution
  // 11b = Reserved
  uint8_t adc_sample_rate();

  // Bit 3 is ADC_AVG (ADC average control)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = Single value
  // 1b = Running average
  bool adc_average_enable();

  // Bit 2 is ADC_AVG_INIT (ADC average initial value control)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = Start average using existing register value
  // 1b = Start average using new ADC conversion
  bool adc_average_init();

  // ADC Channel Control
  // Address: 0x2C

  // Bit 7 is IAC_ADC_DIS (IAC ADC control)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = Enable
  // 1b = Disable
  bool adc_iac_enable();

  // Bit 6 is IBAT_ADC_DIS (IBAT ADC control)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = Enable
  // 1b = Disable
  bool adc_ibat_disable();

  // Bit 5 is VAC_ADC_DIS (VAC ADC control)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = Enable
  // 1b = Disable
  bool adc_vac_disable();

  // Bit 4 is VBAT_ADC_DIS (VBAT ADC control)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = Enable
  // 1b = Disable
  bool adc_vbat_disable();

  // Bit 3 is VSYS_ADC_DIS (VSYS ADC control)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = Enable
  // 1b = Disable
  bool adc_vsys_disable();

  // Bit 2 is TS_ADC_DIS (Temp. sensor ADC control)
  // Reset is 0x0 (reset by: REG_RESET)
  // 0b = Enable
  // 1b = Disable
  bool adc_tempsense_disable();

  // Bit 1 is VFB_ADC_DIS (VFB ADC control)
  // Reset is 0x1 (reset by: REG_RESET)
  // 0b = Enable
  // 1b = Disable
  bool adc_vfb_disable();

  // IAC ADC
  // Address: 0x2D
  // Bit 15:0 is IAC_ADC (IAC ADC reading with 2mΩ RAC_SNS, reported as 2s complement)
  // POR: 0mA (0h)
  // Format: 2s Complement
  // Range: -50000mA-50000mA (9E58h-61A8h)
  // Clamped Low
  // Clamped High
  // Bit Step: 2mAPOR: 0mA (0h)
  // Format: 2s Complement
  // Range: -50000mA-50000mA (9E58h-61A8h)
  // Clamped Low
  // Clamped High
  // Bit Step: 2mA
  int16_t adc_iac();

  // IBAT ADC
  // Address: 0x2F
  // Bit 15:0 is IBAT_ADC (IBAT ADC reading with 5mΩ RBAT_SNS, reported as 2s complement)
  // POR: 0mA (0h)
  // Format: 2s Complement
  // Range: -20000mA-20000mA (D8F0h-2710h)
  // Clamped Low
  // Clamped High
  // Bit Step: 2mA
  int16_t adc_ibat();

  // VAC ADC
  // Address: 0x31
  // Bit 15:0 is VAC_ADC (VAC ADC reading, reported as unsigned integer)
  // POR: 0mV (0h)
  // Format: 2s Complement
  // Range: 0mV-65534mV (0h-7FFFh)
  // Clamped Low
  // Bit Step: 2mV
  uint16_t adc_vac();

  // VBAT ADC
  // Address: 0x33
  // Bit 15:0 is VBAT_ADC (VBAT ADC reading, reported as unsigned integer)
  // POR: 0mV (0h)
  // Format: 2s Complement
  // Range: 0mV-65534mV (0h-7FFFh)
  // Clamped Low
  // Bit Step: 2mV
  uint16_t adc_vbat();

  // VSYS ADC
  // Address: 0x35
  // Bit 15:0 is VSYS_ADC (VSYS ADC reading, reported as unsigned integer)
  // POR: 0mV (0h)
  // Format: 2s Complement
  // Range: 0mV-65534mV (0h-7FFFh)
  // Clamped Low
  // Bit Step: 2mV
  uint16_t adc_vsys();

  // VFB ADC
  // Address: 0x39
  // Bit 15:0 is VFB_ADC (VFB ADC reading)
  // POR: 0mV (0h)
  // Range: 0mV-2047mV (0h-7FFh)
  // Clamped High
  // Bit Step: 1mV
  uint16_t adc_vfb();

  // Gate Driver Strength Control
  // Address: 0x3B

  // Bit 5:4 is BUCK_HS_DRV (Buck High Side FET Gate Driver Strength)
  // Reset is 0x0 (reset by: REG_RESET)
  // 00b = Fastest
  // 01b = Faster
  // 10b = Slower
  // 11b = Slowest
  uint8_t gate_high_strength();

  // Bit 1:0 is BUCK_LS_DRV (Buck Low Side FET Gate Driver Strength)
  // Reset is 0x0 (reset by: REG_RESET)
  // 00b = Fastest
  // 01b = Faster
  // 10b = Slower
  // 11b = Slowest
  uint8_t gate_low_strength();

  // Gate Driver Dead Time Control
  // Address: 0x3C
  // Bit 1:0 is BUCK_DEAD_TIME (Buck Side FETs Dead Time Control)
  // 00b = 45ns
  // 01b = 75ns
  // 10b = 105ns
  // 11b = 135ns
  uint8_t gate_deadtime_ns();

  // Part Information
  // Address: 0x3D

  // Bit 5:3 is PART_NUM (Part number)
  // 011b = BQ25820
  uint8_t part_number();

  // Bit 2:0 is DEV_REV (Device revision, 010b)
  uint8_t device_revision();

  // Reverse Mode Battery Discharge Current
  // Address: 0x62

  // Bit 7:6 IBAT_REV (Reverse mode battery discharge current limit)
  // Reset is 0x0 (reset by: REG_RESET)
  // 00b = 20A
  // 01b = 15A
  // 10b = 10A
  // 11b = 5A
  uint16_t reverse_mode_discharge_current_mA();

  // Bit 1 is EN_CONV_FAST_TRANSIENT (Enable converter fast transient response in reverse mode only)
  // Reset is 0x1 (reset by: REG_RESET)
  // 0b = Disable
  // 1b = Enable
  bool reverse_mode_fast_enable();

  // Battery voltage (in mV)
  uint16_t get_battery_voltage_mV();

  // Battery current (in mA, signed: positive = charging, negative = discharging)
  int16_t get_charge_current_mA();

  // Status flags
  bool is_charging();
  bool is_charged();
  bool is_low_battery();

  // Configuration
  void set_charge_current_mA(uint16_t current);
  void set_watchdog_enabled(bool enabled);
  void set_bat_sns_res_mOhm(float res);
  void set_ac_sns_res_mOhm(float res);

  // Charge Voltage Limit VFB_REG (in mV, 1504-1566)
  void set_charge_voltage_mV(uint16_t mv);
  // Input Current DPM Limit (in mA, 1000-50000)
  void set_input_current_limit_mA(uint16_t ma);
  // Input Voltage DPM Limit (in mV, 4400-65000)
  void set_input_voltage_limit_mV(uint16_t mv);
  // Precharge Current Limit (in mA, 250-10000)
  void set_precharge_current_mA(uint16_t ma);
  // Reverse Mode System Voltage Limit (in mV, 3300-65000)
  void set_reverse_system_voltage_mV(uint16_t mv);
  // Charger Control EN_CHG
  void set_charge_enabled(bool enabled);
  // Charger Control EN_HIZ
  void set_hiz_enabled(bool enabled);
  // CHG_TMR code (00b=5h, 01b=8h, 10b=12h, 11b=24h); also sets EN_CHG_TMR
  void set_charge_timer_code(uint8_t code);
  // TOPOFF_TMR code (00b=off, 01b=15m, 10b=30m, 11b=45m)
  void set_topoff_timer_code(uint8_t code);
  // Power Path EN_BATFET_IDEAL_DIODE
  void set_ideal_diode_enabled(bool enabled);
  // MPPT Control EN_MPPT
  void set_mppt_enabled(bool enabled);
  // MPPT FULL_SWEEP_TMR code (00b=3m, 01b=10m, 10b=15m, 11b=20m)
  void set_mppt_sweep_timer_code(uint8_t code);
  // MPPT FORCE_SWEEP: self-clearing command, written once in setup()
  void set_force_mppt_sweep(bool enabled);
  // TS region behavior EN_TS
  void set_temp_enabled(bool enabled);
  // TS charging thresholds, 2-bit codes each
  void set_ts_thresholds(uint8_t t1, uint8_t t2, uint8_t t3, uint8_t t5);

 private:
  void write_config_registers();

  std::list<BQ25820Listener *> listeners_;
  BQ25820Data data_;
  bool watchdog_enabled_{false};
  uint16_t ichg_mA_{0};
  float bat_sns_res_mOhm_{5.0f};
  float ac_sns_res_mOhm_{2.0f};
  uint16_t ibat_step_mA_{2};
  uint16_t iac_step_mA_{2};
  uint16_t charge_voltage_mV_{1536};
  uint16_t input_current_limit_mA_{50000};
  uint16_t input_voltage_limit_mV_{4400};
  uint16_t precharge_current_mA_{4000};
  uint16_t reverse_system_voltage_mV_{5000};
  bool charge_enabled_{true};
  bool hiz_enabled_{false};
  uint8_t charge_timer_code_{1};
  uint8_t topoff_timer_code_{0};
  bool ideal_diode_enabled_{false};
  bool mppt_enabled_{false};
  uint8_t mppt_sweep_timer_code_{0};
  bool force_mppt_sweep_{false};
  bool temp_enabled_{true};
  uint8_t ts_t1_{2};
  uint8_t ts_t2_{1};
  uint8_t ts_t3_{1};
  uint8_t ts_t5_{0};
};
}  // namespace esphome::bq25820
