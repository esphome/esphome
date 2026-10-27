# ESPHome AI Collaboration Guide

This document provides essential context for AI models interacting with this project. Adhering to these guidelines will ensure consistency and maintain code quality.

## 1. Project Overview & Purpose

*   **Primary Goal:** ESPHome is a system to configure microcontrollers (like ESP32, ESP8266, RP2040, and LibreTiny-based chips) using simple yet powerful YAML configuration files. It generates C++ firmware that can be compiled and flashed to these devices, allowing users to control them remotely through home automation systems.
*   **Business Domain:** Internet of Things (IoT), Home Automation.

## 2. Core Technologies & Stack

*   **Languages:** Python (>=3.12), C++ (gnu++20)
*   **Frameworks & Runtimes:** PlatformIO, Arduino, ESP-IDF.
*   **Build Systems:** PlatformIO is the primary build system. CMake is used as an alternative.
*   **Configuration:** YAML.
*   **Key Libraries/Dependencies:**
    *   **Python:** `voluptuous` (for configuration validation), `PyYAML` (for parsing configuration files), `paho-mqtt` (for MQTT communication), `aioesphomeapi` (for the native API).
    *   **C++:** `ArduinoJson` (for JSON serialization/deserialization), `AsyncMqttClient-esphome` (for MQTT), `ESPAsyncWebServer` (for the web server).
*   **Package Manager(s):** `pip` (for Python dependencies), `platformio` (for C++/PlatformIO dependencies).
*   **Communication Protocols:** Protobuf (for native API), MQTT, HTTP.

## 3. Architectural Patterns

*   **Overall Architecture:** The project follows a code-generation architecture. The Python code parses user-defined YAML configuration files and generates C++ source code. This C++ code is then compiled and flashed to the target microcontroller using PlatformIO.

*   **Directory Structure Philosophy:**
    *   `/esphome`: Contains the core Python source code for the ESPHome application.
    *   `/esphome/components`: Contains the individual components that can be used in ESPHome configurations. Each component is a self-contained unit with its own C++ and Python code.
    *   `/tests`: Contains all unit and integration tests for the Python code.
    *   `/docker`: Contains Docker-related files for building and running ESPHome in a container.
    *   `/script`: Contains helper scripts for development and maintenance.

*   **Core Architectural Components:**
    1.  **Configuration System** (`esphome/config*.py`): Handles YAML parsing and validation using Voluptuous, schema definitions, and multi-platform configurations.
    2.  **Code Generation** (`esphome/codegen.py`, `esphome/cpp_generator.py`): Manages Python to C++ code generation, template processing, and build flag management.
    3.  **Component System** (`esphome/components/`): Contains modular hardware and software components with platform-specific implementations and dependency management.
    4.  **Core Framework** (`esphome/core/`): Manages the application lifecycle, hardware abstraction, and component registration.

*   **Platform Support:**
    1.  **ESP32** (`components/esp32/`): Espressif ESP32 family. Supports multiple variants (Original, C2, C3, C5, C6, H2, P4, S2, S3) with ESP-IDF framework. Arduino framework supports only a subset of the variants (Original, C3, S2, S3).
    2.  **ESP8266** (`components/esp8266/`): Espressif ESP8266. Arduino framework only, with memory constraints.
    3.  **RP2040** (`components/rp2040/`): Raspberry Pi Pico/RP2040. Arduino framework with PIO (Programmable I/O) support.
    4.  **LibreTiny** (`components/libretiny/`): Realtek and Beken chips. Supports multiple chip families and auto-generated components.

## 4. Coding Conventions & Style Guide

**Read the developer documentation before writing a component.** https://developers.esphome.io covers the
component lifecycle, the main loop, and the reasoning behind the rules below in far more depth than this
file does, and it is the authority when they disagree. The most useful starting points:

*   https://developers.esphome.io/architecture/components/ - component lifecycle, `setup()`, `loop()`,
    setup priorities, and how a component is registered.
*   https://developers.esphome.io/architecture/components/advanced/ - choosing between `loop()`,
    `set_interval`, `set_timeout` and `defer`; waking the loop from another thread; the RAM cost of each.
*   https://developers.esphome.io/contributing/code/ - contribution rules, public API and breaking changes.

*   **Skills:** detailed recipes live in skills under `.agents/skills/` (also reachable as `.claude/skills/` and
    `.github/skills/`). Load the matching skill before doing that kind of work; agents without skill support
    should read the `SKILL.md` file directly.
    *   `component-codegen`: component files and metadata, `to_code`, hub child entities, codegen helpers,
        `CORE.data` state.
    *   `config-validation`: schemas, validators, `CONF_` constants, and keeping schema extraction working.
    *   `automations`: triggers, actions, conditions and callback registration.
    *   `embedded-memory`: container and string choices that avoid heap use after `setup()`.
    *   `component-tests`: YAML build tests, bus packages and CI grouping, C++ unit tests, integration tests.
    *   `breaking-changes`: what is public API, deprecation patterns and migration notes.
    *   `pr-workflow`, `code-review`, `hardware-test`, `display-test`: creating PRs, reviewing them, and testing
        on real boards and panels.

*   **Formatting:**
    *   **Python:** Uses `ruff` for linting and formatting. Configuration is in `pyproject.toml`.
    *   **C++:** Uses `clang-format` for formatting. Configuration is in `.clang-format`.

*   **Naming Conventions:**
    *   **Python:** Follows PEP 8. Use clear, descriptive names following snake_case.
    *   **C++:** Follows the Google C++ Style Guide with these specifics (following clang-tidy conventions):
        - Function, method, and variable names: `lower_snake_case`
        - Class/struct/enum names: `UpperCamelCase`
        - Top-level constants (global/namespace scope): `UPPER_SNAKE_CASE`
        - Function-local constants: `lower_snake_case`
        - Protected/private fields: `lower_snake_case_with_trailing_underscore_`
        - Favor descriptive names over abbreviations
        - Device components are named brand plus model (`siemens_ira211`), without a protocol-family prefix
        - Enumerator names: prefix every value of an `enum class` with the enum name converted to
          `UPPER_SNAKE_CASE` (e.g. `UARTFlushResult::UART_FLUSH_RESULT_SUCCESS`). Never use bare
          names like `SUCCESS`, `FAILURE`, `OK`, or `FAIL`: platform SDK headers define macros with
          these common names (for example the Realtek SDKs used by LibreTiny define
          `#define SUCCESS 0` in `basic_types.h`), and the preprocessor replaces the enumerator
          before the compiler sees it, breaking the build and clang-tidy on those platforms.

*   **Python Idioms:**
    *   **Assignment expressions (PEP 572):** Prefer the walrus operator (`:=`) wherever it removes a redundant lookup or a throwaway temporary. The most common case in component code is presence-checking a config key and then indexing it separately — fetch once with `.get()` and bind in the condition instead:
        ```python
        # Bad - looks up CONF_BLAH twice
        if CONF_BLAH in config:
            cg.add(var.set_blah_ms(config[CONF_BLAH].total_milliseconds))

        # Good - single lookup, value bound inline
        if (blah := config.get(CONF_BLAH)) is not None:
            cg.add(var.set_blah_ms(blah.total_milliseconds))
        ```
        The same applies to `while` loops and comprehensions where it avoids recomputing a value. Don't contort code to use it — reach for `:=` only when it genuinely cuts repetition or an extra assignment line.
        When the setter takes the value unchanged, use `cg.add_optional(var.set_blah, config.get(CONF_BLAH))`
        instead; it is not awaited, and it skips only `None`, so `0` and `False` are still set.

*   **C++ Field Visibility:**
    *   **Prefer `protected`:** Use `protected` for most class fields to enable extensibility and testing. Fields should be `lower_snake_case_with_trailing_underscore_`.
    *   **Use `private` for safety-critical cases:** Use `private` visibility when direct field access could introduce bugs or violate invariants:
        1. **Pointer lifetime issues:** When setters validate and store pointers from known lists to prevent dangling references.
           ```cpp
           // Helper to find matching string in vector and return its pointer
           inline const char *vector_find(const std::vector<const char *> &vec, const char *value) {
             for (const char *item : vec) {
               if (strcmp(item, value) == 0)
                 return item;
             }
             return nullptr;
           }

           class ClimateDevice {
            public:
             void set_custom_fan_modes(std::initializer_list<const char *> modes) {
               this->custom_fan_modes_ = modes;
               this->active_custom_fan_mode_ = nullptr;  // Reset when modes change
             }
             bool set_custom_fan_mode(const char *mode) {
               // Find mode in supported list and store that pointer (not the input pointer)
               const char *validated_mode = vector_find(this->custom_fan_modes_, mode);
               if (validated_mode != nullptr) {
                 this->active_custom_fan_mode_ = validated_mode;
                 return true;
               }
               return false;
             }
            private:
             std::vector<const char *> custom_fan_modes_;  // Pointers to string literals in flash
             const char *active_custom_fan_mode_{nullptr};  // Must point to entry in custom_fan_modes_
           };
           ```
        2. **Invariant coupling:** When multiple fields must remain synchronized to prevent buffer overflows or data corruption.
           ```cpp
           class Buffer {
            public:
             void resize(size_t new_size) {
               auto new_data = std::make_unique<uint8_t[]>(new_size);
               if (this->data_) {
                 std::memcpy(new_data.get(), this->data_.get(), std::min(this->size_, new_size));
               }
               this->data_ = std::move(new_data);
               this->size_ = new_size;  // Must stay in sync with data_
             }
            private:
             std::unique_ptr<uint8_t[]> data_;
             size_t size_{0};  // Must match allocated size of data_
           };
           ```
        3. **Resource management:** When setters perform cleanup or registration operations that derived classes might skip.
    *   **Provide `protected` accessor methods:** When derived classes need controlled access to `private` members.

*   **C++ Preprocessor Directives:**
    *   **Avoid `#define` for constants:** Using `#define` for constants is discouraged and should be replaced with `const` variables or enums.
    *   **A constant with a compile-time initializer is `static constexpr`,** not `static const`, at namespace scope
        and inside functions; `static const` is only for a value computed at run time. A function-local
        `static constexpr` table is a static constant to clang-tidy, so it is `UPPER_SNAKE_CASE` too.
    *   **Use `#define` only for:**
        - Conditional compilation (`#ifdef`, `#ifndef`)
        - Compile-time sizes calculated during Python code generation (e.g., configuring `std::array` or `StaticVector` dimensions via `cg.add_define()`)

*   **C++ Additional Conventions:**
    *   **Member access:** Prefix all class member access with `this->` (e.g., `this->value_` not `value_`)
    *   **Indentation:** Use spaces (two per indentation level), not tabs
    *   **Type aliases:** Prefer `using type_t = int;` over `typedef int type_t;`
    *   **Line length:** Wrap lines at no more than 120 characters
    *   **Timing in `loop()`:** Never call `millis()` in a `loop()` body. The current tick's timestamp is
        already cached - use `App.get_loop_component_start_time()` (from `esphome/core/application.h`).
        Only reach for `millis()` when you genuinely need sub-tick resolution inside a long operation.
    *   **The main loop runs every 16 ms.** A rate-limit gate shorter than that does nothing: the check
        passes on essentially every pass of the loop, so it costs a comparison and buys nothing. Pick an
        interval comfortably coarser than 16 ms, or drop the gate entirely and accept running every loop.
        ```cpp
        // Bad - a 10ms gate against a 16ms loop never holds anything back
        static constexpr uint32_t POLL_INTERVAL_MS = 10;
        const uint32_t now = millis();
        if (now - this->last_poll_ < POLL_INTERVAL_MS)
          return;
        this->last_poll_ = now;
        ```
        ```cpp
        // Good - an interval that actually rate limits, off the cached timestamp
        static constexpr uint32_t POLL_INTERVAL_MS = 100;
        const uint32_t now = App.get_loop_component_start_time();
        if (now - this->last_poll_ < POLL_INTERVAL_MS)
          return;
        this->last_poll_ = now;
        ```
        Pick the primitive by cadence: under 250 ms use a gated `loop()`; 500 ms and above use
        `set_interval`. Full reasoning, including why `set_interval` costs more below 500 ms:
        https://developers.esphome.io/architecture/components/advanced/#quick-rule-of-thumb
    *   **Repeating work:** use `set_interval`, never a `set_timeout` that re-arms itself; every re-arm
        rebuilds the callback and re-queues a scheduler item, and the period drifts.
    *   **Scheduler ids:** name a timer only when it must be cancelled or replaced, and use a
        `static constexpr uint32_t` id, never a string. Ids are per component instance and cannot clash with other
        components, so number them from 0 and keep all of a component's ids together in one place.
        ```cpp
        static constexpr uint32_t READ_TIMEOUT_ID = 0;
        this->set_timeout(READ_TIMEOUT_ID, 50, [this]() { this->read_(); });
        ```
    *   **Don't override a default with the same value:** if a base class method already returns what you
        want, do not override it. `Component::get_setup_priority()` returns `setup_priority::DATA`, so a
        component that wants `DATA` should simply leave it alone.
        ```cpp
        // Bad - this is exactly what the base class already does
        float get_setup_priority() const override { return setup_priority::DATA; }
        ```
    *   **Constant data lives in RAM on ESP8266** unless marked `PROGMEM`: `const` tables, class-scope
        constant arrays, and in-class `std::array` initializers (a `.rodata` template plus a copy). Mark the
        table `PROGMEM` and read it with `progmem_memcpy()` or `progmem_read_byte()` (`esphome/core/hal.h`);
        for a table of structs copy the whole entry out and return the copy, never a pointer into flash.
        On other platforms they are a plain `memcpy` and dereference, so no `#ifdef` is needed. Declare log tags with
        `ESPHOME_LOG_TAG(TAG, "name")`, pass status text as `LOG_STR`, e.g.
        `this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL))`. An action that only forwards a byte payload is
        registered with `automation.ApplyBytes` (see Actions below), which keeps a constant payload in flash;
        `TEMPLATABLE_BYTES` is for a `play()` that does more with the bytes.
    *   **Fixed sets of names** (status words, model names): a `PROGMEM_STRING_TABLE` (`esphome/core/progmem.h`),
        logged with `get_log_str` and `LOG_STR_ARG`, published on a text sensor with
        `publish_state(Table::get_progmem_str(idx, fallback))`, which copies it out of flash on ESP8266.
        A `dump_config` helper that picks a name returns `const LogString *` from `LOG_STR(...)`.
    *   **CRCs and bit tricks** come from `esphome/core/helpers.h` (`crc8`, `crc16`, `xor_bytes`, `sum16_bytes`,
        `reverse_bits`, `encode_uint16`, `encode_value_le`, `sign_extend<BITS>`, `bcd_to_uint8`, `assign_bits`); pick
        the helper's arguments instead of writing a loop. 7-byte BCD RTC calendars use `time::decode_bcd_calendar<LAYOUT>`
        and `encode_bcd_calendar<LAYOUT>` with `EPSON_BCD_CALENDAR`, `DS_BCD_CALENDAR` or `PCF_BCD_CALENDAR`.
    *   **Bus helpers:** UART drains with `read_available(on_byte)` and `discard_available()`, text lines with
        `LineReader<N>`, fixed-length binary frames with `FrameReader<N, HEADER...>`; I2C has `update_register`,
        `write_registers` (PROGMEM `I2CRegValue` table), `check_chip_id` and `read_bytes_16_le`.
        `if (!this->status_comm_result(ok)) return;` sets or clears the warning. A child sensor that skips repeated
        values is `SUB_SENSOR_WITH_DEDUP(name, type)`.
    *   **An unset float is `NAN`**, not `optional<float>`; the optional costs 4 to 8 bytes of padding per member.
    *   **`PollingComponent` first runs `update()`** after a random share of the interval; call `this->update()` at the
        end of `setup()` when the entities need a state at boot.
    *   **A multi-step command sequence** leaves the step pending when the answer is malformed, so the existing
        resend and give-up logic handles it; never advance on an answer that was only logged.
    *   **A global feature define** says the code is compiled in, not that this instance uses it: keep the
        per-instance check (`if (this->aux_device_ == ...)`) inside the `#ifdef`.
    *   **Model-specific code** sits behind a define the Python side emits only for configs that use that model
        (`cg.add_define("USE_SELEC_METER_EM4M")`), so the other models' builds leave it out; the runtime model check
        stays inside the `#ifdef`.
    *   **Failures after `setup()`:** `mark_failed()` is for `setup()` only. A runtime read or write failure
        sets `status_set_warning()` and the next success clears it; writes skip work when `is_failed()`.
    *   **`dump_config()` already prints the failure reason:** for a failed component, core prints
        `<component> is marked FAILED: <reason>` right after the component's `dump_config()`, using the reason
        passed to `mark_failed()`. So `dump_config()` never checks `is_failed()` to log a failure, and no
        `error_code_` member exists only to pick that message. Pass the reason where setup fails instead:
        `this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL))` for a failed bus transfer, a short specific reason
        otherwise (`LOG_STR("Wrong chip ID")`). Core also logs it at that moment, so no `ESP_LOGE` repeats it
        before `mark_failed()`; keep a separate line only when it prints a value read at run time, such as the
        chip ID that did not match. Only the first reason is kept, so do not call `status_set_error()` before
        `mark_failed()` and do not mark the component failed again after a helper already did.
    *   **Fixed platform facts compile to nothing:** gate on `#ifdef USE_ESP8266` and friends, not on a
        runtime check of something the build already knows.
    *   **Logging string literals:** wrap literals passed as `%s` arguments in `LOG_STR_LITERAL()` so they
        can be stored in flash rather than RAM.
        ```cpp
        // Bad
        ESP_LOGV(TAG, "Key %u %s", key, pressed ? "pressed" : "released");

        // Good
        ESP_LOGV(TAG, "Key %u %s", key, pressed ? LOG_STR_LITERAL("pressed") : LOG_STR_LITERAL("released"));
        ```
    *   **Constructor parameters vs setters:** Component properties that are both **required** and **invariant**
        (never change after construction) should be constructor parameters rather than set via setter methods.
        This makes the dependency explicit and prevents use of the object in an incompletely-initialized state.
        In code generation, when calling `cg.new_Pvariable()` or the relevant helper function to create the component, pass these as arguments.
        ```cpp
        // Good - required invariant dependency as constructor parameter
        class SourceTextSensor : public text_sensor::TextSensor, public Component {
         public:
          explicit SourceTextSensor(text::Text *source) : source_(source) {}
         protected:
          text::Text *source_;
        };
        ```
        ```cpp
        // Bad - required invariant dependency as setter
        class SourceTextSensor : public text_sensor::TextSensor, public Component {
         public:
          void set_source(text::Text *source) { this->source_ = source; }
         protected:
          text::Text *source_{nullptr};
        };
        ```

*   **Component Code Generation:** every component's `__init__.py` defines `DOMAIN` (its own name, checked by
    CI). Hub entities go in platform files (`sensor.py`, ...) that take the hub's id, not under the hub's own
    key. File layout, metadata, the `to_code` pattern, `sub_sensors` and the other codegen helpers are in the
    `component-codegen` skill.

*   **Automations (Triggers, Actions, Conditions):** prefer `build_callback_automation()` over a `Trigger`
    class, and `register_apply_action` / `register_apply_condition` over a C++ class that only forwards to its
    parent. Callback registration methods are always `template<typename F>`, never `std::function`. Details
    and examples are in the `automations` skill.

*   **Type Hints:** Type-hint all function signatures, including test functions and config validators (e.g. `def validate_x(config: ConfigType) -> ConfigType:`, `def test_x() -> None:`). Import `ConfigType` from `esphome.types`.
    An intentionally empty body (an unused `register_dumper` builder, for example) gets a one-line docstring, not `pass`.

*   **Configuration Validation:** follow the `config-validation` skill. In short: one `cv.typed_schema` per
    model, reuse validators from `config_validation.py`, and never add `CONF_` constants to the frozen
    `esphome/const.py`.
    *   **Schema extraction:** whenever config validation code is added or changed, make sure
        `script/build_language_schema.py` still extracts the full schema. The dumper cannot see inside a plain
        function, so a validator function that wraps, chooses or builds a schema must be decorated with
        `@schema_extractor(...)` and return the schema data when called with `SCHEMA_EXTRACT` (both from
        `esphome.schema_extractors`). CI does not catch a missing schema; build the schema and check the
        component's JSON as the `config-validation` skill describes.

## 5. Key Files & Entrypoints

*   **Main Entrypoint(s):** `esphome/__main__.py` is the main entrypoint for the ESPHome command-line interface.
*   **Configuration:**
    *   `pyproject.toml`: Defines the Python project metadata and dependencies.
    *   `platformio.ini`: Configures the PlatformIO build environments for different microcontrollers.
    *   `.pre-commit-config.yaml`: Configures the lint and format hooks, run by `prek`.
*   **CI/CD Pipeline:** Defined in `.github/workflows`.
*   **Static Analysis & Development:**
    *   `esphome/core/defines.h`: A comprehensive header file containing all `#define` directives that can be added by components using `cg.add_define()` in Python. This file is used exclusively for development, static analysis tools, and CI testing - it is not used during runtime compilation. When developing components that add new defines, they must be added to this file to ensure proper IDE support and static analysis coverage. The file includes feature flags, build configurations, and platform-specific defines that help static analyzers understand the complete codebase without needing to compile for specific platforms.

## 6. Development & Testing Workflow

*   **Local Development Environment:** Use the provided Docker container or create a Python virtual environment and install dependencies from `requirements_dev.txt`.
*   **Running Commands:** Use the `script/run-in-env.py` script to execute commands within the project's virtual environment. For example, to run the linter: `python3 script/run-in-env.py prek run`.
*   **Testing:**
    *   **Python:** Run unit tests with `pytest`.
    *   **C++:** Use `clang-tidy` for static analysis.
    *   **Component tests:** YAML build tests live in `tests/components/<component>/` and run with
        `script/test_build_components -c <component>`. Never define buses directly in test YAML: include the
        shared packages from `tests/test_build_components/common/` with dict-style `packages:`. Test file naming,
        `validate.*.yaml` config-only tests, CI grouping, C++ unit tests and integration tests are in the
        `component-tests` skill.

*   **Debugging and Troubleshooting:**
    *   **Debug Tools:**
        - `esphome config <file>.yaml` to validate configuration.
        - `esphome compile <file>.yaml` to compile without uploading.
        - Use component-specific debug logging.
    *   **Common Issues:**
        - **Import Errors**: Check component dependencies and `PYTHONPATH`.
        - **Validation Errors**: Review configuration schema definitions.
        - **Build Errors**: Check platform compatibility and library versions.
        - **Runtime Errors**: Review generated C++ code and component logic.

## 7. Specific Instructions for AI Collaboration

*   **Contribution Workflow (Pull Request Process):** the `pr-workflow` skill walks through these steps.
    1.  **Fork & Branch:** Create a new branch based on the `dev` branch (always use `git checkout -b <branch-name> dev` to ensure you're branching from `dev`, not the currently checked out branch).
    2.  **Make Changes:** Adhere to all coding conventions and patterns.
    3.  **Test:** Create component tests for all supported platforms and run the full test suite locally.
    4.  **Lint:** Run `prek` to ensure code is compliant.
    5.  **Commit:** Commit your changes. There is no strict format for commit messages.
    6.  **Pull Request:** Submit a PR against the `dev` branch. The Pull Request title must start with a `[tag]` prefix. For component work, use the component name (e.g., `[display] Fix bug`, `[abc123] Add new component`); for changes to shared/core code that isn't tied to a single component, use `[core]` (e.g., `[core] Add validator`). Update documentation, examples, and add `CODEOWNERS` entries as needed. Pull requests should always be made using the `.github/PULL_REQUEST_TEMPLATE.md` template - fill out all sections completely without removing any parts of the template.
    7.  **Comments:** When commenting on GitHub PRs or issues, don't tag contributors, especially bots. Avoid referring to list items (e.g. from reviews) with the form #nn - this will be interpreted by GitHub as a reference to issue or PR nn. Keep comments short and exclude irrelevant details, backstories, restatement of previous comments and anything that is already obvious to the reader.

*   **Documentation Contributions:**
    *   Documentation is hosted in the separate `esphome/esphome.io` repository.
    *   The contribution workflow is the same as for the codebase.
    *   When editing a component's documentation page, also update the corresponding component index page to ensure both pages remain in sync.

*   **Best Practices:**
    *   **Component Development:** Keep dependencies minimal, provide clear error messages, and write comprehensive docstrings and tests.
    *   **Code Generation:** Generate minimal and efficient C++ code. Validate all user inputs thoroughly. Support multiple platform variations.
    *   **Configuration Design:** Aim for simplicity with sensible defaults, while allowing for advanced customization.
    *   **Embedded Systems Optimization:** ESPHome targets resource-constrained microcontrollers. Be mindful of flash size and RAM usage.

        **Heap allocation after `setup()` should be avoided unless absolutely unavoidable.** ESP devices run for
        months with small shared heaps, and repeated allocations fragment them until a large allocation fails; we
        have seen field crashes caused by this. ESPHome treats runtime heap allocation as a long-term reliability
        bug, not a performance issue. Prefer `std::array`, `StaticVector` (sized with `cg.slot_counter()` for
        registration lists), `FixedVector` and `StringRef` over growing `std::vector`, `std::map`, `std::deque`
        and `std::string`. The `embedded-memory` skill has the full container guidelines.

    *   **State Management:** keep code generation state in `CORE.data`, namespaced under the component's
        `DOMAIN` in a `@dataclass`, never in module-level mutable globals (see the `component-codegen` skill).

*   **Security:** Be mindful of security when making changes to the API, web server, or any other network-related code. Do not hardcode secrets or keys.

*   **Dependencies & Build System Integration:**
    *   **Python:** When adding a new Python dependency, add it to the appropriate `requirements*.txt` file and `pyproject.toml`.
    *   **C++ / PlatformIO:** When adding a new C++ dependency, add it to `platformio.ini` and use `cg.add_library`.
    *   **Build Flags:** Use `cg.add_build_flag(...)` to add compiler flags.

## 8. Public API and Breaking Changes

*   Only features documented at [esphome.io](https://esphome.io) are public API for components; all `public`
    members of `esphome/core/` and base classes are public API. Aim for a 6-month deprecation window
    (`ESPDEPRECATED` in C++, `cv.rename_key(..., removed_in=...)` for config keys) and document the migration
    path in the PR description. The `breaking-changes` skill has the full policy, checklist and patterns.

## 9. English Language

The project uses English for non-code content. When drafting documentation, code comments, commit messages,
PR descriptions, and similar text, avoid technical jargon. Instead, express concepts in plain English,
using standard technical terms only when required. Ensure the text is readily comprehensible to a wide
audience, including non-native English speakers.

## 10. Code Comments

Code comments on individual lines should be used only where necessary to flag issues that may not be obvious
on a simple reading of the code. Keep them short (e.g. 1 or 2 lines).

Function and method comment blocks may include more detail as required to make
calling contracts clear and document parameter usage, but should still be kept concise.

Avoid redundancy and repetition; comments should never simply restate what the code already says.
