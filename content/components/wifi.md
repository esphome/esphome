---
description: "Instructions for setting up the WiFi configuration for your ESP node in ESPHome."
title: "WiFi Component"
params:
  seo:
    description: Instructions for setting up the WiFi configuration for your ESP node in ESPHome.
    image: network-wifi.svg
---

This core ESPHome component sets up WiFi connections to access points
for you. You need to have a network configuration (either Wifi or Ethernet)
or ESPHome will fail in the config validation stage. You also can't have both Wifi
and Ethernet setup in same time (even if your ESP has both wired).

It's recommended to provide a static IP for your node, as it can
dramatically improve connection times.

```yaml
# Example configuration entry
wifi:
  ssid: MyHomeNetwork
  password: VerySafePassword

  # Optional manual IP
  manual_ip:
    static_ip: 192.168.0.123
    gateway: 192.168.0.1
    subnet: 255.255.255.0
```

```yaml
# It is highly recommended to use secrets
wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
```

> [!TIP]
> For WiFi security recommendations including `min_auth_mode` configuration, see the [Security Best Practices](/guides/security_best_practices#wifi-security) guide.

{{< anchor "wifi-configuration_variables" >}}

## Configuration variables

- **ssid** (*Optional*, string): The name (or [service set identifier](https://www.lifewire.com/definition-of-service-set-identifier-816547))
  of the WiFi access point your device should connect to.

- **password** (*Optional*, string): The password (or PSK) for your
  WiFi network. Leave empty for no password.

- **networks** (*Optional*): Configure multiple WiFi networks to connect to, the best one
  that is reachable will be connected to. See [Connecting to Multiple Networks](#wifi-networks).

- **manual_ip** (*Optional*): Manually configure the static IP of the node.

  - **static_ip** (**Required**, IPv4 address): The static IP of your node.
  - **gateway** (**Required**, IPv4 address): The gateway of the local network.
  - **subnet** (**Required**, IPv4 address): The subnet of the local network.
  - **dns1** (*Optional*, IPv4 address): The main DNS server to use.
  - **dns2** (*Optional*, IPv4 address): The backup DNS server to use.

- **use_address** (*Optional*, string): Manually override what address to use to connect
  to the ESP. Defaults to auto-generated value. Example, if you have changed your static IP and want to flash OTA to
  the previously configured IP address.

- **ap** (*Optional*): Enable an access point mode on the node.

  - **ssid** (*Optional*, string): The name of the access point to create. Leave empty to use
    the device name.

  - **password** (*Optional*, string): The password for the access point. Leave empty for
    no password.

  - **channel** (*Optional*, int): The channel the AP should operate on from 1 to 14.
    Defaults to 1.

  - **manual_ip** (*Optional*): Manually set the IP options for the AP. Same options as
    manual_ip for station mode.

  - **ap_timeout** (*Optional*, [Time](/guides/configuration-types#time)): The time after which to enable the
    configured fallback hotspot. Can be disabled by setting this to `0s`, which requires manually starting the AP by
    other means (eg: from a button press). Defaults to `90s`.

- **domain** (*Optional*, string): Set the domain of the node hostname used for uploading.
  For example, if it's set to `.local`, all uploads will be sent to `<HOSTNAME>.local`.
  Defaults to `.local`.

- **reboot_timeout** (*Optional*, [Time](/guides/configuration-types#time)): The amount of time to wait before rebooting when no
  WiFi connection exists. Can be disabled by setting this to `0s`, but note that the low level IP stack currently
  seems to have issues with WiFi where a full reboot is required to get the interface back working. Defaults to `15min`.
  Does not apply when in access point mode.

- **power_save_mode** (*Optional*, string): The power save mode for the WiFi interface.
  See [Power Save Mode](#wifi-power_save_mode)

- **output_power** (*Optional*, string): The amount of TX power for the WiFi interface from 8.5dB to 20.5dB. Default
  for ESP8266 is 20dB, 20.5dB might cause unexpected restarts.

- **fast_connect** (*Optional*, boolean): If enabled, directly connects to WiFi network without doing a full scan
  first. This can significantly improve connection times (thus reducing power consumption). Defaults to `off`.
  The downside is that this option connects to the first network the ESP sees, even if that network is very far away and
  better ones are available. If multiple networks are configured, the last successfully connected one is tested first.
  In case it fails, all networks are then tested one after the other in their declared order, starting with the first
  one in the list.

  > [!NOTE]
  > While `fast_connect` skips the initial scan, if the connection attempt fails, ESPHome will still perform a scan
  > to find available networks. For hidden networks, use `hidden: true` on the network configuration (see
  > [Connecting to Multiple Networks](#wifi-networks)) to ensure the device always connects without scanning.
  > Be aware that marking networks as hidden prevents ESPHome from finding the best access point to connect to,
  > so the device may not connect to the AP with the best signal strength.

- **min_auth_mode** (*Optional*, string): Only on `esp32` and `esp8266`. Sets the minimum WiFi authentication mode
  that the device will accept when connecting to access points. This controls the weakest encryption your device will
  allow. Possible values are:

  - `WPA` - Allows WPA, WPA2, and WPA3 networks (least secure, uses TKIP encryption with known vulnerabilities)
  - `WPA2` - Allows WPA2 and WPA3 networks (recommended, uses AES encryption)
  - `WPA3` - Only allows WPA3 networks (most secure, ESP32 only)

  Defaults to `WPA2` on ESP32 and `WPA` on ESP8266 (will change to `WPA2` in 2026.6.0).

  **Security Warning:** Setting `min_auth_mode: WPA` allows connection to networks using deprecated WPA/TKIP encryption,
  which has known security vulnerabilities. Only use this setting for legacy routers that cannot be upgraded to WPA2 or WPA3.
  If your router supports WPA2 or newer, use the default `WPA2` setting for better security.

- **passive_scan** (*Optional*, boolean): If enabled, then the device will perform WiFi scans in a passive fashion.
  Defaults to `false`.

- **enable_btm** (*Optional*, bool): Only on `esp32`. Enable 802.11v BSS Transition Management support.
- **enable_rrm** (*Optional*, bool): Only on `esp32`. Enable 802.11k Radio Resource Management support.

- **on_connect** (*Optional*, [Automation](/automations)): An action to be performed when a connection is established.
- **on_disconnect** (*Optional*, [Automation](/automations)): An action to be performed when the connection is dropped.
- **enable_on_boot** (*Optional*, boolean): If enabled, the WiFi interface will be enabled on boot. Defaults to `true`.
- **use_psram** (*Optional*, boolean): For ESP32 only, requests that the WiFi libraries try to allocate memory from PSRAM.
  Defaults to `false`. Requires PSRAM to be configured.

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.

## Access Point Mode

ESPHome has an optional "Access Point Mode". If you include `ap:`
in your wifi configuration, ESPHome will automatically set up an access point that you
can connect to. Additionally, you can specify both a "normal" station mode and AP mode at the
same time. This will cause ESPHome to only enable the access point when no connection
to the WiFi router can be made.

```yaml
wifi:
  ap:
    ssid: "Livingroom Fallback Hotspot"
    password: "W1PBGyrokfLz"
```

You can also create a simple `ap` config which will set up the access point to have the
devices name as the ssid with no password.

```yaml
wifi:
  ap: {}

# or if you still want the ap to have a password

wifi:
  ap:
    password: "W1PBGyrokfLz"
```

## User Entered Credentials

Some components such as {{< docref "captive_portal/" >}}, {{< docref "improv_serial/" >}} and
{{< docref "esp32_improv/" >}} enable the user to send and save Wi-Fi credentials to the device. Beginning in 2022.11.0,
as long as no credentials are set in the config file, and firmware is uploaded without erasing
the flash (via OTA), the device will keep the saved credentials.

{{< anchor "wifi-manual_ip" >}}

## Manual IPs

If you're having problems with your node not connecting to WiFi or the connection
process taking a long time, it can be a good idea to assign a static IP address
to the ESP. This way, the ESP doesn't need to go through the slow DHCP process.

You can do so with the `manual_ip:` option in the WiFi configuration.

```yaml
wifi:
  # ...
  manual_ip:
    # Set this to the IP of the ESP
    static_ip: 10.0.0.42
    # Set this to the IP address of the router. Often ends with .1
    gateway: 10.0.0.1
    # The subnet of the network. 255.255.255.0 works for most home networks.
    subnet: 255.255.255.0
```

After putting a manual IP in your configuration, the ESP will no longer need to negotiate
a dynamic IP address with the router, thus improving the time until connection.

Additionally, this can help with {{< docref "/components/ota" >}} if for example the
network doesn't allow for `.local` addresses. When a manual IP is in your configuration,
the OTA process will automatically choose that as the target for the upload.

> [!NOTE]
> See also [Changing ESPHome Node Name](/components/esphome#esphome-changing_node_name).

{{< anchor "wifi-power_save_mode" >}}

## Power Save Mode

The WiFi interface of all ESPs offer three power save modes to reduce the amount of power spent on
WiFi. While some options *can* reduce the power usage of the ESP, they generally also decrease the
reliability of the WiFi connection, with frequent disconnections from the router in the highest
power saving mode.

- `NONE` (least power saving, Default for ESP8266)
- `LIGHT` (Default for ESP32)
- `HIGH` (most power saving)

```yaml
wifi:
  # ...
  power_save_mode: none
```

{{< anchor "wifi-min_auth_mode" >}}

## WiFi Authentication Mode

The `min_auth_mode` option allows you to control the minimum WiFi security standard your device will accept.
This is useful for ensuring your device only connects to secure networks, or for maintaining compatibility with
legacy routers that only support older encryption standards.

### Example: Maximum Security (WPA2 or newer)

```yaml
wifi:
  ssid: MyHomeNetwork
  password: VerySafePassword
  min_auth_mode: WPA2  # Reject WPA-only networks
```

### Example: Legacy Router Support (WPA allowed)

```yaml
wifi:
  ssid: OldRouter
  password: VerySafePassword
  min_auth_mode: WPA  # Allow connection to WPA-only routers (less secure)
```

### Example: Modern Security (WPA3 only, ESP32 only)

```yaml
wifi:
  ssid: ModernRouter
  password: VerySafePassword
  min_auth_mode: WPA3  # Only connect to WPA3 networks (most secure)
```

{{< anchor "wifi-networks" >}}

## Connecting to Multiple Networks

You can give ESPHome a number of WiFi networks to connect to.
ESPHome will then attempt to connect to the one with the highest signal strength.

To enable this mode, remove the `ssid` and `password` options from your wifi configuration
and move everything under the `networks` key:

```yaml
# Example configuration entry
wifi:
  networks:
  - ssid: FirstNetworkToConnectTo
    password: VerySafePassword
  - ssid: SecondNetworkToConnectTo
    password: VerySafePassword
  # Other options
  # ...
```

### Configuration variables

- **ssid** (*Optional*, string): The SSID or WiFi network name.
- **password** (*Optional*, string): The password to use for authentication. Leave empty for no password.
- **manual_ip** (*Optional*): Manually configure the static IP of the node when using this network. Note that
  when using different static IP addresses on each network, it is required to set `use_address`, as ESPHome
  cannot infer to which network the node is connected.

  - **static_ip** (**Required**, IPv4 address): The static IP of your node.
  - **gateway** (**Required**, IPv4 address): The gateway of the local network.
  - **subnet** (**Required**, IPv4 address): The subnet of the local network.
  - **dns1** (*Optional*, IPv4 address): The main DNS server to use.
  - **dns2** (*Optional*, IPv4 address): The backup DNS server to use.

- **eap** (*Optional*): See [Enterprise Authentication](#eap).
- **channel** (*Optional*, int): The channel of the network (1-14). If given, only connects to networks
  that are on this channel.

- **bssid** (*Optional*, string): The connection's BSSID (MAC address). BSSIDs must consist of six
  two-digit hexadecimal values separated by colon characters ("`:`  "). All letters must be in upper case.

- **hidden** (*Optional*, boolean): Whether this network is hidden. Defaults to false.
  If you add this option you also have to specify ssid.

  > [!TIP]
  > Set `hidden: true` if your network does not broadcast its SSID. This ensures the device attempts to connect
  > using hidden network mode without first scanning for visible networks. Note that when connecting to a hidden
  > network, ESPHome cannot determine which access point has the best signal strength, potentially resulting in
  > connections to APs with weaker signals when multiple APs share the same SSID.

- **priority** (*Optional*, int): The priority of this network (range: -128 to 127). The network with
  the highest priority is chosen. After each connection failure, the priority is decreased by one.
  If all tracked BSSIDs have identical priorities, they are automatically reset to 0 to start fresh.
  Defaults to `0`.

### Example: Connecting to a Hidden Network

```yaml
wifi:
  networks:
  - ssid: MyHiddenNetwork
    password: VerySafePassword
    hidden: true
```

{{< anchor "eap" >}}

## Enterprise Authentication

WPA2_EAP Enterprise Authentication is supported on ESP32s and ESP8266s.
In order to configure this feature you must use the [Connecting to Multiple Networks](#wifi-networks) style configuration.
The ESP32 is known to work with PEAP, EAP-TTLS, and the certificate based EAP-TLS.
These are advanced settings and you will usually need to consult your enterprise network administrator.

```yaml
# Example EAP configuration
wifi:
  networks:
  - ssid: EAP-TTLS_EnterpriseNetwork
    eap:
      username: bob
      password: VerySafePassword
      ttls_phase_2: mschapv2
  - ssid: EAP-TLS_EnterpriseNetwork
    eap:
      identity: bob
      certificate_authority: ca_cert.pem
      certificate: cert.pem
      key: key.pem
```

### Configuration variables

- **identity** (*Optional*, string): The outer identity to pass to the EAP authentication server.
  This is required for EAP-TLS.

- **username** (*Optional*, string): The username to present to the authenticating server.
- **password** (*Optional*, string): The password to present to the authentication server.
  For EAP-TLS this password may be set to decrypt to private key instead.

- **certificate_authority** (*Optional*, string): Path to a PEM encoded certificate to use when validating the
  authentication server.

- **certificate** (*Optional*, string): Path to a PEM encoded certificate to use for EAP-TLS authentication.
- **key** (*Optional*, string): Path to a PEM encoded private key matching `certificate` for EAP-TLS authentication.
  Optionally encrypted with `password`.

- **ttls_phase_2** (*Optional*, string): The Phase 2 Authentication Method for EAP-TTLS.
  Can be `pap`, `eap`, `mschap`, `mschapv2` or `chap`, defaults to `mschapv2`.

{{< anchor "wifi-on_connect_disconnect" >}}

## `on_connect` / `on_disconnect` Trigger

This trigger is activated when a WiFi connection is established or dropped.

```yaml
wifi:
  # ...
  on_connect:
    - switch.turn_on: switch1
  on_disconnect:
    - switch.turn_off: switch1
```

## Actions

{{< anchor "wifi-on_disable" >}}

### `wifi.disable` Action

This action turns off the WiFi interface on demand.

```yaml
on_...:
  then:
    - wifi.disable:
```

> [!NOTE]
> Be mindful of the reboot timeouts set for both the [API component](/components/api/) and the
> [WiFi component](#configuration-variables) if you disable WiFi. If WiFi remains off for longer than the duration of
> either timeout, the device will reboot!

{{< anchor "wifi-on_enable" >}}

### `wifi.enable` Action

This action turns on the WiFi interface on demand.

```yaml
on_...:
  then:
    - wifi.enable:
```

> [!NOTE]
> The configuration option `enable_on_boot` can be set to `false` if you do not want wifi to be enabled on boot.

{{< anchor "wifi-configure" >}}

### `wifi.configure` Action

This action connects to an SSID and password, optionally saving it in persistent memory so that the next time the WiFi
interface is enabled, it will connect to the stored access point.

```yaml
on_...:
  then:
    - wifi.configure:
        ssid: "MyHomeNetwork"
        password: "VerySafePassword"
        save: true
        timeout: 30000ms
        on_connect:
          - logger.log: "Connected to WiFi!"
        on_error:
          - logger.log: "Failed to connect to WiFi!"
```

#### Configuration variables

- **ssid** (**Required**, string, [templatable](/automations/templates)): The name of the WiFi access point.
- **password** (**Required**, string, [templatable](/automations/templates)): The password of the WiFi access point.
  Leave empty for no password.

- **save** (*Optional*, boolean, [templatable](/automations/templates)): If set to `true`, the SSID and password will be
  saved in persistent memory. Defaults to `true`.

- **timeout** (*Optional*, [Time](/guides/configuration-types#time), [templatable](/automations/templates)): The time to wait for the connection
  to be established. Defaults to 30 seconds.

- **on_connect** (*Optional*, [Automation](/automations)): An action to be performed when a connection is established.
- **on_error** (*Optional*, [Automation](/automations)): An action to be performed when the connection fails.

## Conditions

{{< anchor "wifi-connected_condition" >}}

### `wifi.connected` Condition

This [Condition](/automations/actions#all-conditions) checks if the WiFi client is currently connected to a station.

```yaml
on_...:
  if:
    condition:
      wifi.connected:
    then:
      - logger.log: WiFi is connected!
```

The lambda equivalent for this is `id(wifi_id).is_connected()`.

{{< anchor "wifi-enabled_condition" >}}

### `wifi.enabled` Condition

This [Condition](/automations/actions#all-conditions) checks if WiFi is currently enabled or not.

```yaml
on_...:
  - if:
      condition: wifi.enabled
      then:
        - wifi.disable:
      else:
        - wifi.enable:
```

The lambda equivalent for this is `!id(wifi_id).is_disabled()`.

{{< anchor "wifi-ap-active_condition" >}}

### `wifi.ap_active` Condition

This [Condition](/automations/actions#all-conditions) checks if WiFi AP is currently active or not.

```yaml
on_...:
  - if:
      condition: wifi.ap_active
      then:
        - logger.log: WiFi AP is active!
```

The lambda equivalent for this is `id(wifi_id).is_ap_active()`.

## See Also

- {{< docref "captive_portal/" >}}
- {{< docref "text_sensor/wifi_info" >}}
- {{< docref "sensor/wifi_signal" >}}
- {{< docref "network/" >}}
- {{< docref "/components/ethernet" >}}
- {{< docref "api/" >}}
- {{< apiref "wifi/wifi_component.h" "wifi/wifi_component.h" >}}
