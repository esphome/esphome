#include "pzem6l24.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cmath>
#include <type_traits>

namespace esphome::pzem6l24 {

static const char *const TAG = "pzem6l24";

// -----------------------------------------------------------------------
// Register map (input registers, starting address 0x0000):
//
//  The PZEM-6L24 returns all register bytes in little-endian order,
//  i.e. the low byte of each 16-bit register is transmitted first.
//  32-bit quantities occupy two consecutive registers with the low
//  word at the lower address.
//
//  NOTE: this is the opposite of standard Modbus, and of the single-phase
//  pzemac component, which decodes big-endian. It is not an oversight: the
//  byte order below was established against a live PZEM-6L24, so please do
//  not "correct" it to big-endian without a device to verify against.
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

// Scale factor of a quantity; the table has only these two, so a byte replaces a float per entry.
enum Scale : uint8_t {
  SCALE_DECI,   // ×0.1
  SCALE_CENTI,  // ×0.01
};

// One decodable quantity: where it lives in the payload, how to read it and which sensor it feeds.
// Copied out of flash with memcpy, so it must stay trivially copyable.
struct SensorEntry {
  sensor::Sensor *PZEM6L24::*member;
  uint8_t offset;
  RegType type;
  Scale scale;
};
static_assert(std::is_trivially_copyable_v<SensorEntry>, "SENSORS is copied out of flash with memcpy");

// True for the periodic register read issued by update(); the only other request is the 0x42 reset.
static bool is_register_read(std::span<const uint8_t> request_pdu) {
  return modbus::helpers::pdu_function_code(request_pdu) ==
         static_cast<uint8_t>(modbus::FunctionCode::READ_INPUT_REGISTERS);
}

void PZEM6L24::on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) {
  if (!is_register_read(request_pdu)) {
    return;
  }
  this->read_finished_();
  const auto data = modbus::helpers::server_pdu_payload(response_pdu);
  // Anything but exactly PZEM_PAYLOAD_SIZE bytes cannot be decoded by the table.
  if (data.size() != PZEM_PAYLOAD_SIZE) {
    ESP_LOGW(TAG, "Invalid data size for PZEM-6L24: expected %zu bytes, got %zu", PZEM_PAYLOAD_SIZE, data.size());
    this->read_failed_();
    return;
  }
  this->consecutive_failures_ = 0;
  this->publish_(data.data());
}

void PZEM6L24::on_error(std::span<const uint8_t> request_pdu, modbus::ExceptionCode) {
  this->request_failed_(request_pdu);
}

bool PZEM6L24::on_no_response(std::span<const uint8_t> request_pdu) {
  this->request_failed_(request_pdu);
  return false;  // no retry; the next update() polls again.
}

void PZEM6L24::on_not_sent(std::span<const uint8_t> request_pdu) { this->request_failed_(request_pdu); }

// A register read that produced no measurements counts toward blanking; a failed energy reset does
// not, but the user is told. The hub has already logged the cause.
void PZEM6L24::request_failed_(std::span<const uint8_t> request_pdu) {
  if (is_register_read(request_pdu)) {
    this->read_finished_();
    this->read_failed_();
  } else {
    ESP_LOGW(TAG, "Energy reset failed; the counters were not cleared");
  }
}

// `data` points at PZEM_PAYLOAD_SIZE validated bytes, or is nullptr to blank every sensor; both walk
// the same table.
void PZEM6L24::publish_(const uint8_t *data) {
  const bool available = data != nullptr;

  // Byte offset, width and scale for every quantity, in register-map order. All three phases share the
  // same grid frequency, so phase A's register is reported.
  //
  // PROGMEM: on ESP8266 .rodata is DRAM, so the table would otherwise cost ~280 bytes of RAM; the price
  // is one 8-byte copy per sensor per poll. No name column for the same reason; dump_config() names
  // every sensor from flash.
  static constexpr SensorEntry SENSORS[] PROGMEM = {
      // Voltages (×0.1 V)
      {&PZEM6L24::voltage_a_, 0, REG_U16, SCALE_DECI},
      {&PZEM6L24::voltage_b_, 2, REG_U16, SCALE_DECI},
      {&PZEM6L24::voltage_c_, 4, REG_U16, SCALE_DECI},
      // Currents (×0.01 A)
      {&PZEM6L24::current_a_, 6, REG_U16, SCALE_CENTI},
      {&PZEM6L24::current_b_, 8, REG_U16, SCALE_CENTI},
      {&PZEM6L24::current_c_, 10, REG_U16, SCALE_CENTI},
      // Frequency (×0.01 Hz)
      {&PZEM6L24::frequency_, 12, REG_U16, SCALE_CENTI},
      // Active powers (×0.1 W, signed)
      {&PZEM6L24::active_power_a_, 28, REG_I32, SCALE_DECI},
      {&PZEM6L24::active_power_b_, 32, REG_I32, SCALE_DECI},
      {&PZEM6L24::active_power_c_, 36, REG_I32, SCALE_DECI},
      {&PZEM6L24::total_active_power_, 64, REG_I32, SCALE_DECI},
      // Reactive powers (×0.1 var, signed)
      {&PZEM6L24::reactive_power_a_, 40, REG_I32, SCALE_DECI},
      {&PZEM6L24::reactive_power_b_, 44, REG_I32, SCALE_DECI},
      {&PZEM6L24::reactive_power_c_, 48, REG_I32, SCALE_DECI},
      {&PZEM6L24::total_reactive_power_, 68, REG_I32, SCALE_DECI},
      // Apparent powers (×0.1 VA, signed)
      {&PZEM6L24::apparent_power_a_, 52, REG_I32, SCALE_DECI},
      {&PZEM6L24::apparent_power_b_, 56, REG_I32, SCALE_DECI},
      {&PZEM6L24::apparent_power_c_, 60, REG_I32, SCALE_DECI},
      {&PZEM6L24::total_apparent_power_, 72, REG_I32, SCALE_DECI},
      // Power factors (×0.01), packed two per register:
      //   register 0x0026 (bytes 76/77): lo-byte = phase B, hi-byte = phase A
      //   register 0x0027 (bytes 78/79): lo-byte = combined, hi-byte = phase C
      {&PZEM6L24::power_factor_a_, 77, REG_U8, SCALE_CENTI},
      {&PZEM6L24::power_factor_b_, 76, REG_U8, SCALE_CENTI},
      {&PZEM6L24::power_factor_c_, 79, REG_U8, SCALE_CENTI},
      {&PZEM6L24::total_power_factor_, 78, REG_U8, SCALE_CENTI},
      // Active energies (×0.1 kWh, unsigned)
      {&PZEM6L24::active_energy_a_, 80, REG_U32, SCALE_DECI},
      {&PZEM6L24::active_energy_b_, 84, REG_U32, SCALE_DECI},
      {&PZEM6L24::active_energy_c_, 88, REG_U32, SCALE_DECI},
      {&PZEM6L24::total_active_energy_, 116, REG_U32, SCALE_DECI},
      // Reactive energies (×0.1 kvarh, unsigned)
      {&PZEM6L24::reactive_energy_a_, 92, REG_U32, SCALE_DECI},
      {&PZEM6L24::reactive_energy_b_, 96, REG_U32, SCALE_DECI},
      {&PZEM6L24::reactive_energy_c_, 100, REG_U32, SCALE_DECI},
      {&PZEM6L24::total_reactive_energy_, 120, REG_U32, SCALE_DECI},
      // Apparent energies (×0.1 kVAh, unsigned)
      {&PZEM6L24::apparent_energy_a_, 104, REG_U32, SCALE_DECI},
      {&PZEM6L24::apparent_energy_b_, 108, REG_U32, SCALE_DECI},
      {&PZEM6L24::apparent_energy_c_, 112, REG_U32, SCALE_DECI},
      {&PZEM6L24::total_apparent_energy_, 124, REG_U32, SCALE_DECI},
  };

  for (const SensorEntry &flash_entry : SENSORS) {
    SensorEntry entry;
    progmem_memcpy(&entry, &flash_entry, sizeof(entry));
    sensor::Sensor *sens = this->*entry.member;
    if (sens == nullptr)
      continue;
    if (!available) {
      sens->publish_state(NAN);
      continue;
    }
    // No default: an added RegType must fail to compile. The wire is little-endian, hence the reversed
    // byte arguments.
    const size_t o = entry.offset;
    float raw = 0.0f;
    switch (entry.type) {
      case REG_U8:
        raw = data[o];
        break;
      case REG_U16:
        raw = encode_uint16(data[o + 1], data[o]);
        break;
      case REG_U32:
        raw = encode_uint32(data[o + 3], data[o + 2], data[o + 1], data[o]);
        break;
      case REG_I32:
        raw = static_cast<int32_t>(encode_uint32(data[o + 3], data[o + 2], data[o + 1], data[o]));
        break;
    }
    sens->publish_state(raw * (entry.scale == SCALE_CENTI ? 0.01f : 0.1f));
  }
}

void PZEM6L24::update() {
  if (this->read_input_registers(0x0000, PZEM_REGISTER_COUNT)) {
    this->reads_outstanding_++;
  } else if (this->reads_outstanding_ == 0) {
    // Refused with nothing in flight: no callback is coming, and the hub has logged why. A refusal
    // while a read is outstanding is a duplicate of it, which still resolves in that read's callback.
    this->read_failed_();
  }
}

void PZEM6L24::dump_config() {
  ESP_LOGCONFIG(TAG,
                "PZEM-6L24:\n"
                "  Address: 0x%02X",
                this->address_);
  LOG_UPDATE_INTERVAL(this);
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

// Blank after MAX_CONSECUTIVE_READ_FAILURES; stays blanked until a poll succeeds.
void PZEM6L24::read_failed_() {
  if (this->consecutive_failures_ >= MAX_CONSECUTIVE_READ_FAILURES) {
    return;
  }
  if (++this->consecutive_failures_ == MAX_CONSECUTIVE_READ_FAILURES) {
    ESP_LOGW(TAG, "No valid reading in %u consecutive polls; the readings are now unavailable",
             MAX_CONSECUTIVE_READ_FAILURES);
    this->publish_(nullptr);
  }
}

// One terminal has arrived for a register read, so that read is no longer in flight.
void PZEM6L24::read_finished_() {
  if (this->reads_outstanding_ > 0) {
    this->reads_outstanding_--;
  }
}

void PZEM6L24::reset_energy(ResetPhase phase_option) {
  const auto pdu = build_reset_pdu(phase_option);
  // A refused request gets no callback, so report it here.
  if (!this->queue_pdu(pdu)) {
    this->request_failed_(pdu);
  }
}

}  // namespace esphome::pzem6l24
