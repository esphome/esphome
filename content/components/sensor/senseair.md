---
description: "Instructions for setting up SenseAir S8 CO2 sensor"
title: "SenseAir CO_2 Sensor"
params:
  seo:
    description: Instructions for setting up SenseAir S8 CO2 sensor
    image: senseair_s8.jpg
---

The `senseair` sensor platform allows you to use SenseAir CO_2 sensor
([website](https://senseair.com/products/size-counts/s8-residential/)) with ESPHome.

{{< img src="senseair_s8-full.jpg" alt="Image" caption="SenseAir S8 CO_2 Sensor." width="50.0%" class="align-center" >}}

As the communication with the SenseAir is done using UART, you need
to have an [UART bus](/components/uart) in your configuration with the `rx_pin` connected to the TX pin of the
sensor and the `tx_pin` connected to the RX Pin (it's switched because the
TX/RX labels are from the perspective of the SenseAir sensor). Additionally, you need to set the baud rate to 9600.

```yaml
# Example configuration entry
sensor:
  - platform: senseair
    co2:
      name: "SenseAir CO2 Value"
```

## Configuration variables

- **co2** (*Optional*): The CO_2 data from the sensor in parts per million (ppm).

  - All options from [Sensor](/components/sensor).

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the
  sensor. Defaults to `60s`.

- **uart_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the [UART Component](/components/uart) if you want
  to use multiple UART buses.

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for actions.

{{< img src="senseair_s8-pins.jpg" alt="Image" caption="Pins on the SenseAir S8. Only the ones marked with a red circle need to be connected." width="80.0%" class="align-center" >}}

> [!NOTE]
> `G+` should be connected to power supply (supported voltage is 4.5 V to 5.25 V), `G0` to `GND` pin

{{< anchor "senseair-background_calibration_action" >}}

## `senseair.background_calibration` Action

This [action](/automations/actions#all-actions) initiates a background calibration on the sensor with the given ID: the current
CO2 level will be used as a reference for the 400ppm threshold. Ensure that the sensor is in a stable environment with
fresh ambient air, preferably near a window that has already been opened for a sufficient time.

```yaml
on_...:
  then:
    - senseair.background_calibration: my_senseair_id
```

{{< anchor "senseair-background_calibration_result_action" >}}

## `senseair.background_calibration_result` Action

This [action](/automations/actions#all-actions) requests the result of the background calibration procedure from the sensor
with the given ID. The value will be printed in ESPHome logs.

Wait at least one sensor lamp cycle after having triggered the background calibration before requesting its result.

```yaml
on_...:
  then:
    - senseair.background_calibration_result: my_senseair_id
```

{{< anchor "senseair-abc_get_period_action" >}}

## `senseair.abc_get_period` Action

This [action](/automations/actions#all-actions) requests the currently configured ABC interval from the sensor with the given ID.
The value will be printed in ESPHome logs.

```yaml
on_...:
  then:
    - senseair.abc_get_period: my_senseair_id
```

{{< anchor "senseair-abc_enable_action" >}}

## `senseair.abc_enable` Action

This [action](/automations/actions#all-actions) enables Automatic Baseline Calibration on the sensor with the given ID.
ABC will be activated with the default interval of 180 hours.

```yaml
on_...:
  then:
    - senseair.abc_enable: my_senseair_id
```

{{< anchor "senseair-abc_disable_action" >}}

## `senseair.abc_disable` Action

This [action](/automations/actions#all-actions) disables Automatic Baseline Calibration on the sensor with the given ID.

```yaml
on_...:
  then:
    - senseair.abc_disable: my_senseair_id
```

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< apiref "senseair/senseair.h" "senseair/senseair.h" >}}
