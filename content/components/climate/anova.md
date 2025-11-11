---
description: "Instructions for setting up an Anova sous-vide cooker."
title: "Anova Cooker"
params:
  seo:
    description: Instructions for setting up an Anova sous-vide cooker.
    image: anova.png
---

The `anova` climate platform creates a climate device which can be used
to control a BLE Anova sous-vide stick. Whilst not strictly a climate
device, the concept of keeping something to a set temperature applies
equally here.

This component supports the following functionality:

- Turn the stick on or off
- Set the desired temperature
- Show the current temperature

The timer is not currently supported, you can use automations to achieve
similar functionality.

This platform uses the BLE peripheral on an ESP32, so you also need to enable
this component. Please see the BLE Client docs for how to discover the MAC
address of your Anova device.
You cannot use the Anova app over BLE whilst this component is connected, you
should disconnect it first. To setup a (dis-)connect switch, see {{< docref "/components/switch/ble_client" >}}.

You need to specify unit_of_measurement as 'c' or 'f'. This ensures the stick's display
uses the unit you intend. All values within ESPHome are converted to Celsius so you may
need to do conversion again within the frontend if you use Fahrenheit.

> [!WARNING]
> The Anova Nano is not currently supported as it uses a different BLE protocol.

```yaml
ble_client:
  - mac_address: XX:XX:XX:XX:XX:XX
    id: my_anova

climate:
  - platform: anova
    name: "My Anova stick"
    ble_client_id: my_anova
    unit_of_measurement: c
```

## Configuration variables

- **ble_client_id** (**Required**, [ID](/guides/configuration-types#id)): The ID of the BLE Client.
- **unit_of_measurement** (**Required**, string): Units to use on the device display. 'c' or 'f'.
- All other options from [Climate](/components/climate#config-climate).

## See Also

- {{< docref "/components/ble_client" >}}
- {{< docref "/components/climate" >}}
- {{< apiref "anova/anova.h" "anova/anova.h" >}}
