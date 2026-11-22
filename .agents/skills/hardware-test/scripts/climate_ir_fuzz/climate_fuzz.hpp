#ifndef FUZZ22773_CLIMATE_FUZZ_H
#define FUZZ22773_CLIMATE_FUZZ_H
// Test only (esphome#22773 A/B): feeds a climate's own frames, mutated copies and random frames to on_receive
// and prints whether each was accepted and the state that followed.
#include <cstdio>
#include <random>
#include <vector>
#include "esphome/components/climate_ir/climate_ir.h"
#include "esphome/components/remote_base/aeha_protocol.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/application.h"
#include "esphome/core/preferences.h"

namespace esphome::fuzz22773 {

class Cap : public remote_base::RemoteTransmitterBase {
 public:
  Cap() : RemoteTransmitterBase(nullptr) {}
  std::vector<remote_base::RawTimings> frames;

 protected:
  void send_internal(uint32_t, uint32_t) override { this->frames.push_back(this->temp_.get_data()); }
};

template<typename C> void prep(C &c, Cap &tx) {
  if (global_preferences != nullptr)
    global_preferences->reset();
  static char name[] = "fuzz";
  static char fn[] = "";
  App.pre_setup(name, sizeof(name) - 1, fn, 0);
  c.set_transmitter(&tx);
  static_cast<Component &>(c).setup();
}

template<typename C> void print_state(const char *tag, int i, bool ok, C &c) {
  printf("FUZZ %s %d ok=%d mode=%d t=%.2f fan=%d swing=%d\n", tag, i, ok ? 1 : 0, (int) c.mode,
         (double) c.target_temperature, c.fan_mode.has_value() ? (int) *c.fan_mode : -1, (int) c.swing_mode);
}

// A random AEHA frame of len bytes; half start with the climate's header (set_header), 3 in 4 use its address
template<typename Header>
void aeha_random(std::mt19937 &rng, remote_base::RawTimings &raw, size_t len, uint16_t address, Header set_header) {
  uint8_t d[35];
  for (auto &b : d)
    b = rng();
  if (rng() % 2)
    set_header(d);
  remote_base::RemoteTransmitData tx;
  remote_base::AEHAProtocol().encode(&tx, (rng() % 4) ? address : uint16_t(rng()), d, len);
  raw = tx.get_data();
}

// make() returns a fresh climate; random_frame(rng, raw) fills raw with a random frame of the protocol, without the
// trailing idle gap
template<typename Make, typename Rand> void run(const char *tag, Make make, Rand random_frame) {
  std::mt19937 rng(22773);
  // Valid frames: everything the climate itself sends
  std::vector<remote_base::RawTimings> valid;
  {
    Cap tx;
    auto c = make();
    prep(*c, tx);
    for (auto mode : {climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_COOL, climate::CLIMATE_MODE_HEAT,
                      climate::CLIMATE_MODE_DRY, climate::CLIMATE_MODE_FAN_ONLY, climate::CLIMATE_MODE_HEAT_COOL})
      for (float t : {16.0f, 18.0f, 21.0f, 24.0f, 27.0f, 30.0f, 60.0f, 66.0f, 72.0f, 80.0f, 88.0f})
        for (auto fan : {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                         climate::CLIMATE_FAN_HIGH}) {
          auto call = c->make_call();
          call.set_mode(mode);
          call.set_target_temperature(t);
          call.set_fan_mode(fan);
          call.perform();
        }
    valid = tx.frames;
    for (auto &v : valid)
      v.push_back(-20000);  // the idle gap a receiver records after the last mark
  }
  int i = 0;
  auto feed = [&](const remote_base::RawTimings &raw, const char *kind) {
    Cap tx;
    auto c = make();
    prep(*c, tx);
    remote_base::RemoteReceiveData data(raw, 25, remote_base::TOLERANCE_MODE_PERCENTAGE);
    bool ok = static_cast<remote_base::RemoteReceiverListener &>(*c).on_receive(data);
    printf("%s ", kind);
    print_state(tag, i++, ok, *c);
  };
  for (const auto &v : valid) {
    feed(v, "valid");
    for (int m = 0; m < 4; m++) {
      auto raw = v;
      if (raw.size() < 8)
        continue;
      // a space swapped with another space's length flips a bit (or does nothing when they are equal)
      size_t a = 3 + rng() % (raw.size() - 3), b = 3 + rng() % (raw.size() - 3);
      if (raw[a] < 0 && raw[b] < 0)
        std::swap(raw[a], raw[b]);
      else
        raw[a] = raw[a] < 0 ? -int32_t(100 + rng() % 3000) : int32_t(100 + rng() % 3000);
      feed(raw, "mut");
    }
    auto t = v;
    t.resize(rng() % t.size());
    feed(t, "trunc");
  }
  for (int r = 0; r < 3000; r++) {
    remote_base::RawTimings raw;
    random_frame(rng, raw);
    raw.push_back(-20000);
    feed(raw, "rand");
  }
  fflush(stdout);
}

}  // namespace esphome::fuzz22773
#endif
