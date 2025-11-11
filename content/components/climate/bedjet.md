---
description: "Instructions for setting up a BedJet climate device."
title: "BedJet"
params:
  seo:
    description: Instructions for setting up a BedJet climate device.
    image: bedjet.png
---

The `bedjet` component allows you to communicate with a BedJet V3 Climate Comfort
Sleep System.

This component supports the following functionality:

- Set the operating mode: off, heat, cool, turbo (boost)
- Set the desired target temperature
- Set the desired fan speed
- Start one of the saved memory presets, including "Biorhythm" programs
- Show the current status of the BedJet

This component uses the BLE peripheral on an ESP32, so you also need to enable
this component. Please see the {{< docref "/components/ble_client" >}} docs for how to discover the MAC
address of your BedJet device.

## Component/Hub

This component is a global hub that maintains the connection to the BedJet device
and delegates status updates to individual platform components.

```yaml
esp32_ble_tracker:

ble_client:
  - mac_address: XX:XX:XX:XX:XX:XX
    id: bedjet_ble_id1

bedjet:
  - id: bedjet_1
    ble_client_id: bedjet_ble_id1
```

### Configuration variables

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.
- **ble_client_id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the BLE Client.
- **time_id** (*Optional*, [ID](/guides/configuration-types#id)): The ID of a {{< docref "/components/time" >}} which
  can be used to set the time on the BedJet device.

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval to dispatch status
  changes to child components. Defaults to `5s`. Each child component can decide whether to
  publish its own updated state on this interval, or use another (longer) update interval to
  throttle its own updates.

### lambda calls

From [lambdas](/automations/templates#config-lambda), you can call methods to do some advanced stuff.

- `.upgrade_firmware`  : Check for and install updated BedJet firmware.

```yaml
    button:
      - platform: template
        name: "Check Bedjet(1) Firmware"
        on_press:
          then:
          - lambda: |-
              id(bedjet_1).upgrade_firmware();
```

- `.send_local_time`  : If `time_id` is set, attempt to sync the clock now.

```yaml
    button:
      - platform: template
        name: "Sync Clock"
        on_press:
          then:
          - lambda: |-
              id(my_bedjet_fan).send_local_time();
```

- `.set_clock`  : Set the BedJet clock to a specified time; works with or without a `time_id`.

```yaml
    button:
      - platform: template
        name: "Set Clock to 10:10pm"
        on_press:
          then:
          - lambda: |-
              id(my_bedjet_fan).set_clock(22, 10);
```

## `bedjet` Climate

The `climate` platform exposes the BedJet's climate-related functionality, including
setting the mode and target temperature.

```yaml
climate:
  - platform: bedjet
    id: my_bedjet_climate_entity
    name: "My BedJet"
    bedjet_id: bedjet_1
```

### Configuration variables

- **bedjet_id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the Bedjet component.
- **heat_mode** (*Optional*, string): The primary heating mode to use for `HVACMode.HEAT`  :

  - `heat` (Default) - Setting `hvac_mode=heat` uses the BedJet "HEAT" mode.
  - `extended` - Setting `hvac_mode=heat` uses BedJet "EXT HEAT" mode.

    Whichever is not selected will be made available as a custom preset.

- **temperature_source** (*Optional*, string): The temperature that should be used as the
  climate entity's current temperature:

  - `ambient` (Default) - The temperature of the room the BedJet is in will be

      reported as the climate entity's current temperature.

  - `outlet` - The temperature of the air being discharged by the BedJet will be

      reported as the climate entity's current temperature.

- All other options from [Climate](/components/climate#config-climate).

## `bedjet` Fan

The `fan` platform exposes the BedJet's fan-related functionality, including
on/off and speed control.

When the BedJet is already on, turning the Fan component off will set the BedJet unit's mode to
`OFF`. If it was not already on, it will be turned on to mode `FAN_ONLY`.

```yaml
fan:
  - platform: bedjet
    id: my_bedjet_fan_entity
    name: "My BedJet Fan"
    bedjet_id: bedjet_1
```

### Configuration variables

- **bedjet_id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the Bedjet component.
- Other options from [Fan](/components/fan#config-fan).

## `bedjet` Sensor

The `sensor` platform exposes the BedJet's various temperature readings as sensors.

```yaml
sensor:
  - platform: bedjet
    bedjet_id: bedjet_1
    outlet_temperature:
      name: "My BedJet Outlet Temperature"
    ambient_temperature:
      name: "My BedJet Ambient Temperature"
```

### Configuration variables

- **outlet_temperature** (*Optional*): If specified, the temperature of the air being
  discharged from the BedJet will be reported as a sensor.
  All options from [Sensor](/components/sensor).

- **ambient_temperature** (*Optional*): If specified, the temperature of the room the
  BedJet is in will be reported as a sensor.
  All options from [Sensor](/components/sensor).

## Known issues

> [!WARNING]
> BedJet V2 and other devices are not currently supported. Only BedJet V3 is supported.

> [!NOTE]
> Only one client can be connected to the BedJet BLE service at a time, so you cannot
> use the BedJet mobile app to monitor or control the BedJet device while this component
> is connected. To use the mobile app, you should disconnect the ESP client first.
>
> To set up a (dis-)connect switch, see {{< docref "/components/switch/ble_client" >}}.

> [!NOTE]
> When more than one device is configured and connected, the ESP device may become
> overwhelmed and lead to timeouts while trying to install an updated version of the
> configuration. If this occurs, see the previous note about adding disconnect switches,
> and toggle those off while performing the installation. This will free up resources
> on the ESP and allow the installation to complete.
>
> Additionally, you may use an [ota.on_begin](/components/ota#ota-on_begin) [Automation](/automations)
> to do this automatically:
>
> ```yaml
> ota:
>   on_begin:
>     then:
>       - logger.log: "Disconnecting clients for OTA update..."
>       - switch.turn_off: bedjet_1_monitor
>       - switch.turn_off: bedjet_2_monitor
> ```

## See Also

- {{< docref "/components/ble_client" >}}
- {{< docref "/components/climate" >}}
- {{< apiref "bedjet/bedjet.h" "bedjet/bedjet.h" >}}
