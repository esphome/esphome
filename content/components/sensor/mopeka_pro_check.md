---
description: "Instructions for setting up Mopeka Pro Check or Lippert Propane Tank bluetooth-based sensors in ESPHome."
title: "Mopeka Pro Check BLE Sensor"
params:
  seo:
    description: Instructions for setting up Mopeka Pro Check or Lippert Propane Tank bluetooth-based sensors in ESPHome.
    image: mopeka_pro_check.jpg
---

The `mopeka_pro_check` sensor platform lets you track the output of Mopeka Pro
Check LP, Mopeka Pro Plus, Mopeka Pro Universal or Lippert Propane Tank Sensors, Bluetooth Low
Energy devices using the {{< docref "/components/esp32_ble_tracker" >}}. This component
will track the tank level, distance, temperature, battery percentage, and sensor reading quality of a
device every time the sensor sends out a BLE broadcast. There are additional configuration options
to control handling of poor quality readings and reporting reading quality issues.

> [!WARNING]
> This sensor component only supports the following sensors:
>
> + Mopeka Pro Check devices
> + Mopeka Pro Plus devices
> + Mopeka Pro Check Universal Sensor
> + Lippert Propane Tank Sensor, part number 2021130655
>
> Sensors are calibrated for propane only.
>
> See {{< docref "/components/sensor/mopeka_std_check" >}} for original Mopeka Check sensors support.

{{< img src="mopeka_pro_check.jpg" alt="Image" caption="Mopeka Pro Check LP over BLE." class="align-center" >}}

{{< img src="mopeka_pro_check_lippert.jpg" alt="Image" caption="Lippert™ Propane Tank Sensor" class="align-center" >}}

The original Mopeka Check sensors are not supported.

## Mopeka Pro Check LP over BLE

```yaml
esp32_ble_tracker:

sensor:
  # Example using 20lb vertical propane tank.
  - platform: mopeka_pro_check
    mac_address: XX:XX:XX:XX:XX:XX
    tank_type: 20LB_V
    temperature:
        name: "Propane test temp"
    level:
        name: "Propane test level"
    distance:
        name: "Propane test distance"
    battery_level:
        name: "Propane test battery level"
    signal_quality:
        name: "Propane test read quality"
    ignored_reads:
        name: "Propane test ignored reads"
    # Report sensor distance/level data for all equal or greater than
    minimum_signal_quality: "LOW"

  # Custom example - user defined empty / full points
  - platform: mopeka_pro_check
    mac_address: XX:XX:XX:XX:XX:XX
    tank_type: CUSTOM
    custom_distance_full: 40cm
    custom_distance_empty: 10mm
    temperature:
        name: "Propane c test temp"
    level:
        name: "Propane c test level"
    distance:
        name: "Propane c test distance"
    battery_level:
        name: "Propane c test battery level"
```

## Configuration variables

+ **mac_address** (**Required**, MAC Address): The MAC address of the Mopeka/Lippert
  device.

+ **tank_type** (**Required**): The tank type the sensor is measuring. See below.

+ **custom_distance_full** (*Optional*): distance sensor will read when it should be
  considered full (100%). This is only used when tank_type = CUSTOM

+ **custom_distance_empty** (*Optional*): distance sensor will read when it should be
  considered empty (0%). This is only used when tank_type = CUSTOM

+ **level** (*Optional*): The percentage of full for the tank sensor. If
  read is ignored due to quality this sensor will not be updated.

  + All options from [Sensor](/components/sensor).

+ **distance** (*Optional*): The raw distance/depth of the liquid for the sensor in mm.
  If read is ignored due to quality this sensor will not be updated.

  + All options from [Sensor](/components/sensor).

+ **temperature** (*Optional*): The information for the temperature sensor.
  This temperature is on the sensor and is not calibrated to ambient temperature.

  + All options from [Sensor](/components/sensor).

+ **battery_level** (*Optional*): The information for the battery percentage
  sensor. Sensor uses a standard CR2032 battery.

  + All options from [Sensor](/components/sensor).

+ **signal_quality** (*Optional*): The information for the read quality
  sensor.

  + All options from [Sensor](/components/sensor).

+ **ignored_reads** (*Optional*): A diagnostic sensor indicating the number
  of consecutive ignored reads. This resets to zero each time the read is
  equal or greater than the configured ignored quality. Only the distance
  and level sensors are not reported.

  + All options from [Sensor](/components/sensor).

+ **minimum_signal_quality** (*Optional*, enum): Each report from the sensor
  indicates the quality or confidence in the distance the sensor calculated. Physical
  sensor placement, tank material or quality, or other factors can influence the
  sensors ability to read with confidence. As quality gets lower, the accuracy of the
  distance reading may not align with expectations. This value allows configuration of
  the minimum quality level that the distance should be evaluated/reported.
  Acceptable Values:

  + `HIGH`  : High Quality
  + `MEDIUM`  : Medium Quality (default value)
  + `LOW`  : Low Quality
  + `ZERO`  : Zero Quality

## Tank Types

Currently supported Tank types are:

+ `20LB_V` - 20 LB vertical tank
+ `30LB_V` - 30 LB vertical tank
+ `40LB_V` - 40 LB vertical tank
+ `EUROPE_6KG` - 6kg vertical tank
+ `EUROPE_11KG` - 11kg vertical tank
+ `EUROPE_14KG` - 14kg vertical tank
+ `CUSTOM` - Allows you to define your own full and empty points

## Setting Up Devices

To set up the sensor devices you first need to find the MAC Address so that
ESPHome can identify it. First, create a simple configuration with the `esp32_ble_tracker`
and the `mopeka_ble` component like so:

```yaml
esp32_ble_tracker:

mopeka_ble:
```

After uploading, the ESP32 will immediately try to scan for BLE devices. Press and hold the sync button for it to be identified.
Or alternatively set the configuration flag `show_sensors_without_sync: true` to see all devices.
For all sensors found the `mopeka_ble` component will print a message like this one:

```log
[20:43:26][I][mopeka_ble:074]: MOPEKA PRO (NRF52) SENSOR FOUND: XX:XX:XX:XX:XX:XX
```

Then just copy the address (`XX:XX:XX:XX:XX:XX`  ) into a new
`sensor.mopeka_pro_check` platform entry like in the configuration example at the top.

> [!NOTE]
> The ESPHome Mopeka Pro Check BLE component listens passively to packets the Mopeka/Lippert device sends by itself.
> ESPHome therefore has no impact on the battery life of the device.

## See Also

+ {{< docref "/components/esp32_ble_tracker" >}}
+ {{< docref "/components/sensor" >}}
+ {{< apiref "mopeka_pro_check/mopeka_pro_check.h" "mopeka_pro_check/mopeka_pro_check.h" >}}
+ `Mopeka  <https://www.mopekaiot.com/shop>`
+ [Lippert](https://store.lci1.com/lippert-propane-tank-sensor-2021130655)
