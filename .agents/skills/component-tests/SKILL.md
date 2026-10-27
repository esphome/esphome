---
name: component-tests
description: Write and run tests for ESPHome components - YAML build tests (test.*.yaml, validate.*.yaml, common.yaml) under tests/components, shared bus packages and CI test grouping, C++ gtest unit tests, and host integration tests including the uart_mock fixture. Use when adding or changing files under tests/, or when a code change needs test coverage.
---

# Component tests

## C++ unit tests

gtests live in `tests/components/<component>/*.cpp` and run with `script/cpp_unit_test.py <component>`; see
`tests/components/README.md`. Set the loop clock in a test with `LoopBlockingGuard{nullptr, nullptr,
now_ms}`; the runner resets it before every test.

## Integration tests

`tests/integration/` runs real firmware on the host. Serial components prove timeouts and recovery with the
`uart_mock` fixture (`tests/integration/test_uart_mock_*.py`). Its `responses` match on content every time
they see it; to answer a repeated request differently, use an `on_tx` lambda with a static counter and
`inject_to_rx_buffer` (see `uart_mock_ld2412_requery_after_giveup.yaml`).

## YAML build tests

```
tests/
├── test_build_components/
│   └── common/          # Shared bus packages (uart, i2c, spi, etc.)
│       ├── uart/        # UART at default baud rate
│       ├── uart_115200/ # UART at 115200 baud
│       ├── i2c/         # I2C bus
│       └── spi/         # SPI bus
└── components/[component]/
    ├── common.yaml          # Component-only config (no bus definitions)
    ├── test.esp32-idf.yaml      # config + compile
    ├── test.esp8266-ard.yaml    # config + compile
    ├── test-variant.esp32-idf.yaml  # variant test, config + compile
    ├── validate.esp32-idf.yaml      # config-only (never compiled)
    └── validate-legacy.esp32-idf.yaml  # config-only variant
```
Run them with `script/test_build_components`. Use `-c <component>` to test specific components and
`-t <target>` for specific platforms.

- **Keep build-test variants few:** one `test.<platform>.yaml` per platform the code actually differs on.
  A variant per board for platform-identical code only adds CI time.
- **Config-only test files (`validate.*.yaml`):** use this prefix when a YAML file only needs to exercise
  schema/validation paths and does not need to be compiled. CI runs `validate.*.yaml` files with
  `esphome config` only and skips them during compile. The grammar mirrors `test.*.yaml`:
  - `validate.<platform>.yaml`: base config-only test
  - `validate-<variant>.<platform>.yaml`: config-only variant

  Use this for deprecated-syntax migration tests, schema edge cases, or platform-specific validation
  branches where building firmware adds no signal. A component may have any mix of `test.*.yaml` and
  `validate.*.yaml` files. Validate files never participate in bus grouping; each one runs as its own
  `esphome config` invocation.

  When a PR's only edits to a component are `validate.*.yaml` files (no source changes, no `test.*.yaml`
  changes, and the component isn't pulled in as a dependency of another changed component), CI skips the
  compile stage for that component and only runs config validation. This is decided in
  `script/determine-jobs.py` via `_component_change_is_validate_only` and surfaced as the
  `validate_only_components` output that the `test-build-components-split` job consumes.

## Grouping with packages

Components that use shared bus packages can be grouped together in CI to reduce build count. **Never define
buses (uart, i2c, spi, modbus) directly in test YAML files**; always use packages from
`test_build_components/common/`.

All includes in test files must go through dict-style `packages:` so that batch grouping works; the
grouping scripts only understand dict-style packages. Never use list-style packages
(`packages: [- !include ...]`) or top-level merge keys (`<<: !include common.yaml`). Bus packages are keyed
by the bus name; the component's `common.yaml` is keyed by the component name (e.g.
`cst328: !include common.yaml`):
```yaml
# test.esp32-idf.yaml - everything included via named packages
packages:
  uart: !include ../../test_build_components/common/uart_115200/esp32-idf.yaml
  my_component: !include common.yaml
```
```yaml
# common.yaml - component config only, NO bus definitions
my_component:
  id: my_instance

sensor:
  - platform: my_component
    name: My Sensor
```
Components that define buses directly are flagged as "NEEDS MIGRATION" and cannot be grouped, increasing
CI build time. `!extend` and `!remove` in a `test.*.yaml` also make the component ungroupable; add a
standalone entity with its own id instead.

A test that needs a shared component in a particular state (`esp32_ble` security settings, an option that
PSRAM rejects) cannot be grouped: the merged block wins and the whole batch fails. Add the component to
`ISOLATED_COMPONENTS` in `script/analyze_component_buses.py`, or move the option to a `test-<variant>` file.

To verify that all components can be tested together without ID conflicts or configuration issues:
```bash
./script/test_component_grouping.py -e config --all
```
This tests all components in a single build to catch conflicts that might not appear when testing
components individually. Use `-e config` for fast configuration validation, or `-e compile` for full
compilation testing.
