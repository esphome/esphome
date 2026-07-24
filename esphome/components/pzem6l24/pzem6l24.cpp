#include "pzem6l24.h"
#include "esphome/core/log.h"

namespace esphome::pzem6l24 {

static const char *const TAG = "pzem6l24";

// Modbus function code to read input registers
static const uint8_t PZEM_CMD_READ_IN_REGISTERS = 0x04;
// Reset energy function code (PZEM-6L24 specific, non-standard Modbus)
static const uint8_t PZEM_CMD_RESET_ENERGY = 0x42;
// Number of input registers to read (0x0000 – 0x003F inclusive)
static const uint8_t PZEM_REGISTER_COUNT = 64;  // 64 × 16-bit registers = 128 bytes

// -----------------------------------------------------------------------
// Register map (input registers, starting address 0x0000):
//
//  The PZEM-6L24 returns all register bytes in little-endian order,
//  i.e. the low byte of each 16-bit register is transmitted first.
//  32-bit quantities occupy two consecutive registers with the low
//  word at the lower address.
//
//  Byte offset = register_address × 2
//
//  0x0000 (byte  0) – Voltage A          (uint16, ×0.1 V)
//  0x0001 (byte  2) – Voltage B          (uint16, ×0.1 V)
//  0x0002 (byte  4) – Voltage C          (uint16, ×0.1 V)
//  0x0003 (byte  6) – Current A          (uint16, ×0.01 A)
//  0x0004 (byte  8) – Current B          (uint16, ×0.01 A)
//  0x0005 (byte 10) – Current C          (uint16, ×0.01 A)
//  0x0006 (byte 12) – Frequency A        (uint16, ×0.01 Hz)
//  0x0007 (byte 14) – Frequency B        (uint16, ×0.01 Hz)
//  0x0008 (byte 16) – Frequency C        (uint16, ×0.01 Hz)
//  0x0009 (byte 18) – Voltage angle B    (uint16, ×0.01 °)
//  0x000A (byte 20) – Voltage angle C    (uint16, ×0.01 °)
//  0x000B (byte 22) – Current angle A    (uint16, ×0.01 °)
//  0x000C (byte 24) – Current angle B    (uint16, ×0.01 °)
//  0x000D (byte 26) – Current angle C    (uint16, ×0.01 °)
//  0x000E (byte 28) – Active power A     (int32 lo-word, ×0.1 W)
//  0x000F (byte 30) – Active power A     (int32 hi-word)
//  0x0010 (byte 32) – Active power B     (int32 lo-word, ×0.1 W)
//  0x0011 (byte 34) – Active power B     (int32 hi-word)
//  0x0012 (byte 36) – Active power C     (int32 lo-word, ×0.1 W)
//  0x0013 (byte 38) – Active power C     (int32 hi-word)
//  0x0014 (byte 40) – Reactive power A   (int32 lo-word, ×0.1 var)
//  0x0015 (byte 42) – Reactive power A   (int32 hi-word)
//  0x0016 (byte 44) – Reactive power B   (int32 lo-word, ×0.1 var)
//  0x0017 (byte 46) – Reactive power B   (int32 hi-word)
//  0x0018 (byte 48) – Reactive power C   (int32 lo-word, ×0.1 var)
//  0x0019 (byte 50) – Reactive power C   (int32 hi-word)
//  0x001A (byte 52) – Apparent power A   (int32 lo-word, ×0.1 VA)
//  0x001B (byte 54) – Apparent power A   (int32 hi-word)
//  0x001C (byte 56) – Apparent power B   (int32 lo-word, ×0.1 VA)
//  0x001D (byte 58) – Apparent power B   (int32 hi-word)
//  0x001E (byte 60) – Apparent power C   (int32 lo-word, ×0.1 VA)
//  0x001F (byte 62) – Apparent power C   (int32 hi-word)
//  0x0020 (byte 64) – Total active pwr   (int32 lo-word, ×0.1 W)
//  0x0021 (byte 66) – Total active pwr   (int32 hi-word)
//  0x0022 (byte 68) – Total reactive pwr (int32 lo-word, ×0.1 var)
//  0x0023 (byte 70) – Total reactive pwr (int32 hi-word)
//  0x0024 (byte 72) – Total apparent pwr (int32 lo-word, ×0.1 VA)
//  0x0025 (byte 74) – Total apparent pwr (int32 hi-word)
//  0x0026 (byte 76) – Power factor A/B:  hi-byte = A (×0.01), lo-byte = B (×0.01)
//  0x0027 (byte 78) – Power factor C/tot:hi-byte = C (×0.01), lo-byte = total (×0.01)
//  0x0028 (byte 80) – Active energy A    (uint32 lo-word, ×0.1 kWh)
//  0x0029 (byte 82) – Active energy A    (uint32 hi-word)
//  0x002A (byte 84) – Active energy B    (uint32 lo-word, ×0.1 kWh)
//  0x002B (byte 86) – Active energy B    (uint32 hi-word)
//  0x002C (byte 88) – Active energy C    (uint32 lo-word, ×0.1 kWh)
//  0x002D (byte 90) – Active energy C    (uint32 hi-word)
//  0x002E (byte 92) – Reactive energy A  (uint32 lo-word, ×0.1 kvarh)
//  0x002F (byte 94) – Reactive energy A  (uint32 hi-word)
//  0x0030 (byte 96) – Reactive energy B  (uint32 lo-word, ×0.1 kvarh)
//  0x0031 (byte 98) – Reactive energy B  (uint32 hi-word)
//  0x0032 (byte 100)– Reactive energy C  (uint32 lo-word, ×0.1 kvarh)
//  0x0033 (byte 102)– Reactive energy C  (uint32 hi-word)
//  0x0034 (byte 104)– Apparent energy A  (uint32 lo-word, ×0.1 kVAh)
//  0x0035 (byte 106)– Apparent energy A  (uint32 hi-word)
//  0x0036 (byte 108)– Apparent energy B  (uint32 lo-word, ×0.1 kVAh)
//  0x0037 (byte 110)– Apparent energy B  (uint32 hi-word)
//  0x0038 (byte 112)– Apparent energy C  (uint32 lo-word, ×0.1 kVAh)
//  0x0039 (byte 114)– Apparent energy C  (uint32 hi-word)
//  0x003A (byte 116)– Total active nrg   (uint32 lo-word, ×0.1 kWh)
//  0x003B (byte 118)– Total active nrg   (uint32 hi-word)
//  0x003C (byte 120)– Total reactive nrg (uint32 lo-word, ×0.1 kvarh)
//  0x003D (byte 122)– Total reactive nrg (uint32 hi-word)
//  0x003E (byte 124)– Total apparent nrg (uint32 lo-word, ×0.1 kVAh)
//  0x003F (byte 126)– Total apparent nrg (uint32 hi-word)
// -----------------------------------------------------------------------

void PZEM6L24::on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) {
  auto data = modbus::helpers::server_pdu_payload(response_pdu);
  if (data.size() < 128) {
    ESP_LOGW(TAG, "Invalid data size for PZEM-6L24: expected 128 bytes, got %zu", data.size());
    return;
  }

  // Helper: decode a little-endian 16-bit value at byte offset i.
  auto get_u16 = [&](size_t i) -> uint16_t { return (uint16_t(data[i + 1]) << 8) | uint16_t(data[i]); };

  // Helper: decode a little-endian unsigned 32-bit value at byte offset i
  // (low word at i, high word at i+2).
  auto get_u32 = [&](size_t i) -> uint32_t {
    return (uint32_t(data[i + 3]) << 24) | (uint32_t(data[i + 2]) << 16) | (uint32_t(data[i + 1]) << 8) |
           uint32_t(data[i]);
  };

  // Helper: decode a little-endian signed 32-bit value at byte offset i.
  auto get_i32 = [&](size_t i) -> int32_t { return static_cast<int32_t>(get_u32(i)); };

  // --- Voltages (×0.1 V) ---
  float voltage_a = get_u16(0) * 0.1f;
  float voltage_b = get_u16(2) * 0.1f;
  float voltage_c = get_u16(4) * 0.1f;

  // --- Currents (×0.01 A) ---
  float current_a = get_u16(6) * 0.01f;
  float current_b = get_u16(8) * 0.01f;
  float current_c = get_u16(10) * 0.01f;

  // --- Frequency (×0.01 Hz) — all three phases share the same grid frequency;
  //     we report phase A as the representative value. ---
  float frequency = get_u16(12) * 0.01f;

  // --- Active powers (×0.1 W, signed) ---
  float active_power_a = get_i32(28) * 0.1f;
  float active_power_b = get_i32(32) * 0.1f;
  float active_power_c = get_i32(36) * 0.1f;

  // --- Reactive powers (×0.1 var, signed) ---
  float reactive_power_a = get_i32(40) * 0.1f;
  float reactive_power_b = get_i32(44) * 0.1f;
  float reactive_power_c = get_i32(48) * 0.1f;

  // --- Apparent powers (×0.1 VA, signed) ---
  float apparent_power_a = get_i32(52) * 0.1f;
  float apparent_power_b = get_i32(56) * 0.1f;
  float apparent_power_c = get_i32(60) * 0.1f;

  // --- Combined powers (×0.1 W/var/VA) ---
  float total_active_power = get_i32(64) * 0.1f;
  float total_reactive_power = get_i32(68) * 0.1f;
  float total_apparent_power = get_i32(72) * 0.1f;

  // --- Power factors (×0.01, packed as two uint8 values per register):
  //     Register 0x0026: hi-byte = phase A, lo-byte = phase B
  //     Register 0x0027: hi-byte = phase C, lo-byte = combined ---
  float power_factor_a = data[77] * 0.01f;
  float power_factor_b = data[76] * 0.01f;
  float power_factor_c = data[79] * 0.01f;
  float total_power_factor = data[78] * 0.01f;

  // --- Active energies (×0.1 kWh, unsigned) ---
  float active_energy_a = get_u32(80) * 0.1f;
  float active_energy_b = get_u32(84) * 0.1f;
  float active_energy_c = get_u32(88) * 0.1f;

  // --- Reactive energies (×0.1 kvarh, unsigned) ---
  float reactive_energy_a = get_u32(92) * 0.1f;
  float reactive_energy_b = get_u32(96) * 0.1f;
  float reactive_energy_c = get_u32(100) * 0.1f;

  // --- Apparent energies (×0.1 kVAh, unsigned) ---
  float apparent_energy_a = get_u32(104) * 0.1f;
  float apparent_energy_b = get_u32(108) * 0.1f;
  float apparent_energy_c = get_u32(112) * 0.1f;

  // --- Combined energies ---
  float total_active_energy = get_u32(116) * 0.1f;
  float total_reactive_energy = get_u32(120) * 0.1f;
  float total_apparent_energy = get_u32(124) * 0.1f;

  ESP_LOGD(TAG, "PZEM-6L24 A: V=%.1f V, I=%.2f A, P=%.1f W, Q=%.1f var, S=%.1f VA, PF=%.2f, E=%.1f kWh", voltage_a,
           current_a, active_power_a, reactive_power_a, apparent_power_a, power_factor_a, active_energy_a);
  ESP_LOGD(TAG, "PZEM-6L24 B: V=%.1f V, I=%.2f A, P=%.1f W, Q=%.1f var, S=%.1f VA, PF=%.2f, E=%.1f kWh", voltage_b,
           current_b, active_power_b, reactive_power_b, apparent_power_b, power_factor_b, active_energy_b);
  ESP_LOGD(TAG, "PZEM-6L24 C: V=%.1f V, I=%.2f A, P=%.1f W, Q=%.1f var, S=%.1f VA, PF=%.2f, E=%.1f kWh", voltage_c,
           current_c, active_power_c, reactive_power_c, apparent_power_c, power_factor_c, active_energy_c);
  ESP_LOGD(TAG, "PZEM-6L24 Total: P=%.1f W, Q=%.1f var, S=%.1f VA, PF=%.2f, E=%.1f kWh, F=%.2f Hz", total_active_power,
           total_reactive_power, total_apparent_power, total_power_factor, total_active_energy, frequency);

  // Publish per-phase voltages
  if (this->voltage_a_ != nullptr)
    this->voltage_a_->publish_state(voltage_a);
  if (this->voltage_b_ != nullptr)
    this->voltage_b_->publish_state(voltage_b);
  if (this->voltage_c_ != nullptr)
    this->voltage_c_->publish_state(voltage_c);

  // Publish per-phase currents
  if (this->current_a_ != nullptr)
    this->current_a_->publish_state(current_a);
  if (this->current_b_ != nullptr)
    this->current_b_->publish_state(current_b);
  if (this->current_c_ != nullptr)
    this->current_c_->publish_state(current_c);

  // Publish per-phase active powers
  if (this->active_power_a_ != nullptr)
    this->active_power_a_->publish_state(active_power_a);
  if (this->active_power_b_ != nullptr)
    this->active_power_b_->publish_state(active_power_b);
  if (this->active_power_c_ != nullptr)
    this->active_power_c_->publish_state(active_power_c);

  // Publish per-phase reactive powers
  if (this->reactive_power_a_ != nullptr)
    this->reactive_power_a_->publish_state(reactive_power_a);
  if (this->reactive_power_b_ != nullptr)
    this->reactive_power_b_->publish_state(reactive_power_b);
  if (this->reactive_power_c_ != nullptr)
    this->reactive_power_c_->publish_state(reactive_power_c);

  // Publish per-phase apparent powers
  if (this->apparent_power_a_ != nullptr)
    this->apparent_power_a_->publish_state(apparent_power_a);
  if (this->apparent_power_b_ != nullptr)
    this->apparent_power_b_->publish_state(apparent_power_b);
  if (this->apparent_power_c_ != nullptr)
    this->apparent_power_c_->publish_state(apparent_power_c);

  // Publish per-phase power factors
  if (this->power_factor_a_ != nullptr)
    this->power_factor_a_->publish_state(power_factor_a);
  if (this->power_factor_b_ != nullptr)
    this->power_factor_b_->publish_state(power_factor_b);
  if (this->power_factor_c_ != nullptr)
    this->power_factor_c_->publish_state(power_factor_c);

  // Publish per-phase active energies
  if (this->active_energy_a_ != nullptr)
    this->active_energy_a_->publish_state(active_energy_a);
  if (this->active_energy_b_ != nullptr)
    this->active_energy_b_->publish_state(active_energy_b);
  if (this->active_energy_c_ != nullptr)
    this->active_energy_c_->publish_state(active_energy_c);

  // Publish per-phase reactive energies
  if (this->reactive_energy_a_ != nullptr)
    this->reactive_energy_a_->publish_state(reactive_energy_a);
  if (this->reactive_energy_b_ != nullptr)
    this->reactive_energy_b_->publish_state(reactive_energy_b);
  if (this->reactive_energy_c_ != nullptr)
    this->reactive_energy_c_->publish_state(reactive_energy_c);

  // Publish per-phase apparent energies
  if (this->apparent_energy_a_ != nullptr)
    this->apparent_energy_a_->publish_state(apparent_energy_a);
  if (this->apparent_energy_b_ != nullptr)
    this->apparent_energy_b_->publish_state(apparent_energy_b);
  if (this->apparent_energy_c_ != nullptr)
    this->apparent_energy_c_->publish_state(apparent_energy_c);

  // Publish combined sensors
  if (this->frequency_ != nullptr)
    this->frequency_->publish_state(frequency);
  if (this->total_active_power_ != nullptr)
    this->total_active_power_->publish_state(total_active_power);
  if (this->total_reactive_power_ != nullptr)
    this->total_reactive_power_->publish_state(total_reactive_power);
  if (this->total_apparent_power_ != nullptr)
    this->total_apparent_power_->publish_state(total_apparent_power);
  if (this->total_power_factor_ != nullptr)
    this->total_power_factor_->publish_state(total_power_factor);
  if (this->total_active_energy_ != nullptr)
    this->total_active_energy_->publish_state(total_active_energy);
  if (this->total_reactive_energy_ != nullptr)
    this->total_reactive_energy_->publish_state(total_reactive_energy);
  if (this->total_apparent_energy_ != nullptr)
    this->total_apparent_energy_->publish_state(total_apparent_energy);
}

void PZEM6L24::update() { this->send(PZEM_CMD_READ_IN_REGISTERS, 0x0000, PZEM_REGISTER_COUNT); }

void PZEM6L24::dump_config() {
  ESP_LOGCONFIG(TAG,
                "PZEM-6L24:\n"
                "  Address: 0x%02X",
                this->address_);
  LOG_SENSOR("  ", "Voltage A", this->voltage_a_);
  LOG_SENSOR("  ", "Voltage B", this->voltage_b_);
  LOG_SENSOR("  ", "Voltage C", this->voltage_c_);
  LOG_SENSOR("  ", "Current A", this->current_a_);
  LOG_SENSOR("  ", "Current B", this->current_b_);
  LOG_SENSOR("  ", "Current C", this->current_c_);
  LOG_SENSOR("  ", "Active Power A", this->active_power_a_);
  LOG_SENSOR("  ", "Active Power B", this->active_power_b_);
  LOG_SENSOR("  ", "Active Power C", this->active_power_c_);
  LOG_SENSOR("  ", "Reactive Power A", this->reactive_power_a_);
  LOG_SENSOR("  ", "Reactive Power B", this->reactive_power_b_);
  LOG_SENSOR("  ", "Reactive Power C", this->reactive_power_c_);
  LOG_SENSOR("  ", "Apparent Power A", this->apparent_power_a_);
  LOG_SENSOR("  ", "Apparent Power B", this->apparent_power_b_);
  LOG_SENSOR("  ", "Apparent Power C", this->apparent_power_c_);
  LOG_SENSOR("  ", "Power Factor A", this->power_factor_a_);
  LOG_SENSOR("  ", "Power Factor B", this->power_factor_b_);
  LOG_SENSOR("  ", "Power Factor C", this->power_factor_c_);
  LOG_SENSOR("  ", "Active Energy A", this->active_energy_a_);
  LOG_SENSOR("  ", "Active Energy B", this->active_energy_b_);
  LOG_SENSOR("  ", "Active Energy C", this->active_energy_c_);
  LOG_SENSOR("  ", "Reactive Energy A", this->reactive_energy_a_);
  LOG_SENSOR("  ", "Reactive Energy B", this->reactive_energy_b_);
  LOG_SENSOR("  ", "Reactive Energy C", this->reactive_energy_c_);
  LOG_SENSOR("  ", "Apparent Energy A", this->apparent_energy_a_);
  LOG_SENSOR("  ", "Apparent Energy B", this->apparent_energy_b_);
  LOG_SENSOR("  ", "Apparent Energy C", this->apparent_energy_c_);
  LOG_SENSOR("  ", "Frequency", this->frequency_);
  LOG_SENSOR("  ", "Total Active Power", this->total_active_power_);
  LOG_SENSOR("  ", "Total Reactive Power", this->total_reactive_power_);
  LOG_SENSOR("  ", "Total Apparent Power", this->total_apparent_power_);
  LOG_SENSOR("  ", "Total Power Factor", this->total_power_factor_);
  LOG_SENSOR("  ", "Total Active Energy", this->total_active_energy_);
  LOG_SENSOR("  ", "Total Reactive Energy", this->total_reactive_energy_);
  LOG_SENSOR("  ", "Total Apparent Energy", this->total_apparent_energy_);
}

void PZEM6L24::reset_energy_(ResetPhase phase_option) {
  // The PZEM-6L24 reset command uses function code 0x42 with two extra
  // bytes: a reserved 0x00 byte and the phase selection byte.
  // The ESPHome modbus send_raw() appends the CRC automatically.
  this->send_raw({this->address_, PZEM_CMD_RESET_ENERGY, 0x00, static_cast<uint8_t>(phase_option)});
}

}  // namespace esphome::pzem6l24
