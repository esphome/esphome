#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include "esphome/components/ssd1306_i2c/ssd1306_partial_update.h"
using esphome::ssd1306_i2c::PartialUpdatePlanner;
using esphome::ssd1306_i2c::Region;
using Frame = std::array<uint8_t, 1024>;
struct Workload {
  std::string name;
  Frame previous{};
  Frame current{};
};

void pixel(Frame &frame, unsigned x, unsigned y) { frame[(y / 8) * 128 + x] |= 1U << (y % 8); }
void digit(Frame &frame, unsigned x, unsigned y, unsigned number) {
  const std::array<uint8_t, 10> segments{0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
  const uint8_t value = segments[number];
  for (unsigned n = 0; n < 7; n++) {
    if (!(value & (1U << n)))
      continue;
    for (unsigned offset = 0; offset < 5; offset++) {
      if (n == 0)
        pixel(frame, x + offset + 1, y);
      if (n == 1)
        pixel(frame, x + 6, y + offset + 1);
      if (n == 2)
        pixel(frame, x + 6, y + offset + 7);
      if (n == 3)
        pixel(frame, x + offset + 1, y + 12);
      if (n == 4)
        pixel(frame, x, y + offset + 7);
      if (n == 5)
        pixel(frame, x, y + offset + 1);
      if (n == 6)
        pixel(frame, x + offset + 1, y + 6);
    }
  }
}
void digits(Frame &frame, unsigned x, unsigned y, const std::string &text) {
  for (char value : text) {
    digit(frame, x, y, value - '0');
    x += 9;
  }
}
uint32_t cost(const Region &region) {
  const uint32_t bytes = (region.max_x - region.min_x + 1) * (region.max_page - region.min_page + 1);
  return 72 + bytes * 9 + ((bytes + 15) / 16) * 18;
}
uint32_t global_box(const Workload &work) {
  Region bounds{127, 0, 7, 0};
  bool changed = false;
  for (unsigned page = 0; page < 8; page++) {
    for (unsigned x = 0; x < 128; x++) {
      if (work.current[page * 128 + x] == work.previous[page * 128 + x])
        continue;
      changed = true;
      bounds.min_x = std::min(bounds.min_x, static_cast<uint8_t>(x));
      bounds.max_x = std::max(bounds.max_x, static_cast<uint8_t>(x));
      bounds.min_page = std::min(bounds.min_page, static_cast<uint8_t>(page));
      bounds.max_page = std::max(bounds.max_page, static_cast<uint8_t>(page));
    }
  }
  return changed ? cost(bounds) : 0;
}
uint32_t per_page(const Workload &work) {
  uint32_t total = 0;
  for (unsigned page = 0; page < 8; page++) {
    Region bounds{127, 0, static_cast<uint8_t>(page), static_cast<uint8_t>(page)};
    bool changed = false;
    for (unsigned x = 0; x < 128; x++) {
      if (work.current[page * 128 + x] == work.previous[page * 128 + x])
        continue;
      changed = true;
      bounds.min_x = std::min(bounds.min_x, static_cast<uint8_t>(x));
      bounds.max_x = std::max(bounds.max_x, static_cast<uint8_t>(x));
    }
    if (changed)
      total += cost(bounds);
  }
  return total;
}
template<typename F> double time_calls(F function) {
  [[maybe_unused]] volatile uint32_t sink{0};
  const auto start = std::chrono::steady_clock::now();
  for (unsigned iteration = 0; iteration < 2000; iteration++) {
    std::atomic_signal_fence(std::memory_order_seq_cst);
    sink = function();
  }
  return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / 2000;
}
int main() {
  std::vector<Workload> jobs;
  auto &single = jobs.emplace_back("clock_single_digit");
  digits(single.previous, 8, 0, "123458");
  digits(single.current, 8, 0, "123459");
  auto &rollover = jobs.emplace_back("clock_rollover");
  digits(rollover.previous, 8, 0, "123459");
  digits(rollover.current, 8, 0, "123500");
  auto &sensor = jobs.emplace_back("sensor_value");
  digits(sensor.previous, 8, 24, "479");
  digits(sensor.current, 8, 24, "480");
  auto &split = jobs.emplace_back("two_separated_sensor_values");
  digits(split.previous, 2, 24, "29");
  digits(split.current, 2, 24, "30");
  digits(split.previous, 100, 24, "59");
  digits(split.current, 100, 24, "60");
  auto &progress = jobs.emplace_back("progress_bar");
  for (unsigned y = 20; y < 44; y++) {
    for (unsigned x = 0; x < 51; x++)
      pixel(progress.previous, x, y);
    for (unsigned x = 0; x < 54; x++)
      pixel(progress.current, x, y);
  }
  auto &grid = jobs.emplace_back("sparse_grid_88_bytes");
  for (unsigned page = 0; page < 8; page++) {
    for (unsigned x = 0; x <= 120; x += 12)
      grid.current[page * 128 + x] = 1;
  }
  auto &checker = jobs.emplace_back("checkerboard");
  for (unsigned i = 0; i < 1024; i += 2)
    checker.current[i] = 255;
  auto &full = jobs.emplace_back("full_frame");
  full.current.fill(255);
  jobs.emplace_back("unchanged");
  std::cout
      << "workload,planner_clocks,per_page_clocks,global_clocks,planner_host_us,per_page_host_us,global_host_us\n";
  PartialUpdatePlanner planner;
  for (const auto &job : jobs) {
    const auto plan = [&]() {
      if (!planner.build(job.current.data(), job.previous.data(), 128, 8))
        std::abort();
      return planner.get_wire_clocks();
    };
    const uint32_t plan_cost = plan();
    const uint32_t page_cost = per_page(job);
    const uint32_t global_cost = global_box(job);
    if (plan_cost > std::min(page_cost, global_cost))
      return 1;
    std::cout << job.name << ',' << plan_cost << ',' << page_cost << ',' << global_cost << ',' << std::fixed
              << std::setprecision(3) << time_calls(plan) << ',' << time_calls([&]() { return per_page(job); }) << ','
              << time_calls([&]() { return global_box(job); }) << '\n';
  }
}
