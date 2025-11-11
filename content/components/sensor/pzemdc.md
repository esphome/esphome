---
description: "Instructions for setting up DC PZEM power monitors."
title: "Peacefair PZEM-00X DC Energy Monitor"
params:
  seo:
    description: Instructions for setting up DC PZEM power monitors.
    image: pzem-dc.jpg
---

> [!NOTE]
> This page is incomplete and could use some work. If you want to contribute, please see our
> [developer site](https://developers.esphome.io). This page is missing:
>
> - Images/screenshots/example configs of this device being used in action.

The `pzemdc` sensor platform allows you to use various DC Peacefair PZEM energy monitors
with ESPHome. The supported models are: PZEM-003, PZEM-014, PZEM-016 and PZEM-017.

The communication with this component is via a [UART](/components/uart).
You must therefore have a `uart:` entry in your configuration with both the TX and RX pins set
to some pins on your board and the baud rate set to 9600.

{{< img src="pzem-dc.png" alt="Image" caption="PZEM-0xx Energy Monitor." width="80.0%" class="align-center" >}}

```yaml
# Example configuration entry
uart:
  tx_pin: D1
  rx_pin: D2
  baud_rate: 9600
  stop_bits: 2

sensor:
  - platform: pzemdc
    current:
      name: "PZEM-003 Current"
    voltage:
      name: "PZEM-003 Voltage"
    power:
      name: "PZEM-003 Power"
    energy:
      name: "PZEM-003 Energy"
    update_interval: 60s
```

## Configuration variables

- **current** (*Optional*): Use the current value of the sensor in amperes. All options from
  [Sensor](/components/sensor).

- **power** (*Optional*): Use the power value of the sensor in watts. All options from
  [Sensor](/components/sensor).

- **voltage** (*Optional*): Use the voltage value of the sensor in volts.
  All options from [Sensor](/components/sensor).

- **energy** (*Optional*): Use the energy value of the sensor in kWh.
  All options from [Sensor](/components/sensor).

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the
  sensor. Defaults to `60s`.

- **address** (*Optional*, int): The address of the sensor if multiple sensors are attached to
  the same UART bus. You will need to set the address of each device manually. Defaults to `1`.

{{< anchor "pzemdc-reset_energy_action" >}}

### `pzemdc.reset_energy` Action

This action resets the total energy value of the pzemdc device with the given ID when executed.

```yaml
on_...:
  then:
    - pzemdc.reset_energy: pzemdc_1
```

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< docref "pzem004t/" >}}
- {{< docref "pzemac/" >}}
- {{< apiref "pzemdc/pzemdc.h" "pzemdc/pzemdc.h" >}}
