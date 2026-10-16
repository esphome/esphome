# Experimental ESP32 1-Wire slave

This component lets an externally powered ESP32 emulate one DS18B20-family
1-Wire device with a configured fixed temperature. It is a prototype, not a
production-qualified replacement for a physical DS18B20.

## Minimal configuration

```yaml
esp32:
  board: esp32dev
  framework:
    type: esp-idf

one_wire_slave:
  pin: GPIO4
  temperature: 23.5
```

An omitted `address` (or zero) generates a family-0x28 ROM from the station MAC
with a Dallas CRC. An explicit address must have family 0x28 and a valid CRC.
Temperatures are limited to -55 through 125 degrees Celsius.

The default `interrupt_mode: true` uses GPIO edge interrupts and busy-waited
microsecond timing. `interrupt_mode: false` selects the earlier high-priority
task backend. Neither backend uses a dedicated hardware 1-Wire peripheral.

## Wiring

Connect the emulating ESP32's DQ pin to the master's DQ pin, share ground, and
use an external 4.7 kOhm pull-up from DQ to 3.3 V. Power the boards independently;
do not join their 5 V rails. The slave configures DQ as open-drain and releases
the line rather than driving it high. Parasite power is not supported.

## Compatibility and limitations

- Initial hardware: classic dual-core ESP32, ESP32-D0WD-V3 revision 3.1.
  Other ESP32 variants are not qualified; the task backend pins its task to core 1.
- ROM Search, Read, Match and Skip paths are implemented. DS18B20-like Convert T,
  Read/Write Scratchpad and Read Power Supply paths are implemented, but this is
  not a complete DS18B20 emulation. Copy Scratchpad is not persistent, conversion
  timing/polling is incomplete, and alarm search and recall are not implemented.
- No dynamic sensor source, multiple virtual devices, overdrive or parasite power.
- Pin options accepted by ESPHome's general pin schema are not all honored by
  the raw GPIO setup. Use a plain, non-inverted pin with the external pull-up.
- The ISR busy-waits through slots/reset detection; coexistence and timing
  reliability under background load remain unresolved.
- `timing_scale` defaults to 1 (normal speed). Larger values are bench diagnostics,
  require a correspondingly scaled master, and are not standard-speed 1-Wire.
  The accompanying GPIO-master option exists only to reproduce those diagnostics.

## Verification scope

The original development branch compiled both the normal-speed ESP-IDF ISR
fixture and the Arduino scaled-task fixture. A saved wired bring-up report
records 64 valid temperature reads with no CRC/reset/slot errors in that run.
That is a bring-up result, not long-duration qualification.

Subsequent two-hour fixed-scratchpad diagnostic captures under load failed for
both tested GPIO and GPTimer experiments. The GPIO capture recorded 64,668 valid
transactions, 86 presence failures, 6,607 CRC failures, 6 data mismatches and
1,042 bus-timing failures. The GPTimer experiment is not included in this source.
Those captures are evidence of unresolved transport reliability, not a claim
that this exact restored source has passed or failed a new qualification run.

The Python tests exercise Python validation/encoding helpers, not the C++ timing
backend. The component fixtures cover compilation, not physical timing. Keep the
upstream submission a draft until the protocol and intended background-load
acceptance gates have passed and user documentation is ready.
