#pragma once
#include <cstdio>
#include <cstdlib>
#include <random>
#include "esphome/components/remote_base/aeha_protocol.h"
#include "esphome/components/remote_base/haier_protocol.h"
#include "esphome/components/remote_base/mirage_protocol.h"
using namespace esphome::remote_base;

static std::mt19937 rbh_rng(22773);
static uint32_t rbh_r() { return rbh_rng(); }

template<typename D> static void rbh_print(const char *proto, int i, const char *kind, const esphome::optional<D> &d) {
  printf("CASE %s %d %s has=%d", proto, i, kind, d.has_value() ? 1 : 0);
  if (d.has_value()) {
    printf(" n=%u data=", (unsigned) d->data.size());
    for (uint8_t b : d->data)
      printf("%02X", b);
  }
  printf("\n");
}

// kind 0 clean, 1 one timing corrupted, 2 truncated
static RawTimings rbh_mutate(RawTimings raw, int kind) {
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
static const char *RBH_KIND[] = {"clean", "corrupt", "trunc"};

static void rbh_run() {
  AEHAProtocol aeha;
  HaierProtocol haier;
  MirageProtocol mirage;
  int i = 0;
  for (int rep = 0; rep < 400; rep++) {
    for (int kind = 0; kind < 3; kind++) {
      uint8_t buf[48];
      // AEHA: 1 to 40 bytes (35 is the cap)
      size_t len = 1 + rbh_r() % 40;
      for (auto &b : buf)
        b = rbh_r();
      uint16_t addr = rbh_r();
      {
        RemoteTransmitData t;
        aeha.encode(&t, addr, buf, len);
        RawTimings raw = rbh_mutate(t.get_data(), kind);
        auto d = aeha.decode(RemoteReceiveData(raw, 25, TOLERANCE_MODE_PERCENTAGE));
        printf("AEHA len=%u addr=%04X ", (unsigned) len, addr);
        rbh_print("aeha", i, RBH_KIND[kind], d);
        if (d.has_value()) {
          printf("ADDR %04X\n", d->address);
          aeha.dump(*d);
        }
      }
      // Haier: 8 and 13 mostly, sometimes 1 to 16
      size_t hl = (rep % 4 == 3) ? 1 + rbh_r() % 16 : ((rbh_r() & 1) ? 8 : 13);
      {
        RemoteTransmitData t;
        haier.encode(&t, buf, hl);
        RawTimings raw = rbh_mutate(t.get_data(), kind);
        auto d = haier.decode(RemoteReceiveData(raw, 25, TOLERANCE_MODE_PERCENTAGE));
        printf("HAIER len=%u ", (unsigned) hl);
        rbh_print("haier", i, RBH_KIND[kind], d);
        if (d.has_value())
          haier.dump(*d);
      }
      // Mirage: 14 mostly, sometimes 10 to 18
      size_t ml = (rep % 4 == 3) ? 10 + rbh_r() % 9 : 14;
      {
        RemoteTransmitData t;
        mirage.encode(&t, buf, ml);
        RawTimings raw = rbh_mutate(t.get_data(), kind);
        auto d = mirage.decode(RemoteReceiveData(raw, 25, TOLERANCE_MODE_PERCENTAGE));
        printf("MIRAGE len=%u ", (unsigned) ml);
        rbh_print("mirage", i, RBH_KIND[kind], d);
        if (d.has_value())
          mirage.dump(*d);
      }
      i++;
    }
  }
  fflush(stdout);
  exit(0);
}
