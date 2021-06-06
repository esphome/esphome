#include "ble_utils.h"

#include <BLE2902.h>

#include "esphome/core/log.h"

#include "esp32_ble_controller.h"

namespace esphome {
namespace esp32_ble_controller {

static const char *TAG = "ble_utils";

void show_bonded_devices()
{
    int dev_num = esp_ble_get_bond_device_num();

    esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t*) malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
    esp_ble_get_bond_device_list(&dev_num, dev_list);

    ESP_LOGI(TAG, "Bonded BLE devices (%d):", dev_num);
    for (int i = 0; i < dev_num; i++) {
      esp_bd_addr_t& bd_address = dev_list[i].bd_addr;
      ESP_LOGI(TAG, "%d) BD address %X:%X:%X:%X:%X:%X", i+1, bd_address[0], bd_address[1], bd_address[2], bd_address[3], bd_address[4], bd_address[5]);
    }

    free(dev_list);
}

void remove_all_bonded_devices()
{
    int dev_num = esp_ble_get_bond_device_num();

    esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t*) malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
    esp_ble_get_bond_device_list(&dev_num, dev_list);
    for (int i = 0; i < dev_num; i++) {
        esp_ble_remove_bond_device(dev_list[i].bd_addr);
    }

    free(dev_list);
}

BLECharacteristic* create_ble_characteristic(BLEService* service, const string& characteristic_uuid, uint32_t properties, BLECharacteristicCallbacks* callbacks, const string& description, bool with2902) {
  BLECharacteristic* characteristic = service->createCharacteristic(characteristic_uuid, properties);

  // Set access permissions.
  esp_gatt_perm_t access_permissions;
  if (global_ble_controller->get_security_enabled()) {
    access_permissions = ESP_GATT_PERM_READ_ENC_MITM | ESP_GATT_PERM_WRITE_ENC_MITM; // signing (ESP_GATT_PERM_WRITE_SIGNED_MITM) did not work with iPhone
  } else {
    access_permissions = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE;
  }
  characteristic->setAccessPermissions(access_permissions);

  // Add a 2901 descriptor to the characteristic, which sets a user-friendly description.
  BLEDescriptor* descriptor_2901 = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
  descriptor_2901->setAccessPermissions(access_permissions);
  descriptor_2901->setValue(description);
  characteristic->addDescriptor(descriptor_2901);

  // If requested, add a 2902 descriptor to the characteristic, which lets the client control if it wants to receive new values (and notifications) for this characteristic.
  if (with2902) {
    // With this descriptor clients can switch notifications on and off, but we want to send notifications anyway as long as we are connected. The homebridge plug-in cannot turn notifications on and off.
    // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
    BLEDescriptor* descriptor_2902 = new BLE2902();
    descriptor_2902->setAccessPermissions(access_permissions);
    characteristic->addDescriptor(descriptor_2902);
  }

  if (callbacks != nullptr) {
    characteristic->setCallbacks(callbacks);
  }

  return characteristic;
}

BLECharacteristic* create_read_only_ble_characteristic(BLEService* service, const string& characteristic_uuid, const string& description, bool with2902) {
  uint32_t properties = BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY;
  return create_ble_characteristic(service, characteristic_uuid, properties, nullptr, description, with2902);
}

BLECharacteristic* create_writeable_ble_characteristic(BLEService* service, const string& characteristic_uuid, BLECharacteristicCallbacks* callbacks, const string& description, bool with2902) {
  uint32_t properties = BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE;
  return create_ble_characteristic(service, characteristic_uuid, properties, callbacks, description, with2902);
}

vector<string> split(string text, char delimiter) {
  vector<string> result;
  int j = 0;
  for (int i = 0; i < text.length(); i ++) {
    if (text[i] == delimiter) {
      string token = text.substr(j, i - j);
      if (token.length()) {
        result.push_back(token);
      }
      j = i + 1;
    }
  }
  if (j < text.length()) {
    result.push_back(text.substr(j));
  }
  return result;
}

} // namespace esp32_ble_controller
} // namespace esphome
