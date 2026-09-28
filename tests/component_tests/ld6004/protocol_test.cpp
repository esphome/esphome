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
  assert(valid_bounds(-1, 1));
  assert(!valid_bounds(1, 1));
  assert(!valid_bounds(NAN, 1));
  assert(!valid_bounds(0, INFINITY));
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
  Frame report;
  report.type = 0x0A12;
  report.length = 4;
  report.data[0] = 4;
  assert(valid_report(report));
  report.data[1] = 1;
  assert(!valid_report(report));
  report.data[1] = 0;
  report.length = 1;
  assert(valid_report(report));
  report.type = 0x0A13;
  assert(!valid_report(report));
  report.length = 4;
  assert(valid_report(report));
  report.type = 0x0A0A;
  report.length = 16;
  report.data.fill(0);
  report.data[4] = 2;
  assert(!valid_report(report));
  report.data[4] = 1;
  assert(valid_report(report));
  report.type = 0x1234;
  assert(!valid_report(report));
  Command setting;
  assert(make_number_command(3, 500, setting));
  assert(setting.type == 0x0205 && setting.query == 0x19 && setting.report == 0x0A13);
  assert(setting.length == 4 && setting.data[0] == 244 && setting.data[1] == 1);
  assert(make_number_command(4, 300, setting));
  assert(setting.type == 0x0206 && read_u32(setting.data.data()) == 300);
  assert(!make_number_command(5, 0, setting));
  assert(!make_number_command(0, -1, setting));
  assert(!make_number_command(0, 1.5f, setting));
  assert(!make_number_command(0, std::numeric_limits<float>::infinity(), setting));
  assert(!make_number_command(0, 16777216, setting));
  const uint8_t work_commands[]{0x17, 0x16, 0x1B, 0x1C, 0x24};
  for (size_t i = 0; i < 5; ++i) {
    assert(make_select_command(3, i, setting));
    assert(setting.data[0] == work_commands[i] && setting.query == 0x18 && setting.report == 0x0A12);
  }
  const uint8_t p20_commands[]{0x1D, 0x1E, 0x20, 0x21, 0x22, 0x23};
  for (size_t i = 0; i < 6; ++i) {
    assert(make_select_command(4, i, setting));
    assert(setting.data[0] == p20_commands[i] && setting.query == 0x1F && setting.report == 0x0A15);
  }
  assert(!make_select_command(0, 3, setting));
  assert(!make_select_command(5, 0, setting));
  CommandQueue q;
  Command command;
  command.type = 0x0203;
  command.length = 4;
  command.query = 5;
  command.report = 0x0A0D;
  assert(q.push(command));
  const Command *sent = q.poll(1);
  assert(sent && sent->id != 0);
  assert(!q.reply(100, 0x0A04, 4, 1));
  assert(q.reply(101, 0x0203, 0, 1));
  assert(q.busy());
  sent = q.poll(2);
  assert(sent && sent->type == 0x0201 && sent->data[0] == 5);
  assert(!q.reply(101, 0x0A0D, 4, 2));
  assert(!q.reply(100, 0x0A0D, 4, 2));
  assert(q.reply(102, 0x0A0D, 4, 2));
  assert(!q.busy());
  command.report = 0;
  command.query = 0;
  command.retry = false;
  assert(q.push(command));
  assert(q.poll(3));
  assert(!q.poll(1003));
  assert(!q.busy() && q.failures() == 1);
  q.recover();
  command.retry = true;
  assert(q.push(command));
  assert(q.poll(1004));
  assert(q.poll(2004));
  assert(q.poll(3004));
  assert(!q.poll(4004));
  assert(!q.busy() && q.failures() == 2);
  q.recover();
  for (unsigned i = 0; i < 8; ++i)
    assert(q.push(command));
  assert(!q.push(command));
  CommandQueue wrap;
  assert(wrap.push(command));
  assert(wrap.poll(0));
  assert(!wrap.reply(65535, 0x0A04, 4, 1));
  assert(wrap.reply(0, 0x0203, 0, 2));
  assert(wrap.push(command));
  assert(wrap.poll(3));
  assert(!wrap.reply(65535, 0x0203, 0, 4));
  assert(wrap.reply(1, 0x0203, 0, 5));
  assert(wrap.push(command));
  assert(wrap.poll(3006));
  assert(wrap.reply(0, 0x0203, 0, 3007));
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
  CommandQueue late;
  Command output_command;
  output_command.type = 0x0201;
  output_command.length = 4;
  output_command.data[0] = 8;
  assert(late.push(output_command));
  assert(late.poll(0));
  assert(late.poll(1000));  // Retrying makes a later receipt ambiguous.
  bool output_state;
  assert(!late.reply(10, 0x0201, 0, 1001));
  assert(late.blocked());
  assert(late.status() == 2);
  assert(!late.consume_output_ack(output_state));
  output_command.data[0] = 9;
  assert(!late.push(output_command));
  assert(!late.reply(11, 0x0201, 0, 1002));
  late.recover();
  assert(late.status() == 0);
  assert(late.push(output_command));
  assert(late.status() == 1);
  assert(late.poll(2000));
  assert(late.reply(12, 0x0201, 0, 2001));
  assert(late.consume_output_ack(output_state) && !output_state);
  assert(!late.consume_output_ack(output_state));
  late.recover();
  output_command.retry = false;
  assert(late.push(output_command));
  assert(late.poll(0));
  assert(!late.poll(1000));
  assert(late.blocked() && !late.push(output_command));
  assert(!late.reply(13, 0x0201, 0, 1001));
  assert(!late.consume_output_ack(output_state));
  for (uint8_t first_bound : {uint8_t(1), uint8_t(2)}) {
    CommandQueue z_queue;
    z_queue.update_z_range(-6, 6);
    Command bound;
    bound.type = 0x0204;
    bound.length = 8;
    bound.query = 0x12;
    bound.report = 0x0A10;
    bound.z_bound = first_bound;
    bound.z_value = first_bound == 1 ? -5 : 5;
    assert(z_queue.push(bound));
    bound.z_bound = 3 - first_bound;
    bound.z_value = first_bound == 1 ? 5 : -5;
    assert(z_queue.push(bound));
    const Command *z_command = z_queue.poll(0);
    assert(z_command);
    assert(read_float(z_command->data.data()) == (first_bound == 1 ? -5 : -6));
    assert(read_float(z_command->data.data() + 4) == (first_bound == 1 ? 6 : 5));
    assert(z_queue.reply(1, 0x0204, 0, 0));
    assert(z_queue.poll(1));
    z_queue.update_z_range(first_bound == 1 ? -5 : -6, first_bound == 1 ? 6 : 5);
    assert(z_queue.reply(2, 0x0A10, 8, 1));
    z_command = z_queue.poll(2);
    assert(z_command);
    assert(read_float(z_command->data.data()) == -5 && read_float(z_command->data.data() + 4) == 5);
  }
  CommandQueue retried_readback;
  assert(make_number_command(0, 30, setting));
  assert(retried_readback.push(setting));
  assert(retried_readback.poll(0));
  assert(retried_readback.poll(1000));
  assert(retried_readback.reply(20, 0x0203, 0, 1001));
  assert(retried_readback.poll(1002));
  assert(retried_readback.reply(21, 0x0A0D, 4, 1003));
  assert(retried_readback.blocked());
  assert(!retried_readback.consume_output_ack(output_state));
  for (uint32_t elapsed : {999U, 1000U, 10000U}) {
    CommandQueue deadline;
    output_command.retry = true;
    output_command.data[0] = 8;
    assert(deadline.push(output_command));
    assert(deadline.poll(100));
    output_command.data[0] = 9;
    assert(deadline.push(output_command));
    const bool accepted = deadline.reply(1, 0x0201, 0, 100 + elapsed);
    assert(accepted == (elapsed < 1000));
    assert(deadline.consume_output_ack(output_state) == (elapsed < 1000));
    if (elapsed >= 1000) {
      const Command *retry = deadline.poll(100 + elapsed);
      assert(retry && retry->data[0] == 8);  // The next OFF command was not released.
      assert(!deadline.reply(2, 0x0201, 0, 101 + elapsed));
      assert(deadline.blocked() && !deadline.consume_output_ack(output_state));
    }
  }
  CommandQueue terminal;
  output_command.retry = true;
  assert(terminal.push(output_command));
  assert(terminal.poll(0));
  assert(terminal.poll(1000));
  assert(terminal.poll(2000));
  assert(!terminal.reply(1, 0x0201, 0, 3000));
  assert(terminal.blocked());
  CommandQueue exact;
  output_command.retry = false;
  assert(exact.push(output_command));
  assert(exact.poll(100));
  assert(!exact.reply(1, 0x0201, 0, 1100));
  assert(exact.blocked());
  CommandQueue timer_wrap;
  assert(timer_wrap.push(output_command));
  assert(timer_wrap.poll(0xFFFFFF00));
  assert(!timer_wrap.reply(1, 0x0201, 0, 744));
  assert(timer_wrap.blocked());
  // Startup enables tracking before querying settings. The switch remains unknown until receipt.
  CommandQueue startup;
  Command enable;
  enable.type = 0x0201;
  enable.length = 4;
  write_u32(enable.data.data(), 8);
  assert(startup.push(enable));
  bool output_enabled = false;
  assert(!startup.consume_output_ack(output_enabled));
  assert(startup.poll(0));
  assert(!startup.reply(1, 0x0A04, 4, 1));
  assert(!startup.consume_output_ack(output_enabled));
  assert(startup.reply(2, 0x0201, 0, 2));
  assert(startup.consume_output_ack(output_enabled) && output_enabled);
  assert(!startup.busy());
  DiagnosticCounter counter;
  assert(counter.changed(0));
  assert(!counter.changed(0));
  assert(counter.changed(1));
  assert(!counter.changed(1));
  uint8_t output[40];
  assert(encode(1, 0xFFFF, nullptr, 0, output) == 8);
  const uint8_t request[]{1, 0, 1, 0, 0, 255, 255, 255};
  q.recover();
  for (unsigned i = 0; i < 8; ++i)
    assert(output[i] == request[i]);
}
