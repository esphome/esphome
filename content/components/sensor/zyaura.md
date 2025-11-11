---
description: "Instructions for setting up ZyAura co2, temperature and humidity monitors."
title: "ZyAura CO2 & Temperature & Humidity Sensor"
params:
  seo:
    description: Instructions for setting up ZyAura co2, temperature and humidity monitors.
    image: zgm053.jpg
---

The ZyAura CO2 & Temperature & Humidity sensor allows you to use your
[ZGm05(3)(U)](http://www.zyaura.com/products/ZGm05.asp)
[MT8057](https://masterkit.ru/shop/1266110),
[ZG1683R(U)](http://www.zyaura.com/products/ZG1683R.asp) ([MT8060](https://masterkit.ru/shop/1921398)),
[ZG1583RUD](http://www.zyaura.com/products/ZG1583RUD.asp)
monitors with ESPHome.

{{< img src="zgm053-full.jpg" alt="Image" caption="ZyAura ZGm053U CO2 & Temperature Monitor." width="80.0%" class="align-center" >}}

{{< img src="zgm053-connection.jpg" alt="Image" caption="ZyAura ZGm053U connection diagram (1 - empty, 2 - clock, 3 - data, 4 - GND). In some other models the clock and data pins are swapped." width="80.0%" class="align-center" >}}

```yaml
# Example configuration entry
sensor:
  - platform: zyaura
    clock_pin: D1
    data_pin: D2
    co2:
      name: "ZyAura CO2"
    temperature:
      name: "ZyAura Temperature"
    humidity:
      name: "ZyAura Humidity"
```

## Configuration variables

- **clock_pin** (**Required**, [Pin](/guides/configuration-types#pin)): The pin where the clock bus is connected.
- **data_pin** (**Required**, [Pin](/guides/configuration-types#pin)): The pin where the data bus is connected.
- **co2** (*Optional*): The information for the CO2 sensor.

  - All options from [Sensor](/components/sensor).

- **temperature** (*Optional*): The information for the temperature sensor.

  - All options from [Sensor](/components/sensor).

- **humidity** (*Optional*): The information for the humidity sensor

  - All options from [Sensor](/components/sensor).

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to check the
  sensor. Defaults to `60s`.

> [!NOTE]
> ZGm05 monitor (and maybe others) needs some initial time to get correct data when powered
> on. Only after this timespan will the sensor report correct values. It's not recommended to set
> `update_interval` lower than `20s`.

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< docref "absolute_humidity/" >}}
- {{< docref "mhz19/" >}}
- [CO2mon-esp firmware](https://github.com/Anonym-tsk/co2mon-esp) by [@anonym-tsk](https://github.com/Anonym-tsk)
- [Some information about hacking MT8060](https://habr.com/ru/company/dadget/blog/394333/)
- [CO2MeterHacking project](https://revspace.nl/CO2MeterHacking)
- {{< apiref "zyaura/zyaura.h" "zyaura/zyaura.h" >}}
