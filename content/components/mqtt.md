---
description: "Instructions for setting up the MQTT client to communicate with the local network in ESPHome."
title: "MQTT Client Component"
params:
  seo:
    description: Instructions for setting up the MQTT client to communicate with the local network in ESPHome.
    image: mqtt.png
---

The MQTT Client Component sets up the MQTT connection to your broker.
If you are connecting to Home Assistant, you may prefer to use the native API,
in which case this is not needed.

> [!WARNING]
> If you enable MQTT and you do *not* use the {{< docref "/components/api" >}}, you must
> remove the `api:` configuration or set `reboot_timeout: 0s`, otherwise the ESP will
> reboot every 15 minutes because no client connected to the native API.

```yaml
# Example configuration entry
mqtt:
  broker: 10.0.0.2
  username: livingroom
  password: !secret mqtt_password
```

> [!NOTE]
> Support for esp-idf is still experimental. Please report issues you have with MQTT using the ESP-IDF framework.

## Configuration variables

- **broker** (**Required**, string): The host of your MQTT broker.
- **enable_on_boot** (*Optional*, boolean): If enabled, MQTT will be enabled on boot. Defaults to `true`.
- **port** (*Optional*, int): The port to connect to. Defaults to 1883.
- **username** (*Optional*, string): The username to use for
  authentication. Empty (the default) means no authentication.

- **password** (*Optional*, string): The password to use for
  authentication. Empty (the default) means no authentication.

- **clean_session** (*Optional*, boolean): Whether the broker will clean
  the MQTT session after disconnect. Defaults to `false`.

- **client_id** (*Optional*, string): The client id to use for opening
  connections. See [Defaults](#mqtt-defaults) for more information.

- **discover_ip** (*Optional*, boolean): If Home Assistant automatic device
  discovery should be enabled. Defaults to `true`.

- **discovery** (*Optional*, boolean): If Home Assistant automatic entity
  discovery should be enabled. Defaults to `true`.

- **discovery_retain** (*Optional*, boolean): Whether to retain MQTT
  discovery messages so that entities are added automatically on Home
  Assistant restart. Defaults to `true`.

- **discovery_prefix** (*Optional*, string): The prefix to use for Home
  Assistant's MQTT discovery. Should not contain trailing slash.
  Defaults to `homeassistant`.

- **discovery_unique_id_generator** (*Optional*, string): The unique_id generator
  to use. Can be one of `legacy` or `mac`. Defaults to `legacy`, which
  generates unique_id in format `ESP<component_type><default_object_id>`.
  `mac` generator uses format `<mac_address>-<component_type>-<fnv1_hash(friendly_name)>`.

- **discovery_object_id_generator** (*Optional*, string): The object_id generator
  to use. Can be one of `none` or `device_name`. Defaults to `none` which
  does not generate object_id. `device_name` generator uses format `<device_name>_<friendly_name>`.

- **use_abbreviations** (*Optional*, boolean): Whether to use
  [Abbreviations](https://www.home-assistant.io/docs/mqtt/discovery/)
  in discovery messages. Defaults to `true`.

- **topic_prefix** (*Optional*, string): The prefix used for all MQTT
  messages. Should not contain trailing slash. Defaults to `<APP_NAME>`.
  Use `null` to disable publishing or subscribing of any MQTT topic unless
  it is explicitly configured.

- **log_topic** (*Optional*, [MQTTMessage](#mqtt-message)): The topic to send MQTT log
  messages to. Use `null` if you want to disable sending logs to MQTT.

  The `log_topic` has an additional configuration option:

  - **level** (*Optional*, string): The log level to use for MQTT logs. See
    [Log Levels](/components/logger#logger-log_levels) for options.

- **birth_message** (*Optional*, [MQTTMessage](#mqtt-message)): The message to send when
  a connection to the broker is established. See [Last Will And Birth Messages](#mqtt-last_will_birth) for more information.

- **will_message** (*Optional*, [MQTTMessage](#mqtt-message)): The message to send when
  the MQTT connection is dropped. See [Last Will And Birth Messages](#mqtt-last_will_birth) for more information.

- **shutdown_message** (*Optional*, [MQTTMessage](#mqtt-message)): The message to send when
  the node shuts down and the connection is closed cleanly. See [Last Will And Birth Messages](#mqtt-last_will_birth) for more information.

- **ssl_fingerprints** (*Optional*, list): Only on ESP8266. A list of SHA1 hashes used
  for verifying SSL connections. See [SSL Fingerprints](#mqtt-ssl_fingerprints).
  for more information.

- **certificate_authority** (*Optional*, string): Only with `esp-idf`. CA certificate in PEM format. See
  [TLS with esp-idf (esp32)](#mqtt-tls-idf) for more information.

> [!TIP]
> For MQTT security recommendations including TLS configuration, see the [Security Best Practices](/guides/security_best_practices#mqtt) guide.

- **client_certificate** (*Optional*, string): Only on `esp32`. Client certificate in PEM format.
- **client_certificate_key** (*Optional*, string): Only on `esp32`. Client private key in PEM format.
- **skip_cert_cn_check** (*Optional*, bool): Only with `esp-idf`. Don't verify if the common name in the server
  certificate matches the value of `broker`.

- **idf_send_async** (*Optional*, bool): Only with `esp-idf`. If true publishing the message happens from a separate mqtt task.
  The client only enqueues the message. Defaults to `false`.
  The advantage of asynchronous publishing is that it doesn't block the esphome main thread for potentially tens of seconds.
  The disadvantage is additional memory usage for the thread.
  Set this to true if you need to ensure that mqtt does not block the main thread, especially if you have poor network conditions.

- **reboot_timeout** (*Optional*, [Time](/guides/configuration-types#time)): The amount of time to wait before rebooting when no
  MQTT connection exists. Can be disabled by setting this to `0s`. Defaults to `15min`.

- **keepalive** (*Optional*, [Time](/guides/configuration-types#time)): The time
  to keep the MQTT socket alive, decreasing this can help with overall stability due to more
  WiFi traffic with more pings. Defaults to 15 seconds.

- **on_connect** (*Optional*, [Automation](/automations)): An action to be performed when a connection
  to the broker is established.

- **on_disconnect** (*Optional*, [Automation](/automations)): An action to be performed when the connection
  to the broker is dropped.

- **on_message** (*Optional*, [Automation](/automations)): An action to be
  performed when a message on a specific MQTT topic is received. See [`on_message` Trigger](#mqtt-on_message).

- **on_json_message** (*Optional*, [Automation](/automations)): An action to be
  performed when a JSON message on a specific MQTT topic is received. See [`on_json_message` Trigger](#mqtt-on_json_message).

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code generation.
- **publish_nan_as_none** (*Optional*, bool): Publish `None` instead of `NaN` to handle Unknown/Unavailable sensor
  states in Home Assistant. Defaults to `false`.

- **wait_for_connection** (*Optional*, bool): Blocks other components from starting until the MQTT connection is
  established. Defaults to `false`.

{{< anchor "mqtt-message" >}}

## MQTTMessage

With the MQTT Message schema you can tell ESPHome how a specific MQTT message should be sent.
It is used in several places like last will and birth messages or MQTT log options.

```yaml
# Simple:
some_option: topic/to/send/to

# Disable:
some_option:

# Advanced:
some_option:
  topic: topic/to/send/to
  payload: online
  qos: 0
  retain: true
```

Configuration options:

- **topic** (**Required**, string): The MQTT topic to publish the message.
- **payload** (**Required**, string): The message content. Will be filled by the actual payload with some
   options, like log_topic.

- **qos** (*Optional*, int): The [Quality of Service](https://www.hivemq.com/blog/mqtt-essentials-part-6-mqtt-quality-of-service-levels)
   level of the topic. Defaults to 0.

- **retain** (*Optional*, boolean): If the published message should
   have a retain flag on or not. Defaults to `true`.

{{< anchor "mqtt-device_discovery" >}}

## MQTT device discovery

The ESPHome device will respond to the following MQTT topics if `mqtt.discover_ip` is enabled.

- `esphome/discover` (All ESPHome device will answer)
- `esphome/ping/<APP_NAME>`

The response will be sent to `esphome/discover/<APP_NAME>` and is a JSON encoded message.

The MQTT device discovery is currently used for:

- ESPHome dashboard (online / offline status)
- ESPHome CLI (IP discovery; used to view logs and perform OTA uploads)
- Home Assistant device discovery

Example Payload:

```json
{
  "ip": "192.168.0.122",
  "name": "esp32-test",
  "friendly_name": "Test Device",
  "port": 6053,
  "version": "2024.4.1",
  "mac": "84fce6123456",
  "platform": "ESP32",
  "board": "esp32-c3-devkitm-1",
  "network": "wifi",
  "api_encryption": "Noise_NNpsk0_25519_ChaChaPoly_SHA256"
}
```

JSON keys:

- **ip** (**Required**, ip): The IP address of the ESPHome device.
- **name** (**Required**, string): Name of the device (`esphome.name`  ).
- **mac** (**Required**, string): MAC address of the device.
- **board** (**Required**, string): Board used for the device.
- **version** (**Required**, string): ESPHome version.
- **port** (*Optional*, port): Port of the ESPHome API (if enabled).
- **ipX** (*Optional*, ip): Additional IP addresses (X is a number starting at 1).
- **friendly_name** (*Optional*, string): Friendly name of the device (`esphome.friendly_name`  ).
- **platform** (*Optional*, string): Platform of the device (e.g. ESP32 or ESP8266)
- **network** (*Optional*, string): Network type.
- **project_name** (*Optional*, string): `esphome.project.name`.
- **project_version** (*Optional*, string): `esphome.project.version`.
- **project_version** (*Optional*, string): `dashboard_import.package_import_url`.
- **api_encryption** (*Optional*, string): API encryption type.

{{< anchor "mqtt-using_device_discovery_with_home_assistant" >}}

## Using device discovery with Home Assistant

MQTT can be used to automatically discover the ESPHome devices in Home Assistant.
This allows Home Assistant to find the ESPHome device and connect
to it via the ESPHome API which allows the usage
of more features then MQTT entity discovery alone (e.g. Bluetooth Proxy, Voice Assistant).

This can be achieved by enabling `api` and `mqtt` with `mqtt.discover_ip` enabled.
It may makes sense to disable `mqtt.discovery` since there will be no need to use the
MQTT entity discovery if Home Assistant will connect to the ESPHome API.

Example configuration:

```yaml
api:
  encryption:
    key: "<secret>"

mqtt:
  broker: 10.0.0.2
  username: livingroom
  password: !secret mqtt_password
  discovery: False # disable entity discovery
  discover_ip: True # enable device discovery
```

{{< anchor "mqtt-using_with_home_assistant_entities" >}}

## Using with Home Assistant MQTT entities

Using ESPHome with Home Assistant is easy, simply setup an MQTT
broker (like [mosquitto](https://mosquitto.org/)) and point both your
Home Assistant installation and ESPHome to that broker. Next, enable
discovery in your Home Assistant configuration with the following:

```yaml
# Example Home Assistant configuration.yaml entry
mqtt:
  broker: ...
```

And that should already be it 🎉 All devices defined through ESPHome should show up automatically
in the entities section of Home Assistant.

When adding new entities, you might run into trouble with old entities
still appearing in Home Assistant's front-end. This is because in order
to have Home Assistant “discover” your devices on restart, all discovery
MQTT messages need to be retained. Therefore the old entities will also
re-appear on every Home Assistant restart even though they're in
ESPHome anymore.

To fix this, ESPHome has a simple helper script that purges stale
retained messages for you:

```bash
esphome clean-mqtt configuration.yaml
```

With Docker:

```bash
docker run --rm -v "${PWD}":/config -it ghcr.io/esphome/esphome clean-mqtt configuration.yaml
```

This will remove all retained messages with the topic
`<DISCOVERY_PREFIX>/+/NODE_NAME/#`. If you want to purge on another
topic, simply add `--topic <your_topic>` to the command.

Home Assistant generates entity names for all discovered devices based on entity type and
entity name (e.g. `sensor.uptime`  ). Numeric suffixes are appended to entity names when
multiple devices use the same name for a sensor, making it harder to distinguish between
similar sensors on different devices. Home Assistant 2021.12 allows MQTT devices to change
this behaviour by specifying the `object_id` discovery attribute which replaces the sensor
name part of the generated entity name. Setting `discovery_object_id_generator: device_name`
in the ESPHome MQTT component configuration will cause Home Assistant to include device name
in the generated entity names (e.g. `sensor.uptime` becomes `sensor.<device name>_uptime`  ),
making it easier to distinguish the entities in various entity lists.

{{< anchor "mqtt-defaults" >}}

## Defaults

By default, ESPHome will prefix all messages with your node name or
`topic_prefix` if you have specified it manually. The client id will
automatically be generated by using your node name and adding the MAC
address of your device to it. Next, discovery is enabled by default with
Home Assistant's default prefix `homeassistant`.

If you want to prefix all MQTT messages with a different prefix, like
`home/living_room`, you can specify a custom `topic_prefix` in the
configuration. That way, you can use your existing wildcards like
`home/+/#` together with ESPHome. All other features of ESPHome
(like availability) should still work correctly.

{{< anchor "mqtt-last_will_birth" >}}

## Last Will And Birth Messages

ESPHome uses the [last willtestament](https://www.hivemq.com/blog/mqtt-essentials-part-9-last-will-and-testament)
and birth message feature of MQTT to achieve availability reporting for
Home Assistant. If the node is not connected to MQTT, Home Assistant
will show all its entities as unavailable (a feature 😉).

{{< img src="mqtt-availability.png" alt="Image" width="50.0%" class="align-center" >}}

By default, ESPHome will send a retained MQTT message to
`<TOPIC_PREFIX>/status` with payload `online`, and will tell the
broker to send a message `<TOPIC_PREFIX>/status` with payload
`offline` if the connection drops.

You can change these messages by overriding the `birth_message` and
`will_message` with the following options.

```yaml
mqtt:
  # ...
  birth_message:
    topic: myavailability/topic
    payload: online
  will_message:
    topic: myavailability/topic
    payload: offline
```

- **birth_message** (*Optional*, [MQTTMessage](#mqtt-message))
- **will_message** (*Optional*, [MQTTMessage](#mqtt-message))

If the birth message and last will message have empty topics or topics
that are different from each other, availability reporting will be
disabled.

{{< anchor "mqtt-ssl_fingerprints" >}}

## SSL Fingerprints

On the ESP8266 you have the option to use SSL connections for MQTT. This feature
will get expanded to the ESP32 once the base library, AsyncTCP, supports it. Please
note that the SSL feature only checks the SHA1 hash of the SSL certificate to verify
the integrity of the connection, so every time the certificate changes, you'll have to
update the fingerprints variable. Additionally, SHA1 is known to be partially insecure
and with some computing power the fingerprint can be faked.

To get this fingerprint, first put the broker and port options in the configuration and
then run the `mqtt-fingerprint` script of ESPHome to get the certificate:

```bash
esphome mqtt-fingerprint livingroom.yaml
> SHA1 Fingerprint: a502ff13999f8b398ef1834f1123650b3236fc07
> Copy above string into mqtt.ssl_fingerprints section of livingroom.yaml
```

```yaml
mqtt:
  # ...
  ssl_fingerprints:
    - a502ff13999f8b398ef1834f1123650b3236fc07
```

{{< anchor "mqtt-tls-idf" >}}

## TLS with esp-idf (esp32)

If used with the esp-idf framework a TLS connection to a MQTT broker can be established.
The servers CA certificate is required to validate the connection.

You have to download the server CA certificate in PEM format and add it to `certificate_authority`.
Usually these are .crt files and you can open them with any text editor.
Also make sure to change the `port` of the MQTT broker. Most brokers use port 8883 for TLS connections.

> [!WARNING]
> MbedTLS, the library that handles TLS for the esp-idf, doesn't validate wildcard certificates.
>
> The Common Name check only works if the CN is explicitly reported in the certificate.
>
> - \*.example.com -> Fail
> - mqtt.example.com -> Success
>
> If a secure connection is necessary for your device, you really want to set:
>
> ```yaml
> skip_cert_cn_check: false
> ```

```yaml
mqtt:
  broker: test.mymqtt.local
  port: 8883
  discovery_prefix: ${mqtt_prefix}/homeassistant
  log_topic: ${mqtt_prefix}/logs
  # Evaluate carefully skip_cert_cn_check
  skip_cert_cn_check: true
  idf_send_async: false
  certificate_authority: |
    -----BEGIN CERTIFICATE-----
    MIIEAzCCAuugAwIBAgIUBY1hlCGvdj4NhBXkZ/uLUZNILAwwDQYJKoZIhvcNAQEL
    BQAwgZAxCzAJBgNVBAYTAkdCMRcwFQYDVQQIDA5Vbml0ZWQgS2luZ2RvbTEOMAwG
    A1UEBwwFRGVyYnkxEjAQBgNVBAoMCU1vc3F1aXR0bzELMAkGA1UECwwCQ0ExFjAU
    BgNVBAMMDW1vc3F1aXR0by5vcmcxHzAdBgkqhkiG9w0BCQEWEHJvZ2VyQGF0Y2hv
    by5vcmcwHhcNMjAwNjA5MTEwNjM5WhcNMzAwNjA3MTEwNjM5WjCBkDELMAkGA1UE
    BhMCR0IxFzAVBgNVBAgMDlVuaXRlZCBLaW5nZG9tMQ4wDAYDVQQHDAVEZXJieTES
    MBAGA1UECgwJTW9zcXVpdHRvMQswCQYDVQQLDAJDQTEWMBQGA1UEAwwNbW9zcXVp
    dHRvLm9yZzEfMB0GCSqGSIb3DQEJARYQcm9nZXJAYXRjaG9vLm9yZzCCASIwDQYJ
    KoZIhvcNAQEBBQADggEPADCCAQoCggEBAME0HKmIzfTOwkKLT3THHe+ObdizamPg
    UZmD64Tf3zJdNeYGYn4CEXbyP6fy3tWc8S2boW6dzrH8SdFf9uo320GJA9B7U1FW
    Te3xda/Lm3JFfaHjkWw7jBwcauQZjpGINHapHRlpiCZsquAthOgxW9SgDgYlGzEA
    s06pkEFiMw+qDfLo/sxFKB6vQlFekMeCymjLCbNwPJyqyhFmPWwio/PDMruBTzPH
    3cioBnrJWKXc3OjXdLGFJOfj7pP0j/dr2LH72eSvv3PQQFl90CZPFhrCUcRHSSxo
    E6yjGOdnz7f6PveLIB574kQORwt8ePn0yidrTC1ictikED3nHYhMUOUCAwEAAaNT
    MFEwHQYDVR0OBBYEFPVV6xBUFPiGKDyo5V3+Hbh4N9YSMB8GA1UdIwQYMBaAFPVV
    6xBUFPiGKDyo5V3+Hbh4N9YSMA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQEL
    BQADggEBAGa9kS21N70ThM6/Hj9D7mbVxKLBjVWe2TPsGfbl3rEDfZ+OKRZ2j6AC
    6r7jb4TZO3dzF2p6dgbrlU71Y/4K0TdzIjRj3cQ3KSm41JvUQ0hZ/c04iGDg/xWf
    +pp58nfPAYwuerruPNWmlStWAXf0UTqRtg4hQDWBuUFDJTuWuuBvEXudz74eh/wK
    sMwfu1HFvjy5Z0iMDU8PUDepjVolOCue9ashlS4EB5IECdSR2TItnAIiIwimx839
    LdUdRudafMu5T5Xma182OC0/u/xRlEm+tvKGGmfFcN0piqVl8OrSPBgIlb+1IKJE
    m/XriWr/Cq4h/JfB7NTsezVslgkBaoU=
    -----END CERTIFICATE-----
```

{{< anchor "config-mqtt-component" >}}

## MQTT Component Base Configuration

All components in ESPHome that do some sort of communication through
MQTT can have some overrides for specific options.

```yaml
name: "Component Name"
# Optional variables:
qos: 1
retain: true
availability:
  topic: livingroom/status
  payload_available: online
  payload_not_available: offline
state_topic: livingroom/custom_state_topic
command_topic: livingroom/custom_command_topic
command_retain: false
```

### Configuration variables

- **name** (**Required**, string): The name to use for the MQTT
   Component.

- **qos** (*Optional*, int): The [Quality of Service](https://www.hivemq.com/blog/mqtt-essentials-part-6-mqtt-quality-of-service-levels/)
   level for publishing. Defaults to 0.

- **retain** (*Optional*, boolean): If all MQTT state messages should
   be retained. Defaults to `true`.

- **discovery** (*Optional*, boolean): Manually enable/disable
   discovery for a component. Defaults to the global default.

- **subscribe_qos** (*Optional*, int): The [Quality of Service](https://www.hivemq.com/blog/mqtt-essentials-part-6-mqtt-quality-of-service-levels/)
   level advertised in discovery for subscribing (only if discovery is enabled). Defaults to 0.

- **availability** (*Optional*): Manually set what should be sent to
   Home Assistant for showing entity availability. Default derived from
   [global birth/last will message](#mqtt-last_will_birth).

- **state_topic** (*Optional*, string): The topic to publish state
   updates to. Defaults to
   `<TOPIC_PREFIX>/<COMPONENT_TYPE>/<COMPONENT_NAME>/state`.

   ESPHome will always publish a manually configured state topic, even if
   the component is internal. Use `null` to disable publishing the
   component's state.

- **command_topic** (*Optional*, string): The topic to subscribe to for
   commands from the remote. Defaults to
   `<TOPIC_PREFIX>/<COMPONENT_TYPE>/<COMPONENT_NAME>/command`.

   ESPHome will always subscribe to a manually configured command topic,
   even if the component is internal. Use `null` to disable subscribing
   to the component's command topic.

- **command_retain** (*Optional*, boolean): Whether MQTT command messages
   sent to the device should be retained or not. Default to `false`.

> [!WARNING]
> When changing these options and you're using MQTT discovery, you will need to restart Home Assistant.
> This is because Home Assistant only discovers a device once in every Home Assistant start.

## Triggers

{{< anchor "mqtt-on_connect_disconnect" >}}

### `on_connect` / `on_disconnect` Trigger

This trigger is activated when a connection to the MQTT broker is established or dropped.

```yaml
mqtt:
  # ...
  on_connect:
    - switch.turn_on: switch1
  on_disconnect:
    - switch.turn_off: switch1
```

{{< anchor "mqtt-on_message" >}}

### `on_message` Trigger

With this configuration option you can write complex automations whenever an MQTT
message on a specific topic is received. To use the message content, use a [lambda](/automations/templates#config-lambda)
template, the message payload is available under the name `x` inside that lambda.

```yaml
mqtt:
  # ...
  on_message:
    topic: my/custom/topic
    qos: 0
    then:
      - switch.turn_on: some_switch
```

#### Configuration variables

- **topic** (**Required**, string): The MQTT topic to subscribe to and listen for MQTT
  messages on. Every time a message with **this exact topic** is received, the automation will trigger.

- **qos** (*Optional*, int): The MQTT Quality of Service to subscribe to the topic with. Defaults
  to 0.

- **payload** (*Optional*, string): Optionally set a payload to match. Only if exactly the payload
  you specify with this option is received, the automation will be executed.

> [!NOTE]
> You can even specify multiple `on_message` triggers by using a YAML list:
>
> ```yaml
> mqtt:
>   on_message:
>      - topic: some/topic
>        then:
>          - # ...
>      - topic: some/other/topic
>        then:
>          - # ...
> ```

> [!NOTE]
> This action can also be used in [lambdas](/automations/templates#config-lambda):
>
> ```yaml
> mqtt:
>   # Give the MQTT component an ID
>   id: mqtt_client
> ```
>
> ```cpp
> id(mqtt_client).subscribe("the/topic", [=](const std::string &topic, const std::string &payload) {
>     // do something with payload
> });
> ```

{{< anchor "mqtt-on_json_message" >}}

## `on_json_message` Trigger

With this configuration option you can write complex automations whenever a JSON-encoded MQTT
message is received. To use the message content, use a [lambda](/automations/templates#config-lambda)
template, the decoded message payload is available under the name `x` inside that lambda.

The `x` object is of type `JsonObject` by the [ArduinoJson](https://github.com/bblanchon/ArduinoJson)
library, and you can use all of the methods of that library to access data.

Basically, you can access elements by typing `x["THE_KEY"]` and save them into local variables.
Please note that it's a good idea to check if the key exists in the Json Object by calling
`containsKey` first as the ESP will crash if an element that does not exist is accessed.

```yaml
mqtt:
  # ...
  on_json_message:
    topic: the/topic
    then:
      - light.turn_on:
          id: living_room_lights

          transition_length: !lambda |-
            int length = 1000;
            if (x.containsKey("length"))
              length = x["length"];
            return length;

          brightness: !lambda "return x["bright"];"

          effect: !lambda |-
            const char *effect = "None";
            if (x.containsKey("effect"))
              effect = x["effect"];
            return effect;
```

### Configuration variables

- **topic** (**Required**, string): The MQTT topic to subscribe to and listen for MQTT
  messages on. Every time a message with **this exact topic** is received, the automation will trigger.

- **qos** (*Optional*, int): The MQTT Quality of Service to subscribe to the topic with. Defaults
  to 0.

> [!NOTE]
> Due to the way this trigger works internally it is incompatible with certain actions and will
> trigger a compile failure. For example with the `delay` action.

> [!NOTE]
> This action can also be used in [lambdas](/automations/templates#config-lambda):
>
> ```yaml
> mqtt:
>   # Give the MQTT component an ID
>   id: mqtt_client
> ```
>
> ```cpp
> id(mqtt_client).subscribe_json("the/topic", [=](const std::string &topic, JsonObject root) {
>     // do something with JSON-decoded value root
> });
> ```

## Actions

{{< anchor "mqtt-publish_action" >}}

### `mqtt.publish` Action

Publish an MQTT message on a topic using this action in automations.

```yaml
on_...:
  then:
    - mqtt.publish:
        topic: some/topic
        payload: "Something happened!"

    # Templated:
    - mqtt.publish:
        topic: !lambda |-
          if (id(reed_switch).state) return "topic1";
          else return "topic2";
        payload: !lambda |-
          return id(reed_switch).state ? "YES" : "NO";
```

#### Configuration variables

- **topic** (**Required**, string, [templatable](/automations/templates)):
   The MQTT topic to publish the message.

- **payload** (**Required**, string, [templatable](/automations/templates)): The message content.
- **qos** (*Optional*, int, [templatable](/automations/templates)): The [Quality of
   Service](https://www.hivemq.com/blog/mqtt-essentials-part-6-mqtt-quality-of-service-levels)
   level of the topic. Defaults to 0.

- **retain** (*Optional*, boolean, [templatable](/automations/templates)): If the published message should
   have a retain flag on or not. Defaults to `false`.

> [!NOTE]
> This action can also be written in [lambdas](/automations/templates#config-lambda):
>
> ```yaml
> mqtt:
>   # Give the MQTT component an ID
>   id: mqtt_client
> ```
>
> ```cpp
> id(mqtt_client).publish("the/topic", "The Payload");
> ```

{{< anchor "mqtt-publish_json_action" >}}

### `mqtt.publish_json` Action

Publish a JSON-formatted MQTT message on a topic using this action in automations.

The JSON message will be constructed using the [ArduinoJson](https://github.com/bblanchon/ArduinoJson) library.
In the `payload` option you have access to a `root` object which will represents the base object
of the JSON message. You can assign values to keys by using the `root["KEY_NAME"] = VALUE;` syntax
as seen below.

```yaml
on_...:
  then:
    - mqtt.publish_json:
        topic: the/topic
        payload: |-
          root["key"] = id(my_sensor).state;
          root["greeting"] = "Hello World";

        # Will produce:
        # {"key": 42.0, "greeting": "Hello World"}
```

### Configuration variables

- **topic** (**Required**, string, [templatable](/automations/templates)):
   The MQTT topic to publish the message.

- **payload** (**Required**, [lambda](/automations/templates#config-lambda)): The message content.
- **qos** (*Optional*, int): The [Quality of Service](https://www.hivemq.com/blog/mqtt-essentials-part-6-mqtt-quality-of-service-levels)
   level of the topic. Defaults to 0.

- **retain** (*Optional*, boolean): If the published message should
   have a retain flag on or not. Defaults to `false`.

> [!NOTE]
> This action can also be written in [lambdas](/automations/templates#config-lambda):
>
> ```yaml
> mqtt:
>   # Give the MQTT component an ID
>   id: mqtt_client
> ```
>
> ```cpp
> id(mqtt_client).publish_json("the/topic", [=](JsonObject root) {
>   root["something"] = id(my_sensor).state;
> });
> ```

### `mqtt.disable` Action

This action turns off the MQTT component on demand.

```yaml
on_...:
  then:
    - mqtt.disable:
```

> [!NOTE]
> The configuration option `enable_on_boot` can be set to `false` if you do not want MQTT to be enabled on boot.

### `mqtt.enable` Action

This action turns on the MQTT component on demand.

```yaml
on_...:
  then:
    - mqtt.enable:
```

> [!NOTE]
> The configuration option `enable_on_boot` can be set to `false` if you do not want MQTT to be enabled on boot.
> `mqtt.enable` can be useful for custom setups. For example, if the broker name is negotiated dynamically and
> saved in a global variable.

```yaml
mqtt:
  id: mqtt_id
  broker: ""
  enable_on_boot: False

globals:
  - id: broker_address
    type: std::string
    restore_value: yes
    max_restore_data_length: 24
    initial_value: '"192.168.1.2"'

on_...:
  then:
    - lambda: !lambda id(mqtt_id).set_broker_address(id(broker_address));
    - mqtt.enable:
```

## Conditions

{{< anchor "mqtt-connected_condition" >}}

### `mqtt.connected` Condition

This [Condition](/automations/actions#all-conditions) checks if the MQTT client is currently connected to
the MQTT broker.

```yaml
on_...:
  if:
    condition:
      mqtt.connected:
    then:
      - logger.log: MQTT is connected!
```

> [!NOTE]
> This action can also be written in [lambdas](/automations/templates#config-lambda):
>
> ```yaml
> mqtt:
>   # Give the MQTT component an ID
>   id: mqtt_client
> ```
>
> ```cpp
> if (id(mqtt_client)->is_connected()) {
>   // do something if MQTT is connected
> }
> ```

## See Also

- {{< apiref "mqtt/mqtt_client.h" "mqtt/mqtt_client.h" >}}
