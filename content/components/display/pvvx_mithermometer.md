---
description: "Instructions for setting up BLE devices with PVVX MiThermometer custom firmware as displays."
title: "PVVX MiThermometer Display"
params:
  seo:
    description: Instructions for setting up BLE devices with PVVX MiThermometer custom firmware as displays.
    image: /components/sensor/images/xiaomi_lywsd03mmc.jpg
---

The `pvvx_mithermometer` display platform allows you to use devices running the [ATC_MiThermometer firmware](https://github.com/pvvx/ATC_MiThermometer) by pvvx as display drivers with ESPHome.

{{< img src="xiaomi_lywsd03mmc.jpg" alt="Image" caption="Xiaomi LYWSD03MMC." width="75.0%" class="align-center" >}}

The data to be displayed is transmitted as external data via BLE.
To do this, a `ble_client` component must be set up.
This component can also synchronize the time of the pvvx device by transmitting a timestamp on each connection.
After the data has been transmitted, the BLE connection is terminated in order to be able to receive the advertising data required for the `pvvx_mithermometer` sensor platform.

The pvvx firmware refreshes the screen periodically (can be set as minimum LCD refresh rate in the firmware configuration).
By default, the internal sensor data and, if available and valid (`validity_period`  ), the external data are switched every 2.5 s.
Further firmware configuration makes it possible to activate other display modes such as time and battery status.
The firmware configuration can be changed via browser using [TelinkMiFlasher.html](https://pvvx.github.io/ATC_MiThermometer/TelinkMiFlasher.html).

```yaml
# Example configuration entry
esp32_ble_tracker:

ble_client:
- mac_address: XX:XX:XX:XX:XX:XX
  id: pvvx_ble_display

display:
- platform: pvvx_mithermometer
  ble_client_id: pvvx_ble_display
  lambda: |-
    it.print_bignum(23.1);
    it.print_unit(pvvx_mithermometer::UNIT_DEG_C);
    it.print_smallnum(33);
    it.print_percent(true);
    it.print_happy(true);
    it.print_bracket(true);
```

## Configuration variables

- **ble_client_id** (**Required**, [ID](/guides/configuration-types#id)): ID of the associated BLE client.
- **time_id** (*Optional*, [ID](/guides/configuration-types#id)): ID of a {{< docref "/components/time" >}}. If set, the time will be synchronized with every connection.
- **disconnect_delay** (*Optional*, [Time](/guides/configuration-types#time)): The amount of time the BLE connection is maintained before being disconnected again. Defaults to `5s`.
- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to transmit the display data. Defaults to `60s`.
- **validity_period** (*Optional*, [Time](/guides/configuration-types#time)): The time periode for which the pvvx device should display the information. Defaults to `5min`.
- **lambda** (*Optional*, [lambda](/automations/templates#config-lambda)): The lambda to use to define the information to be displayed.
  See [Rendering Lambda](#display-pvvx_mithermometer_lambda) for more information.

- **auto_clear_enabled** (*Optional*, boolean): Whether to automatically clear the display data before each lambda call,
  or to keep the existing display content (must overwrite explicitly, e.g., only on data change). Defaults to `true` if a lambda or pages are configured, false otherwise.

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.

{{< anchor "display-pvvx_mithermometer_lambda" >}}

## Rendering Lambda

The `pvvx_mithermometer` displays can only show two numbers with optional units and a smiley face. Therefore, the API is tailord to these limitations.
In the lambda you're passed a variable called `it` as with all other displays. In this case however, `it` is a `PVVXDisplay` instance (see API Reference).

```yaml
display:
  - platform: pvvx_mithermometer
    # ...
    lambda: |-
      // Print -2.1 as big number (first row)
      it.print_bignum(-2.1);
      // Print °C next to the big number
      it.print_unit(pvvx_mithermometer::UNIT_DEG_C);
      // Print 88 as small number (second row)
      it.print_smallnum(88);
      // Print % next to the small number
      it.print_percent(true);
      // Print the low battery symbol
      it.print_battery(true);

      // Print a happy smiley. Results in " ^_^ "
      it.print_happy(true);
      // Print a sad smiley. Results in " -∧- "
      it.print_sad(true);
      // The comination of happy and sad simley results in " Δ△Δ "

      // Print round brackets around the simley
      it.print_bracket(true);
      // The final result is "(Δ△Δ)"
```

Valid values for the big number (`it.print_bignum()`  ) are from -99.5 to 1999.5. Smaller values are displayed as `Lo`, larger ones as `Hi`. It will be printed to the screen. If not defined, a 0 will be displayed.

Valid values for the small number (`it.print_smallnum()`  ) are from -9 to 99. Smaller values are displayed as `Lo`, larger ones as `Hi`. If not defined, a 0 will be displayed.

Possible values for the unit of the big number (`it.print_unit()`  ) are:

- `pvvx_mithermometer::UNIT_NONE`  : do not show a unit
- `pvvx_mithermometer::UNIT_DEG_GHE`  : show `°Г`
- `pvvx_mithermometer::UNIT_MINUS`  : show `-`
- `pvvx_mithermometer::UNIT_DEG_F`  : show `°F`
- `pvvx_mithermometer::UNIT_LOWDASH`  : show `_`
- `pvvx_mithermometer::UNIT_DEG_C`  : show `°C`
- `pvvx_mithermometer::UNIT_LINES`  : show `=`
- `pvvx_mithermometer::UNIT_DEG_E`  : show `°E`

The appearance of the smiley can be defined by combining the functions `it.print_happy()`, `it.print_sad()` and `it.print_bracket(true)`  :

| `print_bracket()` | `print_sad()` | `print_happy()` | result  |
| ----------------- | ------------- | --------------- | ------- |
| false             | false         | false           |         |
| false             | false         | true            | `^_^`   |
| false | true | false | `-∧-`   |
| false | true | true | `Δ△Δ`   |
| true | false | false | `(   )` |
| true | false | true | `(^_^)` |
| true | true | false | `(-∧-)` |
| true | true | true | `(Δ△Δ)` |

### Display states of other sensors

The following example display the sensor states of a MiFlora sensor on a pvvx display. The time is also synchronized.

```yaml
time:
  - platform: homeassistant
    id: homeassistant_time

esp32_ble_tracker:

ble_client:
- mac_address: XX:XX:XX:XX:XX:XX
  id: pvvx_ble_display

sensor:
- platform: pvvx_mithermometer
  mac_address: XX:XX:XX:XX:XX:XX
  temperature:
    name: "PVVX Temperature"
  humidity:
    name: "PVVX Humidity"
  battery_level:
    name: "PVVX Battery-Level"
  battery_voltage:
    name: "PVVX Battery-Voltage"
- platform: xiaomi_hhccjcy01
  mac_address: XX:XX:XX:XX:XX:XX
  temperature:
    name: "Xiaomi HHCCJCY01 Temperature"
    id: miflora_temperature
  moisture:
    name: "Xiaomi HHCCJCY01 Moisture"
    id: miflora_moisture
  illuminance:
    name: "Xiaomi HHCCJCY01 Illuminance"
  conductivity:
    name: "Xiaomi HHCCJCY01 Soil Conductivity"

display:
- platform: pvvx_mithermometer
  ble_client_id: pvvx_ble_display
  update_interval: 10min
  validity_period: 15min
  time_id: homeassistant_time
  lambda: |-
    double temp = id(miflora_temperature).state;
    double moisture = id(miflora_moisture).state;
    it.print_bignum(temp);
    it.print_unit(pvvx_mithermometer::UNIT_DEG_C);
    it.print_smallnum(moisture);
    it.print_percent();
    if (temp < 5 || temp > 30 || moisture < 10 || moisture > 50) {
      it.print_sad();
    } else {
      it.print_happy();
    }
```

### Only synchronize the time once a day

The following example will synchronized the time of the pvvx device once a day.

```yaml
time:
  - platform: homeassistant
    id: homeassistant_time

esp32_ble_tracker:

ble_client:
- mac_address: XX:XX:XX:XX:XX:XX
  id: pvvx_ble_display

sensor:
- platform: pvvx_mithermometer
  mac_address: XX:XX:XX:XX:XX:XX
  temperature:
    name: "PVVX Temperature"
  humidity:
    name: "PVVX Humidity"
  battery_level:
    name: "PVVX Battery-Level"
  battery_voltage:
    name: "PVVX Battery-Voltage"

display:
- platform: pvvx_mithermometer
  ble_client_id: pvvx_ble_display
  update_interval: 24h
  validity_period: 0s
  time_id: homeassistant_time
```

## See Also

- {{< docref "index/" >}}
- {{< docref "/components/ble_client" >}}
- {{< docref "/components/sensor/xiaomi_ble" >}}
- {{< apiref "pvvx_mithermometer/display/pvvx_display.h" "pvvx_mithermometer/display/pvvx_display.h" >}}
- [ATC_MiThermometer firmware](https://github.com/pvvx/ATC_MiThermometer) by [pvvx](https://github.com/pvvx)
