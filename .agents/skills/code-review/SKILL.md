---
name: code-review
description: Review guidance for ESPHome pull requests. Use this when reviewing a pull request that changes ESPHome Python, C++, or component code, to check it against the project's coding conventions, embedded-systems memory rules, testing requirements, and breaking-change policy.
---

# Reviewing ESPHome pull requests

ESPHome parses YAML into C++ firmware for memory-constrained microcontrollers
(ESP32, ESP8266, RP2040, LibreTiny). Review changes with that in mind: RAM and
flash are scarce, and code runs unattended for months.

`AGENTS.md` in the repository root is the contributor guide, and the skills it
lists (`component-codegen`, `config-validation`, `automations`,
`embedded-memory`, `component-tests`, `breaking-changes`) hold the detail. They
are the authority when they disagree with this summary. The developer
documentation at https://developers.esphome.io explains the component lifecycle
and the reasoning behind these rules. This skill lists the concrete things worth
flagging in a review; read the matching skill for the detail behind any item.

Only raise findings that the diff actually introduces or changes. Do not ask for
drive-by cleanup of pre-existing code the PR did not touch.

For error paths that fail without telling the user (early returns, swallowed
errors, ignored results), the `silent-failure-review` skill has its own checklist.

## Scope: shared code does not ride a component PR

Check the file list first. A PR titled for one component (`[ld2450] ...`) must
not also change code other components build on: `esphome/core/`, the entity
platform components (`esphome/components/sensor/`, `esphome/components/text_sensor/`
including its `filter.*`, `binary_sensor/`, `number/`, ...), the buses
(`esphome/components/{i2c,spi,uart}/`), `network/`, `socket/`, the shared
constants (`esphome/components/const/`, `esphome/const.py`),
`esphome/automation.py`, `esphome/cpp_generator.py` or `esphome/config_validation.py`.
Flag it and ask for the shared change to move into its own PR, which lands first
and is reviewed by that code's owners; the component PR then builds on it. This
holds even when the shared change is small or only the component uses it today.
The exception is a shared change that must update several components in the same
PR so none of them breaks in between (a `[core]` or `[components]` PR that adds a
helper and moves its callers at once).

Not shared code: the component's own platform files (`esphome/components/<c>/sensor.py`,
`text_sensor/__init__.py`, ...), its new `USE_*` lines in `esphome/core/defines.h`,
and its `CODEOWNERS` entry.

## YAML language freeze

Until configurations move to yaml.rocks (esphome/backlog#201), flag any PR that adds to the YAML language: a new tag
in `esphome/yaml_util.py`, new behaviour in the `packages` or `substitutions` components (a new entry in the Jinja
`SAFE_GLOBALS` in `substitutions/jinja.py`, or a custom Jinja filter, counts), or new merge, override or remove
semantics (`esphome/config_helpers.py`, `!extend`, `!remove`). Bug fixes to the existing features are fine. This
covers how YAML files are read, included and combined, not a component's own configuration options.

## Memory and embedded constraints (highest value)

Heap allocation after `setup()` is treated as a reliability bug, not a
performance nit, because it fragments a small shared heap. Flag:

- New heap allocation on a hot path or after setup that could be avoided.
- `std::vector` where the size is known at compile time (use `std::array`, or
  `StaticVector<T, N>` when a `push_back` API is needed) or fixed at runtime
  init (use `FixedVector<T>`).
- Listener / child-entity registration lists stored as `std::vector`; these have
  a compile-time-known count and should use `cg.slot_list()` plus
  `SLOT_LIST` (or `cg.slot_counter()` plus `StaticVector`).
- `std::vector<uint8_t>` for a byte buffer that never grows: prefer
  `std::unique_ptr<uint8_t[]>` or `std::array`.
- `std::map` / `std::set` / `std::unordered_map` for small datasets (1-16
  elements): a `std::vector` of a small struct with linear search is lighter.
- `std::deque` anywhere: it allocates 512-byte blocks and should be avoided.
- `std::string` storing a value set once from config: prefer `StringRef` (the
  literal already lives in flash).
- `std::string` / `std::to_string` / string-returning helpers on hot paths where
  a buffer or view API exists.
- A `set_timeout` / `set_interval` / `defer` lambda whose captures exceed 8 bytes
  on 32-bit or are not trivially copyable (`this` plus two 4-byte values, `this`
  plus a member function pointer, a `std::string`, a `std::function`):
  `std::function` then heap-allocates each time it is scheduled. Flag it on
  retries, re-armed timeouts and hot-path `defer`; a `set_interval` set up once
  in `setup()` is a single allocation. `[this]` plus one 4-byte value fits.

## C++ conventions

- Include what you use: a file referencing a symbol must include the header that
  declares it, even if it currently arrives transitively. New or changed uses of
  a symbol need the matching include.
- Prefix all member access with `this->`.
- Naming: `lower_snake_case` for functions/methods/variables, `UpperCamelCase`
  for classes/structs/enums, `UPPER_SNAKE_CASE` for namespace-scope constants,
  trailing underscore on protected/private fields.
- `enum class` values must be prefixed with the enum name in `UPPER_SNAKE_CASE`
  (e.g. `UARTFlushResult::UART_FLUSH_RESULT_SUCCESS`). Bare names like `SUCCESS`,
  `FAIL`, or `OK` collide with SDK macros on some platforms and break the build.
- Prefer `const`/`enum` over `#define`; `#define` is only for conditional
  compilation and code-generation sizes.
- Never call `millis()` in a `loop()` body; use
  `App.get_loop_component_start_time()`. A rate-limit gate below ~16 ms (the loop
  period) does nothing.
- Pick the timing primitive by cadence: gated `loop()` under 250 ms,
  `set_interval` at 500 ms and above.
- Do not override a base method to return the value it already returns (e.g.
  `get_setup_priority()` returning `setup_priority::DATA`).
- Wrap string literals passed as printf `%s` args in `LOG_STR_LITERAL()`.
- Required, invariant dependencies should be constructor parameters, not setters.
- Callback registration methods must be templated (`template<typename F>`), not
  typed as `std::function`, so lightweight forwarders avoid a heap allocation.
- Two-space indent, `using` over `typedef`, wrap at 120 columns.

## Use the existing helper

New components often hand-write code a helper already covers. Ask for the
helper; `AGENTS.md` section 4 and the `component-codegen` skill have the detail.

| Seen in the PR | Ask for |
| --- | --- |
| `if (this->x_sensor_ != nullptr) this->x_sensor_->publish_state(v);` | `PUBLISH_STATE(this->x_sensor_, v);` (`esphome/core/entity_base.h`) |
| set warning / `return` / clear warning around a bus result | `if (!this->status_comm_result(ok)) return;` |
| set warning / `return` around an earlier step of a multi-step poll | `if (!this->status_comm_check(ok)) return;` |
| warning set, but no `status_comm_result` or `status_clear_warning` on the success path | the last step uses `status_comm_result(ok)`, or the warning never clears |
| hand-written `set_x_sensor()` plus `x_sensor_` member | `SUB_SENSOR(x)`, `SUB_BINARY_SENSOR`, `SUB_TEXT_SENSOR`, `SUB_SWITCH`, ... |
| `if CONF_X in config: s = await sensor.new_sensor(config[CONF_X])` plus setter | `sensors = sensor.sub_sensors(config)` then `await sensors(CONF_X, hub.set_x_sensor)` |
| hand-written `{"a": MyEnum.MY_ENUM_A, ...}` dict | `MyEnum.options("a", "b", prefix="MY_ENUM_")` |
| loops on `available()` / `read_byte()` | UART `read_available(on_byte)`, `discard_available()`, `LineReader<N>`, `FrameReader<N, HEADER...>`, `LengthFrameReader<N, FrameFormat<...>...>` |
| hand-rolled CRC, checksum or bit loop | `crc8`, `crc16`, `xor_bytes`, `sum16_bytes`, `encode_uint16`, ... (`esphome/core/helpers.h`) |
| integer split into buffer bytes by hand (`buf[0] = v >> 8; buf[1] = v;`) | `put_be16`/`24`/`32`, `put_le16`, `put_le32` (`esphome/core/helpers.h`) |
| bare `mark_failed()`, or `ESP_LOGE` then `mark_failed()` | `this->mark_failed(LOG_STR("reason"))` or `LOG_STR(ESP_LOG_MSG_COMM_FAIL)` |
| button/switch/number/select/text C++ class that only calls the hub | `ApplyButton`, `ApplySwitch`, `ApplyNumber`, `ApplySelect`, `ApplyText` with `apply=` |
| action or condition class that only forwards to the parent | `register_apply_action` / `register_apply_condition` |
| enum-to-string `switch` returning literals | `PROGMEM_STRING_TABLE` with `get_log_str` / `get_progmem_str` |
| `if (x := config.get(CONF_X)) is not None: cg.add(var.set_x(x))` | `cg.add_optional(var.set_x, config.get(CONF_X))`, when the setter takes the value unchanged |
| `x = (x & ~mask) \| (on ? mask : 0)` or a bit-reversal loop | `assign_bits(x, mask, on)`, `reverse_bits(x)` (`esphome/core/helpers.h`) |
| `x < lo ? lo : (x > hi ? hi : x)` | `std::clamp(x, lo, hi)` |
| I2C read, mask, write by hand; private 16-bit register byte helpers | `update_register`, `update_register16`, `read_byte_reg16`, `write_byte_reg16` |
| custom validator that checks which keys are set, or one schema with model checks | `cv.has_exactly_one_key`, `cv.has_at_most_one_key`, `cv.Inclusive`/`cv.Exclusive`, `cv.typed_schema` per model |
| validator that turns a time period into one of a few allowed values | `cv.time_period_enum({10: Rate.RATE_10MS, ...})` (`unit="us"` for microseconds) |
| `unit_of_measurement=`, `device_class=` and `state_class=` written out in `sensor_schema` | `sensor.sensor_schema(kind=sensor.TEMPERATURE)` (or another preset), plus only what it leaves unset (`state_class=` and `accuracy_decimals=` may override) |
| `pin->setup(); pin->digital_write(level);`, often inside `if (pin != nullptr)` | `setup_output_pin(pin, level)` (`esphome/core/gpio_pin.h`), null-safe |
| write level, `delayMicroseconds(n)`, write the opposite level | `pulse_pin(pin, level, n)`; width 0 for back-to-back writes |
| reset pin setup, then low, `delay`, high, `delay` | `pulse_reset(pin, {.high_before_ms = ..., .low_ms = ..., .high_after_ms = ...})`, null-safe |
| `if (cond) this->high_freq_.start(); else this->high_freq_.stop();` | `this->high_freq_.set(cond)` |
| `b >> 4`, `b & 0x0F`, `(hi << 4) \| lo` for 4-bit fields and 12-bit values | `high_nibble`, `low_nibble`, `encode_uint12` (`esphome/core/helpers.h`) |
| `#ifdef USE_ESP8266` around appending a flash string to a buffer | `buf_append_str(buf, size, pos, "...")` / `buf_append_str_p` on every platform |
| `remote_base` protocol whose data is plain fields, with hand-written binary sensor and action registration | `register_simple_protocol(name, protocol, schema, *fields)` |
| buffer sized from the config and allocated in `setup()` | `cg.static_buffer` (`component-codegen` skill) |
| hub keeping a list of child objects that each take a value, or per-child listener classes | `cg.HubFunction` publishers (`component-codegen` skill) |


## Cleanups automated reviews keep missing

Multi-pass cleanup reviews of contributor PRs find these again and again, and single-pass reviews
almost never flag them. Look for each one explicitly; the matching `AGENTS.md` section 4 entry has the
detail.

- **Copies within the PR** (the most common finding): two functions, branches, platform files or tests
  that differ in a constant or two. Ask for one helper, loop or table.
- **State that copies other state:** a `write_pending_` bool beside a timer (use the return value of
  `cancel_timeout(ID)`), a `has_x_` bool beside `x_` (use `NAN` or a sentinel), an enum that can be worked
  out from stored fields.
- **Dead guards:** null checks on members codegen always sets, `is_failed()` checks in `loop()`, range
  checks the schema already enforces, `default:` in a `switch` over every value of an `enum class` the
  component sets itself (not one cast from bus data), calls that set a member to its default
  (`set_cs_pin(nullptr)`).
- **Member layout:** field order that leaves padding, `int`/`uint32_t` for values that fit a `uint8_t`,
  an `enum class` without `: uint8_t`, several `bool` members that could share a byte. Each costs RAM per
  instance.
- **C++ work that belongs in codegen:** scaling, clamping, checksums or register bytes computed at boot
  from config values; ask for the plain value from `to_code`.
- **Two `switch` ladders for one mapping:** encode and decode of the same value pairs should share one
  `static constexpr` table (`PROGMEM` on ESP8266).
- **Compile-time constants:** `static const` or `#define` with a constant value (use `static constexpr`),
  constant tables not in `PROGMEM`, unused constants.
- **Code for one model or feature compiled into every build:** ask for a `cg.add_define()` emitted only
  when the config uses it, with the runtime check inside the `#ifdef`.
- **Failure handling:** `mark_failed()` outside `setup()` (runtime failures set a warning that the next
  success clears), an error enum that only picks a log message.
- **Logging** that does not follow https://developers.esphome.io/architecture/logging/: `ESP_LOGD` on every
  reading, "Setting up" or "initialized" lines in `setup()`, a `dump_config()` made of single-line
  `ESP_LOGCONFIG` calls where related lines fit one call, a name helper that returns `const char *` instead
  of `const LogString *`.
- **Leaf classes** that are not `final`.
- **Unused helpers, includes and imports** left behind; lint does not catch most of them.
- **Comments that restate the code** or tell its history: a block comment that walks through the next
  ten lines, a docstring that repeats the signature, a derivation that a one-line note plus a datasheet
  section would cover.
- **Tests:** near-identical test functions that differ only in inputs (ask for `pytest.mark.parametrize`
  or a table-driven gtest), tests that repeat another test, test YAML that sets options to their defaults.

## Python conventions

- Type-annotate every new function signature (params and return), new dataclass
  fields, and new module-level variables. Import `ConfigType` from
  `esphome.types`.
- Use the walrus operator to avoid a double lookup, e.g.
  `if (blah := config.get(CONF_BLAH)) is not None:`.
- Reuse existing validators from `config_validation.py` (`cv.rename_key`,
  `cv.has_exactly_one_key`, etc.) via `cv.All(...)` instead of hand-rolling.
- A new or changed validator function that wraps, chooses or builds a schema
  must be decorated with `@schema_extractor(...)` and handle `SCHEMA_EXTRACT`,
  or `script/build_language_schema.py` silently drops the keys behind it (see
  the `config-validation` skill).
- `esphome/const.py` is frozen: no new `CONF_` constants there. Define them in
  the component's own `.py`, or in `esphome/components/const/__init__.py` when
  shared. The same constant defined in three or more component files fails CI.
- State that must persist during code generation goes in `CORE.data` namespaced
  under the component `DOMAIN` (a `@dataclass`), not module-level mutable globals.
- Prefer callback-based triggers via `build_callback_automation()`; only use a
  `Trigger<Ts...>` subclass when the forwarder needs mutable state.

## Testing and coverage

- New and changed lines and branches need test coverage, including defensive
  early-returns, error paths, and no-op guards. A mocked-out function is not
  covered; exercise the real call path too.
- Component YAML tests live in `tests/components/<component>/`. Never define
  buses (uart, i2c, spi, modbus) directly in a test file: pull them from
  `tests/test_build_components/common/` through dict-style `packages:` so CI can
  group builds. List-style packages or top-level merge keys block grouping.
- Config-only checks use the `validate.*.yaml` prefix; compiled checks use
  `test.*.yaml`.
- A change to device behaviour (a driver, a bus, a platform, a fix for a fault
  seen on a device) needs a hardware test report on a head that includes the
  last change to device code. Either form is fine:
  - the `Automated testing results` comment from the `hardware-test` skill, or
  - a manual report from the author with: the commit tested; the board, chip
    and framework; the steps taken, including how the changed path was
    triggered; what happened, with the log lines that show it; and, for a fix,
    the same steps on a `dev` build showing the fault.

  Without either, ask for one and list those items, since an author without AI
  tools tests by hand. "Compiles and runs on my board" does not show the
  changed path ran; for a fix whose trigger is rare, ask how it can be forced
  (a simulated device, an injected error, an AT command, a pulled wire, or the
  skill's recipes). If the author has no board, note that the change is
  untested on hardware instead of blocking. Validation, codegen-only and
  documentation changes need no board.

## Naming user-facing functions

Documented lambda methods, actions and config keys are public API, so a bad name
is hard to fix once released. Flag:

- A boolean where `true` means off (`display_off(true)`): name the positive
  state, e.g. `set_power(bool on)`.
- A name that differs from a sibling component's name for the same operation.

Before a release, rename outright; after, follow the breaking-change policy
below.

## Breaking changes and public API

- Base classes under `esphome/core/` and documented config options are public
  API. Undocumented `public` members of a component are internal.
- A breaking change needs justification, a migration path in the PR description,
  and a deprecation window where feasible (`ESPDEPRECATED` in C++,
  `cv.rename_key(..., removed_in=...)` in Python). Changing a codegen-injected
  lambda signature is not a breaking change.

## Process and PR hygiene

- PR titles start with a `[tag]` prefix: the component name (e.g. `[uart] ...`)
  or `[core]` for shared code.
- Prose in docs, comments, and commit messages should be plain English. Keep
  inline comments short and only where the code is not self-explanatory; do not
  restate what the code says.
- Verify the PR fills out `.github/PULL_REQUEST_TEMPLATE.md` and adds
  `CODEOWNERS` entries for a new component.
