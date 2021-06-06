#pragma once

#include <string>
#include <vector>

#include <BLECharacteristic.h>

using std::string;
using std::vector;

namespace esphome {
namespace esp32_ble_controller {

void show_bonded_devices();
void remove_all_bonded_devices();

BLECharacteristic* create_read_only_ble_characteristic(BLEService* service, const string& characteristic_uuid, const string& description, bool with2902 = true);

BLECharacteristic* create_writeable_ble_characteristic(BLEService* service, const string& characteristic_uuid, BLECharacteristicCallbacks* callbacks, const string& description, bool with2902 = true);

vector<string> split(string text, char delimiter = ' ');

} // namespace esp32_ble_controller
} // namespace esphome
