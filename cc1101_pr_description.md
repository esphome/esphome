# What does this implement/fix?

This PR adds comprehensive runtime configuration support to the CC1101 component by introducing `number`, `select`, `sensor`, `text_sensor`, and `switch` platforms. These allow users to adjust nearly every aspect of the radio (frequency, power, modulation, AGC settings, packet mode, etc.) via Home Assistant or MQTT without reflashing.

**Key Features:**
- **Runtime Tuning**: Change frequency, output power, bandwidth, and more on the fly.
- **Switchable Settings**: Toggle DC blocking filters, Manchester encoding, CRC, whitening, and packet mode at runtime.
- **Diagnostics**: Real-time `RSSI` and `LQI` sensors (triggered per-packet).
- **Metadata**: `Chip ID`, `Frequency`, and `Modulation` text sensors for easy monitoring.
- **Config Synchronization**: Implemented a `ConfigListener` pattern so that changing one entity (e.g., `Frequency`) immediately updates related entities (e.g., `Frequency Preset`), keeping the UI perfectly in sync.

**Performance Optimizations:**
- **Spam Prevention**: Strict value-change checks in all C++ setters prevent redundant hardware writes and network updates.
- **Double-Buffering**: `Select` and `Text Sensor` platforms track their last published state to ensure they only send updates when a value truly changes.
- **Smart Sensor Updates**: `RSSI` and `LQI` sensors now include C++ level change-checks to prevent flooding the network with identical values on every received packet.
- **Fixed Deprecations**: Updated code to use `current_option()` and `get_raw_state()` to comply with the latest ESPHome standards.
- **Reliable Imports**: Corrected Python-level constant imports and fixed C++ scope resolution issues.

## Types of changes

- [ ] Bugfix (non-breaking change which fixes an issue)
- [x] New feature (non-breaking change which adds functionality)
- [ ] Breaking change (fix or feature that would cause existing functionality to not work as expected)
- [ ] Developer breaking change (an API change that could break external components)
- [x] Code quality improvements to existing code or addition of tests
- [ ] Other

**Related issue or feature (if applicable):**

- fixes <link to issue>

**Pull request in [esphome-docs](https://github.com/esphome/esphome-docs) with documentation (if applicable):**

- esphome/esphome-docs#<esphome-docs PR number goes here>

## Test Environment

- [x] ESP32
- [x] ESP32 IDF
- [ ] ESP8266
- [ ] RP2040
- [ ] BK72xx
- [ ] RTL87xx
- [ ] LN882x
- [ ] nRF52840

## Example entry for `config.yaml`:

```yaml
cc1101:
  id: transceiver
  cs_pin: GPIO5
  frequency: 433.92MHz

# Expose settings as Number entities
number:
  - platform: cc1101
    cc1101_id: transceiver
    output_power:
      name: "Output Power"
    tuner:
      frequency:
        name: "Frequency"

# Expose settings as Select entities
select:
  - platform: cc1101
    cc1101_id: transceiver
    frequency_preset:
      name: "Frequency Preset"
    tuner:
      modulation:
        name: "Modulation"

# High-frequency diagnostics
sensor:
  - platform: cc1101
    cc1101_id: transceiver
    rssi:
      name: "RSSI"
      filters:
        - throttle: 1s  # Recommended to prevent MQTT spam
        - delta: 1.0
    lqi:
      name: "LQI"
      filters:
        - throttle: 1s
```

## Checklist:
  - [x] The code change is tested and works locally.
  - [x] Tests have been added to verify that the new code works (under `tests/` folder).

If user exposed functionality or configuration variables are added/changed:
  - [ ] Documentation added/updated in [esphome-docs](https://github.com/esphome/esphome-docs).
