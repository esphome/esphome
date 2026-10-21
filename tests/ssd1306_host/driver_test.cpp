// Requires the isolated headers and flags from script/test_ssd1306_partial.py.
#ifdef SSD1306_STANDALONE_TEST

#include <algorithm>
#include <array>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "esphome/components/ssd1306_i2c/ssd1306_i2c.h"

using esphome::Color;
using esphome::ssd1306_i2c::I2CSSD1306;
using namespace esphome::ssd1306_base;
namespace memory = esphome::test;
#define SSD1306_HOST_STRINGIFY_IMPL(value) #value
#define SSD1306_HOST_STRINGIFY(value) SSD1306_HOST_STRINGIFY_IMPL(value)
#define REQUIRE(expression) \
  do { \
    if (!(expression)) \
      throw std::runtime_error(__FILE__ ":" SSD1306_HOST_STRINGIFY(__LINE__) ": " #expression); \
  } while (false)

struct Transfer {
  std::vector<uint8_t> bytes;
  bool failed;
};

class Bus : public esphome::i2c::I2CBus {
 public:
  bool online{true};
  std::vector<Transfer> transfers;
  std::function<bool(const std::vector<uint8_t> &)> fail;
  std::array<uint8_t, 1024> ram{};
  bool on{false};
  uint8_t inverse{0};
  std::array<uint8_t, 256> parameters{};
  uint8_t pending{0};
  unsigned needed{0};
  std::vector<uint8_t> arguments;
  unsigned min_x{0}, max_x{127}, min_page{0}, max_page{7}, x{0}, page{0};
  void reset_controller() {
    on = false;
    inverse = 0;
    pending = 0;
    needed = 0;
    arguments.clear();
    parameters.fill(0);
    min_x = x = min_page = page = 0;
    max_x = 127;
    max_page = 7;
    ram.fill(0);
  }
  esphome::i2c::ErrorCode write_readv(uint8_t, const uint8_t *data, size_t len, uint8_t *, size_t) override {
    const std::vector<uint8_t> bytes = len ? std::vector<uint8_t>(data, data + len) : std::vector<uint8_t>();
    const bool failed = !online || (fail && fail(bytes));
    transfers.push_back({bytes, failed});
    if (failed)
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    if (len == 0)
      return esphome::i2c::ERROR_OK;
    if (bytes[0] == 0x00) {
      for (size_t i = 1; i < bytes.size(); i++)
        command(bytes[i]);
    } else if (bytes[0] == 0x40) {
      for (size_t i = 1; i < bytes.size(); i++) {
        if (x < 128 && page < 8)
          ram[page * 128 + x] = bytes[i];
        if (++x > max_x) {
          x = min_x;
          if (++page > max_page)
            page = min_page;
        }
      }
    }
    return esphome::i2c::ERROR_OK;
  }
  void command(uint8_t byte) {
    if (needed) {
      arguments.push_back(byte);
      if (--needed == 0) {
        parameters[pending] = arguments[0];
        if (pending == 0x21) {
          min_x = x = arguments[0];
          max_x = arguments[1];
        }
        if (pending == 0x22) {
          min_page = page = arguments[0];
          max_page = arguments[1];
        }
      }
      return;
    }
    if (byte == 0xAE)
      on = false;
    if (byte == 0xAF)
      on = true;
    if (byte == 0xA6 || byte == 0xA7)
      inverse = byte & 1;
    if (byte == 0x21 || byte == 0x22)
      needed = 2;
    if (byte == 0xD5 || byte == 0xA8 || byte == 0xD3 || byte == 0xAD || byte == 0x8D || byte == 0x20 || byte == 0xDA ||
        byte == 0xD9 || byte == 0xDB || byte == 0x81 || byte == 0x82 || byte == 0xD8 || byte == 0xDC)
      needed = 1;
    pending = byte;
    arguments.clear();
  }
  size_t probes() const {
    return std::count_if(transfers.begin(), transfers.end(), [](const Transfer &item) { return item.bytes.empty(); });
  }
  size_t display_off_commands() const {
    return std::count_if(transfers.begin(), transfers.end(), [](const Transfer &item) {
      return item.bytes.size() == 2 && item.bytes[0] == 0 && item.bytes[1] == 0xAE;
    });
  }
  void clear_transfers() { transfers.clear(); }
};

class Pin : public esphome::GPIOPin {
 public:
  Bus *bus{nullptr};
  size_t setups{0};
  std::vector<std::pair<uint32_t, bool>> writes;
  void setup() override { ++setups; }
  void digital_write(bool value) override {
    writes.emplace_back(memory::now, value);
    if (!value && bus)
      bus->reset_controller();
  }
};

void configure(I2CSSD1306 &display, Bus &bus, Pin *pin = nullptr, bool partial = true) {
  display.set_i2c_bus(&bus);
  display.set_i2c_address(0x3C);
  display.set_model(SSD1306_MODEL_128_64);
  display.set_reset_pin(pin);
  if (pin)
    pin->bus = &bus;
  if (partial)
    display.set_partial_updates();
}

void healthy(I2CSSD1306 &display, Bus &bus, Pin *pin = nullptr) {
  configure(display, bus, pin);
  display.setup();
  REQUIRE(!display.is_failed());
  display.update();
  REQUIRE(!display.status_has_warning());
  REQUIRE(bus.on);
  bus.clear_transfers();
  if (pin)
    pin->writes.clear();
  memory::delay_total = 0;
}

void advance_reset(I2CSSD1306 &display) {
  memory::now += 1;
  display.loop();
  memory::now += 10;
  display.loop();
}

using Case = std::pair<const char *, std::function<void()>>;
int main() {
  std::vector<Case> cases{
      {"required framebuffer allocation failure",
       [] {
         memory::fail_allocation = 1;
         Bus bus;
         I2CSSD1306 display;
         configure(display, bus);
         display.setup();
         REQUIRE(display.is_failed());
         REQUIRE(memory::allocation_calls == 1);
         REQUIRE(display.test_buffer() == nullptr);
         REQUIRE(bus.transfers.size() == 1);
         display.fill(Color::WHITE);
         display.draw_pixel_at(0, 0);
         display.display();
         display.update();
         display.turn_on();
         display.loop();
         REQUIRE(bus.transfers.size() == 1);
       }},
      {"required allocation precedes optional allocations and is reused",
       [] {
         Bus bus;
         I2CSSD1306 display;
         healthy(display, bus);
         REQUIRE(memory::requests.size() == 3);
         REQUIRE(memory::requests[0] == 1024);
         REQUIRE(memory::requests[2] == 1024);
         REQUIRE(memory::requests[1] >= 512);
         REQUIRE(memory::requests[1] < 1024);
       }},
      {"optional state failure falls back with required buffer intact",
       [] {
         memory::fail_allocation = 2;
         Bus bus;
         I2CSSD1306 display;
         configure(display, bus);
         display.setup();
         REQUIRE(!display.is_failed());
         REQUIRE(memory::allocation_calls == 2);
         REQUIRE(memory::memory_used == 1024);
         display.draw_pixel_at(4, 0);
         display.update();
         REQUIRE(bus.ram[4] == 1);
       }},
      {"optional shadow failure frees state and uses full-frame path",
       [] {
         memory::fail_allocation = 3;
         Bus bus;
         I2CSSD1306 display;
         configure(display, bus);
         display.setup();
         REQUIRE(!display.is_failed());
         REQUIRE(memory::allocation_calls == 3);
         REQUIRE(memory::memory_used == 1024);
         display.draw_pixel_at(7, 0);
         display.update();
         REQUIRE(bus.ram[7] == 1);
       }},
      {"low memory prioritizes framebuffer instead of optional storage",
       [] {
         memory::memory_budget = 2048;
         Bus bus;
         I2CSSD1306 display;
         configure(display, bus);
         display.setup();
         REQUIRE(!display.is_failed());
         REQUIRE(memory::requests[0] == 1024);
         REQUIRE(memory::allocation_calls == 3);
         REQUIRE(memory::requests[1] + 1024 <= memory::memory_budget);
         REQUIRE(memory::requests[0] + memory::requests[1] + memory::requests[2] > memory::memory_budget);
         REQUIRE(memory::memory_used == 1024);
         display.fill(Color::WHITE);
         display.update();
         REQUIRE(bus.ram.back() == 0xFF);
       }},
      {"initial display-on failure remains pending until full recovery",
       [] {
         Bus bus;
         I2CSSD1306 display;
         configure(display, bus);
         bool once = true;
         bus.fail = [&](const std::vector<uint8_t> &bytes) {
           if (bytes == std::vector<uint8_t>{0, 0xAF} && once) {
             once = false;
             return true;
           }
           return false;
         };
         display.setup();
         REQUIRE(display.status_has_warning());
         REQUIRE(!bus.on);
         display.update();
         REQUIRE(bus.on);
         REQUIRE(!display.status_has_warning());
       }},
      {"failed early init command cannot be overwritten by later success",
       [] {
         Bus bus;
         I2CSSD1306 display;
         configure(display, bus);
         bool once = true;
         bus.fail = [&](const std::vector<uint8_t> &bytes) {
           if (bytes == std::vector<uint8_t>{0, 0xD5} && once) {
             once = false;
             return true;
           }
           return false;
         };
         display.setup();
         REQUIRE(display.status_has_warning());
         REQUIRE(!bus.on);
         display.update();
         REQUIRE(bus.parameters[0xD5] == 0x80);
         REQUIRE(bus.on);
         REQUIRE(!display.status_has_warning());
       }},
      {"data NACK gets plain resync without blanking or reset",
       [] {
         Bus bus;
         Pin pin;
         I2CSSD1306 display;
         healthy(display, bus, &pin);
         bool once = true;
         bus.fail = [&](const std::vector<uint8_t> &bytes) {
           if (!bytes.empty() && bytes[0] == 0x40 && once) {
             once = false;
             return true;
           }
           return false;
         };
         display.draw_pixel_at(15, 8);
         display.update();
         REQUIRE(display.status_has_warning());
         REQUIRE(bus.on);
         display.update();
         REQUIRE(!display.status_has_warning());
         REQUIRE(bus.on);
         REQUIRE(pin.writes.empty());
         REQUIRE(bus.display_off_commands() == 0);
         REQUIRE(memory::delay_total == 0);
         REQUIRE(bus.ram[128 + 15] == 1);
       }},
      {"power loss during data write restores settings even if the next probe succeeds",
       [] {
         Bus bus;
         Pin pin;
         I2CSSD1306 display;
         healthy(display, bus, &pin);
         display.set_contrast(0.5f);
         display.set_invert(true);
         bool once = true;
         bus.fail = [&](const std::vector<uint8_t> &bytes) {
           if (!bytes.empty() && bytes[0] == 0x40 && once) {
             once = false;
             bus.reset_controller();
             return true;
           }
           return false;
         };
         bus.clear_transfers();
         display.draw_pixel_at(9, 8);
         display.update();
         REQUIRE(display.status_has_warning());
         REQUIRE(!bus.on);
         display.update();
         REQUIRE(bus.on);
         REQUIRE(bus.inverse == 1);
         REQUIRE(bus.parameters[0x81] == 127);
         REQUIRE(bus.parameters[0xD5] == 0x80);
         REQUIRE(bus.ram[128 + 9] == 1);
         REQUIRE(pin.writes.empty());
         REQUIRE(bus.display_off_commands() == 0);
         REQUIRE(!display.status_has_warning());
       }},
      {"failed plain resync escalates and rate-limits the next attempt",
       [] {
         Bus bus;
         Pin pin;
         I2CSSD1306 display;
         healthy(display, bus, &pin);
         unsigned failures = 2;
         bus.fail = [&](const std::vector<uint8_t> &bytes) {
           if (!bytes.empty() && bytes[0] == 0x40 && failures) {
             --failures;
             return true;
           }
           return false;
         };
         display.draw_pixel_at(1, 0);
         display.update();
         display.update();
         REQUIRE(display.status_has_warning());
         REQUIRE(pin.writes.empty());
         const auto count = bus.transfers.size();
         memory::now += 999;
         display.update();
         REQUIRE(bus.transfers.size() == count);
         memory::now += 1;
         display.update();
         REQUIRE(display.test_loop_enabled());
         REQUIRE(pin.writes.size() == 1);
         advance_reset(display);
         REQUIRE(pin.writes.size() == 3);
         REQUIRE(!display.test_loop_enabled());
         REQUIRE(memory::delay_total == 0);
         REQUIRE(!display.status_has_warning());
         REQUIRE(bus.ram[1] == 1);
       }},
      {"static outage probes at most once per second with no reset pulse",
       [] {
         Bus bus;
         Pin pin;
         I2CSSD1306 display;
         healthy(display, bus, &pin);
         bus.online = false;
         display.update();
         REQUIRE(display.status_has_warning());
         bus.clear_transfers();
         const uint32_t start = memory::now;
         for (unsigned elapsed = 0; elapsed < 10000; elapsed += 25) {
           memory::now = start + elapsed;
           display.update();
           display.loop();
         }
         REQUIRE(bus.probes() == 10);
         REQUIRE(bus.transfers.size() == 10);
         REQUIRE(pin.writes.empty());
         REQUIRE(memory::delay_total == 0);
       }},
      {"reconnect resets asynchronously and preserves settings and latest framebuffer",
       [] {
         Bus bus;
         Pin pin;
         I2CSSD1306 display;
         healthy(display, bus, &pin);
         display.set_contrast(0.5f);
         display.set_invert(true);
         display.set_external_vcc(true);
         display.init_offset_y(3);
         bus.online = false;
         display.update();
         display.update();
         bus.reset_controller();
         bus.online = true;
         memory::now += 1000;
         display.update();
         REQUIRE(display.status_has_warning());
         REQUIRE(display.test_loop_enabled());
         display.draw_pixel_at(10, 16);
         display.loop();
         REQUIRE(pin.writes.size() == 1);
         memory::now += 1;
         display.loop();
         REQUIRE(pin.writes.size() == 2);
         memory::now += 9;
         display.loop();
         REQUIRE(pin.writes.size() == 2);
         memory::now += 1;
         display.loop();
         REQUIRE(pin.writes.size() == 3);
         REQUIRE(bus.parameters[0x81] == 127);
         REQUIRE(bus.parameters[0x8D] == 0x10);
         REQUIRE(bus.parameters[0xD9] == 0x22);
         REQUIRE(bus.parameters[0xD3] == 3);
         REQUIRE(bus.inverse == 1);
         REQUIRE(bus.ram[2 * 128 + 10] == 1);
         REQUIRE(bus.on);
         REQUIRE(!display.status_has_warning());
         REQUIRE(memory::delay_total == 0);
         REQUIRE(pin.setups == 1);
       }},
      {"final display-on failure does not clear warning or commit shadow",
       [] {
         Bus bus;
         I2CSSD1306 display;
         healthy(display, bus);
         bus.online = false;
         display.update();
         display.update();
         bus.online = true;
         bool once = true;
         bool warning_seen = false;
         bus.fail = [&](const std::vector<uint8_t> &bytes) {
           if (bytes == std::vector<uint8_t>{0, 0xAF} && once) {
             once = false;
             warning_seen = display.status_has_warning();
             return true;
           }
           return false;
         };
         memory::now += 1000;
         display.update();
         REQUIRE(warning_seen);
         REQUIRE(display.status_has_warning());
         REQUIRE(!bus.on);
         bus.clear_transfers();
         memory::now += 1000;
         display.update();
         REQUIRE(bus.on);
         REQUIRE(!display.status_has_warning());
         REQUIRE(bus.display_off_commands() == 1);
       }},
      {"desired off state during reset is honored",
       [] {
         Bus bus;
         Pin pin;
         I2CSSD1306 display;
         healthy(display, bus, &pin);
         bus.online = false;
         display.update();
         display.update();
         bus.online = true;
         memory::now += 1000;
         display.update();
         const auto count = bus.transfers.size();
         display.turn_off();
         display.set_contrast(0.25f);
         REQUIRE(bus.transfers.size() == count);
         advance_reset(display);
         REQUIRE(!bus.on);
         REQUIRE(!display.is_on());
         REQUIRE(bus.parameters[0x81] == 63);
         REQUIRE(!display.status_has_warning());
       }},
      {"runtime commands use bool success and configuration failures stay untrusted",
       [] {
         Bus bus;
         I2CSSD1306 display;
         healthy(display, bus);
         display.set_invert(true);
         REQUIRE(!display.status_has_warning());
         bool once = true;
         bus.fail = [&](const std::vector<uint8_t> &bytes) {
           if (bytes == std::vector<uint8_t>{0, 0xA6} && once) {
             once = false;
             return true;
           }
           return false;
         };
         display.set_invert(false);
         REQUIRE(display.status_has_warning());
         display.update();
         REQUIRE(!display.status_has_warning());
         REQUIRE(bus.inverse == 0);
       }},
      {"no new driver allocations after setup on normal or recovery paths",
       [] {
         Bus bus;
         I2CSSD1306 display;
         healthy(display, bus);
         const auto allocations = memory::allocation_calls;
         for (unsigned i = 0; i < 100; i++) {
           display.draw_pixel_at(i % 128, i % 64);
           display.update();
         }
         bus.online = false;
         display.update();
         display.update();
         bus.online = true;
         memory::now += 1000;
         display.update();
         REQUIRE(memory::allocation_calls == allocations);
         REQUIRE(!display.status_has_warning());
       }},
      {"retry arithmetic handles uint32 clock wrap",
       [] {
         Bus bus;
         I2CSSD1306 display;
         healthy(display, bus);
         memory::now = 0xFFFFFF00U;
         bus.online = false;
         display.update();
         display.update();
         const auto count = bus.transfers.size();
         memory::now += 999;
         display.update();
         REQUIRE(bus.transfers.size() == count);
         memory::now += 1;
         display.update();
         REQUIRE(bus.transfers.size() == count + 1);
       }},
      {"initially absent panel marks failed without allocating",
       [] {
         Bus bus;
         bus.online = false;
         I2CSSD1306 display;
         configure(display, bus);
         display.setup();
         REQUIRE(display.is_failed());
         REQUIRE(memory::allocation_calls == 0);
         display.update();
         REQUIRE(bus.transfers.size() == 1);
       }},
      {"partial-disabled instance retains legacy full-frame behavior",
       [] {
         Bus bus;
         I2CSSD1306 display;
         configure(display, bus, nullptr, false);
         display.setup();
         REQUIRE(memory::allocation_calls == 1);
         bus.clear_transfers();
         display.update();
         REQUIRE(bus.transfers.size() == 70);
         REQUIRE(!display.status_has_warning());
       }},
      {"all six geometries initialize and synchronize without optional zero-fill",
       [] {
         const std::array<std::pair<unsigned, unsigned>, 6> dimensions{
             {{128, 32}, {128, 64}, {96, 16}, {64, 48}, {64, 32}, {72, 40}}};
         for (unsigned model = 0; model < dimensions.size(); model++) {
           Bus bus;
           I2CSSD1306 display;
           configure(display, bus);
           display.set_model(static_cast<SSD1306Model>(model));
           display.setup();
           REQUIRE(!display.is_failed());
           display.fill(Color::WHITE);
           display.update();
           REQUIRE(!display.status_has_warning());
           const auto [width, height] = dimensions[model];
           const unsigned column = (model == 3 || model == 4) ? 32 : (model == 5 ? 28 : 0);
           for (unsigned page = 0; page < height / 8; page++) {
             for (unsigned x = column; x < column + width; x++)
               REQUIRE(bus.ram[page * 128 + x] == 255);
           }
           bus.clear_transfers();
           display.update();
           REQUIRE(bus.transfers.size() == 1);
           REQUIRE(bus.probes() == 1);
         }
       }},
      {"direct display keeps the shadow coherent for every geometry and power state",
       [] {
         const std::array<std::pair<unsigned, unsigned>, 6> dimensions{
             {{128, 32}, {128, 64}, {96, 16}, {64, 48}, {64, 32}, {72, 40}}};
         for (unsigned model = 0; model < dimensions.size(); model++) {
           for (bool on : {false, true}) {
             Bus bus;
             I2CSSD1306 display;
             configure(display, bus);
             display.set_model(static_cast<SSD1306Model>(model));
             display.setup();
             display.update();
             if (!on)
               display.turn_off();
             const auto allocations = memory::allocation_calls;
             const auto [width, height] = dimensions[model];
             const unsigned column = (model == 3 || model == 4) ? 32 : (model == 5 ? 28 : 0);
             // Repeat the reviewer's black -> direct white -> updated black sequence.
             for (unsigned cycle = 0; cycle < 2; cycle++) {
               display.fill(Color::WHITE);
               display.display();
               for (unsigned page = 0; page < height / 8; page++) {
                 for (unsigned x = column; x < column + width; x++)
                   REQUIRE(bus.ram[page * 128 + x] == 255);
               }
               display.fill(Color::BLACK);
               display.update();
               REQUIRE(std::all_of(bus.ram.begin(), bus.ram.end(), [](uint8_t byte) { return byte == 0; }));
               display.fill(Color::WHITE);
               display.display();
               bus.clear_transfers();
               display.update();
               REQUIRE(bus.transfers.size() == 1);
               REQUIRE(bus.probes() == 1);
               REQUIRE(bus.on == on);
               REQUIRE(!display.status_has_warning());
               display.fill(Color::BLACK);
               display.update();
             }
             REQUIRE(memory::allocation_calls == allocations);
           }
         }
       }},
      {"initialization display does not prematurely trust the shadow",
       [] {
         Bus bus;
         I2CSSD1306 display;
         configure(display, bus);
         display.setup();
         bus.clear_transfers();
         display.update();
         REQUIRE(bus.probes() == 1);
         REQUIRE(bus.transfers.size() == 66);
         REQUIRE(!display.status_has_warning());
         bus.clear_transfers();
         display.update();
         REQUIRE(bus.transfers.size() == 1);
         REQUIRE(bus.probes() == 1);
       }},
      {"direct display data failure at every chunk keeps recovery pending",
       [] {
         for (unsigned failed_chunk = 0; failed_chunk < 64; failed_chunk++) {
           Bus bus;
           I2CSSD1306 display;
           healthy(display, bus);
           unsigned chunk = 0;
           bus.fail = [&](const std::vector<uint8_t> &bytes) {
             return !bytes.empty() && bytes[0] == 0x40 && chunk++ == failed_chunk;
           };
           display.fill(Color::WHITE);
           display.display();
           REQUIRE(display.status_has_warning());
           REQUIRE(chunk == failed_chunk + 1);
           bus.fail = {};
           // Returning to the old shadow must still repair any white chunks already sent.
           display.fill(Color::BLACK);
           display.update();
           REQUIRE(!display.status_has_warning());
           REQUIRE(std::all_of(bus.ram.begin(), bus.ram.end(), [](uint8_t byte) { return byte == 0; }));
           bus.clear_transfers();
           display.update();
           REQUIRE(bus.transfers.size() == 1);
           REQUIRE(bus.probes() == 1);
         }
       }},
      {"direct display command failure is not hidden by a successful full frame",
       [] {
         for (unsigned failed_command = 0; failed_command < 6; failed_command++) {
           Bus bus;
           I2CSSD1306 display;
           healthy(display, bus);
           unsigned command = 0;
           bus.fail = [&](const std::vector<uint8_t> &bytes) {
             return bytes.size() == 2 && bytes[0] == 0 && command++ == failed_command;
           };
           display.fill(Color::WHITE);
           display.display();
           REQUIRE(display.status_has_warning());
           REQUIRE(command == 6);
           bus.fail = {};
           bus.clear_transfers();
           display.update();
           REQUIRE(!display.status_has_warning());
           REQUIRE(bus.display_off_commands() == 1);
           REQUIRE(std::all_of(bus.ram.begin(), bus.ram.end(), [](uint8_t byte) { return byte == 255; }));
         }
       }},
      {"direct display address-window failure does not commit the shadow",
       [] {
         Bus bus;
         I2CSSD1306 display;
         healthy(display, bus);
         bus.fail = [](const std::vector<uint8_t> &bytes) { return bytes.size() == 7 && bytes[0] == 0; };
         display.fill(Color::WHITE);
         display.display();
         REQUIRE(display.status_has_warning());
         REQUIRE(bus.transfers.size() == 7);
         REQUIRE(bus.transfers.back().failed);
         bus.fail = {};
         display.update();
         REQUIRE(!display.status_has_warning());
         REQUIRE(std::all_of(bus.ram.begin(), bus.ram.end(), [](uint8_t byte) { return byte == 255; }));
       }},
      {"successful direct display cannot bypass pending settings recovery or retry wait",
       [] {
         Bus bus;
         I2CSSD1306 display;
         healthy(display, bus);
         display.set_invert(true);
         display.set_contrast(0.25f);
         bus.online = false;
         display.turn_off();
         display.update();
         bus.reset_controller();
         bus.online = true;
         display.fill(Color::WHITE);
         display.display();
         REQUIRE(display.status_has_warning());
         REQUIRE(bus.inverse == 0);
         bus.clear_transfers();
         memory::now += 999;
         display.update();
         REQUIRE(bus.transfers.empty());
         REQUIRE(display.status_has_warning());
         memory::now += 1;
         display.update();
         REQUIRE(!display.status_has_warning());
         REQUIRE(bus.inverse == 1);
         REQUIRE(bus.parameters[0x81] == 63);
         REQUIRE(!bus.on);
         REQUIRE(!display.is_on());
         REQUIRE(std::all_of(bus.ram.begin(), bus.ram.end(), [](uint8_t byte) { return byte == 255; }));
       }},
      {"direct display cannot bypass a failed recovery display-on command",
       [] {
         Bus bus;
         I2CSSD1306 display;
         healthy(display, bus);
         bus.online = false;
         display.update();
         display.update();
         bus.online = true;
         bus.fail = [](const std::vector<uint8_t> &bytes) { return bytes == std::vector<uint8_t>{0, 0xAF}; };
         memory::now += 1000;
         display.update();
         REQUIRE(display.status_has_warning());
         REQUIRE(!bus.on);
         bus.fail = {};
         display.fill(Color::WHITE);
         display.display();
         REQUIRE(display.status_has_warning());
         REQUIRE(!bus.on);
         bus.clear_transfers();
         display.update();
         REQUIRE(bus.transfers.empty());
         memory::now += 1000;
         display.update();
         REQUIRE(!display.status_has_warning());
         REQUIRE(bus.on);
         REQUIRE(bus.display_off_commands() == 1);
         REQUIRE(std::all_of(bus.ram.begin(), bus.ram.end(), [](uint8_t byte) { return byte == 255; }));
       }},
      {"direct display sends nothing during either asynchronous reset phase",
       [] {
         Bus bus;
         Pin pin;
         I2CSSD1306 display;
         healthy(display, bus, &pin);
         bus.online = false;
         display.update();
         display.update();
         bus.online = true;
         memory::now += 1000;
         display.update();
         REQUIRE(display.test_loop_enabled());
         bus.clear_transfers();
         display.fill(Color::WHITE);
         display.display();
         REQUIRE(bus.transfers.empty());
         memory::now += 1;
         display.loop();
         display.fill(Color::BLACK);
         display.draw_pixel_at(19, 16);
         display.display();
         REQUIRE(bus.transfers.empty());
         REQUIRE(display.status_has_warning());
         memory::now += 10;
         display.loop();
         REQUIRE(!display.test_loop_enabled());
         REQUIRE(!display.status_has_warning());
         REQUIRE(bus.ram[2 * 128 + 19] == 1);
         REQUIRE(bus.on);
         bus.clear_transfers();
         display.update();
         REQUIRE(bus.transfers.size() == 1);
         REQUIRE(bus.probes() == 1);
       }},
  };
  unsigned failures = 0;
  for (const auto &[name, function] : cases) {
    memory::reset_memory();
    memory::now = 0;
    memory::delay_total = 0;
    try {
      function();
      REQUIRE(memory::allocations.empty());
      std::cout << "PASS " << name << '\n';
    } catch (const std::exception &error) {
      ++failures;
      std::cerr << "FAIL " << name << ": " << error.what() << '\n';
    }
  }
  std::cout << "Driver cases: " << cases.size() - failures << " passed, " << failures << " failed\n";
  return failures ? 1 : 0;
}

#endif  // SSD1306_STANDALONE_TEST
