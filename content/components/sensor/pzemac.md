---
description: "Instructions for setting up PZEM-004T power monitors."
title: "Peacefair PZEM-004T V3 Energy Monitor"
params:
  seo:
    description: Instructions for setting up PZEM-004T power monitors.
    image: pzem-ac.jpg
---

> [!NOTE]
> This page is incomplete and could use some work. If you want to contribute, please see our
> [developer site](https://developers.esphome.io). This page is missing:
>
> - Images/screenshots/example configs of this device being used in action.

The `pzemac` sensor platform allows you to use PZEM-004T V3 energy monitors
([website](https://innovatorsguru.com/pzem-004t-v3/),
[datasheet](https://innovatorsguru.com/wp-content/uploads/2019/06/PZEM-004T-V3.0-Datasheet-User-Manual.pdf))
with ESPHome.

The sensor can be connected in various configurations - please see the [manufacturer's website](https://innovatorsguru.com/pzem-004t-v3/)
for more information.

> [!WARNING]
> Please note that metering chip inside of PZEM module is powered from AC side and it has to be on during startup of ESPHome device, othervise measure results won't be visible.

{{< img src="pzem-ac.png" alt="Image" caption="PZEM-004T Version 3." width="80.0%" class="align-center" >}}

> [!WARNING]
> This page refers to version V3 of the PZEM004T.
> For using the older V1 variant of this sensor please see {{< docref "pzem004t" "pzem004t" >}}.

The communication with this component is done via a [UART](/components/uart) using [Modbus](/components/modbus#modbus).
You must therefore have a `uart:` entry in your configuration with both the TX and RX pins set
to some pins on your board and the baud rate set to 9600.

```yaml
# Example configuration entry
uart:
  rx_pin: D1
  tx_pin: D2
  baud_rate: 9600

modbus:

sensor:
  - platform: pzemac
    current:
      name: "PZEM-004T V3 Current"
    voltage:
      name: "PZEM-004T V3 Voltage"
    energy:
      name: "PZEM-004T V3 Energy"
    power:
      name: "PZEM-004T V3 Power"
    frequency:
      name: "PZEM-004T V3 Frequency"
    power_factor:
      name: "PZEM-004T V3 Power Factor"
    update_interval: 60s
```

## Configuration variables

- **current** (*Optional*): Use the current value of the sensor in amperes. All options from
  [Sensor](/components/sensor).

- **energy** (*Optional*): Use the (active) energy value of the sensor in watt*hours. All options from
  [Sensor](/components/sensor).

- **power** (*Optional*): Use the (active) power value of the sensor in watts. All options from
  [Sensor](/components/sensor).

- **voltage** (*Optional*): Use the voltage value of the sensor in volts.
  All options from [Sensor](/components/sensor).

- **frequency** (*Optional*): Use the frequency value of the sensor in hertz.
  All options from [Sensor](/components/sensor).

- **power_factor** (*Optional*): Use the power factor value of the sensor.
  All options from [Sensor](/components/sensor).

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the
  sensor. Defaults to `60s`.

- **address** (*Optional*, int): The address of the sensor if multiple sensors are attached to
  the same UART bus. You will need to set the address of each device manually. Defaults to `1`.

- **modbus_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the Modbus hub.

{{< anchor "pzemac-reset_energy_action" >}}

### `pzemac.reset_energy` Action

This action resets the total energy value of the pzemac device with the given ID when executed.

```yaml
on_...:
  then:
    - pzemac.reset_energy: pzemac_1
```

## Changing the address of a PZEM-004T

You can use the following configuration to change the address of a sensor.
You must set the `address` of the `modbus_controller` to the current address, and `new_address` of the `on_boot` lambda to the new one.

> [!WARNING]
> This should be used only once! After changing the address, this code should be removed from the ESP before using the actual sensor code.

```yaml
esphome:
  ...
  on_boot:
    ## configure controller settings at setup
    ## make sure priority is lower than setup_priority of modbus_controller
    priority: -100
    then:
      - lambda: |-
          auto new_address = 0x03;

          if(new_address < 0x01 || new_address > 0xF7) // sanity check
          {
            ESP_LOGE("ModbusLambda", "Address needs to be between 0x01 and 0xF7");
            return;
          }

          esphome::modbus_controller::ModbusController *controller = id(pzem);
          auto set_addr_cmd = esphome::modbus_controller::ModbusCommandItem::create_write_single_command(
            controller, 0x0002, new_address);

          delay(200) ;
          controller->queue_command(set_addr_cmd);
          ESP_LOGI("ModbusLambda", "PZEM Addr set");

modbus:
  send_wait_time: 200ms
  id: mod_bus_pzem

modbus_controller:
  - id: pzem
    # The current device address.
    address: 0x1
    # The special address 0xF8 is a broadcast address accepted by any pzem device,
    # so if you use this address, make sure there is only one pzem device connected
    # to the uart bus.
    # address: 0xF8
    modbus_id: mod_bus_pzem
    command_throttle: 0ms
    setup_priority: -10
    update_interval: 30s
```

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< docref "pzem004t/" >}}
- {{< docref "pzemdc/" >}}
- {{< apiref "pzemac/pzemac.h" "pzemac/pzemac.h" >}}
