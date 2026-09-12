#include "../common.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>
#include <gtest/gtest.h>

namespace esphome::pzem6l24::testing {

namespace {

// One PZEM6L24 with every sensor it can drive attached, so a test can assert that each byte offset in
// the register map reaches the sensor it is documented to feed.
struct Harness {
  PZEM6L24 pzem;

  sensor::Sensor voltage_a, voltage_b, voltage_c;
  sensor::Sensor current_a, current_b, current_c;
  sensor::Sensor active_power_a, active_power_b, active_power_c;
  sensor::Sensor reactive_power_a, reactive_power_b, reactive_power_c;
  sensor::Sensor apparent_power_a, apparent_power_b, apparent_power_c;
  sensor::Sensor power_factor_a, power_factor_b, power_factor_c;
  sensor::Sensor active_energy_a, active_energy_b, active_energy_c;
  sensor::Sensor reactive_energy_a, reactive_energy_b, reactive_energy_c;
  sensor::Sensor apparent_energy_a, apparent_energy_b, apparent_energy_c;
  sensor::Sensor frequency;
  sensor::Sensor total_active_power, total_reactive_power, total_apparent_power;
  sensor::Sensor total_power_factor;
  sensor::Sensor total_active_energy, total_reactive_energy, total_apparent_energy;

  Harness() {
    this->pzem.set_voltage_sensor_a(&this->voltage_a);
    this->pzem.set_voltage_sensor_b(&this->voltage_b);
    this->pzem.set_voltage_sensor_c(&this->voltage_c);
    this->pzem.set_current_sensor_a(&this->current_a);
    this->pzem.set_current_sensor_b(&this->current_b);
    this->pzem.set_current_sensor_c(&this->current_c);
    this->pzem.set_active_power_sensor_a(&this->active_power_a);
    this->pzem.set_active_power_sensor_b(&this->active_power_b);
    this->pzem.set_active_power_sensor_c(&this->active_power_c);
    this->pzem.set_reactive_power_sensor_a(&this->reactive_power_a);
    this->pzem.set_reactive_power_sensor_b(&this->reactive_power_b);
    this->pzem.set_reactive_power_sensor_c(&this->reactive_power_c);
    this->pzem.set_apparent_power_sensor_a(&this->apparent_power_a);
    this->pzem.set_apparent_power_sensor_b(&this->apparent_power_b);
    this->pzem.set_apparent_power_sensor_c(&this->apparent_power_c);
    this->pzem.set_power_factor_sensor_a(&this->power_factor_a);
    this->pzem.set_power_factor_sensor_b(&this->power_factor_b);
    this->pzem.set_power_factor_sensor_c(&this->power_factor_c);
    this->pzem.set_active_energy_sensor_a(&this->active_energy_a);
    this->pzem.set_active_energy_sensor_b(&this->active_energy_b);
    this->pzem.set_active_energy_sensor_c(&this->active_energy_c);
    this->pzem.set_reactive_energy_sensor_a(&this->reactive_energy_a);
    this->pzem.set_reactive_energy_sensor_b(&this->reactive_energy_b);
    this->pzem.set_reactive_energy_sensor_c(&this->reactive_energy_c);
    this->pzem.set_apparent_energy_sensor_a(&this->apparent_energy_a);
    this->pzem.set_apparent_energy_sensor_b(&this->apparent_energy_b);
    this->pzem.set_apparent_energy_sensor_c(&this->apparent_energy_c);
    this->pzem.set_frequency_sensor(&this->frequency);
    this->pzem.set_total_active_power_sensor(&this->total_active_power);
    this->pzem.set_total_reactive_power_sensor(&this->total_reactive_power);
    this->pzem.set_total_apparent_power_sensor(&this->total_apparent_power);
    this->pzem.set_total_power_factor_sensor(&this->total_power_factor);
    this->pzem.set_total_active_energy_sensor(&this->total_active_energy);
    this->pzem.set_total_reactive_energy_sensor(&this->total_reactive_energy);
    this->pzem.set_total_apparent_energy_sensor(&this->total_apparent_energy);
  }
};

// A payload whose raw values are distinct per quantity, so a transposed offset shows up as a wrong
// value rather than a coincidental match. Registers the component does not read are filled with 0xEE.
PayloadBuilder make_reference_payload() {
  PayloadBuilder p;
  // Voltages (×0.1 V)
  p.u16(0, 2301).u16(2, 2302).u16(4, 2303);
  // Currents (×0.01 A)
  p.u16(6, 1234).u16(8, 1235).u16(10, 1236);
  // Frequency phase A (×0.01 Hz); phases B/C and the angle registers are not read.
  p.u16(12, 5001);
  for (size_t offset = 14; offset < 28; offset++) {
    p.u8(offset, 0xEE);
  }
  // Active power (×0.1 W, signed); phase C exercises the sign extension.
  p.i32(28, 15001).i32(32, 15002).i32(36, -15003).i32(64, 45006);
  // Reactive power (×0.1 var, signed)
  p.i32(40, 2001).i32(44, 2002).i32(48, 2003).i32(68, 6006);
  // Apparent power (×0.1 VA, signed)
  p.i32(52, 3001).i32(56, 3002).i32(60, 3003).i32(72, 9006);
  // Power factors (×0.01), packed two per register: 77=A, 76=B, 79=C, 78=combined
  p.u8(77, 98).u8(76, 97).u8(79, 96).u8(78, 95);
  // Active energy (×0.1 kWh)
  p.u32(80, 100001).u32(84, 100002).u32(88, 100003).u32(116, 300006);
  // Reactive energy (×0.1 kvarh)
  p.u32(92, 200001).u32(96, 200002).u32(100, 200003).u32(120, 600006);
  // Apparent energy (×0.1 kVAh); phase A exceeds 16 bits to exercise the high word.
  p.u32(104, 300001).u32(108, 300002).u32(112, 300003).u32(124, 900006);
  return p;
}

}  // namespace

TEST(PZEM6L24Test, DecodesEveryRegisterToItsSensor) {
  Harness h;
  const auto response = make_reference_payload().response_pdu();

  h.pzem.on_response(READ_REQUEST_PDU, response);

  EXPECT_FLOAT_EQ(h.voltage_a.state, 2301 * 0.1f);
  EXPECT_FLOAT_EQ(h.voltage_b.state, 2302 * 0.1f);
  EXPECT_FLOAT_EQ(h.voltage_c.state, 2303 * 0.1f);

  EXPECT_FLOAT_EQ(h.current_a.state, 1234 * 0.01f);
  EXPECT_FLOAT_EQ(h.current_b.state, 1235 * 0.01f);
  EXPECT_FLOAT_EQ(h.current_c.state, 1236 * 0.01f);

  EXPECT_FLOAT_EQ(h.frequency.state, 5001 * 0.01f);

  EXPECT_FLOAT_EQ(h.active_power_a.state, 15001 * 0.1f);
  EXPECT_FLOAT_EQ(h.active_power_b.state, 15002 * 0.1f);
  EXPECT_FLOAT_EQ(h.active_power_c.state, -15003 * 0.1f);
  EXPECT_FLOAT_EQ(h.total_active_power.state, 45006 * 0.1f);

  EXPECT_FLOAT_EQ(h.reactive_power_a.state, 2001 * 0.1f);
  EXPECT_FLOAT_EQ(h.reactive_power_b.state, 2002 * 0.1f);
  EXPECT_FLOAT_EQ(h.reactive_power_c.state, 2003 * 0.1f);
  EXPECT_FLOAT_EQ(h.total_reactive_power.state, 6006 * 0.1f);

  EXPECT_FLOAT_EQ(h.apparent_power_a.state, 3001 * 0.1f);
  EXPECT_FLOAT_EQ(h.apparent_power_b.state, 3002 * 0.1f);
  EXPECT_FLOAT_EQ(h.apparent_power_c.state, 3003 * 0.1f);
  EXPECT_FLOAT_EQ(h.total_apparent_power.state, 9006 * 0.1f);

  EXPECT_FLOAT_EQ(h.power_factor_a.state, 98 * 0.01f);
  EXPECT_FLOAT_EQ(h.power_factor_b.state, 97 * 0.01f);
  EXPECT_FLOAT_EQ(h.power_factor_c.state, 96 * 0.01f);
  EXPECT_FLOAT_EQ(h.total_power_factor.state, 95 * 0.01f);

  EXPECT_FLOAT_EQ(h.active_energy_a.state, 100001 * 0.1f);
  EXPECT_FLOAT_EQ(h.active_energy_b.state, 100002 * 0.1f);
  EXPECT_FLOAT_EQ(h.active_energy_c.state, 100003 * 0.1f);
  EXPECT_FLOAT_EQ(h.total_active_energy.state, 300006 * 0.1f);

  EXPECT_FLOAT_EQ(h.reactive_energy_a.state, 200001 * 0.1f);
  EXPECT_FLOAT_EQ(h.reactive_energy_b.state, 200002 * 0.1f);
  EXPECT_FLOAT_EQ(h.reactive_energy_c.state, 200003 * 0.1f);
  EXPECT_FLOAT_EQ(h.total_reactive_energy.state, 600006 * 0.1f);

  EXPECT_FLOAT_EQ(h.apparent_energy_a.state, 300001 * 0.1f);
  EXPECT_FLOAT_EQ(h.apparent_energy_b.state, 300002 * 0.1f);
  EXPECT_FLOAT_EQ(h.apparent_energy_c.state, 300003 * 0.1f);
  EXPECT_FLOAT_EQ(h.total_apparent_energy.state, 900006 * 0.1f);
}

// Unconfigured sensors must be skipped rather than dereferenced.
TEST(PZEM6L24Test, PublishesOnlyConfiguredSensors) {
  PZEM6L24 pzem;
  sensor::Sensor voltage_a;
  pzem.set_voltage_sensor_a(&voltage_a);

  pzem.on_response(READ_REQUEST_PDU, make_reference_payload().response_pdu());

  EXPECT_TRUE(voltage_a.has_state());
  EXPECT_FLOAT_EQ(voltage_a.state, 2301 * 0.1f);
}

// The acknowledgement of the 0x42 reset command carries no measurements and must not be decoded.
TEST(PZEM6L24Test, IgnoresResetAcknowledgement) {
  Harness h;
  const uint8_t ack_pdu[] = {0x42, 0x00, 0x0F};

  h.pzem.on_response(RESET_REQUEST_PDU, ack_pdu);

  EXPECT_FALSE(h.voltage_a.has_state());
  EXPECT_FALSE(h.total_active_energy.has_state());
}

// A truncated response must be rejected rather than decoded from out-of-range bytes, and - like every
// other way a read can fail - must not leave the previous poll's readings published.
TEST(PZEM6L24Test, PublishesNanOnShortPayload) {
  Harness h;
  std::vector<uint8_t> short_pdu{0x04, 10};
  short_pdu.resize(12, 0x11);

  h.pzem.on_response(READ_REQUEST_PDU, make_reference_payload().response_pdu());
  ASSERT_FALSE(std::isnan(h.voltage_a.state));

  for (int i = 0; i < FAILURES_BEFORE_BLANKING; i++) {
    h.pzem.on_response(READ_REQUEST_PDU, short_pdu);
  }

  EXPECT_TRUE(std::isnan(h.voltage_a.state));
  EXPECT_TRUE(std::isnan(h.total_active_energy.state));
}

// A response carrying no payload at all - a byte-count-0 reply the hub still dispatches - is as
// undecodable as any other wrong size, and must be reported rather than passed off as an already-logged
// failure.
TEST(PZEM6L24Test, PublishesNanOnEmptyPayload) {
  Harness h;
  const uint8_t empty_pdu[] = {0x04, 0x00};

  h.pzem.on_response(READ_REQUEST_PDU, make_reference_payload().response_pdu());
  ASSERT_FALSE(std::isnan(h.voltage_a.state));

  for (int i = 0; i < FAILURES_BEFORE_BLANKING; i++) {
    h.pzem.on_response(READ_REQUEST_PDU, empty_pdu);
  }

  EXPECT_TRUE(std::isnan(h.voltage_a.state));
  EXPECT_TRUE(std::isnan(h.total_active_energy.state));
}

// A response longer than the register map is not a payload this decode table can be trusted with either -
// it did not come from the frame layout the map was written for - so it must be rejected rather than
// decoded from its first 128 bytes.
TEST(PZEM6L24Test, PublishesNanOnOversizedPayload) {
  Harness h;
  auto long_pdu = make_reference_payload().response_pdu();
  long_pdu.push_back(0x11);

  h.pzem.on_response(READ_REQUEST_PDU, make_reference_payload().response_pdu());
  ASSERT_FALSE(std::isnan(h.voltage_a.state));

  for (int i = 0; i < FAILURES_BEFORE_BLANKING; i++) {
    h.pzem.on_response(READ_REQUEST_PDU, long_pdu);
  }

  EXPECT_TRUE(std::isnan(h.voltage_a.state));
  EXPECT_TRUE(std::isnan(h.total_active_energy.state));
}

// A meter that stops answering must not leave stale readings behind.
TEST(PZEM6L24Test, PublishesNanWhenTheMeterDoesNotRespond) {
  Harness h;
  h.pzem.on_response(READ_REQUEST_PDU, make_reference_payload().response_pdu());
  ASSERT_FALSE(std::isnan(h.voltage_a.state));

  for (int i = 0; i < FAILURES_BEFORE_BLANKING; i++) {
    EXPECT_FALSE(h.pzem.on_no_response(READ_REQUEST_PDU));
  }

  EXPECT_TRUE(std::isnan(h.voltage_a.state));
  EXPECT_TRUE(std::isnan(h.total_active_power.state));
  EXPECT_TRUE(std::isnan(h.total_apparent_energy.state));
}

TEST(PZEM6L24Test, PublishesNanOnExceptionResponse) {
  Harness h;
  h.pzem.on_response(READ_REQUEST_PDU, make_reference_payload().response_pdu());
  ASSERT_FALSE(std::isnan(h.voltage_a.state));

  for (int i = 0; i < FAILURES_BEFORE_BLANKING; i++) {
    h.pzem.on_error(READ_REQUEST_PDU, modbus::ExceptionCode::ILLEGAL_DATA_ADDRESS);
  }

  EXPECT_TRUE(std::isnan(h.voltage_a.state));
  EXPECT_TRUE(std::isnan(h.total_active_energy.state));
}

// A read dropped from the transmit queue never reaches the meter, so it must blank the readings like
// the other read failures do.
TEST(PZEM6L24Test, PublishesNanWhenTheReadIsNotSent) {
  Harness h;
  h.pzem.on_response(READ_REQUEST_PDU, make_reference_payload().response_pdu());
  ASSERT_FALSE(std::isnan(h.voltage_a.state));

  for (int i = 0; i < FAILURES_BEFORE_BLANKING; i++) {
    h.pzem.on_not_sent(READ_REQUEST_PDU);
  }

  EXPECT_TRUE(std::isnan(h.voltage_a.state));
  EXPECT_TRUE(std::isnan(h.total_active_energy.state));
}

// An isolated failure is routine on a shared RS-485 bus, so the readings ride it out; the count of
// failures that have to pile up before blanking starts over as soon as a poll succeeds.
TEST(PZEM6L24Test, KeepsReadingsUntilFailuresReachTheThreshold) {
  Harness h;
  h.pzem.on_response(READ_REQUEST_PDU, make_reference_payload().response_pdu());

  for (int i = 0; i < FAILURES_BEFORE_BLANKING - 1; i++) {
    h.pzem.on_no_response(READ_REQUEST_PDU);
  }
  EXPECT_FLOAT_EQ(h.voltage_a.state, 2301 * 0.1f);

  // A good poll resets the run, so the next failures start counting from zero rather than tipping over.
  h.pzem.on_response(READ_REQUEST_PDU, make_reference_payload().response_pdu());
  for (int i = 0; i < FAILURES_BEFORE_BLANKING - 1; i++) {
    h.pzem.on_error(READ_REQUEST_PDU, modbus::ExceptionCode::SERVICE_DEVICE_FAILURE);
  }

  EXPECT_FLOAT_EQ(h.voltage_a.state, 2301 * 0.1f);
}

// update()'s refusal branch decides between two opposite outcomes - blank everything, or say nothing -
// off reads_outstanding_, so both sides are pinned against a real hub rather than assumed.
TEST(PZEM6L24Test, KeepsReadingsWhenAPollIsAbsorbedIntoAReadInFlight) {
  // Declared before the harness so it outlives it: ~ModbusClientDevice clears its frames from the hub.
  modbus::ModbusClientHub hub;
  Harness h;
  h.pzem.set_parent(&hub);
  h.pzem.set_address(0x01);
  h.pzem.on_response(READ_REQUEST_PDU, make_reference_payload().response_pdu());

  // A read entry serves at most two requests, so the hub takes two polls and refuses the third as an
  // over-cap duplicate. Two terminals are still owed, so that refusal is not a lost reading.
  h.pzem.update();
  h.pzem.update();
  h.pzem.update();

  EXPECT_FLOAT_EQ(h.voltage_a.state, 2301 * 0.1f);
  EXPECT_FLOAT_EQ(h.total_active_energy.state, 300006 * 0.1f);
}

// The same false return with nothing in flight means no callback is ever coming, so it counts as a
// failed poll like any other.
TEST(PZEM6L24Test, PublishesNanWhenThePollCannotBeQueued) {
  modbus::ModbusClientHub hub;
  Harness h;
  h.pzem.set_parent(&hub);
  h.pzem.set_address(0x01);
  h.pzem.on_response(READ_REQUEST_PDU, make_reference_payload().response_pdu());

  // Fill the transmit queue with distinct frames for another address, so this device has nothing in
  // flight and its poll is refused outright rather than absorbed.
  for (uint16_t i = 0; i < modbus::MODBUS_TX_BUFFER_SIZE; i++) {
    const uint8_t filler_pdu[] = {0x04, 0x00, static_cast<uint8_t>(i), 0x00, 0x01};
    ASSERT_TRUE(hub.queue_pdu(0x02, filler_pdu));
  }

  for (int i = 0; i < FAILURES_BEFORE_BLANKING; i++) {
    h.pzem.update();
  }

  EXPECT_TRUE(std::isnan(h.voltage_a.state));
  EXPECT_TRUE(std::isnan(h.total_active_energy.state));
}

// A failed reset command says nothing about the measurements, so it must not blank them.
TEST(PZEM6L24Test, KeepsReadingsWhenTheResetCommandFails) {
  Harness h;
  h.pzem.on_response(READ_REQUEST_PDU, make_reference_payload().response_pdu());

  for (int i = 0; i < FAILURES_BEFORE_BLANKING; i++) {
    EXPECT_FALSE(h.pzem.on_no_response(RESET_REQUEST_PDU));
    h.pzem.on_error(RESET_REQUEST_PDU, modbus::ExceptionCode::ILLEGAL_FUNCTION);
    h.pzem.on_not_sent(RESET_REQUEST_PDU);
  }

  EXPECT_FLOAT_EQ(h.voltage_a.state, 2301 * 0.1f);
  EXPECT_FLOAT_EQ(h.total_active_energy.state, 300006 * 0.1f);
}

// The reset is destructive and irreversible, so the frame it builds - in particular the phase selector -
// is pinned here rather than left to the first user who presses the button.
TEST(PZEM6L24Test, BuildsTheResetFrameForEveryPhase) {
  const std::array<std::pair<ResetPhase, uint8_t>, 5> cases{{
      {RESET_PHASE_A, 0x00},
      {RESET_PHASE_B, 0x01},
      {RESET_PHASE_C, 0x02},
      {RESET_PHASE_COMBINED, 0x03},
      {RESET_PHASE_ALL, 0x0F},
  }};

  for (const auto &[phase, selector] : cases) {
    const auto pdu = build_reset_pdu(phase);
    EXPECT_EQ(pdu[0], 0x42) << "function code for selector " << static_cast<int>(selector);
    EXPECT_EQ(pdu[1], 0x00) << "reserved byte for selector " << static_cast<int>(selector);
    EXPECT_EQ(pdu[2], selector);
  }

  // The reset frame the other tests use as a request argument must be the one the component sends.
  const auto reset_all = build_reset_pdu(RESET_PHASE_ALL);
  EXPECT_TRUE(
      std::equal(reset_all.begin(), reset_all.end(), std::begin(RESET_REQUEST_PDU), std::end(RESET_REQUEST_PDU)));
}

}  // namespace esphome::pzem6l24::testing
