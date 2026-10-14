#include "esphome/components/haier/water_heater/hon_parser.h"
#include <cassert>
#include <sstream>
#include <string>
#include <vector>
using namespace esphome::haier::water_heater_protocol;
namespace esphome::haier::testing {
std::vector<uint8_t> bytes(const char *hex) {
  std::istringstream stream(hex);
  unsigned byte;
  std::vector<uint8_t> result;
  while (stream >> std::hex >> byte)
    result.push_back(byte);
  return result;
}
unsigned feed(FrameParser &parser, const std::vector<uint8_t> &wire) {
  unsigned count = 0;
  for (auto byte : wire)
    count += parser.feed(byte);
  return count;
}
void run() {
  FrameParser parser;
  Status status{};
  uint8_t wire[28];
  const uint8_t version[] = {0x00, 0x07};
  const uint8_t query[] = {0x4D, 0x01};
  auto check_tx = [&](uint8_t type, const uint8_t *payload, size_t size, const char *expected) {
    const auto count = encode_frame(type, payload, size, wire);
    assert(std::vector<uint8_t>(wire, wire + count) == bytes(expected));
    assert(feed(parser, std::vector<uint8_t>(wire, wire + count)) == 1);
    assert(parser.data()[1] == 0 && parser.data()[7] == type);
  };
  check_tx(0x61, version, 2, "FF FF 0A 00 00 00 00 00 00 61 00 07 72");
  check_tx(0x70, nullptr, 0, "FF FF 08 00 00 00 00 00 00 70 78");
  check_tx(0x01, query, 2, "FF FF 0A 00 00 00 00 00 00 01 4D 01 59");
  check_tx(0x05, nullptr, 0, "FF FF 08 00 00 00 00 00 00 05 0D");
  const uint8_t power_on[] = {0x5D, 0x01, 0, 1};
  const uint8_t eco[] = {0x5D, 0x05, 0, 1};
  const uint8_t normal[] = {0x5D, 0x05, 0, 0};
  const uint8_t bps[] = {0x5D, 0x17, 0, 1};
  const uint8_t target[] = {0x5D, 0x02, 0, 6};
  check_tx(1, power_on, 4, "FF FF 0C 00 00 00 00 00 00 01 5D 01 00 01 6C");
  check_tx(1, eco, 4, "FF FF 0C 00 00 00 00 00 00 01 5D 05 00 01 70");
  check_tx(1, normal, 4, "FF FF 0C 00 00 00 00 00 00 01 5D 05 00 00 6F");
  check_tx(1, bps, 4, "FF FF 0C 00 00 00 00 00 00 01 5D 17 00 01 82");
  check_tx(1, target, 4, "FF FF 0C 00 00 00 00 00 00 01 5D 02 00 06 72");
  const uint8_t network[] = {0, 7, 0, 100};
  check_tx(0xF7, network, 4, "FF FF 0C 00 00 00 00 00 00 F7 00 07 00 64 6E");
  const uint8_t escaped[] = {0xFF, 0x55};
  const size_t escaped_size = encode_frame(0x01, escaped, 2, wire);
  assert(feed(parser, std::vector<uint8_t>(wire, wire + escaped_size)) == 1);
  assert(encode_frame(0x01, escaped, 5, wire) == 0);
  // Status fixture based on packet 4, 2026-10-14 09:24:51 session.
  assert(feed(parser,
              bytes("FF FF 3C 00 00 00 00 00 00 06 6D 01 35 05 00 00 00 00 00 00 00 32 19 00 00 35 00 00 00 00 13 00 "
                    "2D 13 00 2D 00 00 00 00 00 00 22 00 00 00 00 00 10 10 00 00 00 00 00 02 00 00 00 00 00 00 2E")) ==
         1);
  assert(decode_status(parser.data(), parser.size(), status));
  assert(status.current_temperature == 53 && status.target_temperature == 35);
  assert(status.powered == false && status.heating == false);
  assert(status.mode == HonMode::HON_MODE_NORMAL && status.available_water == 2);
  // Status fixture based on packet 33, 2026-10-14 09:24:51 session.
  assert(feed(parser,
              bytes("FF FF 3C 00 00 00 00 00 00 06 6D 01 35 05 00 00 00 00 00 00 00 32 19 00 00 35 00 00 00 00 13 00 "
                    "2D 13 00 2D 00 00 00 00 00 00 22 00 00 00 00 00 10 11 00 00 00 00 00 02 00 00 00 00 00 00 2F")) ==
         1);
  assert(decode_status(parser.data(), parser.size(), status));
  assert(status.current_temperature == 53 && status.target_temperature == 35);
  assert(status.powered == true && status.heating == false);
  assert(status.mode == HonMode::HON_MODE_NORMAL && status.available_water == 2);
  // Status fixture based on packet 36, 2026-10-14 09:24:51 session.
  assert(feed(parser,
              bytes("FF FF 3C 00 00 00 00 00 00 06 6D 01 35 05 00 00 00 00 00 00 00 32 19 00 00 35 00 00 00 00 13 00 "
                    "2D 13 00 2D 00 00 00 00 00 00 22 00 00 00 00 00 00 01 00 04 00 00 00 02 00 00 00 00 00 00 13")) ==
         1);
  assert(decode_status(parser.data(), parser.size(), status));
  assert(status.current_temperature == 53 && status.target_temperature == 35);
  assert(status.powered == true && status.heating == false);
  assert(status.mode == HonMode::HON_MODE_ECO && status.available_water == 2);
  // Status fixture based on packet 39, 2026-10-14 09:24:51 session.
  assert(feed(parser,
              bytes("FF FF 3C 00 00 00 00 00 00 06 6D 01 35 32 00 00 00 1D 00 00 00 32 19 00 00 35 00 00 00 00 13 00 "
                    "2D 13 00 2D 00 0F 00 00 00 00 22 00 00 00 00 00 00 01 04 01 00 00 00 02 00 00 00 00 00 00 6D")) ==
         1);
  assert(decode_status(parser.data(), parser.size(), status));
  assert(status.current_temperature == 53 && status.target_temperature == 80);
  assert(status.estimated_heating_time == 29);
  assert(status.powered == true && status.heating == true);
  assert(status.mode == HonMode::HON_MODE_BPS && status.available_water == 2);
  // Status fixture based on packet 42, 2026-10-14 09:24:51 session.
  assert(feed(parser,
              bytes("FF FF 3C 00 00 00 00 00 00 06 6D 01 35 05 00 00 00 00 00 00 00 32 19 00 00 35 00 00 00 00 13 00 "
                    "2D 13 00 2D 00 00 00 00 00 00 22 00 00 00 00 00 10 11 00 00 00 00 00 02 00 00 00 00 00 00 2F")) ==
         1);
  assert(decode_status(parser.data(), parser.size(), status));
  assert(status.current_temperature == 53 && status.target_temperature == 35);
  assert(status.powered == true && status.heating == false);
  assert(status.mode == HonMode::HON_MODE_NORMAL && status.available_water == 2);
  // Status fixture based on packet 50, 2026-10-14 09:24:51 session.
  assert(feed(parser,
              bytes("FF FF 3C 00 00 00 00 00 00 06 6D 01 35 05 00 01 00 00 00 00 00 32 19 00 00 35 00 00 00 00 13 00 "
                    "2D 13 00 2D 00 00 00 00 00 00 22 00 00 00 00 00 00 01 00 04 00 00 00 02 00 00 00 00 00 00 14")) ==
         1);
  assert(decode_status(parser.data(), parser.size(), status));
  assert(status.current_temperature == 53 && status.target_temperature == 35);
  assert(status.powered == true && status.heating == false);
  assert(status.mode == HonMode::HON_MODE_ECO && status.available_water == 2);
  // Status fixture based on packet 52, 2026-10-14 09:24:51 session.
  assert(feed(parser,
              bytes("FF FF 3C 00 00 00 00 00 00 06 6D 01 35 32 00 01 00 1D 00 00 00 32 19 00 00 35 00 00 00 00 13 00 "
                    "2D 13 00 2D 00 0F 00 00 00 00 22 00 00 00 00 00 00 01 04 01 00 00 00 02 00 00 00 00 00 00 6E")) ==
         1);
  assert(decode_status(parser.data(), parser.size(), status));
  assert(status.current_temperature == 53 && status.target_temperature == 80);
  assert(status.estimated_heating_time == 29);
  assert(status.powered == true && status.heating == true);
  assert(status.mode == HonMode::HON_MODE_BPS && status.available_water == 2);
  // Status fixture based on packet 55, 2026-10-14 09:24:51 session.
  assert(feed(parser,
              bytes("FF FF 3C 00 00 00 00 00 00 06 6D 01 35 05 00 01 00 00 00 00 00 32 19 00 00 35 00 00 00 00 13 00 "
                    "2D 13 00 2D 00 00 00 00 00 00 22 00 00 00 00 00 10 11 00 00 00 00 00 02 00 00 00 00 00 00 30")) ==
         1);
  assert(decode_status(parser.data(), parser.size(), status));
  assert(status.current_temperature == 53 && status.target_temperature == 35);
  assert(status.powered == true && status.heating == false);
  assert(status.mode == HonMode::HON_MODE_NORMAL && status.available_water == 2);
  // Status fixture based on packet 58, 2026-10-14 09:24:51 session.
  assert(feed(parser,
              bytes("FF FF 3C 00 00 00 00 00 00 06 6D 01 35 05 00 01 00 00 00 00 00 32 19 00 00 35 00 00 00 00 13 00 "
                    "2D 13 00 2D 00 00 00 00 00 00 22 00 00 00 00 00 10 10 00 00 00 00 00 02 00 00 00 00 00 00 2F")) ==
         1);
  assert(decode_status(parser.data(), parser.size(), status));
  assert(status.current_temperature == 53 && status.target_temperature == 35);
  assert(status.powered == false && status.heating == false);
  assert(status.mode == HonMode::HON_MODE_NORMAL && status.available_water == 2);
  // Target-temperature fixture based on packet 4.
  assert(feed(parser,
              bytes("FF FF 3C 00 00 00 00 00 00 06 6D 01 1E 05 00 00 00 06 00 00 00 32 19 00 00 1E 00 00 00 00 13 00 "
                    "2D 13 00 2D 00 00 00 00 00 00 22 00 00 00 00 00 10 10 00 00 00 00 00 00 00 00 00 00 00 00 04")) ==
         1);
  assert(decode_status(parser.data(), parser.size(), status));
  assert(status.current_temperature == 30 && status.target_temperature == 35);
  assert(status.estimated_heating_time == 6);
  // Target-temperature fixture based on packet 58.
  assert(feed(parser,
              bytes("FF FF 3C 00 00 00 00 00 00 06 6D 01 1E 0A 00 01 00 0B 00 00 00 32 19 00 00 1E 00 00 00 00 13 00 "
                    "2D 13 00 2D 00 0F 00 00 00 00 22 00 00 00 00 00 10 11 00 01 00 00 00 00 00 00 00 00 00 00 20")) ==
         1);
  assert(decode_status(parser.data(), parser.size(), status));
  assert(status.current_temperature == 30 && status.target_temperature == 40);
  // Target-temperature fixture based on packet 82.
  assert(feed(parser,
              bytes("FF FF 3C 00 00 00 00 00 00 06 6D 01 1F 2D 00 01 00 2F 00 00 00 32 19 00 00 1F 00 00 00 00 13 00 "
                    "2D 13 00 2D 00 0F 00 00 00 00 22 00 00 00 00 00 10 11 80 01 00 00 00 00 00 00 00 00 00 00 E9")) ==
         1);
  assert(decode_status(parser.data(), parser.size(), status));
  assert(status.current_temperature == 31 && status.target_temperature == 75);
  // Target-temperature fixture based on packet 95.
  assert(feed(parser,
              bytes("FF FF 3C 00 00 00 00 00 00 06 6D 01 20 05 00 01 00 04 00 00 00 32 19 00 00 20 00 00 00 00 13 00 "
                    "2D 13 00 2D 00 0F 00 00 00 00 22 00 00 00 00 00 10 11 00 01 00 00 00 00 00 00 00 00 00 00 18")) ==
         1);
  assert(decode_status(parser.data(), parser.size(), status));
  assert(status.current_temperature == 32 && status.target_temperature == 35);
  uint8_t independent[61] = {};
  independent[0] = 60;
  independent[7] = 2;
  independent[8] = 0x6D;
  independent[9] = 1;
  independent[10] = 42;
  independent[11] = 5;
  independent[23] = 43;
  assert(decode_status(independent, sizeof(independent), status));
  assert(status.current_temperature == 42 && status.secondary_temperature == 43);
  auto valid = bytes("FF FF 3C 00 00 00 00 00 00 06 6D 01 35 32 00 00 00 1D 00 00 00 32 19 00 00 35 00 00 00 00 13 00 "
                     "2D 13 00 2D 00 0F 00 00 00 00 22 00 00 00 00 00 00 01 04 01 00 00 00 02 00 00 00 00 00 00 6D");
  auto damaged = valid;
  damaged.back() ^= 1;
  assert(feed(parser, damaged) == 0);
  assert(feed(parser, valid) == 1);
  parser.reset();
  for (size_t i = 0; i < 20; i++)
    assert(!parser.feed(valid[i]));
  parser.reset();  // Partial frame timeout.
  assert(feed(parser, valid) == 1);
  parser.reset();
  for (size_t i = 0; i < 20; i++)
    assert(!parser.feed(valid[i]));
  assert(feed(parser, valid) == 1);  // New separator resynchronizes.
  assert(feed(parser, bytes("FF FF 07 7F FF 55 DA")) == 0);
  assert(feed(parser, valid) == 1);
  auto crc = bytes("FF FF 0D 40 00 00 00 00 00 01 4D 01 FF 55 BB FF 55 FF 55 D1 3C");
  assert(feed(parser, crc) == 1);
  assert(!decode_status(parser.data(), parser.size(), status));
  crc.back() ^= 1;
  assert(feed(parser, crc) == 0);
  assert(feed(parser, bytes("FF FF 08 00 00 00 00 00 00 05 0D")) == 1);
  assert(!decode_status(parser.data(), 0, status));
}
}  // namespace esphome::haier::testing
int main() { esphome::haier::testing::run(); }
