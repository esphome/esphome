#include "pzem6l24.h"
#include "esphome/core/log.h"

namespace esphome::pzem6l24 {

static const char *const TAG = "pzem6l24";

// Reset energy function code (PZEM-6L24 specific, non-standard Modbus)
static const uint8_t PZEM_CMD_RESET_ENERGY = 0x42;
// Number of input registers to read (0x0000 – 0x003F inclusive)
static const uint8_t PZEM_REGISTER_COUNT = 64;  // 64 × 16-bit registers = 128 bytes
// Payload size of the register read response
static const size_t PZEM_PAYLOAD_SIZE = PZEM_REGISTER_COUNT * 2;

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

// Width of a quantity in the register map above.
enum RegType : uint8_t {
  REG_U8,   // single byte (the packed power factors)
  REG_U16,  // one register, unsigned
  REG_U32,  // two registers, unsigned, low word first
  REG_I32,  // two registers, signed, low word first
};

// One decodable quantity: where it lives in the payload, how to read it, and which sensor it feeds.
struct SensorEntry {
  sensor::Sensor *PZEM6L24::*member;
  uint8_t offset;
  RegType type;
  float scale;
};

void PZEM6L24::on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) {
  // on_response() is called for every reply routed to this device, including the acknowledgement of the
  // 0x42 reset command. Only the register read carries the payload decoded below.
  if (request_pdu.empty() || request_pdu[0] != static_cast<uint8_t>(modbus::FunctionCode::READ_INPUT_REGISTERS)) {
    return;
  }

  auto data = modbus::helpers::server_pdu_payload(response_pdu);
  if (data.size() < PZEM_PAYLOAD_SIZE) {
    ESP_LOGW(TAG, "Invalid data size for PZEM-6L24: expected %zu bytes, got %zu", PZEM_PAYLOAD_SIZE, data.size());
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

  ESP_LOGD(TAG, "PZEM-6L24 A: V=%.1f V, I=%.2f A, P=%.1f W, Q=%.1f var, S=%.1f VA, PF=%.2f, E=%.1f kWh",
           get_u16(0) * 0.1f, get_u16(6) * 0.01f, get_i32(28) * 0.1f, get_i32(40) * 0.1f, get_i32(52) * 0.1f,
           data[77] * 0.01f, get_u32(80) * 0.1f);
  ESP_LOGD(TAG, "PZEM-6L24 B: V=%.1f V, I=%.2f A, P=%.1f W, Q=%.1f var, S=%.1f VA, PF=%.2f, E=%.1f kWh",
           get_u16(2) * 0.1f, get_u16(8) * 0.01f, get_i32(32) * 0.1f, get_i32(44) * 0.1f, get_i32(56) * 0.1f,
           data[76] * 0.01f, get_u32(84) * 0.1f);
  ESP_LOGD(TAG, "PZEM-6L24 C: V=%.1f V, I=%.2f A, P=%.1f W, Q=%.1f var, S=%.1f VA, PF=%.2f, E=%.1f kWh",
           get_u16(4) * 0.1f, get_u16(10) * 0.01f, get_i32(36) * 0.1f, get_i32(48) * 0.1f, get_i32(60) * 0.1f,
           data[79] * 0.01f, get_u32(88) * 0.1f);
  ESP_LOGD(TAG, "PZEM-6L24 Total: P=%.1f W, Q=%.1f var, S=%.1f VA, PF=%.2f, E=%.1f kWh, F=%.2f Hz", get_i32(64) * 0.1f,
           get_i32(68) * 0.1f, get_i32(72) * 0.1f, data[78] * 0.01f, get_u32(116) * 0.1f, get_u16(12) * 0.01f);

  // Byte offset, width and scaling for every quantity, in register-map order. All three phases share the
  // same grid frequency, so phase A's register is reported as the representative value.
  static constexpr SensorEntry SENSORS[] = {
      // Voltages (×0.1 V)
      {&PZEM6L24::voltage_a_, 0, REG_U16, 0.1f},
      {&PZEM6L24::voltage_b_, 2, REG_U16, 0.1f},
      {&PZEM6L24::voltage_c_, 4, REG_U16, 0.1f},
      // Currents (×0.01 A)
      {&PZEM6L24::current_a_, 6, REG_U16, 0.01f},
      {&PZEM6L24::current_b_, 8, REG_U16, 0.01f},
      {&PZEM6L24::current_c_, 10, REG_U16, 0.01f},
      // Frequency (×0.01 Hz)
      {&PZEM6L24::frequency_, 12, REG_U16, 0.01f},
      // Active powers (×0.1 W, signed)
      {&PZEM6L24::active_power_a_, 28, REG_I32, 0.1f},
      {&PZEM6L24::active_power_b_, 32, REG_I32, 0.1f},
      {&PZEM6L24::active_power_c_, 36, REG_I32, 0.1f},
      {&PZEM6L24::total_active_power_, 64, REG_I32, 0.1f},
      // Reactive powers (×0.1 var, signed)
      {&PZEM6L24::reactive_power_a_, 40, REG_I32, 0.1f},
      {&PZEM6L24::reactive_power_b_, 44, REG_I32, 0.1f},
      {&PZEM6L24::reactive_power_c_, 48, REG_I32, 0.1f},
      {&PZEM6L24::total_reactive_power_, 68, REG_I32, 0.1f},
      // Apparent powers (×0.1 VA, signed)
      {&PZEM6L24::apparent_power_a_, 52, REG_I32, 0.1f},
      {&PZEM6L24::apparent_power_b_, 56, REG_I32, 0.1f},
      {&PZEM6L24::apparent_power_c_, 60, REG_I32, 0.1f},
      {&PZEM6L24::total_apparent_power_, 72, REG_I32, 0.1f},
      // Power factors (×0.01), packed two per register:
      //   register 0x0026 (bytes 76/77): lo-byte = phase B, hi-byte = phase A
      //   register 0x0027 (bytes 78/79): lo-byte = combined, hi-byte = phase C
      {&PZEM6L24::power_factor_a_, 77, REG_U8, 0.01f},
      {&PZEM6L24::power_factor_b_, 76, REG_U8, 0.01f},
      {&PZEM6L24::power_factor_c_, 79, REG_U8, 0.01f},
      {&PZEM6L24::total_power_factor_, 78, REG_U8, 0.01f},
      // Active energies (×0.1 kWh, unsigned)
      {&PZEM6L24::active_energy_a_, 80, REG_U32, 0.1f},
      {&PZEM6L24::active_energy_b_, 84, REG_U32, 0.1f},
      {&PZEM6L24::active_energy_c_, 88, REG_U32, 0.1f},
      {&PZEM6L24::total_active_energy_, 116, REG_U32, 0.1f},
      // Reactive energies (×0.1 kvarh, unsigned)
      {&PZEM6L24::reactive_energy_a_, 92, REG_U32, 0.1f},
      {&PZEM6L24::reactive_energy_b_, 96, REG_U32, 0.1f},
      {&PZEM6L24::reactive_energy_c_, 100, REG_U32, 0.1f},
      {&PZEM6L24::total_reactive_energy_, 120, REG_U32, 0.1f},
      // Apparent energies (×0.1 kVAh, unsigned)
      {&PZEM6L24::apparent_energy_a_, 104, REG_U32, 0.1f},
      {&PZEM6L24::apparent_energy_b_, 108, REG_U32, 0.1f},
      {&PZEM6L24::apparent_energy_c_, 112, REG_U32, 0.1f},
      {&PZEM6L24::total_apparent_energy_, 124, REG_U32, 0.1f},
  };

  for (const auto &entry : SENSORS) {
    sensor::Sensor *sens = this->*entry.member;
    if (sens == nullptr)
      continue;
    float raw;
    switch (entry.type) {
      case REG_U8:
        raw = data[entry.offset];
        break;
      case REG_U16:
        raw = get_u16(entry.offset);
        break;
      case REG_U32:
        raw = get_u32(entry.offset);
        break;
      case REG_I32:
      default:
        raw = get_i32(entry.offset);
        break;
    }
    sens->publish_state(raw * entry.scale);
  }
}

void PZEM6L24::update() { this->read_input_registers(0x0000, PZEM_REGISTER_COUNT); }

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
  // The hub prepends the device address and appends the CRC.
  const uint8_t pdu[] = {PZEM_CMD_RESET_ENERGY, 0x00, static_cast<uint8_t>(phase_option)};
  this->send_pdu(pdu);
}

}  // namespace esphome::pzem6l24
