---
description: "Instructions for setting up Improv via BLE in ESPHome."
title: "Improv via BLE"
params:
  seo:
    description: Instructions for setting up Improv via BLE in ESPHome.
    image: improv-social.png
---

The `esp32_improv` component in ESPHome implements the open [Improv standard](https://www.improv-wifi.com/)
for configuring Wi-Fi on an ESP32 device by using Bluetooth Low Energy (BLE) to receive the credentials.

The `esp32_improv` component will automatically set up the {{< docref "esp32_ble" "BLE Server" >}}.

> [!WARNING]
> The BLE software stack on the ESP32 consumes a significant amount of RAM on the device.
>
> **Crashes are likely to occur** if you include too many additional components in your device's
> configuration. Memory-intensive components such as {{< docref "/components/voice_assistant" >}} and other
> audio components are most likely to cause issues.

```yaml
# Example configuration entry
wifi:
  # ...

esp32_improv:
  authorizer: binary_sensor_id
```

## Configuration variables

- **authorizer** (**Required**, [ID](/guides/configuration-types#id)): A {{< docref "binary_sensor/index" "binary sensor" >}} to authorize with.
  Also accepts `none` to skip authorization.

- **authorized_duration** (*Optional*, [Time](/guides/configuration-types#time)): The amount of time until authorization times out and needs
  to be re-authorized. Defaults to `1min`.

- **status_indicator** (*Optional*, [ID](/guides/configuration-types#id)): An {{< docref "output/index" "output" >}} to display feedback to the user.
- **identify_duration** (*Optional*, [Time](/guides/configuration-types#time)): The amount of time to identify for. Defaults to `10s`.
- **wifi_timeout** (*Optional*, [Time](/guides/configuration-types#time)): The amount of time to wait before starting the Improv service
  after Wi-Fi is no longer connected. Defaults to `90s`.
- **next_url** (*Optional*, string): The URL to open after provisioning is complete. Defaults to
  `https://my.home-assistant.io/redirect/config_flow_start?domain=esphome`.

- **on_start** (*Optional*, [Automation](/automations)): An action to be performed when Improv is waiting for
  authorization and/or upon authorization. See [`on_start`](#improv-on_start).

- **on_provisioned** (*Optional*, [Automation](/automations)): An action to be performed when provisioning has
  completed. See [`on_provisioned`](#improv-on_provisioned).

- **on_provisioning** (*Optional*, [Automation](/automations)): An action to be performed when the device begins the
  provisioning process. See [`on_provisioning`](#improv-on_provisioning).

- **on_stop** (*Optional*, [Automation](/automations)): An action to be performed when Improv has stopped.
  See [`on_stop`](#improv-on_stop).

- **on_state** (*Optional*, [Automation](/automations)): An action to be performed when an Improv state change
  happens. See [`on_state`](#improv-on_state).

{{< anchor "improv-automations" >}}

## Improv Automations

The ESP32 Improv component provides various [automations](/automations) that can be used to provide feedback during
the Improv provisioning process.

{{< anchor "improv-on_start" >}}

### `on_start`

This automation will be triggered when the device is waiting for authorization (usually by pressing a button on the
device, if configured -- see `authorizer` above) and/or upon authorization.

```yaml
esp32_improv:
  on_start:
    then:
      - logger.log: "Improv awaiting authorization/authorized"
```

{{< anchor "improv-on_provisioned" >}}

### `on_provisioned`

This automation will be triggered when provisioning has completed.

```yaml
esp32_improv:
  on_provisioned:
    then:
      - logger.log: "Improv provisioned"
```

{{< anchor "improv-on_provisioning" >}}

### `on_provisioning`

This automation will be triggered when provisioning begins.

```yaml
esp32_improv:
  on_provisioning:
    then:
      - logger.log: "Improv provisioning"
```

{{< anchor "improv-on_stop" >}}

### `on_stop`

This automation will be triggered when Improv has stopped.

```yaml
esp32_improv:
  on_stop:
    then:
      - logger.log: "Improv stopped"
```

{{< anchor "improv-on_state" >}}

### `on_state`

This automation will be triggered on every state change.

Two variables are available for use in [lambdas](/automations/templates#config-lambda) within this automation. They are:

- `state`, an `enum` named `improv::State`, having one of the following values:

  - `improv::STATE_STOPPED`
  - `improv::STATE_AWAITING_AUTHORIZATION`
  - `improv::STATE_AUTHORIZED`
  - `improv::STATE_PROVISIONING`
  - `improv::STATE_PROVISIONED`

- `error`, an `enum` named `improv::Error`, having one of the following values:

  - `improv::ERROR_NONE`
  - `improv::ERROR_INVALID_RPC`
  - `improv::ERROR_UNKNOWN_RPC`
  - `improv::ERROR_UNABLE_TO_CONNECT`
  - `improv::ERROR_NOT_AUTHORIZED`
  - `improv::ERROR_UNKNOWN`

```yaml
esp32_improv:
  on_state:
    then:
      - if:
          condition:
            lambda: return state == improv::STATE_AUTHORIZED;
          then:
            - logger.log: "Improv state is STATE_AUTHORIZED"
```

## Status Indicator

The `status_indicator` has the following patterns:

- solid: The improv service is active and waiting to be authorized.
- blinking once per second: The improv service is awaiting credentials.
- blinking 3 times per second with a break in between: The identify command has been used by the client.
- blinking 5 times per second: Credentials are being verified and saved to the device.
- off: The improv service is not running.

## See Also

- {{< docref "wifi/" >}}
- {{< docref "improv_serial/" >}}
- {{< docref "captive_portal/" >}}
- [Improv Wi-Fi](https://www.improv-wifi.com/)
- {{< apiref "esp32_improv/esp32_improv_component.h" "esp32_improv/esp32_improv_component.h" >}}
