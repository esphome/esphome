#pragma once
#include <string>
#include <cstdio>
// the flooded template sensors and text sensors, filled at boot by putbe-sensors.yaml (gen_sensors.py)
static constexpr size_t BS_COUNT = 60;
static esphome::sensor::Sensor *BS[BS_COUNT];
static constexpr size_t BT_COUNT = 4;
static esphome::text_sensor::TextSensor *BT[BT_COUNT];
// 800 chars: 8 digit n, '|', then letters from n and the slot
static std::string make_txt(uint32_t n, int slot) {
  std::string s(800, ' ');
  char head[16];
  snprintf(head, sizeof(head), "%08u|", (unsigned) n);
  for (int i = 0; i < 9; i++)
    s[i] = head[i];
  for (int i = 9; i < 800; i++)
    s[i] = 'A' + (n * 7 + slot * 3 + i) % 26;
  return s;
}
