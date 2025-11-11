---
description: "Instructions for setting up a Growatt inverter reading on modbus."
title: "Growatt Solar"
params:
  seo:
    description: Instructions for setting up a Growatt inverter reading on modbus.
    image: growatt.jpg
---

The `Growatt Inverter` sensor platform allows you to use growatt inverter data reading on modbus with ESPHome.

{{< img src="growatt.jpg" alt="Image" caption="Growatt Logo" width="50.0%" class="align-center" >}}

The communication with this component is done over a [UART bus](/components/uart) using [Modbus](/components/modbus#modbus).
You must therefore have a `uart:` and `modbus:` entry in your configuration with both the TX and RX pins set
to some pins on your board and the baud rate set to 9600.

```yaml
# Example configuration
sensor:
  - platform: growatt_solar
    protocol_version: RTU

    inverter_status:
      name: "Growatt Status Code"

    phase_a:
      voltage:
          name: "Growatt Voltage Phase A"
      current:
          name: "Growatt Current Phase A"
      active_power:
          name: "Growatt Power Phase A"
    phase_b:
      voltage:
          name: "Growatt Voltage Phase B"
      current:
          name: "Growatt Current Phase B"
      active_power:
          name: "Growatt Power Phase B"
    phase_c:
      voltage:
          name: "Growatt Voltage Phase C"
      current:
          name: "Growatt Current Phase C"
      active_power:
          name: "Growatt Power Phase C"

    pv1:
      voltage:
          name: "Growatt PV1 Voltage"
      current:
          name: "Growatt PV1 Current"
      active_power:
          name: "Growatt PV1 Active Power"

    pv2:
      voltage:
          name: "Growatt PV2 Voltage"
      current:
          name: "Growatt PV2 Current"
      active_power:
          name: "Growatt PV2 Active Power"

    active_power:
      name: "Growatt Grid Active Power"

    pv_active_power:
      name: "Growatt PV Active Power"

    frequency:
      name: "Growatt Frequency"

    energy_production_day:
      name: "Growatt Today's Generation"

    total_energy_production:
      name: "Growatt Total Energy Production"

    inverter_module_temp:
      name: "Growatt Inverter Module Temp"
```

## Configuration variables

- **inverter_status** (*Optional*): Status code of the inverter (0: waiting, 1: normal, 3:fault)

- **protocol_version** (*Optional*): Version of the protocol used by your inverter.
  Old inverters use RTU (default). Newer ones use RTU2 (e.g. MIC, MIN, MAX series)

- **phase_a** (*Optional*): The group of exposed sensors for Phase A/1.

  - **current** (*Optional*): Use the current value of the sensor in amperes. All options from
    [Sensor](/components/sensor).

  - **voltage** (*Optional*): Use the voltage value of the sensor in volts.
    All options from [Sensor](/components/sensor).

  - **active_power** (*Optional*): Use the (active) power value of the sensor in watts. All options
    from [Sensor](/components/sensor).

- **phase_b** (*Optional*): The group of exposed sensors for Phase B/2 on applicable inverters.

  - All options from **phase_a**

- **phase_c** (*Optional*): The group of exposed sensors for Phase C/3 on applicable inverters.

  - All options from **phase_a**

- **pv1** (*Optional*): The group of exposed sensors for Photo Voltaic 1.

  - **current** (*Optional*): Use the current value of the sensor in amperes. All options from
    [Sensor](/components/sensor).

  - **voltage** (*Optional*): Use the voltage value of the sensor in volts.
    All options from [Sensor](/components/sensor).

  - **active_power** (*Optional*): Use the (active) power value of the sensor in watts. All options
    from [Sensor](/components/sensor).

- **pv2** (*Optional*): The group of exposed sensors for Photo Voltaic 2.

  - All options from **pv1**

- **active_power** (*Optional*): Use the (active) power value for the Grid in watts. All options
  from [Sensor](/components/sensor).

- **pv_active_power** (*Optional*): Use the (active) power value of PVs in total in watts. All options
  from [Sensor](/components/sensor).

- **frequency** (*Optional*): Use the frequency value of the sensor in hertz.
  All options from [Sensor](/components/sensor).

- **energy_production_day** (*Optional*): Use the export active energy value for same day of the
  sensor in kilo watt hours. All options from [Sensor](/components/sensor).

- **total_energy_production** (*Optional*): Use the total exported energy value of the sensor in
  kilo watt hours. All options from [Sensor](/components/sensor).

- **inverter_module_temp** (*Optional*): Use the inverter module temperature value of the sensor in
  degree celsius. All options from [Sensor](/components/sensor).

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the
  sensor. Defaults to `10s`.

- **address** (*Optional*, int): The address of the sensor if multiple sensors are attached to
  the same UART bus. You will need to set the address of each device manually. Defaults to `1`.

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
