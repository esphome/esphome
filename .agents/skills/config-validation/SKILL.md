---
name: config-validation
description: Write or change ESPHome configuration validation - CONFIG_SCHEMA, cv.Schema, cv.typed_schema, custom validator functions, config keys and CONF_ constants, renamed or deprecated keys, and the language schema that script/build_language_schema.py extracts from them. Use whenever a change touches a component's schema or a validator in config_validation.py, automation.py or a component's .py files.
---

# Configuration validation

ESPHome validates YAML with `voluptuous` schemas built from `esphome/config_validation.py` (`cv`). The
same schemas are also walked by `script/build_language_schema.py`, which produces the JSON language schema
that editors and the dashboard use for completion and checking. A validation change is only finished when
both still work.

## Writing schemas

- **Type hints:** validators are typed like any other function: `def validate_x(config: ConfigType) ->
  ConfigType:`, with `ConfigType` imported from `esphome.types`.
- **One schema per model:** when a component's options depend on a `model` key, use
  `cv.typed_schema({...}, key=CONF_MODEL, default_type=...)` with each model's own keys and defaults, not
  one schema plus a validator that rejects keys and fills defaults afterwards.
- **Reuse existing validators:** before writing a custom validator, check for an existing one in
  `config_validation.py` and compose it in `cv.All(...)` rather than duplicating logic across components.
  For example, rename a config key with `cv.rename_key(CONF_OLD, CONF_NEW, removed_in="2026.6.0")`, and
  reject mutually-exclusive keys with `cv.has_at_most_one_key(...)` / `cv.has_exactly_one_key(...)`. See
  how `api` composes `cv.has_exactly_one_key` + `cv.rename_key`.
- **Common validators:** `cv.int_`, `cv.float_`, `cv.string`, `cv.boolean`, `cv.int_range(min=0,
  max=100)`, `cv.positive_int`, `cv.percentage`.
- **Complex validation:** `cv.All(cv.string, cv.Length(min=1, max=50))`, `cv.Any(cv.int_, cv.string)`.
- **Platform-specific:** `cv.only_on(["esp32", "esp8266"])`, `esp32.only_on_variant(...)`,
  `cv.only_on_esp32`, `cv.only_on_esp8266`, `cv.only_on_rp2040`.
- **Framework-specific:** `cv.only_with_framework(...)`, `cv.only_with_arduino`. On the ESP32, Arduino is
  built as a component of ESP-IDF, so an ESP-IDF API, sdkconfig option or version floor is there on both
  frameworks: gate on `esp32.require_idf_version(cv.Version(5, 5, 3), "psram_dma")`, which checks the
  platform and the ESP-IDF under either framework, as `spi`, `usb_host` and `wifi` do. Reserve
  `cv.only_with_framework("esp-idf")` for a build that has been tried on Arduino and fails there, and write
  the actual failure next to it (`sendspin`: arduino-esp32 below ESP-IDF 6.0 bundles a second libsodium,
  and from 6.0 sendspin-cpp's enumerators collide with `Arduino.h` macros such as `DISPLAY`).
- **Schema extensions:**
  ```python
  CONFIG_SCHEMA = cv.Schema({ ... })
   .extend(cv.COMPONENT_SCHEMA)
   .extend(uart.UART_DEVICE_SCHEMA)
   .extend(i2c.i2c_device_schema(0x48))
   .extend(spi.spi_device_schema(cs_pin_required=True))
  ```
- **Constants:** `esphome/const.py` is frozen; do not add new `CONF_` constants there. Define a
  component-local constant in the component's own `.py`; for a constant shared by multiple components, add
  it to `esphome/components/const/__init__.py`. CI (`lint_constants_usage`) fails if the same constant is
  defined in three or more component files. Constants used in core files (those not under
  `esphome/components`) may be added to `esphome/const.py` but will require adjustment to the CI check.
- **Renamed or removed keys:** see the `breaking-changes` skill for `cv.rename_key` and manual
  deprecation warnings.
- **Config-only tests:** schema edge cases and deprecated-syntax migrations go in `validate.*.yaml` test
  files (see the `component-tests` skill).

## Schema extraction

`script/build_language_schema.py` sets `esphome.schema_extractors.EnableSchemaExtraction = True` before it
imports any component, then walks every `CONFIG_SCHEMA` (and platform schema) recursively. It understands
`cv.Schema`, `cv.All`, `cv.Any`, dicts of `cv.Required` / `cv.Optional` keys, `.extend()`, `cv.ensure_list`,
`cv.typed_schema`, registries and the known `cv` validators. **A plain function it cannot see into**: a
custom validator that builds or picks a schema at validation time hides every key behind it, and the
dumper silently emits nothing for them. CI only runs the script with `--check`, which does not compare the
output, so a lost schema is not caught.

When a validator function wraps, chooses or builds a schema (rather than checking a single value), make it
visible to the dumper:

1. Decorate it with `@schema_extractor("<type>")` from `esphome.schema_extractors`.
2. At the top of the function, return the data for that type when it is called with `SCHEMA_EXTRACT`
   instead of a config value. Compare with `is` (`value is SCHEMA_EXTRACT`); older code uses `==`.

```python
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor


@schema_extractor("schema")
def _outgoing_connection_schema(config: ConfigType | None) -> ConfigType:
    if config is SCHEMA_EXTRACT:
        return _OUTGOING_CONNECTION_SCHEMA
    if config is None:
        config = {}
    return _OUTGOING_CONNECTION_SCHEMA(config)
```

The type names the dumper accepts, and what `SCHEMA_EXTRACT` must return for each, are in the
`hidden_schemas` branch of `convert()` in `script/build_language_schema.py`. The common ones:

| Type | Return for `SCHEMA_EXTRACT` | Example |
|------|-----------------------------|---------|
| `"schema"` | the schema (or a representative one) to walk | `api` `_outgoing_connection_schema`, `restore_state_schema` in `esphome/core/entity_helpers.py`, `model_schema_extractor` in `esphome/components/mipi/__init__.py` |
| `"enum"` | the dict of options | `sensor_entity_category` in `esphome/components/sensor/__init__.py` |
| `"one_of"` | the list of options | `cv.one_of` |
| `"maybe"` | `(validator, key)` for a scalar shorthand | `cv.maybe_simple_value`, `maybe_conf` in `esphome/automation.py` |
| `"templatable"` | the inner validator | `cv.templatable` |
| `"automation"` | the automation schema | `automation.validate_automation` |

Points to watch:

- The dumper matches the function object by `repr`, so the decorated function must be the exact object
  placed in the schema. In a factory that returns a new closure per call, decorate the inner function (as
  `restore_state_schema` and `cv.maybe_simple_value` do), not the factory.
- The decorator only records anything when extraction is enabled, and that flag is set before imports, so
  the decorator must run at import time (module level or inside a factory called at module level).
- A schema chosen per config value (for example by `model`) has no single answer: return a representative
  schema, as `model_schema_extractor` does.
- Plain value checks (`cv.All(cv.string, my_check)`) need nothing: the dumper still sees the surrounding
  types and keys.
- `cv.Schema`, `cv.typed_schema`, `cv.ensure_list` and the `cv` helpers are already handled; prefer
  composing them over a custom function, which also avoids the extractor work.

### Checking the output

Build the language schema into a scratch directory and look at the component's JSON (it takes about ten
seconds):

```bash
python3 script/build_language_schema.py --output-path /tmp/schema
python3 -c "import json; print(json.dumps(json.load(open('/tmp/schema/my_component.json')), indent=1))" | less
```

Every key a user can write should appear under `config_vars` with a sensible `type`; compare against the
same output from `dev` when changing an existing schema. If the script prints `Unknown extracted schema
type` or raises, the decorator type or the value returned for `SCHEMA_EXTRACT` is wrong. Unit tests for
the dumper live in `tests/script/test_build_language_schema.py`.
