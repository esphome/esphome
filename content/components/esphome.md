---
description: "Instructions for setting up the core ESPHome configuration."
title: "ESPHome Core Configuration"
params:
  seo:
    description: Instructions for setting up the core ESPHome configuration.
    image: cloud-circle.svg
---

Here you specify some core information that ESPHome needs to create
firmwares. Most importantly, this is the section of the configuration
where you specify the **name** of the node.

```yaml
# Example configuration entry
esphome:
    name: livingroom
    comment: Living room ESP32 controller
    area: Living Room
```

{{< anchor "esphome-configuration_variables" >}}

## Configuration variables

- **name** (**Required**, string): This is the name of the node. It
  should always be unique in your ESPHome network. May only contain lowercase
  characters, digits and hyphens, and can be at most 24 characters long by default, or 31
  characters long if `name_add_mac_suffix` is `false`.
  See [Changing ESPHome Node Name](#esphome-changing_node_name).

- **friendly_name** (*Optional*, string):
  This name is sent to the frontend and used by Home Assistant as
  the integration and device name. It also gets prefixed to entity
  names when needed. While optional, leaving it out can result in
  less intuitive names and a less polished experience in Home
  Assistant. Setting a `friendly_name` helps keep things clear,
  consistent, and easier to manage.

- **area** (*Optional*, string or [Area Configuration](#esphome-area)): The area configuration for this device. This is sent to
  Home Assistant to specify which area/zone the device belongs to. Can be either a simple string (e.g., `"Living Room"`  )
  or a structured format with `id` and `name`.

Advanced options:

- **build_path** (*Optional*, string): Customize where ESPHome will store the build files
  for your node. By default, ESPHome puts the PlatformIO project it uses to build the
  firmware in the `.esphome/build/<NODE>` (or into path from `ESPHOME_BUILD_PATH` environment variable if specified) directory,
  but you can customize this behavior using this option. Official docker image automatically use `/build` folder
  as default one in case it is mounted to it.

- **platformio_options** (*Optional*, mapping): Additional options to pass over to PlatformIO in the
  platformio.ini file. See [`platformio_options`](#esphome-platformio_options).

- **environment_variables** (*Optional*, mapping): Environment variables to set during the build process.
  See [`environment_variables`](#esphome-environment_variables).

- **includes** (*Optional*, list of files): A list of C/C++ files to include in the (auto-generated) `main` file.
  The paths in this list are relative to the directory where the YAML configuration file is located or `<...>` includes.
  See [`includes`](#esphome-includes).

- **includes_c** (*Optional*, list of files): The same as `includes` but for files that require C linkage. All includes
  will be wrapped in `extern "C" {}`. See [`includes`](#esphome-includes).

- **libraries** (*Optional*, list of libraries): A list of libraries to include in the project. See
  [`libraries`](#esphome-libraries).

- **comment** (*Optional*, string): Additional text information about this node. Only for display in UI.
- **name_add_mac_suffix** (*Optional*, boolean): Appends the last 3 bytes of the mac address of the device to
  the name in the form `<name>-aabbcc`. Defaults to `false`.
  See [Adding the MAC address as a suffix to the device name](#esphome-mac_suffix).

- **project** (*Optional*): ESPHome Creator's Project information. See [Project information](#esphome-creators_project).

  - **name** (**Required**, string): Name of the project
  - **version** (**Required**, string): Version of the project
  - **on_update** (*Optional*, [Automation](/automations)): An automation to perform when the device firmware is updated.
    This compares the above `version` field with the `version` that was in the previous firmware
    as long as the `name` matches.
    The `version` is stored in flash memory when the firmware is first run for future comparisons.

- **min_version** (*Optional*, string): The minimum ESPHome version required to compile this configuration.
  See [Minimum ESPHome version](#esphome-min_version).

- **compile_process_limit** (*Optional*, int): The maximum number of simultaneous compile processes to run.
  Defaults to the number of cores of the CPU which is also the maximum you can set.

- **debug_scheduler** (*Optional*, boolean): If set, the scheduler will print debug information about scheduled tasks at log level DEBUG.
- **areas** (*Optional*, list of [Area Configuration](#esphome-area)): Additional areas that can be referenced by devices.
- **devices** (*Optional*, list of [Sub-Devices](#esphome-devices)): Sub-devices to group entities under.

Automations:

- **on_boot** (*Optional*, [Automation](/automations)): An automation to perform
  when the node starts. See [`on_boot`](#esphome-on_boot).

- **on_shutdown** (*Optional*, [Automation](/automations)): An automation to perform
  right before the node shuts down. See [`on_shutdown`](#esphome-on_shutdown).

- **on_loop** (*Optional*, [Automation](/automations)): An automation to perform
  on each `loop()` iteration. See [`on_loop`](#esphome-on_loop).

{{< anchor "esphome-on_boot" >}}

## `on_boot`

This automation will be triggered when the ESP boots up. By default, it is executed after everything else
is already set up. You can however change this using the `priority` parameter.

```yaml
esphome:
  # ...
  on_boot:
    - priority: 600
      then:
        - switch.turn_off: switch_1
```

### Configuration variables

- **priority** (*Optional*, float): The priority to execute your custom initialization code. A higher value
  means a high priority and thus also your code being executed earlier. Please note this is an ESPHome-internal
  value and any change will not be marked as a breaking change. Defaults to `600`. Priorities (you can use any value between them too):

  - `800.0`  : This is where all hardware initialization of vital components is executed. For example setting switches
    to their initial state.

  - `600.0`  : This is where most sensors are set up.
  - `250.0`  : At this priority, WiFi is initialized.
  - `200.0`  : Network connections like MQTT/native API are set up at this priority.
  - `-100.0`  : At this priority, pretty much everything should already be initialized.

- See [Automation](/automations).

{{< anchor "esphome-on_shutdown" >}}

## `on_shutdown`

This automation will be triggered when the ESP is about to shut down. Shutting down is usually caused by
too many WiFi/MQTT connection attempts, Over-The-Air updates being applied or through the {{< docref "deep_sleep/" >}}.

> [!NOTE]
> It's not guaranteed that all components are in a connected state when this automation is triggered. For
> example, the MQTT client may have already disconnected. For use-cases that require specific shutdown ordering, look at the `priority` parameter.

```yaml
esphome:
  # ...
  on_shutdown:
    - priority: 700
      then:
        - switch.turn_off: switch_1
```

### Configuration variables

- **priority** (*Optional*, float): The priority to execute your custom shutdown code. A higher value
  means a high priority and in case of shutdown triggers that the code is executed **later**.
  Priority is used primarily for the initialization order of components. Shutdowns for these components are handled in *reverse* order, such that e.g. sensors (600) are shutdown before the hardware components (800) they depend on.
  Please note this is an ESPHome-internal value and any change will not be marked as a breaking change.
  Defaults to `600`. For priority values refer to the list in the [`on_boot`](#esphome-on_boot) section.

- See [Automation](/automations).

{{< anchor "esphome-on_loop" >}}

## `on_loop`

This automation will be triggered on every `loop()` iteration (usually around every 16 milliseconds).

```yaml
esphome:
  # ...
  on_loop:
    then:
      # do something
```

{{< anchor "esphome-platformio_options" >}}

## `platformio_options`

PlatformIO supports a number of options in its `platformio.ini` file. With the `platformio_options`
parameter you can tell ESPHome what options to pass into the `env` section of the PlatformIO file
(note you can also do this by editing the `platformio.ini` file manually).

You can view a full list of PlatformIO options here: <https://docs.platformio.org/en/latest/projectconf/section_env.html>

```yaml
# Example configuration entry
esphome:
  # ...
  platformio_options:
    upload_speed: 115200
    board_build.f_flash: 80000000L
```

{{< anchor "esphome-environment_variables" >}}

## `environment_variables`

With the `environment_variables` option, you can set environment variables that will be available during the
compilation process. This is useful when you need to pass configuration to build scripts, custom libraries,
or PlatformIO build environments.

```yaml
# Example configuration entry
esphome:
  # ...
  environment_variables:
    HTTP_PROXY: "http://user:pass@10.10.1.10:3128/"
    PLATFORMIO_SETTING_ENABLE_PROXY_STRICT_SSL: "false"
```

> [!NOTE]
> Environment variables set here are only available during the build process. They do not affect
> the runtime behavior of your device.

{{< anchor "esphome-includes" >}}

## `includes`

With `includes` you can include source files in the generated PlatformIO project.
All files declared with this option are copied to the project each time it is compiled.

You can always look at the generated PlatformIO project (`.esphome/build/<NODE>`  ) to see what
is happening - and if you want you can even copy the include files directly into the `src/` folder.
The `includes` option is only a helper option that does that for you.

```yaml
# Example configuration entry
esphome:
  # ...
  includes:
    - my_switch.h
    - <mylib.h>
```

This option behaves differently depending on what the included file is pointing at:

- If the include string is written as `<mylib>` or `"<mylib>"`, the line `#include <mylib>` is
   added to the beginning of the `main.cpp` file.

- If the include string is pointing at a directory, the entire directory tree is copied into the
   src/ folder.

- If the include string points to a header file (.h, .hpp, .tcc), it is copied in the src/ folder
   AND included in the `main.cpp` file. This way the lambda code can access it.

- If the include string points to a regular source file (.c, .cpp), it is copied in the src/ folder
   AND compiled into the binary. This way implementation of classes and functions in header files can
   be provided.

{{< anchor "esphome-libraries" >}}

## `libraries`

The `libraries` option allows you to include libraries in the PlatformIO project. These libraries will then be
compiled into the resulting firmware and may be used by [lambdas](/automations/templates#config-lambda).

```yaml
# Example configuration entry
esphome:
  # ...
  libraries:
    # a library from PlatformIO
    - espressif/esp32-camera

    # a library bundled with Arduino
    - Wire

    # use the git version of a library used by a component
    - Improv=https://github.com/improv-wifi/sdk-cpp.git#v1.0.0
```

The most common usage of this option is to include third-party libraries that are available in the [PlatformIO registry](https://platformio.org/lib). They can be added by listing their name under this option. It is also possible to use
specific versions, or to fetch libraries from a file or git repository. ESPHome accepts the same syntax as the
[lib_deps](https://docs.platformio.org/en/latest/projectconf/sections/env/options/library/lib_deps.html) option.

Using `<name>=<source>` syntax, it is possible to override the version used for libraries that are automatically added
by one of ESPHome's components. This can be useful during development to make ESPHome use a custom fork of a library.

By default, ESPHome does not include any libraries into the project. This means that libraries that are bundled with
Arduino, such as `Wire` or `EEPROM`, aren't available. If you need to use them, you should list them manually under
this option. If they are used by another library, they should be listed before the library that uses them.

{{< anchor "preferences-flash_write_interval" >}}

## Preferences Component

This component is used to store data in the flash memory which is persisted across device reboots, e.g. the latest
state of a light or the accumulated energy used by an appliance.

```yaml
# Example configuration entry
preferences:
  flash_write_interval: 1min
```

### Configuration variables

- **flash_write_interval** (*Optional*, [Time](/guides/configuration-types#time)): Customize the frequency in which data is
  flushed to the flash. This setting helps to prevent rapid changes to a component from being quickly
  written to the flash and wearing it out. Defaults to `1min`. Set to `never` to disable this feature.

As all devices have a limited number of flash write cycles, this setting helps to reduce the number of flash writes
due to quickly changing components. In the past, when components such as `light`, `switch`, `fan` and `globals`
were changed, the state was immediately committed to flash. The result of this was that the last state of these
components would always restore to its last state on power loss, however, this has the cost of potentially quickly
damaging the flash if these components are quickly changed.

A safety feature has thus been implemented to mitigate issues resulting from the limited number of flash write cycles,
the state is first stored in memory before being flushed to flash after the `flash_write_interval` has passed. This
results in fewer flash writes, preserving the flash health.

This behavior can be modified by setting `flash_write_interval` to `0s` to commit the changes to flash as soon as possible,
however, be aware that this may lead to increased flash wearing and a shortened device lifespan!

For {{< docref "/components/esp8266" "ESP8266" >}}, `restore_from_flash` must also be set to `true` for states to be written to flash.

{{< anchor "esphome-changing_node_name" >}}

## Changing ESPHome Node Name

Trying to change the name of a node or its address in the network?
You can do so with the `use_address` option of the {{< docref "wifi" "WiFi configuration" >}}.

Change the device name or address in your YAML to the new value and additionally
set `use_address` to point to the old address like so:

```yaml
# Step 1. Changing name from test8266 to kitchen
esphome:
  name: kitchen
  # ...

wifi:
  # ...
  use_address: test8266.local
```

Now upload the updated config to the device. As a second step, you now need to remove the
`use_address` option from your configuration again so that subsequent uploads will work again
(otherwise it will try to upload to the old address).

```yaml
# Step 2
esphome:
  name: kitchen
  # ...

wifi:
  # ...
  # Remove or comment out use_address
  # use_address: test8266.local
```

The same procedure can be done for changing the static IP of a device.

{{< anchor "esphome-mac_suffix" >}}

## Adding the MAC address as a suffix to the device name

Using `name_add_mac_suffix` allows {{< docref "/guides/creators" "creators" >}} to
provision multiple devices at the factory with a single firmware and still
have unique identification for customer installs.

> [!NOTE]
> End users will need to create an individual YAML config file if they want to OTA update the
> devices in the future. Creators can facilitate this process by providing `dashboard_import` URL
> for end users. This allows them to easily update their devices as new features are made available
> upstream.

{{< anchor "esphome-creators_project" >}}

## Project information

This allows creators to add the project name and version to the compiled code. It is currently only
exposed via the logger, mDNS and the device_info response via the native API. The format of the name
should be `author_name.project_name`.

```yaml
# Example configuration
esphome:
  ...
  project:
    name: "jesse.leds_party"
    version: "1.0.0"
```

{{< anchor "esphome-min_version" >}}

## Minimum ESPHome version

This allows YAML files to specify the minimum version of ESPHome required to compile.
This is useful in the case of packages where a published package might use features only
available in a newer version of ESPHome. This allows for a more friendly error message.

```yaml
# Example configuration
esphome:
  min_version: 2025.11.0
```

{{< anchor "esphome-area" >}}

## Area Configuration

Areas help organize your devices in Home Assistant by location. ESPHome supports two formats for area configuration:

**Simple String Format:**

```yaml
esphome:
  name: my-device
  area: "Living Room"
```

**Structured Format:**

```yaml
esphome:
  name: my-device
  area:
    id: living_room
    name: "Living Room"
```

The simple string format is convenient for basic use cases where you just want to assign the ESP device to a room.
The structured format is recommended when using sub-devices, as it allows you to reference areas by their ID.

Configuration variables (structured format):

- **id** (**Required**, string): Unique identifier for the area.
- **name** (**Required**, string): Display name for the area.

{{< anchor "esphome-devices" >}}

## Sub-Devices

ESPHome supports creating sub-devices within a single ESP controller. This allows you to group entities
into logical devices that appear separately in Home Assistant. This is particularly useful when:

- One ESP acts as a hub/gateway for multiple physical devices (RF bridges, Modbus devices, etc.)
- A single ESP controls multiple zones or rooms
- You want better organization of entities in Home Assistant

### Configuration variables

- **id** (**Required**, string): Unique identifier for the device.
- **name** (**Required**, string): Display name for the device.
- **area_id** (*Optional*, string): Reference to an area ID defined in `areas`.

### Example: RF Bridge Gateway

```yaml
# ESP32 acting as RF bridge for multiple 433MHz devices
esphome:
  name: rf-bridge
  area: "Utility Room"  # Simple string format for ESP device's area

  devices:
    - id: front_door_device
      name: "Front Door Sensor"
      area_id: entrance_area
    - id: kitchen_motion_device
      name: "Kitchen Motion"
      area_id: kitchen_area
    - id: garage_door_device
      name: "Garage Door"
      area_id: garage_area

  areas:
    - id: entrance_area
      name: "Entrance"
    - id: kitchen_area
      name: "Kitchen"
    - id: garage_area
      name: "Garage"

# Each RF device appears as a separate device in HA
binary_sensor:
  - platform: remote_receiver
    name: "Front Door"
    device_id: front_door_device
    rc_switch_raw:
      code: '101010110101'

  - platform: remote_receiver
    name: "Kitchen Motion"
    device_id: kitchen_motion_device
    rc_switch_raw:
      code: '110011001100'
```

### Example: Multi-Zone Controller

```yaml
esphome:
  name: multi-room-controller

  devices:
    - id: living_room_device
      name: "Living Room Controller"
      area_id: living_area
    - id: kitchen_device
      name: "Kitchen Controller"
      area_id: kitchen_area

  areas:
    - id: living_area
      name: "Living Room"
    - id: kitchen_area
      name: "Kitchen"

sensor:
  - platform: dht
    pin: GPIO5
    temperature:
      name: "Temperature"
      device_id: living_room_device
    humidity:
      name: "Humidity"
      device_id: living_room_device
```

## See Also
