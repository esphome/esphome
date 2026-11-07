---
name: code-review
description: Review guidance for ESPHome pull requests. Use this when reviewing a pull request that changes ESPHome Python, C++, or component code, to check it against the project's coding conventions, embedded-systems memory rules, testing requirements, and breaking-change policy.
---

# Reviewing ESPHome pull requests

ESPHome parses YAML into C++ firmware for memory-constrained microcontrollers
(ESP32, ESP8266, RP2040, LibreTiny). Review changes with that in mind: RAM and
flash are scarce, and code runs unattended for months.

`AGENTS.md` in the repository root is the full contributor guide and the
authority when it disagrees with this summary. The developer documentation at
https://developers.esphome.io explains the component lifecycle and the reasoning
behind these rules. This skill lists the concrete things worth flagging in a
review; read `AGENTS.md` for the detail behind any item.

Only raise findings that the diff actually introduces or changes. Do not ask for
drive-by cleanup of pre-existing code the PR did not touch.

## Memory and embedded constraints (highest value)

Heap allocation after `setup()` is treated as a reliability bug, not a
performance nit, because it fragments a small shared heap. Flag:

- New heap allocation on a hot path or after setup that could be avoided.
- `std::vector` where the size is known at compile time (use `std::array`, or
  `StaticVector<T, N>` when a `push_back` API is needed) or fixed at runtime
  init (use `FixedVector<T>`).
- Listener / child-entity registration lists stored as `std::vector`; these have
  a compile-time-known count and should use `cg.slot_counter()` plus
  `StaticVector`.
- `std::vector<uint8_t>` for a byte buffer that never grows: prefer
  `std::unique_ptr<uint8_t[]>` or `std::array`.
- `std::map` / `std::set` / `std::unordered_map` for small datasets (1-16
  elements): a `std::vector` of a small struct with linear search is lighter.
- `std::deque` anywhere: it allocates 512-byte blocks and should be avoided.
- `std::string` storing a value set once from config: prefer `StringRef` (the
  literal already lives in flash).
- `std::string` / `std::to_string` / string-returning helpers on hot paths where
  a buffer or view API exists.

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

## Python conventions

- Type-annotate every new function signature (params and return), new dataclass
  fields, and new module-level variables. Import `ConfigType` from
  `esphome.types`.
- Use the walrus operator to avoid a double lookup, e.g.
  `if (blah := config.get(CONF_BLAH)) is not None:`.
- Reuse existing validators from `config_validation.py` (`cv.rename_key`,
  `cv.has_exactly_one_key`, etc.) via `cv.All(...)` instead of hand-rolling.
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
