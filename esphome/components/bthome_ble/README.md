# BTHome BLE usage example

```yaml
esphome:
  name: living-room-bthome
  platform: ESP32
  board: esp32dev

wifi:
  ssid: "MyWiFi"
  password: "secret"

logger:

# Enable BLE tracking
esp32_ble_tracker:
  id: ble_tracker

# BTHome BLE sensors
sensor:
  - platform: bthome_ble
    mac_address: AA:BB:CC:DD:EE:FF
    esp32_ble_id: ble_tracker
    temperature:
      name: "BTHome Temperature"
    humidity:
      name: "BTHome Humidity"
    battery_level:
      name: "BTHome Battery"
    signal_strength:
      name: "BTHome RSSI"
```

Replace the `mac_address` value with the address of your BTHome device. Configure the sensors you want to expose; any omitted sensors are simply not created. Setting an explicit `esp32_ble_id` on each platform entry ties the BTHome entities to the tracker declared above and avoids ID resolution errors during validation.
