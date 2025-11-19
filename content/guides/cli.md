---
description: "Documentation for the command line interface of ESPHome."
title: "Command Line Interface"
params:
  seo:
    description: Documentation for the command line interface of ESPHome.
---

## Base Usage

ESPHome's command line interface always has the following format

```shell
esphome [OPTIONS] <COMMAND> <CONFIGURATION...> [ARGUMENTS]
```

> [!NOTE]
> You can specify multiple configuration files in the command line interface for some commands,
> just list all files after the `<COMMAND>` like so:
>
> ```shell
> esphome run livingroom.yaml kitchen.yaml
> ```

## Options

### `--help` Option

{{< option "-h|--help" >}}
Output possible `<commands>` and `[arguments]`.
Note: you can also use `--help` for any command to get arguments specific to that command.
{{< /option >}}

```shell
esphome <some_command> --help
```

### `--verbose` Option

{{< option "-v|--verbose" >}}
Enable verbose esphome logs.
Can also be enabled via environment variable `ESPHOME_VERBOSE=true`.

{{< /option >}}

### `--quiet` Option

{{< option "-q|--quiet" >}}
Disable all esphome logs.

{{< /option >}}

### `--substitution` Option

*Can be issued multiple times.*

{{< option "-s|--substitution KEY VALUE" >}}
Defines or overrides substitution KEY with value VALUE.

{{< /option >}}
Please see [command line substitutions](/components/substitutions#command-line-substitutions) for details.

## Commands

### `run` Command

The `esphome run <CONFIG>` command is the most common command for ESPHome. It

* Validates the configuration
* Compiles a firmware
* Uploads the firmware (over OTA or USB)
* Starts the log view

#### Options

{{< option "--device UPLOAD_PORT" >}}
Manually specify the upload port/IP to use. For example `/dev/cu.SLAB_USBtoUART`, or `192.168.1.176`
to perform an OTA.

Multiple `--device` options can be specified to provide fallback addresses. ESPHome will
try each address in order until one succeeds. This is particularly useful for devices with
multiple IP addresses (IPv4/IPv6).

Example:

```shell
esphome run my-device.yaml --device 192.168.1.100 --device 2001:db8::1
```

{{< /option >}}

{{< option "--upload_speed BAUD_RATE" >}}
The upload speed for serial flashing defaults to 460800 or as set with the environment variable `ESPHOME_UPLOAD_SPEED`.
This can be overridden in the platformio options on a per-config
basis, or set with this option at the time of uploading.

{{< /option >}}

{{< option "--no-logs" >}}
Disable starting log view.
{{< /option >}}

{{< option "--topic TOPIC" >}}
Manually set the topic to subscribe to for MQTT logs (defaults to the one in the configuration).
{{< /option >}}

{{< option "--username USERNAME" >}}
Manually set the username to subscribe with for MQTT logs (defaults to the one in the configuration).
{{< /option >}}

{{< option "--password PASSWORD" >}}
Manually set the password to subscribe with for MQTT logs (defaults to the one in the configuration).
{{< /option >}}

{{< option "--client-id CLIENT_ID" >}}
Manually set the client ID to subscribe with for MQTT logs (defaults to a randomly chosen one).
{{< /option >}}

{{< option "--host-port HOST_PORT" >}}
Specify the host port to use for legacy Over the Air uploads.
{{< /option >}}

{{< option "--reset" >}}
If set, reset the device before starting the logs. May also be configured with the environment variable
`ESPHOME_SERIAL_LOGGING_RESET=true`.
{{< /option >}}

### `config` Command

The `esphome config <CONFIG>` validates the configuration and displays the validation result.

### `compile` Command

The `esphome compile <CONFIG>` validates the configuration and compiles the firmware.

#### Options

{{< option "--only-generate" >}}
If set, only generates the C++ source code and does not compile the firmware.
{{< /option >}}

### `upload` Command

The `esphome upload <CONFIG>` validates the configuration and uploads the most recent firmware build.

#### Options

{{< option "--device UPLOAD_PORT" >}}
Manually specify the upload port/IP address to use. For example `/dev/cu.SLAB_USBtoUART`, or `192.168.1.176`
to perform an OTA.

Multiple `--device` options can be specified to provide fallback addresses. ESPHome will
try each address in order until one succeeds.

Example:

```shell
esphome upload my-device.yaml --device 192.168.1.100 --device 2001:db8::1
```

{{< /option >}}

{{< option "--upload_speed BAUD_RATE" >}}
The upload speed for serial flashing defaults to 460800 or as set with the environment variable `ESPHOME_UPLOAD_SPEED`.
This can be overridden in the platformio options on a per-config
basis, or set with this option at the time of uploading.
{{< /option >}}

{{< option "--host-port HOST_PORT" >}}
Specify the host port to use for legacy Over the Air uploads.
{{< /option >}}

### `clean-mqtt` Command

The `esphome clean-mqtt <CONFIG>` cleans retained MQTT discovery messages from the MQTT broker.
See [Using with Home Assistant MQTT entities](/components/mqtt#mqtt-using_with_home_assistant_entities).

#### Options

{{< option "--topic TOPIC" >}}
Manually set the topic to clean retained messages from (defaults to the MQTT discovery topic of the
node).
{{< /option >}}

{{< option "--username USERNAME" >}}
Manually set the username to subscribe with.
{{< /option >}}

{{< option "--password PASSWORD" >}}
Manually set the password to subscribe with.
{{< /option >}}

{{< option "--client-id CLIENT_ID" >}}
Manually set the client ID to subscribe with.
{{< /option >}}

### `wizard` Command

The `esphome wizard <CONFIG>` command starts the ESPHome configuration creation wizard.

### `mqtt-fingerprint` Command

The `esphome mqtt-fingerprint <CONFIG>` command shows the MQTT SSL fingerprints of the remote used
for SSL MQTT connections. See [SSL Fingerprints](/components/mqtt#mqtt-ssl_fingerprints).

### `version` Command

The `esphome version` command shows the current ESPHome version and exits.

### `clean` Command

The `esphome clean <CONFIG>` command cleans all build files and can help with some build issues.

### `dashboard` Command

The `esphome dashboard <CONFIG>` command starts the ESPHome dashboard server for using ESPHome
through a graphical user interface. This command accepts a configuration directory instead of a
single configuration file.

#### Options

{{< option "--address ADDRESS" >}}
Manually set the address to bind to (defaults to 0.0.0.0)
{{< /option >}}

{{< option "--port PORT" >}}
Manually set the HTTP port to open connections on (defaults to 6052)
{{< /option >}}

{{< option "--socket SOCKET" >}}
Manually set the unix socket to bind to. If specified along with `--address` or `--port` the values
for those parameters will be ignored. Cannot be used along with `--systemd-socket`.
{{< /option >}}

{{< option "--username USERNAME" >}}
The optional username to require for authentication.
{{< /option >}}

{{< option "--password PASSWORD" >}}
The optional password to require for authentication.
{{< /option >}}

{{< option "--open-ui" >}}
If set, opens the dashboard UI in a browser once the server is up and running. Does not work when using
`--socket`.
{{< /option >}}

### `analyze-memory` Command

> [!NOTE]
> This is an advanced command primarily intended for developers and debugging memory issues.

The `esphome analyze-memory <CONFIG>` command compiles the configuration and analyzes memory usage by component.

This command is useful for:

* Understanding which components are consuming the most memory
* Identifying optimization opportunities to reduce flash or RAM usage
* Analyzing memory usage before adding more components to a tight build
* Debugging builds that are approaching memory limits (especially on ESP8266)
* Contributing to ESPHome development and optimization efforts

The command automatically compiles the configuration if needed (or quickly relinks if sources haven't changed), then analyzes the resulting firmware to show a detailed breakdown of memory usage by component, including flash memory (code and data) and RAM usage (data and BSS).

### `logs` Command

The `esphome logs <CONFIG>` command validates the configuration and shows all logs.

#### Options

{{< option "--topic TOPIC" >}}
Manually set the topic to subscribe to.
{{< /option >}}

{{< option "--username USERNAME" >}}
Manually set the username.
{{< /option >}}

{{< option "--password PASSWORD" >}}
Manually set the password.
{{< /option >}}

{{< option "--client-id CLIENT_ID" >}}
Manually set the client id.
{{< /option >}}

{{< option "--device SERIAL_PORT" >}}
Manually specify a serial port/IP to use. For example `/dev/cu.SLAB_USBtoUART`.

Multiple `--device` options can be specified to provide fallback addresses. When using the
native API for logs, all addresses are passed to the API client which uses the Happy Eyeballs
algorithm (RFC 8305) to efficiently connect using the fastest available address.

Example:

```shell
esphome logs my-device.yaml --device 192.168.1.100 --device 2001:db8::1
```

{{< /option >}}

{{< option "--reset" >}}
If set, reset the device before starting the logs. May also be configured with the environment variable
`ESPHOME_SERIAL_LOGGING_RESET=true`.
{{< /option >}}

## Using Bash or ZSH auto-completion

ESPHome's command line interface provides the ability to use auto-completion features provided by Bash or ZSH.

You can register ESPHome for auto-completion by adding the following to your ~/.bashrc file:

```shell
eval "$(register-python-argcomplete esphome)"
```

For more information, see [argcomplete](https://kislyuk.github.io/argcomplete/) documentation.

## Using logging tools supplied with ESPHome

There are two types of logging interfaces supplied with ESPHome: API and Serial (UART) logging.
For serial logging, there are many options including [ESPHome Web](https://web.esphome.io) and
the ESPHome CLI's `run` command.

For basic API based logging uses, one can use the `aioesphomeapi-logs` command bundled with ESPHome,
Which is especially useful for ESP devices in a remote/inaccessible location.

The syntax is as follows:

```shell
aioesphomeapi-logs <IPv4 pr IPv6 address>
```

Some working examples include:

```shell
aioesphomeapi-logs 192.168.x.y
aioesphomeapi-logs fe80::cdef:0123:4567:89ab
aioesphomeapi-logs 2001:0db8:3333:4444:5555:6666:7777:8888
```

Press `CTRL+C` to exit the logging view.

If you have configured encryption for API, provide the key from the yaml as follows:

```shell
aioesphomeapi-logs 192.168.x.y --noise-psk <your-api-key-from-yaml>
```

If you do not know/wish to know the IP address of an ESPHome device,
one can also use `aioesphomeapi-discover` to discover online ESPHome devices on the local network.

The syntax is as follows:

```shell
aioesphomeapi-discover
```

The response lists info about currently available ESPHome devices:

`Status |Name |Address |MAC |Version |Platform |Board`

## See Also

* {{< docref "/guides/index" "Guides" >}}
