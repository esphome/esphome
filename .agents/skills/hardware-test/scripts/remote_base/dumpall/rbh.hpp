#pragma once
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>
#include "dumpers.h"

static std::mt19937 rbh_rng(22814);
static uint32_t rbh_r() { return rbh_rng(); }
static RawDumper rbh_raw;

static void rbh_dump_all(const RawTimings &raw, uint32_t tol, ToleranceMode mode, const char *what, int i) {
  printf("INPUT %d %s tol=%u mode=%d n=%u\n", i, what, (unsigned) tol, (int) mode, (unsigned) raw.size());
  bool any = false;
  for (auto &nd : RB_DUMPERS) {
    RemoteReceiveData src(raw, tol, mode);
    bool r = nd.d->dump(src);
    if (r) {
      printf("HIT %s\n", nd.name);
      any = true;
    }
  }
  if (!any) {
    RemoteReceiveData src(raw, tol, mode);
    rbh_raw.dump(src);
  }
  RemoteReceiveData s1(raw, tol, mode);
  auto a = AOKProtocol().decode(s1);
  if (a)
    printf("AOK id=%06X ch=%04X cmd=%02X\n", (unsigned) a->remote_id, a->channels, a->command);
  RemoteReceiveData s2(raw, tol, mode);
  auto c = CardinS438Protocol().decode(s2);
  if (c)
    printf("CARDIN code=%04X ch=%u\n", c->code, c->channel);
  RemoteReceiveData s3(raw, tol, mode);
  auto d = DraytonProtocol().decode(s3);
  if (d)
    printf("DRAYTON addr=%04X ch=%u cmd=%u data=%u\n", d->address, d->channel, d->command, d->data);
  RemoteReceiveData s4(raw, tol, mode);
  auto b = Beo4Protocol().decode(s4);
  if (b)
    printf("BEO4 src=%u cmd=%u rep=%u\n", b->source, b->command, b->repeats);
}

struct Mode {
  uint32_t tol;
  ToleranceMode mode;
};
static const Mode RBH_MODES[] = {{25, TOLERANCE_MODE_PERCENTAGE},
                                 {10, TOLERANCE_MODE_PERCENTAGE},
                                 {150, TOLERANCE_MODE_TIME},
                                 {60, TOLERANCE_MODE_TIME}};

// One mark or space nudged to just inside or just outside the tolerance of its own length
static int32_t rbh_edge(int32_t v, const Mode &m) {
  int32_t n = v < 0 ? -v : v;
  int32_t lo, hi;
  if (m.mode == TOLERANCE_MODE_TIME) {
    lo = n - (int32_t) m.tol;
    hi = n + (int32_t) m.tol;
  } else {
    lo = int32_t(100 - m.tol) * n / 100;
    hi = int32_t(100 + m.tol) * n / 100;
  }
  int32_t pick[] = {lo - 1, lo, hi, hi + 1};
  int32_t e = pick[rbh_r() % 4];
  if (e < 1)
    e = 1;
  return v < 0 ? -e : e;
}

static RawTimings rbh_frame(int proto) {
  RemoteTransmitData t;
  switch (proto) {
    case 0:
      AOKProtocol().encode(&t, AOKData{rbh_r() & 0xFFFFFF, (uint16_t) rbh_r(), (uint8_t) rbh_r()});
      break;
    case 1: {
      CardinS438Data c;
      c.code = rbh_r();
      c.channel = rbh_r() % 4;
      c.seed = rbh_r();
      c.repeats = 1 + rbh_r() % 3;
      CardinS438Protocol().encode(&t, c);
      break;
    }
    case 2:
      DraytonProtocol().encode(&t, DraytonData{(uint16_t) rbh_r(), 0, 0, (uint8_t) (rbh_r() & 0x7F)});
      break;
    default:
      Beo4Protocol().encode(&t, Beo4Data{(uint8_t) rbh_r(), (uint8_t) rbh_r(), 0});
      break;
  }
  // A receiver sees back to back marks (or spaces) as one longer timing
  RawTimings merged;
  for (int32_t v : t.get_data()) {
    if (!merged.empty() && (merged.back() > 0) == (v > 0))
      merged.back() += v;
    else
      merged.push_back(v);
  }
  return merged;
}

static void rbh_noise(RawTimings &raw, int n) {
  bool mark = raw.empty() || raw.back() < 0;
  for (int i = 0; i < n; i++, mark = !mark) {
    int32_t v = 50 + rbh_r() % 12000;
    raw.push_back(mark ? v : -v);
  }
}

static const char *RBH_PROTO[] = {"aok", "cardin", "drayton", "beo4"};

struct RbhInput {
  std::string what;
  int mode;
  RawTimings raw;
};
static void rbh_generate(std::vector<RbhInput> &out) {
  for (int rep = 0; rep < 250; rep++) {
    for (int proto = 0; proto < 4; proto++) {
      RawTimings f = rbh_frame(proto);
      for (int variant = 0; variant < 9; variant++) {
        RawTimings raw;
        if (variant == 1 || variant == 4)
          rbh_noise(raw, 1 + rbh_r() % 30);  // leading noise
        if (!raw.empty() && raw.back() > 0 && f[0] > 0)
          raw.push_back(-(int32_t) (3000 + rbh_r() % 9000));
        int copies = (variant == 2 || variant == 4) ? 2 + rbh_r() % 2 : 1;  // repeats
        for (int c = 0; c < copies; c++) {
          if (c > 0 && raw.back() > 0)
            raw.push_back(-(int32_t) (5000 + rbh_r() % 8000));
          raw.insert(raw.end(), f.begin(), f.end());
        }
        for (int mi = 0; mi < 4; mi++) {
          const auto &m = RBH_MODES[mi];
          RawTimings x = raw;
          if (variant == 3) {  // nudge about a quarter of the timings to the tolerance edges
            for (auto &v : x)
              if (rbh_r() % 4 == 0)
                v = rbh_edge(v, m);
          } else if (variant >= 5 && proto < 3) {  // only the sync mark, at one tolerance edge, the rest exact
            const int32_t nominal[] = {5000, 6400, 1000};
            int32_t n = nominal[proto], lo, hi;
            if (m.mode == TOLERANCE_MODE_TIME) {
              lo = n - (int32_t) m.tol;
              hi = n + (int32_t) m.tol;
            } else {
              lo = int32_t(100 - m.tol) * n / 100;
              hi = int32_t(100 + m.tol) * n / 100;
            }
            const int32_t edge[] = {lo - 1, lo, hi, hi + 1};
            for (auto &v : x) {
              if (v == n) {
                v = edge[variant - 5];
                if (proto == 2)
                  break;  // Drayton: the first 1000 us mark is the sync
              }
            }
          } else if (variant == 4) {  // jitter every timing by up to 30%
            for (auto &v : x) {
              int32_t n = v < 0 ? -v : v;
              n = n + (int32_t) (n * ((int) (rbh_r() % 61) - 30) / 100);
              if (n < 1)
                n = 1;
              v = v < 0 ? -n : n;
            }
          }
          x.push_back(-20000);
          char what[32];
          snprintf(what, sizeof(what), "%s/v%d", RBH_PROTO[proto], variant);
          out.push_back({what, mi, x});
        }
      }
    }
  }
  for (int r = 0; r < 4000; r++) {  // random timing corpus
    RawTimings raw;
    rbh_noise(raw, 4 + rbh_r() % 250);
    raw.push_back(-20000);
    out.push_back({"random", r % 4, raw});
  }
}

static void rbh_run() {
  std::vector<RbhInput> in;
  const char *path = getenv("RBH_CORPUS");
  FILE *f = path ? fopen(path, "rb") : nullptr;
  if (f != nullptr) {
    char what[64];
    int mode;
    unsigned n;
    while (fscanf(f, "%63s %d %u", what, &mode, &n) == 3) {
      RawTimings raw(n);
      for (auto &v : raw)
        fscanf(f, "%d", &v);
      in.push_back({what, mode, raw});
    }
    fclose(f);
  } else {
    rbh_generate(in);
    if (path != nullptr && (f = fopen(path, "wb")) != nullptr) {
      for (auto &x : in) {
        fprintf(f, "%s %d %u", x.what.c_str(), x.mode, (unsigned) x.raw.size());
        for (auto v : x.raw)
          fprintf(f, " %d", v);
        fprintf(f, "\n");
      }
      fclose(f);
    }
  }
  int i = 0;
  for (auto &x : in)
    rbh_dump_all(x.raw, RBH_MODES[x.mode].tol, RBH_MODES[x.mode].mode, x.what.c_str(), i++);
  fflush(stdout);
  exit(0);
}
