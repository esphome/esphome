# 1-Wire model bench fixtures

These fixtures are intentionally outside grouped component builds. They exercise
one emulated endpoint at a time, not several virtual devices on one DQ pin.

## Native protocol check

Compile the actual GPIO-independent C++ model with the test program:

```sh
g++ -std=c++20 -Wall -Wextra -Werror -I . \
  tests/hardware/one_wire_device/protocol_test.cpp \
  esphome/components/one_wire_device/device_model.cpp -o /path/to/model-test
/path/to/model-test
```

This covers thermometer encodings/CRC, signed DS18S20 interpolation and alarms,
Read ROM followed by a function, ready polling, RAM Copy/Recall, ROM-only
isolation, and DS2413 read/write status plus invalid-complement rejection. It is
not an electrical timing test.

## Physical bench

The checked-in controller fixtures describe the development rig: classic ESP32,
DS2482-800 at I2C address 0x18, SDA GPIO21/SCL GPIO22, emulator on bridge channel 2.
Change those assignments for another bench. The emulator uses GPIO4 and a shared
ground, with DQ pulled up to 3.3 V. Never join two push-pull outputs or 5 V rails.
Identify each board by esptool MAC and USB path immediately before uploading;
serial ordinals are not identity. Back up the installed images first.

- `thermometer.yaml`: use `device_type` substitutions DS18B20, DS1822 or DS18S20.
  Give each compiled variant a distinct `node_name`. The ordinary template
  sensor is an explicitly synthetic input: 23.5, -10.125, 85, NaN, and 9.5 C,
  each lasting 15 seconds. It follows the foreground value-evaluation and quantization path.
- `controller-temperature.yaml`: unchanged stock Dallas reader behind the bridge.
  Capture both endpoints for 100 seconds. Require all four valid values, correct
  CRCs, no unexpected resets, no fabricated reads during the NaN interval, and
  valid reads again after recovery. Missing-presence warnings are expected only
  while the source is unavailable, not elsewhere in the capture.
- `identity.yaml` and `controller-raw.yaml`: DS2401 normal/legacy ROM reads must
  agree with discovery; DS2413 read status must have a valid complemented nibble.
- `digital.yaml` and `controller-raw.yaml`: test low output latches, independent
  input levels, repeated writes, and rejection of a bad complement without
  changing the latches. Capture at least 35 seconds and require both input states.
- `controller-protocol.yaml`: the thermometer Read ROM/function chain and signed
  alarm decision, through the bridge's normal public API. Run with a thermometer
  fixture for 40 seconds, including the negative input stage.
- `final-bench.yaml`: fixed **test** temperature and historical bench ROM for
  restoring ordinary operation with the rig's original controller firmware.

## Generic value interface

`computed-values.yaml` feeds two ordinary template sensors and a mutable global
into the configured `value` lambda. With the stock Dallas controller, capture
65 seconds: compare each decoded reading against an independent sum of the
logged X, Y and offset inputs, require all five valid results, reject unexpected
CRC/reset errors, and verify the NaN interval suppresses data until recovery.

`named-values.yaml` and `controller-values.yaml` exercise DS2413 channels
independently. Alternate explicit recomputation of named bindings with generic
`set_value` actions; require all four A/B combinations and correlate each wire
status to the separately logged source fields. Merely seeing two changing bits
is not sufficient: swapping the channel assignments must fail the check.

The configuration CLI tests in `tests/component_tests/one_wire_device/test_values.py`
reject unsupported channel names, ambiguous multi-value actions, identity-only
value assignments and invalid digital literals. The protocol model itself is
unchanged by this interface extension.

Use the ESPHome checkout containing the candidate for compile, upload and logs.
Require a verified flash hash, the `one-wire-models-v3` startup marker and the
fixture's device-type marker before interpreting any capture. Save complete logs
from both endpoints, source/firmware identity, duration and parsed outcomes.

The temperature profiles deliberately test unavailable input; do not classify
those expected warnings as transport failures or hide failures outside that
window. A successful firmware build or native test does not establish physical
behavior. These bounded serial/bridge checks are not Wi-Fi-load or long-duration
production qualification. Copy/Recall remain RAM-only; overdrive, parasite power
and multiple virtual devices per output are unsupported.
