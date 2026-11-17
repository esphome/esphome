#pragma once
#include <cstdio>
#include <cstdlib>
#include <random>
#include <type_traits>
#include <vector>
#include "esphome/components/remote_base/aeha_protocol.h"
#include "esphome/components/remote_base/haier_protocol.h"
#include "esphome/components/remote_base/mirage_protocol.h"
#include "esphome/core/alloc_helpers.h"
#include "esphome/core/log.h"
using namespace esphome::remote_base;

inline std::mt19937 rbh_rng(22773);
inline uint32_t rbh_r() { return rbh_rng(); }

template<typename D> void rbh_print(const char *proto, int i, const char *kind, const esphome::optional<D> &d) {
  printf("CASE %s %d %s has=%d", proto, i, kind, d.has_value() ? 1 : 0);
  if (d.has_value()) {
    printf(" n=%u data=", (unsigned) d->data.size());
    for (uint8_t b : d->data)
      printf("%02X", b);
  }
  printf("\n");
}

// kind 0 clean, 1 one timing corrupted, 2 truncated
inline RawTimings rbh_mutate(RawTimings raw, int kind) {
  raw.push_back(-20000);  // the idle gap a receiver records after the last mark
  if (kind == 1 && !raw.empty()) {
    size_t k = rbh_r() % raw.size();
    int32_t v = 100 + rbh_r() % 6000;
    raw[k] = raw[k] < 0 ? -v : v;
  } else if (kind == 2 && raw.size() > 1) {
    raw.resize(rbh_r() % raw.size());
  }
  return raw;
}
inline const char *const RBH_KIND[] = {"clean", "corrupt", "trunc"};

// Encode one frame with enc, mutate it by kind, decode it, print the case and dump what decoded.
template<typename P, typename Enc>
void rbh_case(P &proto, const char *name, const char *prefix, int i, int kind, Enc enc) {
  RemoteTransmitData t;
  enc(&t);
  RawTimings raw = rbh_mutate(t.get_data(), kind);
  auto d = proto.decode(RemoteReceiveData(raw, 25, TOLERANCE_MODE_PERCENTAGE));
  printf("%s", prefix);
  rbh_print(name, i, RBH_KIND[kind], d);
  if (!d.has_value())
    return;
  if constexpr (std::is_same_v<P, AEHAProtocol>)
    printf("ADDR %04X\n", d->address);
  proto.dump(*d);
}

inline void rbh_run() {
  AEHAProtocol aeha;
  HaierProtocol haier;
  MirageProtocol mirage;
  char prefix[48];
  int i = 0;
  for (int rep = 0; rep < 400; rep++) {
    for (int kind = 0; kind < 3; kind++) {
      uint8_t buf[48];
      // AEHA: 1 to 40 bytes (35 is the cap)
      size_t len = 1 + rbh_r() % 40;
      for (auto &b : buf)
        b = rbh_r();
      uint16_t addr = rbh_r();
      snprintf(prefix, sizeof(prefix), "AEHA len=%u addr=%04X ", (unsigned) len, addr);
      rbh_case(aeha, "aeha", prefix, i, kind, [&](RemoteTransmitData *t) { aeha.encode(t, addr, buf, len); });
      // Haier: 8 and 13 mostly, sometimes 1 to 16
      size_t hl = (rep % 4 == 3) ? 1 + rbh_r() % 16 : ((rbh_r() & 1) ? 8 : 13);
      snprintf(prefix, sizeof(prefix), "HAIER len=%u ", (unsigned) hl);
      rbh_case(haier, "haier", prefix, i, kind, [&](RemoteTransmitData *t) { haier.encode(t, buf, hl); });
      // Mirage: 14 mostly, sometimes 10 to 18
      size_t ml = (rep % 4 == 3) ? 10 + rbh_r() % 9 : 14;
      snprintf(prefix, sizeof(prefix), "MIRAGE len=%u ", (unsigned) ml);
      rbh_case(mirage, "mirage", prefix, i, kind, [&](RemoteTransmitData *t) { mirage.encode(t, buf, ml); });
      i++;
    }
  }
  fflush(stdout);
  exit(0);
}

// Loopback trigger log (ir-common.yaml): also checks that the vector idioms compile against the trigger's data type.
template<typename T> void rbh_on_frame(const char *tag, const T &data) {
  std::vector<uint8_t> v = data;
  uint32_t s = 0;
  for (auto b : data)
    s += b;
  bool eq = (v == data) && (data == v) && !(data != v);
  ESP_LOGI("loop", "%s n=%u hex=%s sum=%u eq=%d front=%u", tag, (unsigned) data.size(),
           esphome::format_hex(data).c_str(), (unsigned) s, eq ? 1 : 0, (unsigned) data.front());
}
