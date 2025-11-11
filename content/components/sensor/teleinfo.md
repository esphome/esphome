---
description: "Instructions for setting up French Teleinformation"
title: "Teleinformation from Linky electrical counter."
params:
  seo:
    description: Instructions for setting up French Teleinformation
    image: teleinfo.jpg
---

## Component/Hub

The `teleinfo` component allows you to retrieve data from a
French electrical counter using Teleinformation ([datasheet](https://www.enedis.fr/media/2035/download)). It works with Linky electrical
counter but also legacy EDF electrical counter.

{{< img src="teleinfo-full.jpg" alt="Image" caption="Linky electrical counter" width="50.0%" class="align-center" >}}

..

A simple electronic assembly with an optocoupler and a resistor could
let you retrieve detailed power consumption or power production.
There is plenty of example on the web.

As the communication with the Teleinformation is done using UART, you need to
have an [UART bus](/components/uart) in your configuration with the `rx_pin`
connected to the output of the optocoupler component. Additionally, you need to
set the baud rate to 9600bps if counter is configured to work in standard
mode or 1200bps in historical mode. To find out which mode you are using,
simply press -/+ buttons on the counter and look for `Standard mode` or
`Historical mode` as below.

{{< img src="teleinfo-standard.jpg" alt="Image" caption="Linky electrical counter configured in standard mode." width="50.0%" class="align-center" >}}

..

{{< img src="teleinfo-historical.jpg" alt="Image" caption="Linky electrical counter configured in historical mode." width="50.0%" class="align-center" >}}

..

```yaml
# Example configuration entry
teleinfo:
  id: myteleinfo
```

## Configuration variables

In teleinfo platform:

- **historical_mode** (*Optional*): Whether to use historical mode or standard mode.
  With historical mode, baudrate of 1200 must be used whereas 9600 must be used in
  standard mode. Defaults to `false`.

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the
  sensor. Defaults to `60s`.

- **uart_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the [UART Component](/components/uart) if you want
  to use multiple UART buses.

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation or multiple hubs.

### Sensor

```yaml
sensor:
  - platform: teleinfo
    tag_name: "HCHC"
    name: "hchc"
    unit_of_measurement: "Wh"
    icon: mdi:flash
    teleinfo_id: myteleinfo
  - platform: teleinfo
    tag_name: "HCHP"
    name: "hchp"
    unit_of_measurement: "Wh"
    icon: mdi:flash
    teleinfo_id: myteleinfo
  - platform: teleinfo
    tag_name: "PAPP"
    name: "papp"
    unit_of_measurement: "VA"
    icon: mdi:flash
    teleinfo_id: myteleinfo
```

- **tag_name** (**Required**, string): Specify the tag you want to retrieve from the Teleinformation.
- **teleinfo_id** (*Optional*, [ID](/guides/configuration-types#id)): Specify the ID of used hub.
- All other options from [Sensor](/components/sensor).

### Text Sensor

```yaml
text_sensor:
  - platform: teleinfo
    tag_name: "OPTARIF"
    name: "optarif"
    teleinfo_id: myteleinfo
```

- **tag_name** (**Required**, string): Specify the tag you want to retrieve from the Teleinformation.
- **teleinfo_id** (*Optional*, [ID](/guides/configuration-types#id)): Specify the ID of used hub.
- All other options from [Text Sensor](/components/text_sensor#config-text_sensor).

## See Also

- {{< apiref "teleinfo/teleinfo.h" "teleinfo/teleinfo.h" >}}
