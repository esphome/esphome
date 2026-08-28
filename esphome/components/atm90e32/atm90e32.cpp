#include "atm90e32.h"
#include <cinttypes>
#include <cmath>
#include <cstring>
#include <numbers>
#include "esphome/core/log.h"

namespace esphome::atm90e32 {

static const char *const TAG = "atm90e32";

static const LogString *offset_calibration_name(bool power_offsets) {
  return power_offsets ? LOG_STR("Power offset") : LOG_STR("Offset");
}

static uint32_t pref_hash(const char *prefix, const char *name_space) {
  auto hash = fnv1_hash(prefix);
  return fnv1_hash_extend(hash, name_space);
}

template<typename T>
static int migrate_legacy_pref_if_needed(ESPPreferenceObject &current_pref, ESPPreferenceObject &legacy_pref,
                                         T *scratch) {
  T current{};
  if (current_pref.load(&current)) {
    return 0;
  }
  if (!legacy_pref.load(scratch)) {
    return 0;
  }
  return current_pref.save(scratch) ? 1 : -1;
}

void ATM90E32Component::loop() {
  if (this->get_publish_interval_flag_()) {
    this->set_publish_interval_flag_(false);
    for (uint8_t phase = 0; phase < 3; phase++) {
      if (this->phase_[phase].voltage_sensor_ != nullptr)
        this->phase_[phase].voltage_ = this->get_phase_voltage_(phase);

      if (this->phase_[phase].current_sensor_ != nullptr)
        this->phase_[phase].current_ = this->get_phase_current_(phase);

      if (this->phase_[phase].power_sensor_ != nullptr)
        this->phase_[phase].active_power_ = this->get_phase_active_power_(phase);

      if (this->phase_[phase].power_factor_sensor_ != nullptr)
        this->phase_[phase].power_factor_ = this->get_phase_power_factor_(phase);

      if (this->phase_[phase].reactive_power_sensor_ != nullptr)
        this->phase_[phase].reactive_power_ = this->get_phase_reactive_power_(phase);

      if (this->phase_[phase].apparent_power_sensor_ != nullptr)
        this->phase_[phase].apparent_power_ = this->get_phase_apparent_power_(phase);

      if (this->phase_[phase].forward_active_energy_sensor_ != nullptr)
        this->phase_[phase].forward_active_energy_ = this->get_phase_forward_active_energy_(phase);

      if (this->phase_[phase].reverse_active_energy_sensor_ != nullptr)
        this->phase_[phase].reverse_active_energy_ = this->get_phase_reverse_active_energy_(phase);

      if (this->phase_[phase].phase_angle_sensor_ != nullptr)
        this->phase_[phase].phase_angle_ = this->get_phase_angle_(phase);

      if (this->phase_[phase].harmonic_active_power_sensor_ != nullptr)
        this->phase_[phase].harmonic_active_power_ = this->get_phase_harmonic_active_power_(phase);

      if (this->phase_[phase].peak_current_sensor_ != nullptr)
        this->phase_[phase].peak_current_ = this->get_phase_peak_current_(phase);

      // After the local store is collected we can publish them trusting they are within +-1 hardware sampling
      if (this->phase_[phase].voltage_sensor_ != nullptr)
        this->phase_[phase].voltage_sensor_->publish_state(this->get_local_phase_voltage_(phase));

      if (this->phase_[phase].current_sensor_ != nullptr)
        this->phase_[phase].current_sensor_->publish_state(this->get_local_phase_current_(phase));

      if (this->phase_[phase].power_sensor_ != nullptr)
        this->phase_[phase].power_sensor_->publish_state(this->get_local_phase_active_power_(phase));

      if (this->phase_[phase].power_factor_sensor_ != nullptr)
        this->phase_[phase].power_factor_sensor_->publish_state(this->get_local_phase_power_factor_(phase));

      if (this->phase_[phase].reactive_power_sensor_ != nullptr)
        this->phase_[phase].reactive_power_sensor_->publish_state(this->get_local_phase_reactive_power_(phase));

      if (this->phase_[phase].apparent_power_sensor_ != nullptr)
        this->phase_[phase].apparent_power_sensor_->publish_state(this->get_local_phase_apparent_power_(phase));

      if (this->phase_[phase].forward_active_energy_sensor_ != nullptr) {
        this->phase_[phase].forward_active_energy_sensor_->publish_state(
            this->get_local_phase_forward_active_energy_(phase));
      }

      if (this->phase_[phase].reverse_active_energy_sensor_ != nullptr) {
        this->phase_[phase].reverse_active_energy_sensor_->publish_state(
            this->get_local_phase_reverse_active_energy_(phase));
      }

      if (this->phase_[phase].phase_angle_sensor_ != nullptr)
        this->phase_[phase].phase_angle_sensor_->publish_state(this->get_local_phase_angle_(phase));

      if (this->phase_[phase].harmonic_active_power_sensor_ != nullptr) {
        this->phase_[phase].harmonic_active_power_sensor_->publish_state(
            this->get_local_phase_harmonic_active_power_(phase));
      }

      if (this->phase_[phase].peak_current_sensor_ != nullptr)
        this->phase_[phase].peak_current_sensor_->publish_state(this->get_local_phase_peak_current_(phase));
    }
    if (this->freq_sensor_ != nullptr)
      this->freq_sensor_->publish_state(this->get_frequency_());

    if (this->chip_temperature_sensor_ != nullptr)
      this->chip_temperature_sensor_->publish_state(this->get_chip_temperature_());
  }
}

void ATM90E32Component::update() {
  if (this->read16_(ATM90E32_REGISTER_METEREN) != 1) {
    this->status_set_warning();
    return;
  }
  this->set_publish_interval_flag_(true);
  this->status_clear_warning();

#ifdef USE_TEXT_SENSOR
  this->check_phase_status();
  this->check_over_current();
  this->check_freq_status();
#endif
}

void ATM90E32Component::get_cs_summary_(std::span<char, GPIO_SUMMARY_MAX_LEN> buffer) {
  this->cs_->dump_summary(buffer.data(), buffer.size());
}

const char *ATM90E32Component::get_calibration_id_() { return this->instance_id_; }

void ATM90E32Component::setup() {
  this->spi_setup();
  const char *cs = this->get_calibration_id_();
  char legacy_cs[GPIO_SUMMARY_MAX_LEN];
  this->get_cs_summary_(legacy_cs);
  const bool has_distinct_legacy_namespace = strcmp(cs, legacy_cs) != 0;

  uint16_t mmode0 = 0x87;  // 3P4W 50Hz
  uint16_t high_thresh = 0;
  uint16_t low_thresh = 0;

  if (line_freq_ == 60) {
    mmode0 |= 1 << 12;  // sets 12th bit to 1, 60Hz
    // for freq threshold registers
    high_thresh = 6300;  // 63.00 Hz
    low_thresh = 5700;   // 57.00 Hz
  } else {
    high_thresh = 5300;  // 53.00 Hz
    low_thresh = 4700;   // 47.00 Hz
  }

  if (current_phases_ == 2) {
    mmode0 |= 1 << 8;  // sets 8th bit to 1, 3P3W
    mmode0 |= 0 << 1;  // sets 1st bit to 0, phase b is not counted into the all-phase sum energy/power (P/Q/S)
  }

  this->write16_(ATM90E32_REGISTER_SOFTRESET, 0x789A, false);  // Perform soft reset
  delay(6);                                                    // Wait for the minimum 5ms + 1ms
  this->write16_(ATM90E32_REGISTER_CFGREGACCEN, 0x55AA);       // enable register config access
  if (!this->validate_spi_read_(0x55AA, "setup()")) {
    ESP_LOGW(TAG, "Could not initialize ATM90E32 IC, check SPI settings");
    this->mark_failed();
    return;
  }

  this->write16_(ATM90E32_REGISTER_METEREN, 0x0001);        // Enable Metering
  this->write16_(ATM90E32_REGISTER_SAGPEAKDETCFG, 0xFF3F);  // Peak Detector time (15:8) 255ms, Sag Period (7:0) 63ms
  this->write16_(ATM90E32_REGISTER_PLCONSTH, 0x0861);       // PL Constant MSB (default) = 140625000
  this->write16_(ATM90E32_REGISTER_PLCONSTL, 0xC468);       // PL Constant LSB (default)
  this->write16_(ATM90E32_REGISTER_ZXCONFIG, 0xD654);       // Zero crossing (ZX2, ZX1, ZX0) pin config
  this->write16_(ATM90E32_REGISTER_MMODE0, mmode0);         // Mode Config (frequency set in main program)
  this->write16_(ATM90E32_REGISTER_MMODE1, pga_gain_);      // PGA Gain Configuration for Current Channels
  this->write16_(ATM90E32_REGISTER_FREQHITH, high_thresh);  // Frequency high threshold
  this->write16_(ATM90E32_REGISTER_FREQLOTH, low_thresh);   // Frequency low threshold
  this->write16_(ATM90E32_REGISTER_PSTARTTH, 0x1D4C);       // All Active Startup Power Threshold - 0.02A/0.00032 = 7500
  this->write16_(ATM90E32_REGISTER_QSTARTTH, 0x1D4C);       // All Reactive Startup Power Threshold - 50%
  this->write16_(ATM90E32_REGISTER_SSTARTTH, 0x1D4C);       // All Reactive Startup Power Threshold - 50%
  this->write16_(ATM90E32_REGISTER_PPHASETH, 0x02EE);       // Each Phase Active Phase Threshold - 0.002A/0.00032 = 750
  this->write16_(ATM90E32_REGISTER_QPHASETH, 0x02EE);       // Each phase Reactive Phase Threshold - 10%

  if (this->enable_offset_calibration_) {
    // Initialize flash storage for offset calibrations
    uint32_t o_hash = pref_hash("_offset_calibration_", cs);
    this->offset_pref_ = global_preferences->make_preference<OffsetCalibration[3]>(o_hash, true);
    bool migrated_offset = false;
    if (has_distinct_legacy_namespace) {
      uint32_t legacy_o_hash = pref_hash("_offset_calibration_", legacy_cs);
      auto legacy_offset_pref = global_preferences->make_preference<OffsetCalibration[3]>(legacy_o_hash, true);
      OffsetCalibration offset_data[3]{};
      int migration_status = migrate_legacy_pref_if_needed(this->offset_pref_, legacy_offset_pref, &offset_data);
      migrated_offset = migration_status > 0;
      if (migration_status > 0) {
        ESP_LOGI(TAG, "[CALIBRATION][%s] Migrated offset calibrations from legacy storage.", cs);
      } else if (migration_status < 0) {
        ESP_LOGW(TAG, "[CALIBRATION][%s] Failed to migrate offset calibrations from legacy storage.", cs);
      }
    }

    // Initialize flash storage for power offset calibrations
    uint32_t po_hash = pref_hash("_power_offset_calibration_", cs);
    this->power_offset_pref_ = global_preferences->make_preference<OffsetCalibration[3]>(po_hash, true);
    bool migrated_power_offset = false;
    if (has_distinct_legacy_namespace) {
      uint32_t legacy_po_hash = pref_hash("_power_offset_calibration_", legacy_cs);
      auto legacy_power_offset_pref = global_preferences->make_preference<OffsetCalibration[3]>(legacy_po_hash, true);
      OffsetCalibration power_offset_data[3]{};
      int migration_status =
          migrate_legacy_pref_if_needed(this->power_offset_pref_, legacy_power_offset_pref, &power_offset_data);
      migrated_power_offset = migration_status > 0;
      if (migration_status > 0) {
        ESP_LOGI(TAG, "[CALIBRATION][%s] Migrated power offset calibrations from legacy storage.", cs);
      } else if (migration_status < 0) {
        ESP_LOGW(TAG, "[CALIBRATION][%s] Failed to migrate power offset calibrations from legacy storage.", cs);
      }
    }

    if (migrated_offset || migrated_power_offset) {
      global_preferences->sync();
    }

    this->restore_offset_calibrations_(OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_VOLTAGE_CURRENT);
    this->restore_offset_calibrations_(OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_POWER);
  } else {
    ESP_LOGI(TAG, "[CALIBRATION][%s] Power & Voltage/Current offset calibration is disabled. Using config file values.",
             cs);
    for (uint8_t phase = 0; phase < 3; ++phase) {
      this->write16_(this->voltage_offset_registers[phase],
                     static_cast<uint16_t>(this->offset_phase_[phase].first_offset));
      this->write16_(this->current_offset_registers[phase],
                     static_cast<uint16_t>(this->offset_phase_[phase].second_offset));
      this->write16_(this->power_offset_registers[phase],
                     static_cast<uint16_t>(this->power_offset_phase_[phase].first_offset));
      this->write16_(this->reactive_power_offset_registers[phase],
                     static_cast<uint16_t>(this->power_offset_phase_[phase].second_offset));
    }
  }

  if (this->enable_gain_calibration_) {
    // Initialize flash storage for gain calibration
    uint32_t g_hash = pref_hash("_gain_calibration_", cs);
    this->gain_calibration_pref_ = global_preferences->make_preference<GainCalibration[3]>(g_hash, true);
    bool migrated_gain = false;
    if (has_distinct_legacy_namespace) {
      uint32_t legacy_g_hash = pref_hash("_gain_calibration_", legacy_cs);
      auto legacy_gain_calibration_pref = global_preferences->make_preference<GainCalibration[3]>(legacy_g_hash, true);
      GainCalibration gain_data[3]{};
      int migration_status =
          migrate_legacy_pref_if_needed(this->gain_calibration_pref_, legacy_gain_calibration_pref, &gain_data);
      migrated_gain = migration_status > 0;
      if (migration_status > 0) {
        ESP_LOGI(TAG, "[CALIBRATION][%s] Migrated gain calibrations from legacy storage.", cs);
      } else if (migration_status < 0) {
        ESP_LOGW(TAG, "[CALIBRATION][%s] Failed to migrate gain calibrations from legacy storage.", cs);
      }
    }

    if (migrated_gain) {
      global_preferences->sync();
    }

    this->restore_gain_calibrations_();
  } else {
    ESP_LOGI(TAG, "[CALIBRATION][%s] Gain calibration is disabled. Using config file values.", cs);
    for (uint8_t phase = 0; phase < 3; ++phase) {
      this->write16_(voltage_gain_registers[phase], this->phase_[phase].voltage_gain_);
      this->write16_(current_gain_registers[phase], this->phase_[phase].ct_gain_);
    }
  }

  // Sag threshold (78%)
  uint16_t sagth = calculate_voltage_threshold(line_freq_, this->phase_[0].voltage_gain_, 0.78f);
  // Overvoltage threshold (122%)
  uint16_t ovth = calculate_voltage_threshold(line_freq_, this->phase_[0].voltage_gain_, 1.22f);

  // Write to registers
  this->write16_(ATM90E32_REGISTER_SAGTH, sagth);
  this->write16_(ATM90E32_REGISTER_OVTH, ovth);

  this->write16_(ATM90E32_REGISTER_CFGREGACCEN, 0x0000);  // end configuration
}

void ATM90E32Component::log_calibration_status_() {
  const char *cs = this->get_calibration_id_();

  bool offset_mismatch = false;
  bool power_mismatch = false;
  bool gain_mismatch = false;

  for (uint8_t phase = 0; phase < 3; ++phase) {
    offset_mismatch |= this->offset_calibration_mismatch_[phase];
    power_mismatch |= this->power_offset_calibration_mismatch_[phase];
    gain_mismatch |= this->gain_calibration_mismatch_[phase];
  }

  if (offset_mismatch) {
    ESP_LOGW(TAG, "[CALIBRATION][%s] ", cs);
    ESP_LOGW(TAG,
             "[CALIBRATION][%s] ===================== Offset mismatch: using flash values =====================", cs);
    ESP_LOGW(TAG, "[CALIBRATION][%s] ------------------------------------------------------------------------------",
             cs);
    ESP_LOGW(TAG, "[CALIBRATION][%s] | Phase |   offset_voltage   |   offset_current   |", cs);
    ESP_LOGW(TAG, "[CALIBRATION][%s] |       |  config  |  flash  |  config  |  flash  |", cs);
    ESP_LOGW(TAG, "[CALIBRATION][%s] ------------------------------------------------------------------------------",
             cs);
    for (uint8_t phase = 0; phase < 3; ++phase) {
      ESP_LOGW(TAG, "[CALIBRATION][%s] |   %c   |  %6d  | %6d  |  %6d  | %6d  |", cs, 'A' + phase,
               this->config_offset_phase_[phase].first_offset, this->offset_phase_[phase].first_offset,
               this->config_offset_phase_[phase].second_offset, this->offset_phase_[phase].second_offset);
    }
    ESP_LOGW(TAG,
             "[CALIBRATION][%s] ===============================================================================", cs);
  }
  if (power_mismatch) {
    ESP_LOGW(TAG, "[CALIBRATION][%s] ", cs);
    ESP_LOGW(TAG,
             "[CALIBRATION][%s] ================= Power offset mismatch: using flash values =================", cs);
    ESP_LOGW(TAG, "[CALIBRATION][%s] ------------------------------------------------------------------------------",
             cs);
    ESP_LOGW(TAG, "[CALIBRATION][%s] | Phase | offset_active_power|offset_reactive_power|", cs);
    ESP_LOGW(TAG, "[CALIBRATION][%s] |       |  config  |  flash  |  config  |  flash  |", cs);
    ESP_LOGW(TAG, "[CALIBRATION][%s] ------------------------------------------------------------------------------",
             cs);
    for (uint8_t phase = 0; phase < 3; ++phase) {
      ESP_LOGW(TAG, "[CALIBRATION][%s] |   %c   |  %6d  | %6d  |  %6d  | %6d  |", cs, 'A' + phase,
               this->config_power_offset_phase_[phase].first_offset, this->power_offset_phase_[phase].first_offset,
               this->config_power_offset_phase_[phase].second_offset, this->power_offset_phase_[phase].second_offset);
    }
    ESP_LOGW(TAG,
             "[CALIBRATION][%s] ===============================================================================", cs);
  }
  if (gain_mismatch) {
    ESP_LOGW(TAG, "[CALIBRATION][%s] ", cs);
    ESP_LOGW(TAG,
             "[CALIBRATION][%s] ====================== Gain mismatch: using flash values =====================", cs);
    ESP_LOGW(TAG, "[CALIBRATION][%s] ------------------------------------------------------------------------------",
             cs);
    ESP_LOGW(TAG, "[CALIBRATION][%s] | Phase |    voltage_gain    |    current_gain    |", cs);
    ESP_LOGW(TAG, "[CALIBRATION][%s] |       |  config  |  flash  |  config  |  flash  |", cs);
    ESP_LOGW(TAG, "[CALIBRATION][%s] ------------------------------------------------------------------------------",
             cs);
    for (uint8_t phase = 0; phase < 3; ++phase) {
      ESP_LOGW(TAG, "[CALIBRATION][%s] |   %c   |  %6u  | %6u  |  %6u  | %6u  |", cs, 'A' + phase,
               this->config_gain_phase_[phase].voltage_gain, this->gain_phase_[phase].voltage_gain,
               this->config_gain_phase_[phase].current_gain, this->gain_phase_[phase].current_gain);
    }
    ESP_LOGW(TAG,
             "[CALIBRATION][%s] ===============================================================================", cs);
  }
  if (!this->enable_offset_calibration_) {
    ESP_LOGI(TAG, "[CALIBRATION][%s] Power & Voltage/Current offset calibration is disabled. Using config file values.",
             cs);
  } else if (this->restored_offset_calibration_ && !offset_mismatch) {
    ESP_LOGI(TAG, "[CALIBRATION][%s] ", cs);
    ESP_LOGI(TAG, "[CALIBRATION][%s] ============== Restored offset calibration from memory ==============", cs);
    ESP_LOGI(TAG, "[CALIBRATION][%s] --------------------------------------------------------------", cs);
    ESP_LOGI(TAG, "[CALIBRATION][%s] | Phase | offset_voltage | offset_current |", cs);
    ESP_LOGI(TAG, "[CALIBRATION][%s] --------------------------------------------------------------", cs);
    for (uint8_t phase = 0; phase < 3; phase++) {
      ESP_LOGI(TAG, "[CALIBRATION][%s] |   %c   |     %6d      |     %6d      |", cs, 'A' + phase,
               this->offset_phase_[phase].first_offset, this->offset_phase_[phase].second_offset);
    }
    ESP_LOGI(TAG, "[CALIBRATION][%s] ==============================================================\\n", cs);
  }

  if (this->restored_power_offset_calibration_ && !power_mismatch) {
    ESP_LOGI(TAG, "[CALIBRATION][%s] ", cs);
    ESP_LOGI(TAG, "[CALIBRATION][%s] ============ Restored power offset calibration from memory ============", cs);
    ESP_LOGI(TAG, "[CALIBRATION][%s] ---------------------------------------------------------------------", cs);
    ESP_LOGI(TAG, "[CALIBRATION][%s] | Phase | offset_active_power | offset_reactive_power |", cs);
    ESP_LOGI(TAG, "[CALIBRATION][%s] ---------------------------------------------------------------------", cs);
    for (uint8_t phase = 0; phase < 3; phase++) {
      ESP_LOGI(TAG, "[CALIBRATION][%s] |   %c   |       %6d        |        %6d        |", cs, 'A' + phase,
               this->power_offset_phase_[phase].first_offset, this->power_offset_phase_[phase].second_offset);
    }
    ESP_LOGI(TAG, "[CALIBRATION][%s] =====================================================================\n", cs);
  }
  if (!this->enable_gain_calibration_) {
    ESP_LOGI(TAG, "[CALIBRATION][%s] Gain calibration is disabled. Using config file values.", cs);
  } else if (this->restored_gain_calibration_ && !gain_mismatch) {
    ESP_LOGI(TAG, "[CALIBRATION][%s] ", cs);
    ESP_LOGI(TAG, "[CALIBRATION][%s] ============ Restoring saved gain calibrations to registers ============", cs);
    ESP_LOGI(TAG, "[CALIBRATION][%s] ---------------------------------------------------------------------", cs);
    ESP_LOGI(TAG, "[CALIBRATION][%s] | Phase | voltage_gain | current_gain |", cs);
    ESP_LOGI(TAG, "[CALIBRATION][%s] ---------------------------------------------------------------------", cs);
    for (uint8_t phase = 0; phase < 3; phase++) {
      ESP_LOGI(TAG, "[CALIBRATION][%s] |   %c   |    %6u    |    %6u    |", cs, 'A' + phase,
               this->gain_phase_[phase].voltage_gain, this->gain_phase_[phase].current_gain);
    }
    ESP_LOGI(TAG, "[CALIBRATION][%s] =====================================================================\\n", cs);
    ESP_LOGI(TAG, "[CALIBRATION][%s] Gain calibration loaded and verified successfully.\n", cs);
  }
  this->calibration_message_printed_ = true;
}

void ATM90E32Component::dump_config() {
  ESP_LOGCONFIG("", "ATM90E32:");
  LOG_PIN("  CS Pin: ", this->cs_);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Voltage A", this->phase_[PHASEA].voltage_sensor_);
  LOG_SENSOR("  ", "Current A", this->phase_[PHASEA].current_sensor_);
  LOG_SENSOR("  ", "Power A", this->phase_[PHASEA].power_sensor_);
  LOG_SENSOR("  ", "Reactive Power A", this->phase_[PHASEA].reactive_power_sensor_);
  LOG_SENSOR("  ", "Apparent Power A", this->phase_[PHASEA].apparent_power_sensor_);
  LOG_SENSOR("  ", "PF A", this->phase_[PHASEA].power_factor_sensor_);
  LOG_SENSOR("  ", "Active Forward Energy A", this->phase_[PHASEA].forward_active_energy_sensor_);
  LOG_SENSOR("  ", "Active Reverse Energy A", this->phase_[PHASEA].reverse_active_energy_sensor_);
  LOG_SENSOR("  ", "Harmonic Power A", this->phase_[PHASEA].harmonic_active_power_sensor_);
  LOG_SENSOR("  ", "Phase Angle A", this->phase_[PHASEA].phase_angle_sensor_);
  LOG_SENSOR("  ", "Peak Current A", this->phase_[PHASEA].peak_current_sensor_);
  LOG_SENSOR("  ", "Voltage B", this->phase_[PHASEB].voltage_sensor_);
  LOG_SENSOR("  ", "Current B", this->phase_[PHASEB].current_sensor_);
  LOG_SENSOR("  ", "Power B", this->phase_[PHASEB].power_sensor_);
  LOG_SENSOR("  ", "Reactive Power B", this->phase_[PHASEB].reactive_power_sensor_);
  LOG_SENSOR("  ", "Apparent Power B", this->phase_[PHASEB].apparent_power_sensor_);
  LOG_SENSOR("  ", "PF B", this->phase_[PHASEB].power_factor_sensor_);
  LOG_SENSOR("  ", "Active Forward Energy B", this->phase_[PHASEB].forward_active_energy_sensor_);
  LOG_SENSOR("  ", "Active Reverse Energy B", this->phase_[PHASEB].reverse_active_energy_sensor_);
  LOG_SENSOR("  ", "Harmonic Power B", this->phase_[PHASEB].harmonic_active_power_sensor_);
  LOG_SENSOR("  ", "Phase Angle B", this->phase_[PHASEB].phase_angle_sensor_);
  LOG_SENSOR("  ", "Peak Current B", this->phase_[PHASEB].peak_current_sensor_);
  LOG_SENSOR("  ", "Voltage C", this->phase_[PHASEC].voltage_sensor_);
  LOG_SENSOR("  ", "Current C", this->phase_[PHASEC].current_sensor_);
  LOG_SENSOR("  ", "Power C", this->phase_[PHASEC].power_sensor_);
  LOG_SENSOR("  ", "Reactive Power C", this->phase_[PHASEC].reactive_power_sensor_);
  LOG_SENSOR("  ", "Apparent Power C", this->phase_[PHASEC].apparent_power_sensor_);
  LOG_SENSOR("  ", "PF C", this->phase_[PHASEC].power_factor_sensor_);
  LOG_SENSOR("  ", "Active Forward Energy C", this->phase_[PHASEC].forward_active_energy_sensor_);
  LOG_SENSOR("  ", "Active Reverse Energy C", this->phase_[PHASEC].reverse_active_energy_sensor_);
  LOG_SENSOR("  ", "Harmonic Power C", this->phase_[PHASEC].harmonic_active_power_sensor_);
  LOG_SENSOR("  ", "Phase Angle C", this->phase_[PHASEC].phase_angle_sensor_);
  LOG_SENSOR("  ", "Peak Current C", this->phase_[PHASEC].peak_current_sensor_);
  LOG_SENSOR("  ", "Frequency", this->freq_sensor_);
  LOG_SENSOR("  ", "Chip Temp", this->chip_temperature_sensor_);
  if (this->restored_offset_calibration_ || this->restored_power_offset_calibration_ ||
      this->restored_gain_calibration_ || !this->enable_offset_calibration_ || !this->enable_gain_calibration_) {
    this->log_calibration_status_();
  }
}

float ATM90E32Component::get_setup_priority() const { return setup_priority::IO; }

// R/C registers can conly be cleared after the LastSPIData register is updated (register 78H)
// Peakdetect period: 05H. Bit 15:8 are PeakDet_period in ms. 7:0 are Sag_period
// Default is 143FH (20ms, 63ms)
uint16_t ATM90E32Component::read16_(uint16_t a_register) {
  this->enable();
  delay_microseconds_safe(1);  // min delay between CS low and first SCK is 200ns - 1us is plenty
  uint8_t addrh = (1 << 7) | ((a_register >> 8) & 0x03);
  uint8_t addrl = (a_register & 0xFF);
  uint8_t data[4] = {addrh, addrl, 0x00, 0x00};
  this->transfer_array(data, 4);
  uint16_t output = encode_uint16(data[2], data[3]);
  ESP_LOGVV(TAG, "read16_ 0x%04" PRIX16 " output 0x%04" PRIX16, a_register, output);
  delay_microseconds_safe(1);  // allow the last clock to propagate before releasing CS
  this->disable();
  delay_microseconds_safe(1);  // meet minimum CS high time before next transaction
  return output;
}

int ATM90E32Component::read32_(uint16_t addr_h, uint16_t addr_l) {
  const uint16_t val_h = this->read16_(addr_h);
  const uint16_t val_l = this->read16_(addr_l);
  const int32_t val = (val_h << 16) | val_l;

  ESP_LOGVV(TAG,
            "read32_ addr_h 0x%04" PRIX16 " val_h 0x%04" PRIX16 " addr_l 0x%04" PRIX16 " val_l 0x%04" PRIX16
            " = %" PRId32,
            addr_h, val_h, addr_l, val_l, val);

  return val;
}

void ATM90E32Component::write16_(uint16_t a_register, uint16_t val, bool validate) {
  ESP_LOGVV(TAG, "write16_ 0x%04" PRIX16 " val 0x%04" PRIX16, a_register, val);
  uint8_t addrh = ((a_register >> 8) & 0x03);
  uint8_t addrl = (a_register & 0xFF);
  uint8_t data[4] = {addrh, addrl, uint8_t((val >> 8) & 0xFF), uint8_t(val & 0xFF)};
  this->enable();
  delay_microseconds_safe(1);  // ensure CS setup time
  this->write_array(data, 4);
  delay_microseconds_safe(1);  // allow clock to settle before raising CS
  this->disable();
  delay_microseconds_safe(1);  // ensure minimum CS high time
  if (validate)
    this->validate_spi_read_(val, "write16()");
}

float ATM90E32Component::get_local_phase_voltage_(uint8_t phase) { return this->phase_[phase].voltage_; }

float ATM90E32Component::get_local_phase_current_(uint8_t phase) { return this->phase_[phase].current_; }

float ATM90E32Component::get_local_phase_active_power_(uint8_t phase) { return this->phase_[phase].active_power_; }

float ATM90E32Component::get_local_phase_reactive_power_(uint8_t phase) { return this->phase_[phase].reactive_power_; }

float ATM90E32Component::get_local_phase_apparent_power_(uint8_t phase) { return this->phase_[phase].apparent_power_; }

float ATM90E32Component::get_local_phase_power_factor_(uint8_t phase) { return this->phase_[phase].power_factor_; }

float ATM90E32Component::get_local_phase_forward_active_energy_(uint8_t phase) {
  return this->phase_[phase].forward_active_energy_;
}

float ATM90E32Component::get_local_phase_reverse_active_energy_(uint8_t phase) {
  return this->phase_[phase].reverse_active_energy_;
}

float ATM90E32Component::get_local_phase_angle_(uint8_t phase) { return this->phase_[phase].phase_angle_; }

float ATM90E32Component::get_local_phase_harmonic_active_power_(uint8_t phase) {
  return this->phase_[phase].harmonic_active_power_;
}

float ATM90E32Component::get_local_phase_peak_current_(uint8_t phase) { return this->phase_[phase].peak_current_; }

float ATM90E32Component::get_phase_voltage_(uint8_t phase) {
  const uint16_t voltage = this->read16_(ATM90E32_REGISTER_URMS + phase);
  this->validate_spi_read_(voltage, "get_phase_voltage()");
  return (float) voltage / 100;
}

float ATM90E32Component::get_phase_voltage_avg_(uint8_t phase) {
  const uint8_t reads = 10;
  uint32_t accumulation = 0;
  uint16_t voltage = 0;
  for (uint8_t i = 0; i < reads; i++) {
    voltage = this->read16_(ATM90E32_REGISTER_URMS + phase);
    this->validate_spi_read_(voltage, "get_phase_voltage_avg_()");
    accumulation += voltage;
  }
  voltage = accumulation / reads;
  this->phase_[phase].voltage_ = (float) voltage / 100;
  return this->phase_[phase].voltage_;
}

float ATM90E32Component::get_phase_current_avg_(uint8_t phase) {
  const uint8_t reads = 10;
  uint32_t accumulation = 0;
  uint16_t current = 0;
  for (uint8_t i = 0; i < reads; i++) {
    current = this->read16_(ATM90E32_REGISTER_IRMS + phase);
    this->validate_spi_read_(current, "get_phase_current_avg_()");
    accumulation += current;
  }
  current = accumulation / reads;
  this->phase_[phase].current_ = (float) current / 1000;
  return this->phase_[phase].current_;
}

float ATM90E32Component::get_phase_current_(uint8_t phase) {
  const uint16_t current = this->read16_(ATM90E32_REGISTER_IRMS + phase);
  this->validate_spi_read_(current, "get_phase_current_()");
  return (float) current / 1000;
}

float ATM90E32Component::get_phase_active_power_(uint8_t phase) {
  const int val = this->read32_(ATM90E32_REGISTER_PMEAN + phase, ATM90E32_REGISTER_PMEANLSB + phase);
  return val * 0.00032f;
}

float ATM90E32Component::get_phase_reactive_power_(uint8_t phase) {
  const int val = this->read32_(ATM90E32_REGISTER_QMEAN + phase, ATM90E32_REGISTER_QMEANLSB + phase);
  return val * 0.00032f;
}

float ATM90E32Component::get_phase_apparent_power_(uint8_t phase) {
  const int val = this->read32_(ATM90E32_REGISTER_SMEAN + phase, ATM90E32_REGISTER_SMEANLSB + phase);
  return val * 0.00032f;
}

float ATM90E32Component::get_phase_power_factor_(uint8_t phase) {
  uint16_t powerfactor = this->read16_(ATM90E32_REGISTER_PFMEAN + phase);  // unsigned to compare to lastspidata
  this->validate_spi_read_(powerfactor, "get_phase_power_factor_()");
  return (float) ((int16_t) powerfactor) / 1000;  // make it signed again
}

float ATM90E32Component::get_phase_forward_active_energy_(uint8_t phase) {
  const uint16_t val = this->read16_(ATM90E32_REGISTER_APENERGY + phase);
  if ((UINT32_MAX - this->phase_[phase].cumulative_forward_active_energy_) > val) {
    this->phase_[phase].cumulative_forward_active_energy_ += val;
  } else {
    this->phase_[phase].cumulative_forward_active_energy_ = val;
  }
  // 0.01CF resolution = 0.003125 Wh per count
  return ((float) this->phase_[phase].cumulative_forward_active_energy_ * (10.0f / 3200.0f));
}

float ATM90E32Component::get_phase_reverse_active_energy_(uint8_t phase) {
  const uint16_t val = this->read16_(ATM90E32_REGISTER_ANENERGY + phase);
  if (UINT32_MAX - this->phase_[phase].cumulative_reverse_active_energy_ > val) {
    this->phase_[phase].cumulative_reverse_active_energy_ += val;
  } else {
    this->phase_[phase].cumulative_reverse_active_energy_ = val;
  }
  // 0.01CF resolution = 0.003125 Wh per count
  return ((float) this->phase_[phase].cumulative_reverse_active_energy_ * (10.0f / 3200.0f));
}

float ATM90E32Component::get_phase_harmonic_active_power_(uint8_t phase) {
  int val = this->read32_(ATM90E32_REGISTER_PMEANH + phase, ATM90E32_REGISTER_PMEANHLSB + phase);
  return val * 0.00032f;
}

float ATM90E32Component::get_phase_angle_(uint8_t phase) {
  float val = this->read16_(ATM90E32_REGISTER_PANGLE + phase) / 10.0f;
  return (val > 180.0f) ? val - 360.0f : val;
}

float ATM90E32Component::get_phase_peak_current_(uint8_t phase) {
  int16_t val = (float) this->read16_(ATM90E32_REGISTER_IPEAK + phase);
  if (!this->peak_current_signed_)
    val = std::abs(val);
  // phase register * phase current gain value  / 1000 * 2^13
  return (val * this->phase_[phase].ct_gain_ / 8192000.0);
}

float ATM90E32Component::get_frequency_() {
  const uint16_t freq = this->read16_(ATM90E32_REGISTER_FREQ);
  return (float) freq / 100;
}

float ATM90E32Component::get_chip_temperature_() {
  const uint16_t ctemp = this->read16_(ATM90E32_REGISTER_TEMP);
  return (float) ctemp;
}

void ATM90E32Component::run_gain_calibrations() {
  const char *cs = this->get_calibration_id_();
  if (!this->enable_gain_calibration_) {
    ESP_LOGW(TAG, "[CALIBRATION][%s] Gain calibration is disabled! Enable it first with enable_gain_calibration: true",
             cs);
    return;
  }

  float ref_voltages[3] = {
      this->get_reference_voltage(0),
      this->get_reference_voltage(1),
      this->get_reference_voltage(2),
  };
  float ref_currents[3] = {this->get_reference_current(0), this->get_reference_current(1),
                           this->get_reference_current(2)};

  ESP_LOGI(TAG, "[CALIBRATION][%s] ", cs);
  ESP_LOGI(TAG, "[CALIBRATION][%s] ========================= Gain Calibration  =========================", cs);
  ESP_LOGI(TAG, "[CALIBRATION][%s] ---------------------------------------------------------------------", cs);
  ESP_LOGI(
      TAG,
      "[CALIBRATION][%s] | Phase | V_meas (V) | I_meas (A) | V_ref | I_ref  | V_gain (old→new) | I_gain (old→new) |",
      cs);
  ESP_LOGI(TAG, "[CALIBRATION][%s] ---------------------------------------------------------------------", cs);

  GainCalibration previous_gains[3] = {this->gain_phase_[0], this->gain_phase_[1], this->gain_phase_[2]};
  const bool previous_restored = this->restored_gain_calibration_;
  const bool previous_using_saved = this->using_saved_calibrations_;

  for (uint8_t phase = 0; phase < 3; phase++) {
    float measured_voltage = this->get_phase_voltage_avg_(phase);
    float measured_current = this->get_phase_current_avg_(phase);

    float ref_voltage = ref_voltages[phase];
    float ref_current = ref_currents[phase];

    uint16_t current_voltage_gain = this->read16_(voltage_gain_registers[phase]);
    uint16_t current_current_gain = this->read16_(current_gain_registers[phase]);

    bool did_voltage = false;
    bool did_current = false;

    // Voltage calibration
    if (ref_voltage <= 0.0f) {
      ESP_LOGW(TAG, "[CALIBRATION][%s] Phase %s - Skipping voltage calibration: reference voltage is 0.", cs,
               phase_labels[phase]);
    } else if (measured_voltage == 0.0f) {
      ESP_LOGW(TAG, "[CALIBRATION][%s] Phase %s - Skipping voltage calibration: measured voltage is 0.", cs,
               phase_labels[phase]);
    } else {
      uint32_t new_voltage_gain = static_cast<uint32_t>((ref_voltage / measured_voltage) * current_voltage_gain);
      if (new_voltage_gain == 0) {
        ESP_LOGW(TAG, "[CALIBRATION][%s] Phase %s - Voltage gain would be 0. Check reference and measured voltage.", cs,
                 phase_labels[phase]);
      } else {
        if (new_voltage_gain >= 65535) {
          ESP_LOGW(TAG,
                   "[CALIBRATION][%s] Phase %s - Voltage gain exceeds 65535. You may need a higher output voltage "
                   "transformer.",
                   cs, phase_labels[phase]);
          new_voltage_gain = 65535;
        }
        this->gain_phase_[phase].voltage_gain = static_cast<uint16_t>(new_voltage_gain);
        did_voltage = true;
      }
    }

    // Current calibration
    if (ref_current == 0.0f) {
      ESP_LOGW(TAG, "[CALIBRATION][%s] Phase %s - Skipping current calibration: reference current is 0.", cs,
               phase_labels[phase]);
    } else if (measured_current == 0.0f) {
      ESP_LOGW(TAG, "[CALIBRATION][%s] Phase %s - Skipping current calibration: measured current is 0.", cs,
               phase_labels[phase]);
    } else {
      uint32_t new_current_gain = static_cast<uint32_t>((ref_current / measured_current) * current_current_gain);
      if (new_current_gain == 0) {
        ESP_LOGW(TAG, "[CALIBRATION][%s] Phase %s - Current gain would be 0. Check reference and measured current.", cs,
                 phase_labels[phase]);
      } else {
        if (new_current_gain >= 65535) {
          ESP_LOGW(TAG, "[CALIBRATION][%s] Phase %s - Current gain exceeds 65535. You may need to turn up pga gain.",
                   cs, phase_labels[phase]);
          new_current_gain = 65535;
        }
        this->gain_phase_[phase].current_gain = static_cast<uint16_t>(new_current_gain);
        did_current = true;
      }
    }

    // Final row output
    ESP_LOGI(TAG, "[CALIBRATION][%s] |   %c   |  %9.2f |  %9.4f | %5.2f | %6.4f |  %5u → %-5u  |  %5u → %-5u  |", cs,
             'A' + phase, measured_voltage, measured_current, ref_voltage, ref_current, current_voltage_gain,
             did_voltage ? this->gain_phase_[phase].voltage_gain : current_voltage_gain, current_current_gain,
             did_current ? this->gain_phase_[phase].current_gain : current_current_gain);
  }

  ESP_LOGI(TAG, "[CALIBRATION][%s] =====================================================================\n", cs);

  this->write_gains_to_registers_();
  this->finish_gain_calibration_(previous_gains, previous_restored, previous_using_saved);
}

void ATM90E32Component::finish_gain_calibration_(const GainCalibration (&previous)[3], bool previous_restored,
                                                 bool previous_using_saved) {
  const char *cs = this->get_calibration_id_();
  const bool writes_verified = this->verify_gain_writes_();
  bool saved = false;
  bool synced = false;
  if (writes_verified) {
    saved = this->gain_calibration_pref_.save(&this->gain_phase_);
    synced = global_preferences->sync();
  }

  if (writes_verified && saved && synced) {
    this->using_saved_calibrations_ = true;
    this->has_stored_gain_calibration_ = true;
    this->restored_gain_calibration_ = true;
    for (bool &phase : this->gain_calibration_mismatch_)
      phase = false;
    ESP_LOGI(TAG, "[CALIBRATION][%s] Gain calibration saved to memory.", cs);
    ESP_LOGI(TAG, "[CALIBRATION][%s] Gain calibration completed and verified.", cs);
    return;
  }

  if (writes_verified)
    ESP_LOGE(TAG, "[CALIBRATION][%s] Failed to save gain calibration to memory!", cs);

  for (uint8_t phase = 0; phase < 3; phase++)
    this->gain_phase_[phase] = previous[phase];
  this->write_gains_to_registers_();
  const bool rollback_verified = this->verify_gain_writes_();

  bool rollback_persisted = false;
  if (writes_verified) {
    GainCalibration rollback[3]{};
    prepare_calibration_rollback(previous, previous_restored, GainCalibration{0, 0}, rollback);
    const bool rollback_saved = this->gain_calibration_pref_.save(&rollback);
    const bool rollback_synced = global_preferences->sync();
    rollback_persisted = rollback_saved && rollback_synced;
    if (!rollback_persisted)
      ESP_LOGE(TAG, "[CALIBRATION][%s] Failed to persist restored gain calibration values!", cs);
  }

  this->restored_gain_calibration_ = previous_restored;
  if (rollback_persisted)
    this->has_stored_gain_calibration_ = previous_restored;
  this->using_saved_calibrations_ = previous_using_saved;
  if (!rollback_verified) {
    ESP_LOGE(TAG, "[CALIBRATION][%s] Gain calibration failed; rollback readback verification failed.", cs);
    return;
  }
  ESP_LOGE(TAG, "[CALIBRATION][%s] Gain calibration failed; previous values restored.", cs);
}

void ATM90E32Component::finish_offset_calibration_(const OffsetCalibration (&previous)[3], bool previous_restored,
                                                   bool previous_using_saved, OffsetCalibrationType type) {
  const bool power_offsets = type == OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_POWER;
  const char *cs = this->get_calibration_id_();
  const LogString *name = offset_calibration_name(power_offsets);
  OffsetCalibration(*offsets)[3] = power_offsets ? &this->power_offset_phase_ : &this->offset_phase_;
  ESPPreferenceObject *preference = power_offsets ? &this->power_offset_pref_ : &this->offset_pref_;
  bool *has_stored =
      power_offsets ? &this->has_stored_power_offset_calibration_ : &this->has_stored_offset_calibration_;
  bool *restored = power_offsets ? &this->restored_power_offset_calibration_ : &this->restored_offset_calibration_;
  bool *mismatches = power_offsets ? this->power_offset_calibration_mismatch_ : this->offset_calibration_mismatch_;

  const bool writes_verified = this->verify_offset_writes_(type);
  bool saved = false;
  bool synced = false;
  if (writes_verified) {
    saved = preference->save(offsets);
    synced = global_preferences->sync();
  }

  if (writes_verified && saved && synced) {
    this->using_saved_calibrations_ = true;
    *has_stored = true;
    *restored = true;
    for (uint8_t phase = 0; phase < 3; phase++)
      mismatches[phase] = false;
    ESP_LOGI(TAG, "[CALIBRATION][%s] %s calibration saved to memory. %s calibration completed and verified.", cs,
             LOG_STR_ARG(name), LOG_STR_ARG(name));
    return;
  }

  if (writes_verified)
    ESP_LOGE(TAG, "[CALIBRATION][%s] Failed to save %s calibration to memory!", cs, LOG_STR_ARG(name));

  for (uint8_t phase = 0; phase < 3; phase++) {
    this->write_offsets_to_registers_(phase, previous[phase].first_offset, previous[phase].second_offset, type);
  }
  const bool rollback_verified = this->verify_offset_writes_(type);

  bool rollback_persisted = false;
  if (writes_verified) {
    OffsetCalibration rollback[3]{};
    prepare_calibration_rollback(previous, previous_restored, OffsetCalibration{}, rollback);
    const bool rollback_saved = preference->save(&rollback);
    const bool rollback_synced = global_preferences->sync();
    rollback_persisted = rollback_saved && rollback_synced;
    if (!rollback_saved || !rollback_synced)
      ESP_LOGE(TAG, "[CALIBRATION][%s] Failed to persist restored %s calibration values!", cs, LOG_STR_ARG(name));
  }

  *restored = previous_restored;
  if (rollback_persisted)
    *has_stored = previous_restored;
  this->using_saved_calibrations_ = previous_using_saved;
  if (!rollback_verified) {
    ESP_LOGE(TAG, "[CALIBRATION][%s] %s calibration failed; rollback readback verification failed.", cs,
             LOG_STR_ARG(name));
    return;
  }
  ESP_LOGE(TAG, "[CALIBRATION][%s] %s calibration failed; previous values restored.", cs, LOG_STR_ARG(name));
}

void ATM90E32Component::run_offset_calibrations() {
  const char *cs = this->get_calibration_id_();
  if (!this->enable_offset_calibration_) {
    ESP_LOGW(TAG,
             "[CALIBRATION][%s] Offset calibration is disabled! Enable it first with enable_offset_calibration: true",
             cs);
    return;
  }

  ESP_LOGI(TAG, "[CALIBRATION][%s] ", cs);
  ESP_LOGI(TAG, "[CALIBRATION][%s] ======================== Offset Calibration ========================", cs);
  ESP_LOGI(TAG, "[CALIBRATION][%s] ------------------------------------------------------------------", cs);
  ESP_LOGI(TAG, "[CALIBRATION][%s] | Phase | offset_voltage | offset_current |", cs);
  ESP_LOGI(TAG, "[CALIBRATION][%s] ------------------------------------------------------------------", cs);

  OffsetCalibration previous_offsets[3] = {this->offset_phase_[0], this->offset_phase_[1], this->offset_phase_[2]};
  const bool previous_restored = this->restored_offset_calibration_;
  const bool previous_using_saved = this->using_saved_calibrations_;

  for (uint8_t phase = 0; phase < 3; phase++) {
    int16_t voltage_offset = calibrate_offset(phase, true);
    int16_t current_offset = calibrate_offset(phase, false);

    this->write_offsets_to_registers_(phase, voltage_offset, current_offset,
                                      OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_VOLTAGE_CURRENT);

    ESP_LOGI(TAG, "[CALIBRATION][%s] |   %c   |     %6d      |     %6d      |", cs, 'A' + phase, voltage_offset,
             current_offset);
  }

  ESP_LOGI(TAG, "[CALIBRATION][%s] ==================================================================\n", cs);

  this->finish_offset_calibration_(previous_offsets, previous_restored, previous_using_saved,
                                   OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_VOLTAGE_CURRENT);
}

void ATM90E32Component::run_power_offset_calibrations() {
  const char *cs = this->get_calibration_id_();
  if (!this->enable_offset_calibration_) {
    ESP_LOGW(
        TAG,
        "[CALIBRATION][%s] Offset power calibration is disabled! Enable it first with enable_offset_calibration: true",
        cs);
    return;
  }

  ESP_LOGI(TAG, "[CALIBRATION][%s] ", cs);
  ESP_LOGI(TAG, "[CALIBRATION][%s] ===================== Power Offset Calibration =====================", cs);
  ESP_LOGI(TAG, "[CALIBRATION][%s] ---------------------------------------------------------------------", cs);
  ESP_LOGI(TAG, "[CALIBRATION][%s] | Phase | offset_active_power | offset_reactive_power |", cs);
  ESP_LOGI(TAG, "[CALIBRATION][%s] ---------------------------------------------------------------------", cs);

  OffsetCalibration previous_offsets[3] = {this->power_offset_phase_[0], this->power_offset_phase_[1],
                                           this->power_offset_phase_[2]};
  const bool previous_restored = this->restored_power_offset_calibration_;
  const bool previous_using_saved = this->using_saved_calibrations_;

  for (uint8_t phase = 0; phase < 3; ++phase) {
    int16_t active_offset = calibrate_power_offset(phase, false);
    int16_t reactive_offset = calibrate_power_offset(phase, true);

    this->write_offsets_to_registers_(phase, active_offset, reactive_offset,
                                      OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_POWER);

    ESP_LOGI(TAG, "[CALIBRATION][%s] |   %c   |       %6d        |        %6d        |", cs, 'A' + phase, active_offset,
             reactive_offset);
  }
  ESP_LOGI(TAG, "[CALIBRATION][%s] =====================================================================\n", cs);

  this->finish_offset_calibration_(previous_offsets, previous_restored, previous_using_saved,
                                   OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_POWER);
}

void ATM90E32Component::write_gains_to_registers_() {
  this->write16_(ATM90E32_REGISTER_CFGREGACCEN, 0x55AA);

  for (int phase = 0; phase < 3; phase++) {
    this->write16_(voltage_gain_registers[phase], this->gain_phase_[phase].voltage_gain);
    this->write16_(current_gain_registers[phase], this->gain_phase_[phase].current_gain);
  }

  this->write16_(ATM90E32_REGISTER_CFGREGACCEN, 0x0000);
}

void ATM90E32Component::write_offsets_to_registers_(uint8_t phase, int16_t first_offset, int16_t second_offset,
                                                    OffsetCalibrationType type) {
  const bool power_offsets = type == OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_POWER;
  OffsetCalibration &offsets = power_offsets ? this->power_offset_phase_[phase] : this->offset_phase_[phase];
  offsets.first_offset = first_offset;
  offsets.second_offset = second_offset;
  if (power_offsets) {
    this->phase_[phase].active_power_offset_ = first_offset;
    this->phase_[phase].reactive_power_offset_ = second_offset;
  } else {
    this->phase_[phase].voltage_offset_ = first_offset;
    this->phase_[phase].current_offset_ = second_offset;
  }

  const uint16_t *first_registers = power_offsets ? this->power_offset_registers : this->voltage_offset_registers;
  const uint16_t *second_registers =
      power_offsets ? this->reactive_power_offset_registers : this->current_offset_registers;
  this->write16_(ATM90E32_REGISTER_CFGREGACCEN, 0x55AA);
  this->write16_(first_registers[phase], static_cast<uint16_t>(first_offset));
  this->write16_(second_registers[phase], static_cast<uint16_t>(second_offset));
  this->write16_(ATM90E32_REGISTER_CFGREGACCEN, 0x0000);
}

void ATM90E32Component::restore_gain_calibrations_() {
  const char *cs = this->get_calibration_id_();
  for (uint8_t i = 0; i < 3; ++i) {
    this->config_gain_phase_[i].voltage_gain = this->phase_[i].voltage_gain_;
    this->config_gain_phase_[i].current_gain = this->phase_[i].ct_gain_;
    this->gain_phase_[i] = this->config_gain_phase_[i];
  }

  const bool have_data = this->gain_calibration_pref_.load(&this->gain_phase_);
  bool all_zero = true;
  bool same_as_config = true;
  if (have_data) {
    for (uint8_t phase = 0; phase < 3; ++phase) {
      const auto &config = this->config_gain_phase_[phase];
      const auto &saved = this->gain_phase_[phase];
      if (saved.voltage_gain != 0 || saved.current_gain != 0)
        all_zero = false;
      if (saved.voltage_gain != config.voltage_gain || saved.current_gain != config.current_gain)
        same_as_config = false;
    }
  }

  this->has_stored_gain_calibration_ = have_data && !all_zero && !same_as_config;
  this->restored_gain_calibration_ = false;
  for (uint8_t phase = 0; phase < 3; ++phase) {
    this->gain_calibration_mismatch_[phase] = false;
    if (this->has_stored_gain_calibration_) {
      this->gain_calibration_mismatch_[phase] =
          (this->has_config_voltage_gain_[phase] &&
           this->gain_phase_[phase].voltage_gain != this->config_gain_phase_[phase].voltage_gain) ||
          (this->has_config_current_gain_[phase] &&
           this->gain_phase_[phase].current_gain != this->config_gain_phase_[phase].current_gain);
    }
  }

  if (!this->has_stored_gain_calibration_) {
    for (uint8_t phase = 0; phase < 3; ++phase)
      this->gain_phase_[phase] = this->config_gain_phase_[phase];
    ESP_LOGW(TAG, "[CALIBRATION][%s] No stored gain calibrations found. Using config file values.", cs);
  }

  this->write_gains_to_registers_();
  const bool initial_values_verified = this->verify_gain_writes_();
  if (initial_values_verified) {
    const auto state = resolve_calibration_restore_state(this->has_stored_gain_calibration_, true, false);
    this->restored_gain_calibration_ = state.restored;
    this->using_saved_calibrations_ = this->restored_offset_calibration_ || this->restored_power_offset_calibration_ ||
                                      this->restored_gain_calibration_;
    return;
  }

  for (uint8_t phase = 0; phase < 3; ++phase) {
    this->gain_calibration_mismatch_[phase] = false;
    this->gain_phase_[phase] = this->config_gain_phase_[phase];
  }
  this->write_gains_to_registers_();
  const auto state =
      resolve_calibration_restore_state(this->has_stored_gain_calibration_, false, this->verify_gain_writes_());
  this->restored_gain_calibration_ = state.restored;
  this->using_saved_calibrations_ = this->restored_offset_calibration_ || this->restored_power_offset_calibration_ ||
                                    this->restored_gain_calibration_;
  if (state.values_verified) {
    ESP_LOGE(TAG, "[CALIBRATION][%s] Gain calibration restore failed verification; config values verified.", cs);
  } else {
    ESP_LOGE(TAG, "[CALIBRATION][%s] Gain calibration restore failed; config readback verification failed.", cs);
  }
}

void ATM90E32Component::restore_offset_calibrations_(OffsetCalibrationType type) {
  const bool power_offsets = type == OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_POWER;
  const char *cs = this->get_calibration_id_();
  const LogString *name = power_offsets ? LOG_STR("power offset") : LOG_STR("offset");
  OffsetCalibration(*offsets)[3] = power_offsets ? &this->power_offset_phase_ : &this->offset_phase_;
  OffsetCalibration(*config_offsets)[3] =
      power_offsets ? &this->config_power_offset_phase_ : &this->config_offset_phase_;
  ESPPreferenceObject *preference = power_offsets ? &this->power_offset_pref_ : &this->offset_pref_;
  bool *has_stored =
      power_offsets ? &this->has_stored_power_offset_calibration_ : &this->has_stored_offset_calibration_;
  bool *restored = power_offsets ? &this->restored_power_offset_calibration_ : &this->restored_offset_calibration_;
  bool *mismatches = power_offsets ? this->power_offset_calibration_mismatch_ : this->offset_calibration_mismatch_;
  const bool *has_first = power_offsets ? this->has_config_active_power_offset_ : this->has_config_voltage_offset_;
  const bool *has_second = power_offsets ? this->has_config_reactive_power_offset_ : this->has_config_current_offset_;

  for (uint8_t i = 0; i < 3; ++i)
    (*config_offsets)[i] = (*offsets)[i];

  const bool have_data = preference->load(offsets);
  bool all_zero = true;
  if (have_data) {
    for (const auto &phase : *offsets) {
      if (phase.first_offset != 0 || phase.second_offset != 0) {
        all_zero = false;
        break;
      }
    }
  }

  *has_stored = have_data && !all_zero;
  *restored = false;
  for (uint8_t phase = 0; phase < 3; phase++) {
    mismatches[phase] = false;
    if (*has_stored) {
      mismatches[phase] =
          (has_first[phase] && (*offsets)[phase].first_offset != (*config_offsets)[phase].first_offset) ||
          (has_second[phase] && (*offsets)[phase].second_offset != (*config_offsets)[phase].second_offset);
    }
  }

  if (!*has_stored) {
    for (uint8_t phase = 0; phase < 3; phase++)
      (*offsets)[phase] = (*config_offsets)[phase];
    ESP_LOGW(TAG, "[CALIBRATION][%s] No stored %s calibrations found. Using default values.", cs, LOG_STR_ARG(name));
  }

  for (uint8_t phase = 0; phase < 3; phase++) {
    this->write_offsets_to_registers_(phase, (*offsets)[phase].first_offset, (*offsets)[phase].second_offset, type);
  }
  const bool initial_values_verified = this->verify_offset_writes_(type);
  if (initial_values_verified) {
    const auto state = resolve_calibration_restore_state(*has_stored, true, false);
    *restored = state.restored;
    ESP_LOGI(TAG, "[CALIBRATION][%s] %s calibration values verified.", cs, LOG_STR_ARG(name));
    return;
  }

  this->using_saved_calibrations_ = false;
  for (uint8_t phase = 0; phase < 3; phase++)
    mismatches[phase] = false;
  for (uint8_t phase = 0; phase < 3; phase++) {
    (*offsets)[phase] = (*config_offsets)[phase];
    this->write_offsets_to_registers_(phase, (*offsets)[phase].first_offset, (*offsets)[phase].second_offset, type);
  }
  const auto state = resolve_calibration_restore_state(*has_stored, false, this->verify_offset_writes_(type));
  *restored = state.restored;
  if (state.values_verified) {
    ESP_LOGE(TAG, "[CALIBRATION][%s] %s calibration restore failed verification; config values verified.", cs,
             LOG_STR_ARG(name));
  } else {
    ESP_LOGE(TAG, "[CALIBRATION][%s] %s calibration restore and config fallback both failed verification.", cs,
             LOG_STR_ARG(name));
  }
}

void ATM90E32Component::clear_gain_calibrations() {
  const char *cs = this->get_calibration_id_();
  if (!this->has_stored_gain_calibration_) {
    ESP_LOGI(TAG, "[CALIBRATION][%s] No stored gain calibrations to clear. Current values:", cs);
    ESP_LOGI(TAG, "[CALIBRATION][%s] ----------------------------------------------------------", cs);
    ESP_LOGI(TAG, "[CALIBRATION][%s] | Phase | voltage_gain | current_gain |", cs);
    ESP_LOGI(TAG, "[CALIBRATION][%s] ----------------------------------------------------------", cs);
    for (int phase = 0; phase < 3; phase++) {
      ESP_LOGI(TAG, "[CALIBRATION][%s] |   %c   |    %6u    |    %6u    |", cs, 'A' + phase,
               this->gain_phase_[phase].voltage_gain, this->gain_phase_[phase].current_gain);
    }
    ESP_LOGI(TAG, "[CALIBRATION][%s] ==========================================================\n", cs);
    return;
  }

  ESP_LOGI(TAG, "[CALIBRATION][%s] Clearing stored gain calibrations and restoring config-defined values", cs);
  ESP_LOGI(TAG, "[CALIBRATION][%s] ----------------------------------------------------------", cs);
  ESP_LOGI(TAG, "[CALIBRATION][%s] | Phase | voltage_gain | current_gain |", cs);
  ESP_LOGI(TAG, "[CALIBRATION][%s] ----------------------------------------------------------", cs);

  for (int phase = 0; phase < 3; phase++) {
    uint16_t voltage_gain = this->phase_[phase].voltage_gain_;
    uint16_t current_gain = this->phase_[phase].ct_gain_;

    this->config_gain_phase_[phase].voltage_gain = voltage_gain;
    this->config_gain_phase_[phase].current_gain = current_gain;
    this->gain_phase_[phase].voltage_gain = voltage_gain;
    this->gain_phase_[phase].current_gain = current_gain;

    ESP_LOGI(TAG, "[CALIBRATION][%s] |   %c   |    %6u    |    %6u    |", cs, 'A' + phase, voltage_gain, current_gain);
  }
  ESP_LOGI(TAG, "[CALIBRATION][%s] ==========================================================\n", cs);

  GainCalibration zero_gains[3]{{0, 0}, {0, 0}, {0, 0}};
  const bool saved = this->gain_calibration_pref_.save(&zero_gains);
  const bool synced = global_preferences->sync();

  if (saved && synced)
    this->has_stored_gain_calibration_ = false;
  this->restored_gain_calibration_ = false;
  this->using_saved_calibrations_ = this->restored_offset_calibration_ || this->restored_power_offset_calibration_;
  for (bool &phase : this->gain_calibration_mismatch_)
    phase = false;

  if (!saved || !synced) {
    ESP_LOGE(TAG, "[CALIBRATION][%s] Failed to clear gain calibrations!", cs);
  }

  this->write_gains_to_registers_();  // Apply them to the chip immediately
}

void ATM90E32Component::clear_offset_calibrations() {
  const char *cs = this->get_calibration_id_();
  if (!this->has_stored_offset_calibration_) {
    ESP_LOGI(TAG, "[CALIBRATION][%s] No stored offset calibrations to clear. Current values:", cs);
    ESP_LOGI(TAG, "[CALIBRATION][%s] --------------------------------------------------------------", cs);
    ESP_LOGI(TAG, "[CALIBRATION][%s] | Phase | offset_voltage | offset_current |", cs);
    ESP_LOGI(TAG, "[CALIBRATION][%s] --------------------------------------------------------------", cs);
    for (uint8_t phase = 0; phase < 3; phase++) {
      ESP_LOGI(TAG, "[CALIBRATION][%s] |   %c   |     %6d      |     %6d      |", cs, 'A' + phase,
               this->offset_phase_[phase].first_offset, this->offset_phase_[phase].second_offset);
    }
    ESP_LOGI(TAG, "[CALIBRATION][%s] ==============================================================\n", cs);
    return;
  }

  ESP_LOGI(TAG, "[CALIBRATION][%s] Clearing stored offset calibrations and restoring config-defined values", cs);
  ESP_LOGI(TAG, "[CALIBRATION][%s] --------------------------------------------------------------", cs);
  ESP_LOGI(TAG, "[CALIBRATION][%s] | Phase | offset_voltage | offset_current |", cs);
  ESP_LOGI(TAG, "[CALIBRATION][%s] --------------------------------------------------------------", cs);

  for (uint8_t phase = 0; phase < 3; phase++) {
    int16_t voltage_offset =
        this->has_config_voltage_offset_[phase] ? this->config_offset_phase_[phase].first_offset : 0;
    int16_t current_offset =
        this->has_config_current_offset_[phase] ? this->config_offset_phase_[phase].second_offset : 0;
    this->write_offsets_to_registers_(phase, voltage_offset, current_offset,
                                      OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_VOLTAGE_CURRENT);
    ESP_LOGI(TAG, "[CALIBRATION][%s] |   %c   |     %6d      |     %6d      |", cs, 'A' + phase, voltage_offset,
             current_offset);
  }
  ESP_LOGI(TAG, "[CALIBRATION][%s] ==============================================================\n", cs);

  OffsetCalibration zero_offsets[3]{{0, 0}, {0, 0}, {0, 0}};
  this->offset_pref_.save(&zero_offsets);  // Clear stored values in flash
  global_preferences->sync();

  this->has_stored_offset_calibration_ = false;
  this->restored_offset_calibration_ = false;
  for (bool &phase : this->offset_calibration_mismatch_)
    phase = false;

  ESP_LOGI(TAG, "[CALIBRATION][%s] Offsets cleared.", cs);
}

void ATM90E32Component::clear_power_offset_calibrations() {
  const char *cs = this->get_calibration_id_();
  if (!this->has_stored_power_offset_calibration_) {
    ESP_LOGI(TAG, "[CALIBRATION][%s] No stored power offsets to clear. Current values:", cs);
    ESP_LOGI(TAG, "[CALIBRATION][%s] ---------------------------------------------------------------------", cs);
    ESP_LOGI(TAG, "[CALIBRATION][%s] | Phase | offset_active_power | offset_reactive_power |", cs);
    ESP_LOGI(TAG, "[CALIBRATION][%s] ---------------------------------------------------------------------", cs);
    for (uint8_t phase = 0; phase < 3; phase++) {
      ESP_LOGI(TAG, "[CALIBRATION][%s] |   %c   |       %6d        |        %6d        |", cs, 'A' + phase,
               this->power_offset_phase_[phase].first_offset, this->power_offset_phase_[phase].second_offset);
    }
    ESP_LOGI(TAG, "[CALIBRATION][%s] =====================================================================\n", cs);
    return;
  }

  ESP_LOGI(TAG, "[CALIBRATION][%s] Clearing stored power offsets and restoring config-defined values", cs);
  ESP_LOGI(TAG, "[CALIBRATION][%s] ---------------------------------------------------------------------", cs);
  ESP_LOGI(TAG, "[CALIBRATION][%s] | Phase | offset_active_power | offset_reactive_power |", cs);
  ESP_LOGI(TAG, "[CALIBRATION][%s] ---------------------------------------------------------------------", cs);

  for (uint8_t phase = 0; phase < 3; phase++) {
    int16_t active_offset =
        this->has_config_active_power_offset_[phase] ? this->config_power_offset_phase_[phase].first_offset : 0;
    int16_t reactive_offset =
        this->has_config_reactive_power_offset_[phase] ? this->config_power_offset_phase_[phase].second_offset : 0;
    this->write_offsets_to_registers_(phase, active_offset, reactive_offset,
                                      OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_POWER);
    ESP_LOGI(TAG, "[CALIBRATION][%s] |   %c   |       %6d        |        %6d        |", cs, 'A' + phase, active_offset,
             reactive_offset);
  }
  ESP_LOGI(TAG, "[CALIBRATION][%s] =====================================================================\n", cs);

  OffsetCalibration zero_power_offsets[3]{{0, 0}, {0, 0}, {0, 0}};
  this->power_offset_pref_.save(&zero_power_offsets);
  global_preferences->sync();

  this->has_stored_power_offset_calibration_ = false;
  this->restored_power_offset_calibration_ = false;
  for (bool &phase : this->power_offset_calibration_mismatch_)
    phase = false;

  ESP_LOGI(TAG, "[CALIBRATION][%s] Power offsets cleared.", cs);
}

int16_t ATM90E32Component::calibrate_offset(uint8_t phase, bool voltage) {
  const uint8_t num_reads = 5;
  uint64_t total_value = 0;

  for (uint8_t i = 0; i < num_reads; ++i) {
    uint32_t reading = voltage ? this->read32_(ATM90E32_REGISTER_URMS + phase, ATM90E32_REGISTER_URMSLSB + phase)
                               : this->read32_(ATM90E32_REGISTER_IRMS + phase, ATM90E32_REGISTER_IRMSLSB + phase);
    total_value += reading;
  }

  const uint32_t average_value = total_value / num_reads;
  const uint32_t shifted = average_value >> 7;
  const uint32_t offset = ~shifted + 1;
  return static_cast<int16_t>(offset);  // Takes lower 16 bits
}

int16_t ATM90E32Component::calibrate_power_offset(uint8_t phase, bool reactive) {
  const uint8_t num_reads = 5;
  int64_t total_value = 0;

  for (uint8_t i = 0; i < num_reads; ++i) {
    int32_t reading = reactive ? this->read32_(ATM90E32_REGISTER_QMEAN + phase, ATM90E32_REGISTER_QMEANLSB + phase)
                               : this->read32_(ATM90E32_REGISTER_PMEAN + phase, ATM90E32_REGISTER_PMEANLSB + phase);
    total_value += reading;
  }

  int32_t average_value = total_value / num_reads;
  int32_t power_offset = -average_value;
  return static_cast<int16_t>(power_offset);  // Takes the lower 16 bits
}

bool ATM90E32Component::verify_gain_writes_() {
  const char *cs = this->get_calibration_id_();
  bool success = true;
  for (uint8_t phase = 0; phase < 3; phase++) {
    uint16_t read_voltage = this->read16_(voltage_gain_registers[phase]);
    uint16_t read_current = this->read16_(current_gain_registers[phase]);

    if (read_voltage != this->gain_phase_[phase].voltage_gain ||
        read_current != this->gain_phase_[phase].current_gain) {
      ESP_LOGE(TAG, "[CALIBRATION][%s] Mismatch detected for Phase %s!", cs, phase_labels[phase]);
      success = false;
    }
  }
  return success;  // Return true if all writes were successful, false otherwise
}

bool ATM90E32Component::verify_offset_writes_(OffsetCalibrationType type) {
  const bool power_offsets = type == OffsetCalibrationType::OFFSET_CALIBRATION_TYPE_POWER;
  const char *cs = this->get_calibration_id_();
  const LogString *name = offset_calibration_name(power_offsets);
  const LogString *first_name = power_offsets ? LOG_STR("active") : LOG_STR("voltage");
  const LogString *second_name = power_offsets ? LOG_STR("reactive") : LOG_STR("current");
  const OffsetCalibration *offsets = power_offsets ? this->power_offset_phase_ : this->offset_phase_;
  const uint16_t *first_registers = power_offsets ? this->power_offset_registers : this->voltage_offset_registers;
  const uint16_t *second_registers =
      power_offsets ? this->reactive_power_offset_registers : this->current_offset_registers;
  bool success = true;
  for (uint8_t phase = 0; phase < 3; phase++) {
    const uint16_t first = this->read16_(first_registers[phase]);
    const uint16_t second = this->read16_(second_registers[phase]);
    if (!offset_register_value_matches(first, offsets[phase].first_offset) ||
        !offset_register_value_matches(second, offsets[phase].second_offset)) {
      ESP_LOGE(TAG, "[CALIBRATION][%s] %s readback failed for Phase %s: %s %d/%d, %s %d/%d.", cs, LOG_STR_ARG(name),
               phase_labels[phase], LOG_STR_ARG(first_name), static_cast<int16_t>(first), offsets[phase].first_offset,
               LOG_STR_ARG(second_name), static_cast<int16_t>(second), offsets[phase].second_offset);
      success = false;
    }
  }
  return success;
}

#ifdef USE_TEXT_SENSOR
void ATM90E32Component::check_phase_status() {
  uint16_t state0 = this->read16_(ATM90E32_REGISTER_EMMSTATE0);
  uint16_t state1 = this->read16_(ATM90E32_REGISTER_EMMSTATE1);

  for (int phase = 0; phase < 3; phase++) {
    std::string status;

    if (state0 & over_voltage_flags[phase])
      status += "Over Voltage; ";
    if (state1 & voltage_sag_flags[phase])
      status += "Voltage Sag; ";
    if (state1 & phase_loss_flags[phase])
      status += "Phase Loss; ";

    auto *sensor = this->phase_status_text_sensor_[phase];
    if (sensor == nullptr)
      continue;

    if (!status.empty()) {
      status.pop_back();  // remove space
      status.pop_back();  // remove semicolon
      ESP_LOGW(TAG, "%s: %s", sensor->get_name().c_str(), status.c_str());
      sensor->publish_state(status);
    } else {
      sensor->publish_state("Okay");
    }
  }
}

void ATM90E32Component::check_freq_status() {
  uint16_t state1 = this->read16_(ATM90E32_REGISTER_EMMSTATE1);

  std::string freq_status;

  if (state1 & ATM90E32_STATUS_S1_FREQHIST) {
    freq_status = "HIGH";
  } else if (state1 & ATM90E32_STATUS_S1_FREQLOST) {
    freq_status = "LOW";
  } else {
    freq_status = "Normal";
  }
  if (this->freq_status_text_sensor_ != nullptr) {
    if (freq_status == "Normal") {
      ESP_LOGD(TAG, "Frequency status: %s", freq_status.c_str());
    } else {
      ESP_LOGW(TAG, "Frequency status: %s", freq_status.c_str());
    }
    this->freq_status_text_sensor_->publish_state(freq_status);
  }
}

void ATM90E32Component::check_over_current() {
  constexpr float max_current_threshold = 65.53f;

  for (uint8_t phase = 0; phase < 3; phase++) {
    float current_val =
        this->phase_[phase].current_sensor_ != nullptr ? this->phase_[phase].current_sensor_->state : 0.0f;

    if (current_val > max_current_threshold) {
      ESP_LOGW(TAG, "Over current detected on Phase %c: %.2f A", 'A' + phase, current_val);
      ESP_LOGW(TAG, "You may need to half your gain_ct: value & multiply the current and power values by 2");
      if (this->phase_status_text_sensor_[phase] != nullptr) {
        this->phase_status_text_sensor_[phase]->publish_state("Over Current; ");
      }
    }
  }
}
#endif

uint16_t ATM90E32Component::calculate_voltage_threshold(int line_freq, uint16_t ugain, float multiplier) {
  // this assumes that 60Hz electrical systems use 120V mains,
  // which is usually, but not always the case
  float nominal_voltage = (line_freq == 60) ? 120.0f : 220.0f;
  float target_voltage = nominal_voltage * multiplier;

  float peak_01v = target_voltage * 100.0f * std::numbers::sqrt2_v<float>;  // convert RMS → peak, scale to 0.01V
  float divider = (2.0f * ugain) / 32768.0f;

  float threshold = peak_01v / divider;

  return static_cast<uint16_t>(threshold);
}

bool ATM90E32Component::validate_spi_read_(uint16_t expected, const char *context) {
  uint16_t last = this->read16_(ATM90E32_REGISTER_LASTSPIDATA);
  if (last != expected) {
    if (context != nullptr) {
      ESP_LOGW(TAG, "[%s] SPI read mismatch: expected 0x%04X, got 0x%04X", context, expected, last);
    } else {
      ESP_LOGW(TAG, "SPI read mismatch: expected 0x%04X, got 0x%04X", expected, last);
    }
    return false;
  }
  return true;
}

}  // namespace esphome::atm90e32
