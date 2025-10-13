#pragma once

#include <vector>

namespace esphome {
namespace wmbus {

  typedef struct WMbusData{
    uint16_t  length;
    uint8_t   lengthField;
    uint8_t   data[500];
    char      mode;
    char      block;
  } WMbusData;

  typedef struct WMbusFrame {
    std::vector<unsigned char> frame{};
    char mode;
    char block;
    int8_t rssi;
    uint8_t lqi;
  } WMbusFrame;

}
}