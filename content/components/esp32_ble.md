---
description: "Instructions for setting up Bluetooth LE in ESPHome."
title: "BLE Component"
params:
  seo:
    description: Instructions for setting up Bluetooth LE in ESPHome.
    image: bluetooth.svg
---

The `esp32_ble` component in ESPHome sets up the Bluetooth LE stack on the device so that a {{< docref "esp32_ble_server/" >}}
can run.

{{< warning >}}
The BLE software stack on the ESP32 consumes a significant amount of RAM on the device.

**Crashes are likely to occur** if you include too many additional components in your device's
configuration. Memory-intensive components such as {{< docref "/components/voice_assistant" >}} and other
audio components are most likely to cause issues.

{{< /warning >}}

```yaml
# Example configuration

esp32_ble:
  io_capability: keyboard_only
  disable_bt_logs: true  # Default, saves flash
  connection_timeout: 20s  # Default, matches client timeout
  # advertising: true  # Only needed for advanced use cases
  max_notifications: 12  # Default, increase if needed
```

## Configuration variables

- **io_capability** (*Optional*, enum): The IO capability of this ESP32, used for securely connecting to other BLE devices. Defaults to `none`.

  - `none` - No IO capability (Connections that require PIN code authentication will fail)
  - `keyboard_only` - Only a keyboard to enter PIN codes (or a fixed PIN code)
  - `display_only` - Only a display to show PIN codes
  - `keyboard_display` - A keyboard and a display
  - `display_yes_no` - A display to show PIN codes and buttons to confirm or deny the connection

- **enable_on_boot** (*Optional*, boolean): If enabled, the BLE interface will be enabled on boot. Defaults to `true`.

- **name** (*Optional*, string): The name of the BLE device.
  - Defaults to the hostname of the device.
  - Must be 20 characters or less.
  - Must be 13 characters or less when using `name_add_mac_suffix: true` - [Adding the MAC address as a suffix to the device name](#esphome-mac_suffix).

- **disable_bt_logs** (*Optional*, boolean): When enabled, disables Bluetooth logging categories that are not used by the configured components. This saves flash memory by only including the loggers needed by your configuration. Defaults to `true`.

{{< note >}}
The `disable_bt_logs` option intelligently disables only the Bluetooth logging categories that are not required by your configuration. Each Bluetooth component registers the specific loggers it needs, and all unused loggers are automatically disabled during compilation. This includes loggers for Classic Bluetooth features (like RFCOMM, A2DP, HID) that are not used by ESPHome's BLE implementation.

{{< /note >}}

- **connection_timeout** (*Optional*, [Time](#config-time)): The maximum time to wait for a BLE connection to be established. Defaults to `20s`.

  - Range: 10 to 180 seconds
  - This timeout should align with the timeout used by your BLE client software to prevent connection slot waste

{{< note >}}
The `connection_timeout` option is particularly important when using ESPHome as a Bluetooth proxy. The default of 20 seconds matches the timeout used by aioesphomeapi and bleak-retry-connector. If a connection attempt times out on the client side but ESP-IDF continues trying to connect, the connection slot remains occupied and unavailable for new connections. Setting this to match your client timeout ensures connection slots are freed immediately when a connection fails.

{{< /note >}}

- **advertising** (*Optional*, boolean): Manually enable BLE advertising support. This is automatically enabled when using {{< docref "esp32_ble_server/" >}} or {{< docref "esp32_ble_beacon/" >}}. Only set this to `true` if you need advertising functionality without those components. Defaults to `false`.

{{< note >}}
The `advertising` option is an advanced feature that manually enables BLE advertising compilation. In most cases, you don't need to set this as advertising is automatically enabled when using components that require it (like `esp32_ble_server` or `esp32_ble_beacon`  ). This option is primarily useful for custom components or special use cases where you need advertising functionality without the standard server or beacon components.

{{< /note >}}

- **advertising_cycle_time** (*Optional*, [Time](#config-time)): The time interval for cycling through multiple advertisements. Only applicable when advertising is enabled. Defaults to `10s`.
- **max_notifications** (*Optional*, integer): The maximum number of BLE characteristics that can have notifications enabled across all connections. Defaults to `12`.

  - Range: 1 to 64
  - This is a global limit shared across all BLE connections
  - Increase if you see `ESP_GATT_NO_RESOURCES` (status=128) errors when enabling notifications

{{< note >}}
The `max_notifications` option controls the `CONFIG_BT_GATTC_NOTIF_REG_MAX` ESP-IDF setting. This limit is per GATT client interface, not per connection. If you're using ESPHome as a Bluetooth proxy with multiple devices that have many characteristics requiring notifications, you may need to increase this value. The error `status=128` in logs indicates you've hit this limit.

{{< /note >}}

## `ble.disable` Action

This action turns off the BLE interface on demand.

```yaml
on_...:
  then:
    - ble.disable:
```

{{< note >}}
The configuration option `enable_on_boot` can be set to `false` if you do not want BLE to be enabled on boot.

{{< /note >}}

## `ble.enable` Action

This action turns on the BLE interface on demand.

```yaml
on_...:
  then:
    - ble.enable:
```

{{< note >}}
The configuration option `enable_on_boot` can be set to `false` if you do not want BLE to be enabled on boot.

{{< /note >}}
{{< anchor "ble-enabled_condition" >}}

## `ble.enabled` Condition

This [Condition](#config-condition) checks if BLE is currently enabled or not.

```yaml
on_...:
  - if:
      condition: ble.enabled
      then:
        - ble.disable:
      else:
        - ble.enable:
```

The lambda equivalent for this is `id(ble_id).is_active()`.

## See Also

- {{< docref "esp32_ble_server/" >}}
- {{< docref "esp32_improv/" >}}
- {{< apiref "esp32_ble/ble.h" "esp32_ble/ble.h" >}}
