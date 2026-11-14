#include "esphome/components/zigbee/zigbee_endpoint_callbacks.h"

#include <cassert>
#include <cstdint>

int main() {
  esphome::zigbee::EndpointCallbacks<2> callbacks;
  int first = 0;
  int second = 0;
  assert(!callbacks.dispatch(0, 1));
  assert(!callbacks.dispatch(17, 1));
  assert(!callbacks.add(0, [](uint8_t) {}));
  assert(!callbacks.add(240, [](uint8_t) {}));
  assert(!callbacks.add(255, [](uint8_t) {}));
  assert(callbacks.add(17, [&](uint8_t value) { first += value; }));
  assert(!callbacks.add(17, [&](uint8_t) { first = -100; }));
  assert(callbacks.add(239, [&](uint8_t value) { second += value; }));
  assert(!callbacks.add(1, [](uint8_t) {}));
  assert(callbacks.dispatch(239, 3));
  assert(callbacks.dispatch(17, 7));
  assert(first == 7 && second == 3);
  assert(!callbacks.dispatch(0, 10));
  assert(!callbacks.dispatch(1, 10));
  assert(!callbacks.dispatch(240, 10));
  assert(first == 7 && second == 3);

  esphome::zigbee::EndpointCallbacks<8> full;
  int calls = 0;
  for (uint8_t endpoint = 1; endpoint <= 8; endpoint++) {
    assert(full.add(endpoint, [&](uint8_t) { calls++; }));
  }
  assert(!full.add(9, [](uint8_t) {}));
  for (uint8_t endpoint = 1; endpoint <= 8; endpoint++) {
    assert(full.dispatch(endpoint, 0));
  }
  assert(calls == 8);
  esphome::zigbee::EndpointCallbacks<0> empty;
  assert(!empty.add(1, [](uint8_t) {}));
  assert(!empty.dispatch(1, 0));
}
