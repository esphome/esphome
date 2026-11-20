#include "esphome/components/one_wire_device/source_snapshot.h"
#include <gtest/gtest.h>
using esphome::one_wire_device::DeviceModel;
using esphome::one_wire_device::SourceSnapshot;
static void write(DeviceModel &model, uint8_t byte, int16_t value) {
  for (unsigned i = 0; i < 8; i++)
    model.receive_bit((byte >> i) & 1, value, 3);
}
static uint8_t read(DeviceModel &model) {
  uint8_t byte = 0;
  for (unsigned i = 0; i < 8; i++)
    byte |= model.transmit_bit(3) << i;
  return byte;
}
TEST(OneWireDevice, CapturedSourceRemainsCoherent) {
  std::atomic<int32_t> temperature{376};
  std::atomic<uint32_t> pio{0x303};
  const auto captured = SourceSnapshot::capture(temperature, pio);
  // A writer invalidates the live source between capture and check/use.
  temperature.store(INT32_MIN);
  pio.store(0x203);
  EXPECT_TRUE(captured.available(0x28) && captured.available(0x3a));
  EXPECT_TRUE(captured.pio == 0x303);
  DeviceModel model;
  model.configure(0x28);
  write(model, 0xcc, 0);
  write(model, 0x44, static_cast<int16_t>(captured.temperature));
  model.reset();
  write(model, 0xcc, 0);
  write(model, 0xbe, 0);
  uint8_t low = read(model), high = read(model);
  EXPECT_TRUE(int16_t(low | (high << 8)) == 376);  // Never the narrowed unavailable sentinel (zero).
  const auto unavailable = SourceSnapshot::capture(temperature, pio);
  // Conversely, recovery after capture must not make an invalid captured word valid.
  temperature.store(376);
  pio.store(0x303);
  EXPECT_TRUE(!unavailable.available(0x28) && !unavailable.available(0x3a));
  EXPECT_TRUE(unavailable.available(0x01));
}
