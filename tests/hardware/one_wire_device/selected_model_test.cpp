// Compile with ONE_WIRE_DEVICE_SELECTED_MODEL and one USE_ONE_WIRE_DEVICE_<model> define.
#include "esphome/components/one_wire_device/device_model.h"
#include <cassert>
#include <cstdio>
using esphome::one_wire_device::DeviceModel;
static void write(DeviceModel &model, uint8_t byte) {
  for (unsigned bit = 0; bit < 8; bit++)
    model.receive_bit((byte >> bit) & 1, 376, 3);
}
static void select(DeviceModel &model) {
  model.reset();
  write(model, 0xcc);
}
int main() {
  DeviceModel model;
#ifdef USE_ONE_WIRE_DEVICE_DS2413
  model.configure(0x3a);
  select(model);
  write(model, 0xf5);
  assert(model.is_transmitting());
  model.configure(0x28);
  select(model);
  write(model, 0xbe);
  assert(!model.is_transmitting());
#elif defined(USE_ONE_WIRE_DEVICE_DS2401)
  model.configure(0x01);
  model.reset();
  write(model, 0x33);
  assert(model.is_transmitting());
  model.configure(0x28);
  select(model);
  write(model, 0xbe);
  assert(!model.is_transmitting());
  model.configure(0x3a);
  select(model);
  write(model, 0xf5);
  assert(!model.is_transmitting());
#else
#ifdef USE_ONE_WIRE_DEVICE_DS18S20
  model.configure(0x10);
#elif defined(USE_ONE_WIRE_DEVICE_DS1822)
  model.configure(0x22);
#else
  model.configure(0x28);
#endif
  select(model);
  write(model, 0xbe);
  assert(model.is_transmitting());
  model.configure(0x3a);
  select(model);
  write(model, 0xf5);
  assert(!model.is_transmitting());
#endif
  std::puts("PASS: selected model responds; unrelated function state machine is unavailable");
}
