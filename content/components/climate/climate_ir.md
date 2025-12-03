---
description: "Controls a variety of compatible Climate devices via IR"
title: "IR Remote Climate"
params:
  seo:
    description: Controls a variety of compatible Climate devices via IR
    image: air-conditioner-ir.svg
---

This climate component allows you to control compatible AC units by sending an infrared (IR)
control signal, just as the unit's handheld remote controller would.

{{< img src="climate-ui.png" alt="Image" width="40.0%" class="align-center" >}}

There is a growing list of compatible units. If your unit is not listed below you should
submit a feature request (see FAQ).

| Supported units                           | Platform name                      | Supports receiver |
| ----------------------------------------- | ---------------------------------- | ----------------- |
| Ballu                                     | `ballu`                            | yes               |
| Coolix                                    | `coolix`                           | yes               |
| Daikin | `daikin`                           | yes |
| [Daikin ARC](#daikin_arc) | `daikin_arc`                       | yes |
| [Daikin BRC](#daikin_brc) | `daikin_brc`                       | yes |
| [Delonghi](#delonghi_ir) | `delonghi`                         | yes |
| Emmeti | `emmeti`                           | yes |
| Fujitsu General | `fujitsu_general`                  | yes |
| [GREE](#gree_ir) | `gree`                             | |
| Hitachi | `hitachi_ac344`, `hitachi_ac424` | yes |
| [LG](#climate_ir_lg) | `climate_ir_lg`                    | yes |
| [Midea](#midea_ir) | `midea_ir`                         | yes |
| [Mitsubishi](#mitsubishi) | `mitsubishi`                       | yes |
| Noblex | `noblex`                           | yes |
| Electrolux, TCL, Fuego | `tcl112`                           | yes |
| [Toshiba](#toshiba) | `toshiba`                          | yes |
| [Whirlpool](#whirlpool) | `whirlpool`                        | yes |
| Yashima | `yashima`                          | |
| [Whynter](#whynter) | `whynter`                          | yes |
| [ZH/LT-01](#zhlt01) | `zhlt01`                           | yes |
| [Arduino-HeatpumpIR](#heatpumpir) library | `heatpumpir`                       | |

This component requires that you have configured a {{< docref "/components/remote_transmitter" >}}.

Due to the unidirectional nature of IR remote controllers, this component cannot determine the
actual state of the device and will assume the state of the device is the latest state requested.
The assumed state can be restored at boot.

However, when receiver is supported, you can optionally add a {{< docref "/components/remote_receiver" >}}
component so the climate state will be tracked when it is operated with the original remote
controller unit.

```yaml
# Example configuration entry
remote_transmitter:
  pin: GPIOXX
  carrier_duty_percent: 50%

climate:
  - platform: REPLACEME
    name: "Living Room AC"
```

## Configuration Variables

- **sensor** (*Optional*, [ID](/guides/configuration-types#id)): The sensor that is used to measure the ambient
  temperature. This is only for reporting the current temperature in the frontend.
- **humidity_sensor** (*Optional*, [ID](/guides/configuration-types#id)): The sensor that is used to measure the ambient
  humidity. This is only for reporting the current humidity in the frontend.

- **supports_cool** (*Optional*, boolean): Enables setting cooling mode for this climate device. Defaults to `true`.
- **supports_heat** (*Optional*, boolean): Enables setting heating mode for this climate device. Defaults to `true`.
- **receiver_id** (*Optional*, [ID](/guides/configuration-types#id)): The id of the remote_receiver if this platform supports
  receiver. see: [Using a Receiver](#ir-receiver_id).

- All other options from [Climate](/components/climate#config-climate).

### Advanced Options

- **transmitter_id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID of the remote transmitter.

{{< anchor "climate_ir_lg" >}}

### `climate_ir_lg`

- **header_high** (*Optional*, [Time](/guides/configuration-types#time)): time for the high part of the header for the LG protocol. Defaults to `8000us`
- **header_low** (*Optional*, [Time](/guides/configuration-types#time)): time for the low part of the header for the LG protocol. Defaults to `4000us`
- **bit_high** (*Optional*, [Time](/guides/configuration-types#time)): time for the high part of any bit in the LG protocol. Defaults to `600us`
- **bit_one_low** (*Optional*, [Time](/guides/configuration-types#time)): time for the low part of a '1' bit in the LG protocol. Defaults to `1600us`
- **bit_zero_low** (*Optional*, [Time](/guides/configuration-types#time)): time for the low part of a '0' bit in the LG protocol. Defaults to `550us`

```yaml
# Example configuration entry
climate:
  - platform: climate_ir_lg
    name: "AC"
    sensor: room_temperature
    header_high: 3265us # AC Units from LG in Brazil, for example use these timings
    header_low: 9856us
```

{{< anchor "daikin_brc" >}}

### `daikin_brc`

The Daikin BRC remotes are used by the ceiling cassette model of Daikin heatpumps.

- **use_fahrenheit** (*Optional*, boolean): U.S. models of the Daikin BRC remote send the temperature in Fahrenheit, if your remote shows Fahrenheit and can not be changed to Celsius then set this to true. Defaults to `false`.

```yaml
# Example configuration entry
climate:
  - platform: daikin_brc
    name: "AC"
    sensor: room_temperature
    use_fahrenheit: true
```

{{< anchor "delonghi_ir" >}}

### `delonghi`

The `delonghi` climate currently supports the protocol used by some Delonghi portable units, known working with Delonghi PAC WE 120HP.

{{< anchor "daikin_arc" >}}

### `daikin_arc`

The Daikin ARC remotes (`daikin_arc` climate, `daikin_arc417`, `daikin_arc480` protocols of [Arduino-HeatpumpIR](#heatpumpir)) are used by the japanese model of Daikin.

{{< anchor "gree_ir" >}}

### `gree`

- **model** (**Required**, string): GREE has a few different protocols depending on model. One of these will likely work for you:

  - `generic`
  - `yan`
  - `yaa`
  - `yac`
  - `yac1fb9`
  - `yx1ff`
  - `yag`

```yaml
# Example configuration entry for climate only
climate:
  - platform: gree
    name: "AC"
    id: my_gree_ac
    sensor: room_temperature
    model: yan
```

Models `yan`, `yaa`, `yac` and `yac1fb9` support a couple of additional features which can be controlled with switches:

- **gree_id** (**Required**, [ID](/guides/configuration-types#id)): Specify the ID of the `gree` climate to which these swicthes should belong.
- **light** (*Optional*, [Switch](/components/switch#config-switch)): To turn off indoor unit display/LED at night for complete room darkness.
- **turbo** (*Optional*, [Switch](/components/switch#config-switch)): For maximum fan speed and fastest results.
- **health** (*Optional*, [Switch](/components/switch#config-switch)): Removal of dust and germs from the environment by ionizing the air flowing through the blades.
- **xfan** (*Optional*, [Switch](/components/switch#config-switch)): Prevention of excess moisture in the machine that cause mold, mildew, and unpleasant odors. Indoor fan will keep running for short period after turning turn off the AC, to dry the blades.

```yaml
# Example configuration entry for switches of the climate
switch:
  - platform: gree
    gree_id: my_gree_ac
    light:
      name: "AC Lights"
    turbo:
      name: "AC Turbo"
    health:
      name: "AC Health"
    xfan:
      name: "AC X-Fan"
```

{{< anchor "midea_ir" >}}

### `midea_ir`

These air conditioners support two protocols: Midea and Coolix. Therefore, when using an IR receiver, it considers both protocols and publishes the received states.

- **use_fahrenheit** (*Optional*, boolean): Allows you to transfer the temperature to the air conditioner in degrees Fahrenheit. The air conditioner display also shows the temperature in Fahrenheit. Defaults to `false`.

```yaml
# Example configuration entry
climate:
  - platform: midea_ir
    name: "AC"
    sensor: room_temperature
    use_fahrenheit: true
```

> [!NOTE]
>
> - See [Transmit Midea](/components/remote_transmitter#remote_transmitter-transmit_midea) to send custom commands, including Follow Me mode.
> - See [Toshiba](#toshiba) below if you are looking for compatibility with Midea model MAP14HS1TBL or similar.

{{< anchor "mitsubishi" >}}

### `mitsubishi`

> [!NOTE]
>
> - When using this component with Mitsubishi units that only support cooling mode, the Off command may not work. Set **supports_heat** to `false` to resolve that issue.

- **set_fan_mode** (*Optional*, string): Select the fan modes desired or that are supported on your remote. Defaults to `3levels`

  - Options are: `3levels`, `4levels`, `quiet_4levels`.

    - `3levels`  ; Low [fan speed 1], Medium [2], High [3]
    - `4levels`  ; Low [1], Middle [2], Medium [3], High [4]
    - `quiet_4levels`  ; Low [1], Middle [2], Medium [3], High [4], Quiet [5]

- **supports_dry** (*Optional*, boolean): Enables setting dry mode for this unit. Defaults to `false`.
- **supports_fan_only** (*Optional*, boolean): Enables setting fan only mode for this unit. Confirm that mode is supported on your remote. Defaults to `false`.

- **horizontal_default** (*Optional*, string): What to default to when the AC unit's horizontal direction is *not* set to swing. Defaults to `middle`.

  - Options are: `left`, `middle-left`, `middle`, `middle-right`, `right`, `auto`
- **vertical_default** (*Optional*, string): What to default to when the AC unit's vertical direction is *not* set to swing. Defaults to `middle`.

  - Options are: `down`, `middle-down`, `middle`, `middle-up`, `up`, `auto`

> [!NOTE]
>
> - This climate IR component is also known to work with some Stiebel Eltron Units. It has been tested with Stiebel Eltron IR-Remote `KM07F` and unit `ACW 25 i`

```yaml
# Example configuration entry
climate:
  - platform: mitsubishi
    name: "Heatpump"
    set_fan_mode: "quiet_4levels"
    supports_dry: "true"
    supports_fan_only: "true"
    horizontal_default: "left"
    vertical_default: "down"
```

{{< anchor "toshiba" >}}

### `toshiba`

- **model** (*Optional*, string): There are four valid models:

  - `GENERIC`  : Temperature range is from 17 to 30 (default)
  - `RAC-PT1411HWRU-C`  : Temperature range is from 16 to 30; unit displays temperature in degrees Celsius
  - `RAC-PT1411HWRU-F`  : Temperature range is from 16 to 30; unit displays temperature in degrees Fahrenheit
  - `RAS-2819T`  : Temperature range is from 18 to 30; supports two-packet IR protocol

> [!NOTE]
>
> - While they are identified as separate models here, the `RAC-PT1411HWRU-C` and `RAC-PT1411HWRU-F` are
>   in fact the same physical model/unit. They are separated here only because different IR codes are used
>   depending on the desired unit of measurement. This only affects how temperature is displayed on the unit itself.
>
> - The `RAC-PT1411HWRU` model supports a feature Toshiba calls "Comfort Sense". The handheld remote control
>   has a built-in temperature sensor and it will periodically transmit the temperature from this sensor to the
>   AC unit. If a `sensor` is provided in the configuration with this model, the sensor's temperature will be
>   transmitted to the `RAC-PT1411HWRU` in the same manner as the original remote controller. How often the
>   temperature is transmitted is determined by the `update_interval` assigned to the `sensor`. Note that
>   `update_interval` must be less than seven minutes or the `RAC-PT1411HWRU` will revert to using its own
>   internal temperature sensor; a value of 30 seconds seems to work well. See {{< docref "/components/sensor" >}}
>   for more information.
>
> - The `RAS-2819T` model uses a two-packet IR protocol where most commands send a primary packet (containing
>   temperature, mode, and fan speed) followed by a secondary packet (containing fan speed confirmation and
>   mode-specific data). Single-packet commands are used for power-off and swing toggle operations.
>
> - This climate IR component is also known to work with Midea model MAP14HS1TBL and may work with other similar
>   models, as well. (Midea acquired Toshiba's product line and re-branded it.)

```yaml
# Example configuration entry for RAS-2819T
climate:
  - platform: toshiba
    name: "Toshiba AC"
    model: RAS-2819T
    sensor: room_temperature
```

{{< anchor "whirlpool" >}}

### `whirlpool`

- **model** (*Optional*, string): There are two valid models to choose from:

  - `DG11J1-3A`  : Temperature range is from 18 to 32 (default)
  - `DG11J1-91`  : Temperature range is from 16 to 30

{{< anchor "whynter" >}}

### `whynter`

- **use_fahrenheit** (*Optional*, boolean): Allows you to transfer the temperature to the air conditioner in degrees Fahrenheit. The air conditioner display also shows the temperature in Fahrenheit. Defaults to `false`.

```yaml
# Example configuration entry
climate:
  - platform: whynter
    name: "AC"
    sensor: room_temperature
    use_fahrenheit: true
    supports_heat: true
```

{{< anchor "zhlt01" >}}

### `zhlt01`

The `zhlt01` climate and protocol, based on the ZH/LT-01 remote controller, is used with many locally branded airconditioners, like: Eurom, Chigo, Tristar, Tecnomaster, Elgin, Geant, Tekno, Topair, Proma, Sumikura, JBS, Turbo Air, Nakatomy, Celestial Air, Ager, Blueway, Airlux, etc.

{{< anchor "ir-receiver_id" >}}

## Using a Receiver

> [!NOTE]
> This is only supported with select climate devices, see "Supports receiver" in the table at the top of the page.

Optionally, some platforms can listen to data the climate device sends over infrared to update their state (
for example what mode the device is in). By setting up a {{< docref "/components/remote_receiver" "remote_receiver" >}}
and passing its ID to the climate platform you can enable this mode.

When using a receiver it is recommended to put the IR receiver as close as possible to the equipment's
IR receiver.

```yaml
# Example configuration entry
remote_receiver:
  id: rcvr
  pin:
    number: GPIOXX
    inverted: true
    mode:
      input: true
      pullup: true
  # high 55% tolerance is recommended for some remote control units
  tolerance: 55%

climate:
  - platform: REPLACEME
    name: "Living Room AC"
    receiver_id: rcvr
```

{{< anchor "heatpumpir" >}}

## Arduino-HeatpumpIR

The `heatpumpir` platform supports dozens of manufacturers and hundreds of AC units by utilising the [Arduino-HeatpumpIR library](https://github.com/ToniA/arduino-heatpumpir).

This platform compiles only under `arduino` framework or LibreTiny, and should only be used if your AC unit is not supported by any of the other (native) platforms from above. No support can be provided for Arduino-HeatpumpIR, because it is a third party library.

This platform utilises the library's generic one-size-fits-all API, which might not line up perfectly with all of the supported AC units. For example, some AC units have more fan speed options than what the generic API supports.

Additional configuration must be specified for this platform:

- **protocol** (**Required**, string): Choose one of Arduino-HeatpumpIR's supported protcols:

    `airway`, `aux`, `ballu`, `bgh_aud`, `carrier_mca`, `carrier_nqv`, `carrier_qlima_1`, `carrier_qlima_1`, `daikin`,
    `daikin_arc417`, `daikin_arc480`, `electroluxyal`, `fuego`, `fujitsu_awyz`, `gree`, `greeyaa`, `greeyac`, `greeyan`,
    `greeyap`, `greeyt`, `hisense_aud`, `hitachi`, `hyundai`, `ivt`, `midea`, `mitsubishi_fa`, `mitsubishi_fd`,
    `mitsubishi_fe`, `mitsubishi_heavy_fdtc`, `mitsubishi_heavy_zj`, `mitsubishi_heavy_zm`, `mitsubishi_heavy_zmp`, `mitsubishi_kj`,
    `mitsubishi_msc`, `mitsubishi_msy`, `mitsubishi_sez`, `nibe`, `panasonic_altdke`, `panasonic_ckp`, `panasonic_dke`, `panasonic_eke`,
    `panasonic_jke`, `panasonic_lke`, `panasonic_nke`, `philco_phs32`, `r51m`, `samsung_aqv`, `samsung_aqv12msan`, `samsung_fjm`, `sharp`,
    `toshiba`, `toshiba_daiseikai`, `vaillantvai8`, `zhjg01`, `zhlt01`

- **horizontal_default** (**Required**, string): What to default to when the AC unit's horizontal direction is *not* set to swing. Options are: `left`, `mleft`, `middle`, `mright`, `right`, `auto`
- **vertical_default** (**Required**, string): What to default to when the AC unit's vertical direction is *not* set to swing. Options are: `down`, `mdown`, `middle`, `mup`, `up`, `auto`
- **max_temperature** (**Required**, float): The maximum temperature that the AC unit supports being set to.
- **min_temperature** (**Required**, float): The minimum temperature that the AC unit supports being set to.
- **sensor** (*Optional*, [ID](/guides/configuration-types#id)): The sensor that is used to measure the ambient temperature.

> [!NOTE]
> The `greeyac` protocol in `heatpumpir` supports a feature Gree calls "I-Feel". The handheld remote control
> has a built-in temperature sensor and it will periodically transmit the temperature from this sensor to the
> AC unit. If a `sensor` is provided in the configuration with this model, the sensor's temperature will be
> transmitted to the `greeyac` device in the same manner as the original remote controller. How often the
> temperature is transmitted is determined by the `update_interval` assigned to the `sensor`. Note that
> `update_interval` must be less than 10 minutes or the `greeyac` device will revert to using its own
> internal temperature sensor; a value of 2 minutes seems to work well. See {{< docref "/components/sensor" >}}
> for more information.

## See Also

- {{< docref "/components/climate" >}}
- {{< docref "/components/remote_receiver" >}}
- {{< docref "/components/remote_transmitter" >}}
- {{< docref "/components/sensor" >}}
- {{< apiref "ballu.h" "ballu/ballu.h" >}}
- {{< apiref "climate_ir_lg.h" "climate_ir_lg/climate_ir_lg.h" >}}
- {{< apiref "coolix.h" "coolix/coolix.h" >}}
- {{< apiref "daikin.h" "daikin/daikin.h" >}}
- {{< apiref "fujitsu_general.h" "fujitsu_general/fujitsu_general.h" >}}
- {{< apiref "gree.h" "gree/gree.h" >}}
- {{< apiref "hitachi_ac344.h" "hitachi_ac344/hitachi_ac344.h" >}}
- {{< apiref "midea_ir.h" "midea_ir/midea_ir.h" >}}
- {{< apiref "mitsubishi.h" "mitsubishi/mitsubishi.h" >}}
- {{< apiref "tcl112.h" "tcl112/tcl112.h" >}}
- {{< apiref "toshiba.h" "toshiba/toshiba.h" >}}
- {{< apiref "whirlpool.h" "whirlpool/whirlpool.h" >}}
- {{< apiref "whynter.h" "whynter/whynter.h" >}}
- {{< apiref "yashima.h" "yashima/yashima.h" >}}
