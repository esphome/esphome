---
description: "Instructions for setting up RuuviTag bluetooth-based sensors in ESPHome."
title: "RuuviTag Open Source BLE Sensor"
params:
  seo:
    description: Instructions for setting up RuuviTag bluetooth-based sensors in ESPHome.
    image: ruuvitag.jpg
---

The `ruuvitag` sensor platform lets you track the output of RuuviTag
Bluetooth Low Energy devices using the {{< docref "/components/esp32_ble_tracker" >}}.
This component will track the temperature, humidity, acceleration and battery
voltage of a RuuviTag device with RAWv1 protocol every time the sensor sends
out a BLE broadcast. RAWv2 protocol is supported too. Then tx power,
movement count and measurement sequence number are also tracked.

{{< img src="ruuvitag-full.jpg" alt="Image" caption="RuuviTagSensor over BLE." width="80.0%" class="align-center" >}}

{{< img src="ruuvitag-ui.jpg" alt="Image" width="80.0%" class="align-center" >}}

```yaml
# Example configuration entry
esp32_ble_tracker:

sensor:
- platform: ruuvitag
  mac_address: XX:XX:XX:XX:XX:XX
  humidity:
    name: "RuuviTag Humidity"
  temperature:
    name: "RuuviTag Temperature"
  pressure:
    name: "RuuviTag Pressure"
  acceleration:
    name: "RuuviTag Acceleration"
  acceleration_x:
    name: "RuuviTag Acceleration X"
  acceleration_y:
    name: "RuuviTag Acceleration Y"
  acceleration_z:
    name: "RuuviTag Acceleration Z"
  battery_voltage:
    name: "RuuviTag Battery Voltage"
  tx_power:
    name: "RuuviTag TX Power"
  movement_counter:
    name: "RuuviTag Movement Counter"
  measurement_sequence_number:
    name: "RuuviTag Measurement Sequence Number"
```

## Configuration variables

- **mac_address** (**Required**, MAC Address): The MAC address of the RuuviTag
  device.

- **humidity** (*Optional*): The information for the humidity sensor

  - All options from [Sensor](/components/sensor).

- **temperature** (*Optional*): The information for the temperature sensor.

  - All options from [Sensor](/components/sensor).

- **pressure** (*Optional*): The information for the pressure sensor.

  - All options from [Sensor](/components/sensor).

- **acceleration** (*Optional*): The information for the acceleration
  sensor.

  - All options from [Sensor](/components/sensor).

- **acceleration_x** (*Optional*): The information for the acceleration x
  sensor.

  - All options from [Sensor](/components/sensor).

- **acceleration_y** (*Optional*): The information for the acceleration y
  sensor.

  - All options from [Sensor](/components/sensor).

- **acceleration_z** (*Optional*): The information for the acceleration z
  sensor.

  - All options from [Sensor](/components/sensor).

- **battery_voltage** (*Optional*): The information for the battery voltage
  sensor.

  - All options from [Sensor](/components/sensor).

- **tx_power** (*Optional*): The information for the transmit power
  sensor

  - All options from [Sensor](/components/sensor).
  - Only available if RAWv2 protocol is used.

- **movement_counter** (*Optional*): The information for the movement count
  sensor

  - All options from [Sensor](/components/sensor).
  - Only available if RAWv2 protocol is used.

- **measurement_sequence_number** (*Optional*): The information for the
  measurement sequence number sensor

  - All options from [Sensor](/components/sensor).
  - Only available if RAWv2 protocol is used.

## Setting Up Devices

To set up RuuviTag devices you first need to find their MAC Address so that
ESPHome can identify them. So first, create a simple configuration without any
`ruuvitag` entries but with `ruuvi_ble` enabled like so:

```yaml
esp32_ble_tracker:

ruuvi_ble:
```

After uploading the ESP32 will immediately try to scan for BLE devices.
When it detects these sensors, it will automatically parse the BLE message
print a message like this one:

```log
Got ruuvi RuuviTag (XX:XX:XX:XX:XX:XX): Humidity: 67.5%, Temperature: 22.97°C,
Pressure: 977.09hPa, Acceleration X: 0.005G, Acceleration Y: 0.017G, Acceleration Z: 1.066G,
Battery Voltage: 3.223V
```

Then just copy the address (`XX:XX:XX:XX:XX:XX`  ) into a new
`sensor.ruuvitag` platform entry like in the configuration example at the top.

> [!NOTE]
> The ESPHome Ruuvi BLE component listens passively to packets the RuuviTag device sends by itself.
> ESPHome therefore has no impact on the battery life of the device.

## See Also

- {{< docref "/components/esp32_ble_tracker" >}}
- {{< docref "/components/sensor" >}}
- {{< docref "absolute_humidity/" >}}
- {{< apiref "ruuvitag/ruuvitag.h" "ruuvitag/ruuvitag.h" >}}
- [Ruuvi](https://ruuvi.com)
