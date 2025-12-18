---
description: "How to use packages in ESPHome"
title: "Packages"
params:
  seo:
    description: How to use packages in ESPHome
    image: settings.svg
---

When you have many ESPHome devices (or are producing and distributing them at scale), a common need tends to surface:
configuration modularization. You'll likely want to break your configuration into common (groups of) elements, building
it into reusable pieces which can subsequently be used by many/all devices. Only unique pieces of your configuration
remain in any given device's YAML configuration file.

This can be accomplished with ESPHome's `packages` feature.

All definitions from packages will be merged with your device's main configuration in a non-destructive way. This
allows overriding (parts of) configuration contained in the package(s). Substitutions in your main configuration will
override substitutions with the same name in a package.

Dictionaries are merged key-by-key. Lists of components are merged by component ID (if specified). Other lists are
merged by concatenation. All other configuration values are replaced with the later value.

ESPHome uses `!include` to "bring in" packages from other files; this feature is described in [!include](/guides/yaml#yaml-include).

The `packages:` key may have a value that is:

- A list of valid package references
- A mapping of keys to package references. When a mapping is used, the keys are for reference only and have no significance in themselves.

Examples of all formats are shown below.

## Local Packages

Consider the following example where the author put common pieces of configuration (like Wi-Fi and API) into base files
and then extends it with some device-specific configuration in the main configuration.

Note how the piece of configuration describing `api` component in `device_base.yaml` gets merged with the actions
definitions from main configuration file.

```yaml
# In config.yaml
packages:   # as a list
  - !include common/wifi.yaml
  - !include common/device_base.yaml

api:
  actions:
    - action: start_laundry
      then:
        - switch.turn_on: relay

# any additional configuration...
```

```yaml
# In wifi.yaml
wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
```

```yaml
# In device_base.yaml
esphome:
  name: ${node_name}

esp32:
  board: wemos_d1_mini32

logger:

api:
  encryption:
    key: !secret api_encryption_key
```

{{< anchor "config-git_packages" >}}

## Remote/Git Packages

Packages can also be loaded from a Git repository by utilizing the correct configuration syntax.
{{< docref "/components/substitutions" >}} can be used inside the remote packages which allows users to override
them locally with their own substitution value.

> [!NOTE]
> Remote packages cannot have `secret` lookups in them. They should instead make use of substitutions with an
> optional default in the packaged YAML, which the local device YAML can set using values from the local secrets.

```yaml
# Git repo examples as a mapping
packages:
  # shorthand form github://username/repository/[folder/]file-path.yml[@branch-or-tag]
  remote_package_shorthand: github://esphome/non-existant-repo/file1.yml@main

  remote_package_files:
    url: https://github.com/esphome/non-existant-repo
    files: [file1.yml, file2.yml]  # optional; if not specified, all files will be included
    ref: main  # optional
    refresh: 1d  # optional

  remote_package_files2:
    url: https://github.com/esphome/non-existant-repo
    files:
      - path: file1.yml
        vars:
          a: 1
          b: 2
      - path: file1.yml #Same file can be specified multiple times with different vars.
        vars:
          a: 3
          b: 4
      - file2.yml
    ref: main  # optional
    refresh: 1d  # optional
```

## Configuration variables

For each package:

- **url** (**Required**, string): The URL for the repository.
- **path** (*Optional*, string): Base common path of included files.
- **username** (*Optional*, string): Username to be used for authentication, if required.
- **password** (*Optional*, string): Password to be used for authentication, if required.
- **files** (**Required**): List of files to include. Can be one of:

  - list of file paths
  - list of objects containing `path` and `vars`

- **ref** (*Optional*, string): The Git ref(erence) to be used when pulling content from the repository.
- **refresh** (*Optional*, [Time](/guides/configuration-types#time)): The interval at which the content from the repository should be refreshed.

## Packages as Templates

Since packages are incorporated using the `!include` system, variables can be provided to them. This means that
packages can be used as *templates*, allowing complex or repetitive configurations to be stored in a package file
and then incorporated into the configuration more than once.

Packages may also contain a `defaults` block which provides subsitutions for variables not provided by the
`!include` block.

As an example, if the configuration needed to support three garage doors using the `gpio` switch platform and the
`time_based` cover platform, it could be constructed like this:

```yaml
# In config.yaml
packages:
  left_garage_door: !include
    file: garage-door.yaml
    vars:
      door_name: Left
  middle_garage_door: !include
    file: garage-door.yaml
    vars:
      door_name: Middle
  right_garage_door: !include
    file: garage-door.yaml
    vars:
      door_name: Right
```

```yaml
# In garage-door.yaml
switch:
  - name: ${door_name} Garage Door Switch
    platform: gpio
    # ...
```

## Conditionally including a package

You can include a package based on a condition, or choose a package dynamically by loading your package with
`!include` into a substitution variable instead of under `packages`:

```yaml
# In config.yaml
substitutions:
  left_garage_door: !include
    file: garage-door.yaml
    vars:
      door_name: Left

  enable_extra_door: true

packages:
  extra_door: ${ left_garage_door if enable_extra_door else {} }
```

The above utilizes a [Jinja expression](/components/substitutions#jinja-expressions) to determine whether
`left_garage_door` package is actually included. `enable_extra_door` can be set from the
[command line](/components/substitutions#command-line-substitutions) as well.

{{< anchor "config-packages_extend" >}}

## Extend

To make changes or add additional configuration to included configurations, `!extend config_id` can be used, where
`config_id` is the ID of the configuration to modify.

For example, to set a specific update interval on a common uptime sensor that is shared between configurations:

```yaml
# In common.yaml
captive_portal:

sensor:
  - platform: uptime
    id: uptime_sensor
    update_interval: 5min
```

```yaml
packages:
  - !include common.yaml

sensor:
  - id: !extend uptime_sensor
    update_interval: 10s
```

LVGL-style configuration hierarchies are also supported:

```yaml
# In interface.yaml
lvgl:
  pages:
    - id: main_page
      widgets:
        - label:
            id: title_label
            text: "Main Page"
```

```yaml
packages:
  - !include interface.yaml
lvgl:
  pages:
    - id: !extend main_page
      widgets:
        - label:
            id: !extend title_label
            text: "New Title"
            text_color: red
```

`!extend` also works with substitutions and jinja:

```yaml
substitutions:
  switches:
    - left_switch
    - right_switch
    - center_switch
  
  mains_switch: 1

switch:
  - platform: gpio
    id: left_switch
    pin: 1
  - platform: gpio
    id: right_switch
    pin: 2
  - platform: gpio
    id: center_switch
    pin: 3
  - id: !extend ${ switches[mains_switch] }
    name: "Mains switch"
```

{{< anchor "config-packages_remove" >}}

## Remove

To remove existing entries from included configurations, `!remove [config_id]` can be used, where `config_id` is
the ID of the entry to modify.

For example, to remove a common uptime sensor that is shared between configurations:

```yaml
packages:
  - !include common.yaml  # see above

sensor:
  - id: !remove uptime_sensor
```

Remove an item in a LVGL-style configuration:

```yaml
packages:
  - !include interface.yaml # see above
lvgl:
  pages:
    - id: !extend main_page
      widgets:
        - label:
            id: !remove title_label
```

To remove captive portal for a specific device:

```yaml
packages:
  - !include common.yaml  # see above

captive_portal: !remove
```

To remove only an attribute for a specific device:

```yaml
packages:
  common: !include common.yaml  # see above

sensor:
  - id: !extend uptime_sensor
    update_interval: !remove
```

`!remove` also works with substitutions:

```yaml
substitutions:
  disable_reboot: true

switch:
  - platform: restart
    id: restart_switch
    name: "Living Room Restart"
  - id: !remove ${disable_reboot and "restart_switch"}
```

## See Also

- {{< docref "/index" "ESPHome index" >}}
- {{< docref "/guides/getting_started_command_line" >}}
- {{< docref "/guides/faq" >}}
