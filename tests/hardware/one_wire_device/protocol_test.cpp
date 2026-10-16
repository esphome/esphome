// Exercise the real device model through bit slots, without a second simulator.
#include "esphome/components/one_wire_device/device_model.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
using esphome::one_wire_device::DeviceModel;
static void write(DeviceModel &d, uint8_t b, int16_t value) {
  for (unsigned i = 0; i < 8; ++i)
    d.receive_bit((b >> i) & 1, value, 3);
}
static uint8_t read(DeviceModel &d) {
  uint8_t b = 0;
  for (unsigned i = 0; i < 8; ++i) {
    assert(d.is_transmitting());
    b |= d.transmit_bit(3) << i;
  }
  return b;
}
static uint8_t crc(const uint8_t *p, unsigned n) {
  uint8_t c = 0;
  while (n--) {
    uint8_t v = *p++;
    for (unsigned b = 0; b < 8; ++b) {
      bool x = (c ^ v) & 1;
      c >>= 1;
      if (x)
        c ^= 0x8c;
      v >>= 1;
    }
  }
  return c;
}
static void select(DeviceModel &d) {
  d.reset();
  write(d, 0xcc, 0);
}
int main() {
  for (uint8_t family : {0x28, 0x22, 0x10}) {
    DeviceModel d;
    d.configure(family);
    for (int16_t value : {376, -162, 1360, -880, 2000}) {
      select(d);
      write(d, 0x44, value);
      assert(read(d) == 0xff);
      select(d);
      write(d, 0xbe, 0);
      uint8_t sp[9];
      for (auto &b : sp)
        b = read(d);
      assert(crc(sp, 8) == sp[8]);
      int raw = int16_t(sp[0] | (sp[1] << 8));
      if (family == 0x10)
        assert((raw >> 1) * 16 + 12 - sp[6] == value && sp[7] == 16);
      else
        assert(raw == value && sp[6] != 0x0c);
    }
    d.reset();
    write(d, 0x33, 0);
    for (unsigned i = 0; i < 8; ++i)
      read(d);
    write(d, 0x44, 376);
    assert(read(d) == 0xff);
    select(d);
    write(d, 0x4e, 0);
    write(d, 100, 0);
    write(d, static_cast<uint8_t>(-11), 0);
    if (family != 0x10)
      write(d, 0x7f, 0);
    select(d);
    write(d, 0x44, -162);
    d.reset();
    write(d, 0xec, 0);
    assert(d.is_transmitting() == (family != 0x10));
    select(d);
    write(d, 0x4e, 0);
    write(d, 11, 0);
    write(d, 2, 0);
    if (family != 0x10)
      write(d, 0x1f, 0);
    select(d);
    write(d, 0x48, 0);
    select(d);
    write(d, 0xb8, 0);
    select(d);
    write(d, 0xbe, 0);
    uint8_t sp[9];
    for (auto &b : sp)
      b = read(d);
    assert(sp[2] == 11 && sp[3] == 2);
  }
  DeviceModel io;
  io.configure(0x3a);
  select(io);
  write(io, 0xf5, 0);
  assert(read(io) == 0x0f);
  assert(read(io) == 0x0f);
  select(io);
  write(io, 0x5a, 0);
  write(io, 0xfc, 0);
  write(io, 0x03, 0);
  assert(read(io) == 0xaa);
  assert(read(io) == 0xf0);
  write(io, 0xfd, 0);
  write(io, 0x02, 0);
  assert(read(io) == 0xaa);
  assert(read(io) == 0xc3);
  write(io, 0xff, 0);
  write(io, 0xff, 0);
  assert(read(io) == 0xff);
  DeviceModel id;
  id.configure(0x01);
  select(id);
  write(id, 0x44, 0);
  assert(!id.is_transmitting());
  std::puts("PASS: three thermometer encodings/CRC, ready polling, RAM copy/recall, DS2413 read/write/complement, "
            "ROM-only isolation");
}
