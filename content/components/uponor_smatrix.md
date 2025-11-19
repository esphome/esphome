---
description: "Instructions for setting up an Uponor Smatrix Base Pulse underfloor heating control system in ESPHome."
title: "Uponor Smatrix Base Pulse Underfloor Heating"
params:
  seo:
    description: Instructions for setting up an Uponor Smatrix Base Pulse underfloor heating control system in ESPHome.
---

The Uponor Smatrix component allows you to integrate an Uponor Smatrix Base Pulse underfloor heating control system in ESPHome without the need for an Smatrix Pulse Com R-208 communication module.
It directly communicates with the controller and thermostats via the RS485 thermostat bus.

## Connecting to the bus

This component is able to communicate directly with the RS485 thermostat bus. For that, you will need to connect an RS485 to TTL converter to a UART bus of your ESPHome device.

The RS485 side of the converter can either be connected to one of the A/B terminals on the controller or on one of the thermostats.
The +/- terminals provide 5 volts and can be used to power your ESPHome device.

The [UART Component](/components/uart) must be configured with a baud rate of 19200, 8 data bits, no parity, 1 stop bit.

{{< anchor "uponor-gettingstarted" >}}

## Getting started

The thermostats have unique addresses used for communication that are not displayed anywhere but can only be found when scanning the bus.
Start with a basic configuration that just contains the UART and Uponor hub components. Make sure that the UART pins are configured according to your wiring and the baud rate is set to 19200.

```yaml
uponor_smatrix:
```

When you upload this configuration to your ESPHome device and connect it to the Uponor Smatrix bus, it will print a list of detected addresses to the log output.

```text
[00:00:00][C][uponor_smatrix:020]: Uponor Smatrix
[00:00:00][C][uponor_smatrix:031]:   Detected unknown device addresses:
[00:00:00][C][uponor_smatrix:033]:     0x110BDE62
[00:00:00][C][uponor_smatrix:033]:     0x110BDDFF
[00:00:00][C][uponor_smatrix:033]:     0x110BDE72
[00:00:00][C][uponor_smatrix:033]:     0x110BDE4A
[00:00:00][C][uponor_smatrix:033]:     0x110BDE13
```

With that you can then add `climate` or `sensor` components for the detected devices.

```yaml
uponor_smatrix:

climate:
  - platform: uponor_smatrix
    address: 0x110BDE13
    name: Thermostat Living Room
```

> [!IMPORTANT]
> Previous versions of the component used a 16 bit system address in addition to 16 bit device addresses.
> This has now been combined into 32 bit device addresses.
> Please update your configuration by prepending the old system address to your device addresses.
> **Example:** The system address 0x110B and device address 0xDE13 should now become 0x110BDE13.

## Component/Hub

The main `uponor_smatrix` component is responsible for the communication with the controller and thermostats and distributes data to the climate and sensor components described below.

It is also able to synchronize the date and time of the thermostats with a time source in case your system has thermostats that can be programmed with a time schedule.

```yaml
uponor_smatrix:
  uart_id: my_uart
  time_id: my_time
```

### Configuration variables

- **uart_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the
  [UART Component](/components/uart) if you want to use multiple UART buses.
- **time_id** (*Optional*, [ID](/guides/configuration-types#id)): Specify the ID of the
  {{< docref "time/index" "Time Component" >}} to use as the time source if you want ESPHome to automatically
  synchronize the date and time of the thermostats.
- **time_device_address** (*Optional*, int): The 32 bit device address of the thermostat that keeps the system time.
  This will be automatically detected from the bus if not specified.
  It needs to be the device address of the first thermostat that was paired to the controller, and the one where you can manually change the date and time via the buttons on the thermostat.

> [!NOTE]
> The address of the thermostat keeping the time will be automatically detected from the bus if not specified in the configuration!
> You can safely leave it out in almost all cases. Time synchronization should work automatically as long as you add any time component to your configuration.

## Climate

```yaml
climate:
  - platform: uponor_smatrix
    address: 0x110BDE13
    name: Thermostat Living Room
```

### Configuration variables

- **address** (**Required**, int): The 32 bit device address of the thermostat.
  See [Getting started](#uponor-gettingstarted) on how to find the address.
- **uponor_smatrix_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the
  `uponor_smatrix` hub component if you want to use multiple hub components on one ESPHome device.
- All options from [Climate](/components/climate#config-climate).

## Sensor

```yaml
sensor:
  - platform: uponor_smatrix
    address: 0x110BDE13
    humidity:
      name: Humidity Living Room
    temperature:
      name: Temperature Living Room
    external_temperature:
      name: Floor Temperature Living Room
    target_temperature:
      name: Thermostat Target Temperature Living Room
```

### Configuration variables

- **address** (**Required**, int): The 32 bit device address of the thermostat.
  See [Getting started](#uponor-gettingstarted) on how to find the address.
- **uponor_smatrix_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the
  `uponor_smatrix` hub component if you want to use multiple hub components on one ESPHome device.
- **humidity** (*Optional*): A sensor reading the current humidity the thermostat reports.
  All options from [Sensor](/components/sensor).

- **temperature** (*Optional*): A sensor reading the current temperature the thermostat reports.
  All options from [Sensor](/components/sensor).

- **external_temperature** (*Optional*): A sensor reading the current external temperature the thermostat reports.
  This comes from an optionally attached external temperature sensor that can measure the floor or outdoor temperature.
  All options from [Sensor](/components/sensor).

- **target_temperature** (*Optional*): A sensor reading the currently set target temperature the thermostat reports.
  All options from [Sensor](/components/sensor).

## See Also

- [Protocol Analysis](https://github.com/kroimon/uponor-smatrix-analysis)
- {{< apiref "uponor_smatrix/uponor_smatrix.h" "uponor_smatrix/uponor_smatrix.h" >}}
