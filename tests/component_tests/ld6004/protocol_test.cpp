#include "esphome/components/ld6004/protocol.h"
#include <cassert>
#include <vector>
#include <limits>
using namespace esphome::ld6004;
static std::vector<uint8_t> frame(uint16_t id, uint16_t type, const std::vector<uint8_t> &data) {
  std::vector<uint8_t> out{1,
                           uint8_t(id >> 8),
                           uint8_t(id),
                           uint8_t(data.size() >> 8),
                           uint8_t(data.size()),
                           uint8_t(type >> 8),
                           uint8_t(type)};
  uint8_t sum = 255;
  for (uint8_t b : out)
    sum ^= b;
  out.push_back(sum);
  if (!data.empty()) {
    sum = 255;
    for (uint8_t b : data) {
      out.push_back(b);
      sum ^= b;
    }
    out.push_back(sum);
  }
  return out;
}
static void feed(Parser &p, const std::vector<uint8_t> &bytes, unsigned &seen, uint32_t now = 1) {
  for (uint8_t b : bytes) {
    if (p.feed(b, now))
      ++seen;
  }
}
int main() {
  const std::vector<uint8_t> vendor{1, 0, 6, 0, 4, 10, 8, 254, 0, 0, 0, 0, 255};
  for (size_t split = 0; split <= vendor.size(); ++split) {
    Parser p;
    unsigned seen = 0;
    feed(p, {vendor.begin(), vendor.begin() + split}, seen);
    feed(p, {vendor.begin() + split, vendor.end()}, seen, 2);
    assert(seen == 1 && p.frame().type == 0x0A08 && p.frame().length == 4);
  }
  Parser p;
  unsigned seen = 0;
  feed(p, vendor, seen);
  feed(p, vendor, seen);
  assert(seen == 2);
  std::vector<uint8_t> oversized(MAX_PAYLOAD + 1, 0);
  const std::vector<uint8_t> embedded_ack = frame(1, 0x0201, {});
  std::copy(embedded_ack.begin(), embedded_ack.end(), oversized.begin());
  feed(p, frame(2, 0x0A08, oversized), seen);
  assert(seen == 2);  // Ignored payloads cannot become command acknowledgements.
  feed(p, vendor, seen);
  assert(seen == 3);
  seen = 2;
  auto bad = vendor;
  bad[7] ^= 2;
  feed(p, bad, seen);
  assert(seen == 2);
  bad = vendor;
  bad.back() ^= 2;
  feed(p, bad, seen);
  assert(seen == 2);
  feed(p, frame(1, 0x1234, std::vector<uint8_t>(2565)), seen);
  feed(p, vendor, seen, 1000);
  assert(seen == 3);
  feed(p, {1, 0, 0, 0, 100, 10, 4, 148, 0}, seen, 1001);
  feed(p, vendor, seen, 2000);
  assert(seen == 4);
  Tracker tracker;
  std::vector<uint8_t> data(44);
  write_u32(data.data(), 2);
  write_float(data.data() + 4, 1.5f);
  write_u32(data.data() + 20, 42);
  write_float(data.data() + 24, -2.0f);
  write_u32(data.data() + 40, 7);
  assert(tracker.update(data.data(), data.size(), 10));
  assert(tracker.slots()[0].id == 42 && tracker.slots()[1].id == 7);
  std::swap_ranges(data.begin() + 4, data.begin() + 24, data.begin() + 24);
  assert(tracker.update(data.data(), data.size(), 20));
  assert(tracker.slots()[0].id == 42 && tracker.slots()[0].x == 1.5f);
  write_float(data.data() + 4, std::numeric_limits<float>::infinity());
  assert(!tracker.update(data.data(), data.size(), 30));
  assert(tracker.slots()[0].active);
  assert(!tracker.update(data.data(), 43, 30));
  assert(tracker.expire(1021, 1000));
  assert(!tracker.valid());
  assert(tracker.update(vendor.data() + 8, 4, 1030));
  assert(tracker.valid() && tracker.count() == 0);
  // Empty reports clear presence, corruption and impossible counts leave the last observation intact.
  write_u32(data.data(), 0xFFFFFFFF);
  assert(!tracker.update(data.data(), data.size(), 1031));
  data.assign(44, 0);
  write_u32(data.data(), 2);
  assert(!tracker.update(data.data(), data.size(), 1032));  // Duplicate identities.
  assert(tracker.valid() && tracker.count() == 0);
  // A truncated frame times out across the millisecond counter wrap.
  Parser wrapped_parser;
  unsigned wrapped_seen = 0;
  feed(wrapped_parser, {1, 0, 0}, wrapped_seen, 0xFFFFFF00);
  feed(wrapped_parser, vendor, wrapped_seen, 100);
  assert(wrapped_seen == 1);
  Frame report;
  report.type = 0x0A0A;
  report.length = 16;
  report.data.fill(0);
  report.data[4] = 2;
  assert(!valid_report(report));
  report.data[4] = 1;
  assert(valid_report(report));
  report.type = 0x1234;
  assert(!valid_report(report));
  Startup startup;
  assert(startup.poll(0) == (Startup::TARGETS | Startup::VERSION));
  assert(startup.poll(999) == 0);
  startup.received(Startup::VERSION);
  assert(startup.poll(1000) == Startup::TARGETS);
  assert(startup.poll(2000) == Startup::TARGETS);
  assert(startup.poll(3000) == 0);
  Startup complete;
  complete.received(Startup::TARGETS | Startup::VERSION);
  assert(complete.poll(0) == 0);
  Startup wrap;
  assert(wrap.poll(UINT32_MAX - 500) != 0);
  assert(wrap.poll(498) == 0);
  assert(wrap.poll(499) != 0);
  DiagnosticCounter counter;
  assert(counter.changed(0));
  assert(!counter.changed(0));
  assert(counter.changed(1));
  assert(!counter.changed(1));
  uint8_t output[40];
  assert(encode(1, 0xFFFF, nullptr, 0, output) == 8);
  const uint8_t request[]{1, 0, 1, 0, 0, 255, 255, 255};
  for (unsigned i = 0; i < 8; ++i)
    assert(output[i] == request[i]);
  const uint8_t enable[]{8, 0, 0, 0};
  assert(encode(1, 0x0201, enable, sizeof(enable), output) == 13);
  feed(p, {output, output + 13}, seen, 3000);
  assert(p.frame().type == 0x0201 && read_u32(p.frame().data.data()) == 8);
  report.type = 0xFFFF;
  report.length = 4;
  assert(valid_report(report));
  report.length = 0;
  assert(!valid_report(report));
  return 0;
}
