---
description: "Configuration for the ESP32 platform for ESPHome."
title: "ESP32 Platform"
params:
  seo:
    description: Configuration for the ESP32 platform for ESPHome.
    image: esp32.svg
---

This component contains platform-specific options for the ESP32 platform.

```yaml
# Example configuration entry
esp32:
  variant: esp32s3
```

## Configuration variables

- **variant** (*Optional*, string): The ESP32 mcu/chip to use for this device configuration. One of `esp32`,
  `esp32s2`, `esp32s3`, `esp32c2`, `esp32c3`, `esp32c5`, `esp32c6`, `esp32c61`, `esp32h2` or `esp32p4`.
  This must match the hardware in use, or it will fail to flash.

- **board** (*Optional*, string): The PlatformIO board ID that should be used. Choose the appropriate board from
  [this list](https://registry.platformio.org/platforms/platformio/espressif32/boards) (the icon next
  to the name can be used to copy the board ID). *This only affects pin aliases and some internal settings*;
  This setting is no longer recommended, `variant` should be used instead.

> [!NOTE]
> At least one of `board` or `variant` must be specified. If `variant` alone is specified (the recommended practice),
> the board configuration will be automatically filled using a standard Espressif devkit board
> suitable for that variant. Both may be specified (for backwards compatibility) but they must define the same variant.

- **flash_size** (*Optional*, string): The amount of flash memory available on the ESP32 board/module. One of `2MB`,
  `4MB`, `8MB`, `16MB` or `32MB`. Defaults to `4MB`. **Warning: specifying a size larger than that available
  on your board will cause the ESP32 to fail to boot.**

- **cpu_frequency** (*Optional*, string): The CPU frequency to use. One of `40MHz`, `80MHz`, `160MHz`, `240MHz`,
  `360MHz` or `400MHz`. Defaults to `160MHz`. Not all values are available for all chips.

- **partitions** (*Optional*, filename): The name of (optionally including the path to) the file containing the
  partitioning scheme to be used. When not specified, partitions are automatically generated based on `flash_size`.

- **framework** (*Optional*): Options for the underlying framework used by ESPHome. See [Framework](#esp32-framework).

{{< anchor "esp32-framework" >}}

## Framework

ESPHome supports two framework options for ESP32 chips:

### Arduino Framework

The Arduino framework is integrated as an ESP-IDF component. This provides Arduino API compatibility
within the ESP-IDF build system. Arduino framework is available for ESP32 (classic), ESP32-C3, ESP32-S2, and ESP32-S3 variants.

```yaml
# Example configuration entry
esp32:
  board: ...
  framework:
    type: arduino
```

### ESP-IDF Framework

ESP-IDF is Espressif's native development framework. It is required for ESP32-C2, ESP32-C5, ESP32-C6, ESP32-C61,
ESP32-H2, and ESP32-P4 variants, as these are not supported by the Arduino framework. It is recommended for
all ESP32 chips when possible. See the {{< docref "/guides/esp32_arduino_to_idf" "migration guide" >}} for help transitioning from Arduino.

```yaml
# Example configuration entry
esp32:
  board: ...
  framework:
    type: esp-idf
```

### Configuration variables

- **type** (*Optional*, string): The framework type, either `esp-idf` or `arduino`. Defaults to `arduino` for ESP32 (classic), ESP32-C3, ESP32-S2, and ESP32-S3. Defaults to `esp-idf` for ESP32-C2, ESP32-C5, ESP32-C6, ESP32-C61, ESP32-H2, and ESP32-P4 (Arduino is not supported on these variants)

- **version** (*Optional*, string): The base framework version number to use, from
  [ESP32 ESP-IDF releases](https://github.com/espressif/esp-idf/releases) or
  [ESP32 Arduino releases](https://github.com/espressif/arduino-esp32/releases). Defaults to `recommended`.
  Additional values are:

  - `dev`  : Use the latest commit, note this may break at any time
  - `latest`  : Use the latest *release*, even if it hasn't been recommended yet.
  - `recommended`  : Use the recommended framework version.

- **source** (*Optional*, string): The PlatformIO package to use for the framework. This variable provides
  the URL of the git repository or file archive of a custom or patched version of the
  [pioarduino/framework-arduinoespressif32](https://github.com/espressif/arduino-esp32) or
  [pioarduino/framework-espidf](https://github.com/pioarduino/esp-idf) package for the framework type. Refer to
  [PlatformIO package specifications](https://docs.platformio.org/en/latest/core/userguide/pkg/cmd_install.html#package-specifications)
  for the supported URL schemes. Examples:

  - `https://github.com/user/arduino-esp32/releases/download/archive.zip`
  - `https://github.com/user/esp-idf.git#branch`
  - `symlink:///path/to/esp-idf`

- **platform_version** (*Optional*, string): The version of the
  [pioarduino/espressif32](https://github.com/pioarduino/platform-espressif32/releases/) package to use. For known framework versions
  this value will be set automatically.

- **sdkconfig_options** (*Optional*, mapping): Custom sdkconfig
  [compiler options](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/kconfig.html#compiler-options)
  to set in the ESP-IDF project.

- **log_level** (*Optional*, string): Log level of the framework, one of `ERROR` (default), `NONE`, `WARN`, `INFO`,
  `DEBUG` or `VERBOSE`.

- **advanced** (*Optional*, mapping): See [Advanced Configuration](#esp32-advanced_configuration) below.
- **components** (*Optional*, list of components): See [IDF Components](#esp32-idf_components) below.

{{< anchor "esp32-advanced_configuration" >}}

## Advanced Configuration

- **assertion_level** (*Optional*, enum): One of `ENABLE` (default), `SILENT` or `DISABLE`. Changing away from
  the default will reduce the size of the compiled binary, albeit at the expense of ease of troubleshooting. See
  [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/v5.3.3/esp32/api-reference/kconfig.html#config-compiler-optimization-assertion-level)
  for more information.

- **compiler_optimization** (*Optional*, enum): One of `SIZE` (default), `PERF`, `NONE` or `DEBUG`. Changing
  away from the default will increase the size of the compiled binary but may increase performance or allow for easier
  troubleshooting. See
  [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/v5.3.3/esp32/api-reference/kconfig.html#config-compiler-optimization)
  for more information.

- **enable_lwip_assert** (*Optional*, boolean): Can be set to `false` to reduce the size of the compiled binary by
  disabling LWIP assertions. Defaults to `true` (as recommended by Espressif). See
  [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/v5.3.3/esp32/api-reference/kconfig.html#config-lwip-esp-lwip-assert)
  for more information.

- **execute_from_psram** (*Optional*, boolean): On ESP32S3 only may be set to `true` to enable executing code from PSRAM.
  With octal PSRAM this can be faster than executing from FLASH memory, and enables code such as display drawing
  to execute normally when writing to FLASH, e.g. during an OTA update. The default is `false`.

- **ignore_efuse_custom_mac** (*Optional*, boolean): Can be set to `true` for devices on which the burned-in custom
  MAC address is not valid.

- **ignore_efuse_mac_crc** (*Optional*, boolean): Can be set to `true` for devices on which the burned-in MAC
  address is not consistent with the burned-in CRC for that MAC address, resulting in an error like
  `Base MAC address from BLK0 of EFUSE CRC error`. **Valid only on original ESP32 with** `esp-idf` **framework.**

- **enable_idf_experimental_features** (*Optional*, boolean): Can be set to `true` to enable experimental features. Use of
  experimental features may cause instability or other issues.

- **loop_task_stack_size** (*Optional*, int): Loop task stack size in bytes. Increase if experiencing stack overflow
  errors (e.g., with complex code or deep recursion). Higher values reduce heap availability. Valid range is 8192-32768
  bytes. Defaults to 8192 bytes.

**LWIP Optimization Options (ESP-IDF only):**

The following options are available under the `advanced` section when using the ESP-IDF framework to optimize
LWIP (Lightweight IP) behavior. Some options improve performance while others save flash memory:

- **enable_lwip_dhcp_server** (*Optional*, boolean): Enable DHCP server functionality. Only needed if the device will act
  as a DHCP server (necessary for WiFi AP mode). When the WiFi component is used, it automatically handles enabling/disabling
  the DHCP server based on whether AP mode is configured. When WiFi is not used, defaults to `false`.

- **enable_lwip_mdns_queries** (*Optional*, boolean): Enable mDNS query support in the DNS resolver. This allows resolving
  local hostnames (like `broker.local`  ) for MQTT brokers and other services. While ESPHome has its own mDNS responder
  for advertising, this option is needed for resolving mDNS names. Defaults to `true`.

- **enable_lwip_bridge_interface** (*Optional*, boolean): Enable bridge interface support for bridging multiple network
  interfaces. Defaults to `false`.

- **enable_lwip_tcpip_core_locking** (*Optional*, boolean): Enable LWIP TCP/IP core locking for better socket performance.
  This uses direct function calls with mutex protection instead of mailbox message passing between threads. Enabling this
  improves socket operation performance by 20-200% but may reduce multi-threaded scalability. Defaults to `true`.

- **enable_lwip_check_thread_safety** (*Optional*, boolean): Enable LWIP thread safety checks to detect incorrect usage of
  the TCP/IP stack from multiple threads. This helps catch thread safety issues when core locking is enabled. Defaults to `true`.

- **disable_libc_locks_in_iram** (*Optional*, boolean): Disable placing libc lock functions in IRAM. This saves approximately
  1.3 KB of IRAM by placing these functions in flash memory instead. This is safe for ESPHome since no IRAM interrupt service
  routines (ISRs that run while cache is disabled) use libc lock APIs. Defaults to `true` (IRAM placement disabled to save RAM).

**VFS (Virtual File System) Optimization Options:**

The following options disable unused VFS features to save flash memory:

- **disable_vfs_support_termios** (*Optional*, boolean): Disable VFS support for termios (terminal I/O) functions. ESPHome
  doesn't use termios functions on ESP32 (they're only used in the host UART driver for Linux/macOS). Disabling this saves
  approximately 1.8 KB of flash. Defaults to `true` (VFS termios disabled to save flash).

- **disable_vfs_support_select** (*Optional*, boolean): Disable VFS support for select() with file descriptors. ESPHome uses
  `lwip_select()` for socket operations, which works independently of VFS select support. VFS select is only needed for UART
  and eventfd file descriptors. Socket operations continue to work normally with this disabled. Components that require VFS
  select (e.g., OpenThread) automatically enable it regardless of this setting. Disabling this saves approximately 2.7 KB of
  flash. Defaults to `true` (VFS select disabled to save flash).

- **disable_vfs_support_dir** (*Optional*, boolean): Disable VFS support for directory-related functions (opendir, readdir,
  mkdir, rmdir, etc.). ESPHome doesn't use directory operations on ESP32. Components that require directory support (e.g.,
  future storage components) automatically enable it regardless of this setting. Disabling this saves approximately 0.5 KB+
  of flash. Defaults to `true` (VFS directory support disabled to save flash).

**FreeRTOS Memory Options:**

- **freertos_in_iram** (*Optional*, boolean): Keep FreeRTOS functions in IRAM instead of moving them to flash. By default,
  non-ISR FreeRTOS functions are placed in flash to save up to 8 KB of IRAM. ISR-safe functions (`FromISR` variants) always
  remain in IRAM. Testing on ESP-IDF 5.5 with Bluetooth proxies shows no performance difference thanks to fast XIP (execute
  in place) from flash. Bluetooth proxies are one of the most IRAM-intensive and timing-sensitive use cases, which is likely
  why Espressif made this the default in IDF 6.0. This matches
  the default behavior in ESP-IDF 6.0 where `CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH` is removed and replaced by
  `CONFIG_FREERTOS_IN_IRAM` to restore the old behavior (see [ESP-IDF 6.0 breaking changes](https://github.com/espressif/esp-idf/issues/17052)
  and [migration guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/migration-guides/release-6.x/6.0/system.html#memory-placement)).
  Set to `true` only if you encounter issues with code that incorrectly calls FreeRTOS functions from ISRs with flash cache
  disabled. Defaults to `false` (FreeRTOS functions in flash to save IRAM).

- **ringbuf_in_iram** (*Optional*, boolean): Keep ring buffer functions in IRAM instead of moving them to flash. By default,
  ring buffer functions are placed in flash to save ~1.5 KB of IRAM. Ring buffer functions are typically only called every
  ~10ms for audio components, so the overhead of loading from flash vs IRAM is negligible compared to actual data processing.
  This matches the default behavior in ESP-IDF 6.0 (see [migration guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/migration-guides/release-6.x/6.0/system.html#id1)).
  Set to `true` only if you encounter issues. Defaults to `false` (ring buffer functions in flash to save IRAM).

Some options can be disabled to save flash memory without affecting typical ESPHome functionality. The performance
options (defaulting to `true`  ) improve socket operation performance but can be disabled if you need better
multi-threaded scalability (which is uncommon since ESPHome uses an event loop).

**Example configuration with advanced LWIP and VFS options:**

```yaml
# Example configuration entry
esp32:
  board: esp32dev
  framework:
    type: esp-idf
    advanced:
      # Performance options (enabled by default)
      enable_lwip_tcpip_core_locking: true  # Better socket performance
      enable_lwip_check_thread_safety: true  # Thread safety validation

      # Memory saving options
      disable_libc_locks_in_iram: true  # Enabled by default, saves 1.3 KB IRAM
      disable_vfs_support_termios: true  # Enabled by default, saves 1.8 KB flash
      disable_vfs_support_select: true  # Enabled by default, saves 2.7 KB flash (auto-enabled by openthread)
      disable_vfs_support_dir: true  # Enabled by default, saves 0.5 KB+ flash
      enable_lwip_dhcp_server: false  # Disabled by default, only needed for AP mode
      enable_lwip_mdns_queries: false  # Enabled by default, can disable if not using .local hostnames
      enable_lwip_bridge_interface: false  # Disabled by default
```

{{< anchor "esp32-idf_components" >}}

## IDF Components

The `components` option allows you to include IDF components. These components will then be compiled into the resulting
firmware and may be used by [lambdas](/automations/templates#config-lambda). The most common usage of this option is to include third-party
components that are available in the [ESP Component Registry](https://components.espressif.com/).

### Simple

For components from the ESP Component Registry, you can use the shorthand syntax `owner/component<operator>version`.
All [IDF Component Manager version operators](https://docs.espressif.com/projects/idf-component-manager/en/latest/reference/versioning.html)
are supported (e.g., `^`, `~`, `==`, `>=`):

```yaml
esp32:
  framework:
    components:
      - espressif/esp_hosted^2.6.1
```

### Advanced

For more complex configurations (custom git repositories, local paths, etc.), use the advanced syntax:

```yaml
esp32:
  framework:
    components:
      - name: my_custom_component
        source: https://github.com/user/component.git
        ref: main
        path: components/custom
```

#### Configuration Variables

- **name** (*Required*, string): Name of the component e.g. `espressif/esp_hosted`.
- **ref** (*Optional*, string): Component registry version or a git ref.
- **source** (*Optional*, string): The git repository to use for the component. This can be used for a
  custom or patched version of the component.
- **path** (*Optional*, string): The path of the component in the git repository or a local path to the
  component if `source` is not set.

## GPIO Pin Numbering

The ESP32 boards often use the internal GPIO pin numbering based on the microcontroller, so you likely don't have to
worry about pin alias names or numbering...yay!

Some notes about the pins on the original ESP32:

- `GPIO0` is used to determine the boot mode on startup; note that **ESP32 variants use different pins to determine
  the boot mode.** Bootstrapping pin(s) should **not** be pulled LOW on startup to avoid booting into flash mode when
  it's not desired. You can, however, still use the strapping pins as output pins.

- `GPIO34` to `GPIO39`  : These pins **cannot** be used as outputs (yes, even though GPIO stands for "general purpose
  input/**output**"...).

- `GPIO32` to `GPIO39`  : These pins can be used with the {{< docref "/components/sensor/adc" >}} to measure voltages.
- `GPIO2`  : On the `esp32dev` board, this pin is connected to the blue LED. It also supports the
  {{< docref "/components/binary_sensor/esp32_touch" "touch pad binary sensor" >}} (in addition to a few other pins).

```yaml
# Example configuration entry
binary_sensor:
  - platform: gpio
    name: "Pin GPIO23"
    pin: GPIO23
```

## See Also

- {{< docref "esphome/" >}}
