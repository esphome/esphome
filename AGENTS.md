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
        logged with `get_log_str` and `LOG_STR_ARG`, copied out with `ESPHOME_strncpy_P` before `publish_state`.
        A `dump_config` helper that picks a name returns `const LogString *` from `LOG_STR(...)`.
    *   **CRCs and bit tricks** come from `esphome/core/helpers.h` (`crc8`, `crc16`, `xor_bytes`, `sum16_bytes`,
        `reverse_bits`, `encode_uint16`, `encode_value_le`, `sign_extend<BITS>`, `bcd_to_uint8`, `assign_bits`); pick
        the helper's arguments instead of writing a loop. 7-byte BCD RTC calendars use `time::decode_bcd_calendar<LAYOUT>`
        and `encode_bcd_calendar<LAYOUT>` with `EPSON_BCD_CALENDAR`, `DS_BCD_CALENDAR` or `PCF_BCD_CALENDAR`.
    *   **Bus helpers:** UART drains with `read_available(on_byte)` and `discard_available()`, text lines with
        `LineReader<N>`; I2C has `update_register`, `write_registers` (PROGMEM `I2CRegValue` table), `check_chip_id`
        and `read_bytes_16_le`. `if (!this->status_comm_result(ok)) return;` sets or clears the warning.
        A child sensor that skips repeated values is `SUB_SENSOR_WITH_DEDUP(name, type)`.
    *   **An unset float is `NAN`**, not `optional<float>`; the optional costs 4 to 8 bytes of padding per member.
    *   **`PollingComponent` first runs `update()`** after a random share of the interval; call `this->update()` at the
        end of `setup()` when the entities need a state at boot.
    *   **A multi-step command sequence** leaves the step pending when the answer is malformed, so the existing
        resend and give-up logic handles it; never advance on an answer that was only logged.
    *   **Entities that each watch a register field:** one class taking register, shift, mask and match from a Python
        tuple table, in typed `StaticVector`s sized by `cg.slot_counter`; not a template per bit, not a virtual
        listener base (a second vptr per entity).
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

*   **Component Structure:**
    *   **Standard Files:**
        ```
        components/[component_name]/
        ├── __init__.py          # Component configuration schema and code generation
        ├── [component].h        # C++ header file (if needed)
        ├── [component].cpp      # C++ implementation (if needed)
        └── [platform]/          # Platform-specific implementations
            ├── __init__.py      # Platform-specific configuration
            ├── [platform].h     # Platform C++ header
            └── [platform].cpp   # Platform C++ implementation
        ```

    *   **Component Metadata:**
        - `DOMAIN`: Required in every component's `__init__.py`, set to the component's own name as a plain
          string (e.g. `DOMAIN = "my_component"`). Other code refers to the component through it, e.g.
          `from esphome.components.my_component import DOMAIN`. Keep it in alphabetical order with the other
          metadata constants. CI (`lint_component_domain`) fails without it.
        - `DEPENDENCIES`: List of required components
        - `AUTO_LOAD`: Components to automatically load. Not a way to put entities under the component's
          own key; see "Entities of a hub" below.
        - `CONFLICTS_WITH`: Incompatible components
        - `CODEOWNERS`: GitHub usernames responsible for maintenance
        - `MULTI_CONF`: Whether multiple instances are allowed

*   **Code Generation & Common Patterns:**
    *   **Configuration Schema Pattern:**
        ```python
        import esphome.codegen as cg
        import esphome.config_validation as cv
        from esphome.const import CONF_KEY, CONF_ID

        DOMAIN = "my_component"
        CONF_PARAM = "param"  # A constant that does not yet exist in esphome/const.py

        my_component_ns = cg.esphome_ns.namespace("my_component")
        MyComponent = my_component_ns.class_("MyComponent", cg.Component)

        CONFIG_SCHEMA = cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(MyComponent),
                cv.Required(CONF_KEY): cv.string,
                cv.Optional(CONF_PARAM, default=42): cv.int_,
            }
        ).extend(cv.COMPONENT_SCHEMA)


        async def to_code(config):
            var = cg.new_Pvariable(config[CONF_ID])
            await cg.register_component(var, config)
            cg.add(var.set_key(config[CONF_KEY]))
            cg.add(var.set_param(config[CONF_PARAM]))
        ```

    *   **C++ Class Pattern:**
        ```cpp
        namespace esphome::my_component {

        class MyComponent : public Component {
         public:
          void setup() override;
          void loop() override;
          void dump_config() override;

          void set_key(const std::string &key) { this->key_ = key; }
          void set_param(int param) { this->param_ = param; }

         protected:
          std::string key_;
          int param_{0};
        };

        }  // namespace esphome::my_component
        ```

    *   **Common Component Examples:**
        - **Sensor:**
          ```python
          from esphome.components import sensor

          CONFIG_SCHEMA = sensor.sensor_schema(MySensor).extend(
              cv.polling_component_schema("60s")
          )


          async def to_code(config):
              var = await sensor.new_sensor(config)
              await cg.register_component(var, config)
          ```

        - **Binary Sensor:**
          ```python
          from esphome.components import binary_sensor

          CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend({...})


          async def to_code(config):
              var = await binary_sensor.new_binary_sensor(config)
          ```

        - **Switch:**
          ```python
          from esphome.components import switch

          CONFIG_SCHEMA = switch.switch_schema().extend({...})


          async def to_code(config):
              var = await switch.new_switch(config)
          ```

        - **Entities of a hub:** a new component's entities go in platform files (`sensor.py`,
          `binary_sensor.py`, ...) that take the hub's id, never under the hub's own key with their platforms
          added to `AUTO_LOAD` to make that work. Auto loading a platform because the C++ uses its type is
          fine. See https://developers.esphome.io/architecture/components/#exposing-multiple-entities.
          ```yaml
          # Bad - entities under the hub's key
          my_hub:
            connected:
              name: Connected

          # Good - the hub, then one platform entry that holds its entities
          my_hub:
            id: hub1
          binary_sensor:
            - platform: my_hub
              my_hub_id: hub1  # optional with a single hub
              connected:
                name: Connected
          ```

        - **Optional child entities of a hub:** bind the config once with `sensor.sub_sensors(config)` (or
          `sub_binary_sensors`, `sub_text_sensors`, `sub_buttons`, `sub_switches`, `sub_numbers`,
          `sub_selects` in their domains), adding `parent=hub` for entities that derive from `Parented<T>`,
          then make one call per key, even when there is only one. A call creates the entity only when its key
          is configured, passes it to the setter and returns it (or `None`); extra arguments such as
          `min_value` or `options` go on the call. Always name the setter explicitly on the object that owns
          it, never with `getattr` and an f-string, and keep that variable short (`var` for the component
          itself, `hub` for one fetched with `cg.get_variable`) so the calls fit on one line. Loops whose
          setter also takes an index, such as `set_gate_threshold(x, n)`, stay as they are.
          ```python
          # my_hub/sensor.py
          async def to_code(config):
              hub = await cg.get_variable(config[CONF_MY_HUB_ID])
              sensors = sensor.sub_sensors(config)
              await sensors(CONF_TEMPERATURE, hub.set_temperature_sensor)
              await sensors(CONF_HUMIDITY, hub.set_humidity_sensor)


          # my_hub/button.py
          async def to_code(config):
              hub = await cg.get_variable(config[CONF_MY_HUB_ID])
              buttons = button.sub_buttons(config, parent=hub)
              await buttons(CONF_RESTART, hub.set_restart_button)
          ```

        - **Pins and referenced components:** set them with `await cg.add_pin(var.set_cs_pin, config[CONF_CS_PIN])`
          and `await cg.add_variable(var.set_bus, config[CONF_BUS_ID])`; both skip a `None` value, so pass
          `config.get(KEY)` for an optional key, and return what they created. Use `cg.register_parented` for `set_parent`.

        - **Hub forwarding entities need no C++ class:** a button, switch, number or select that only calls its hub
          uses `button.ApplyButton` (`switch.ApplySwitch`, `number.ApplyNumber`, `select.ApplySelect`) in its schema
          and `apply=` on the `sub_*` call. The body is a call on the hub with `entity` and `state`, `value` or
          `index`/`option` in scope; on a switch, number or select `publish="before"` or `"after"` publishes the state.
          ```python
          switches = switch.sub_switches(config, parent=hub)
          await switches(CONF_BLUETOOTH, None, apply="set_bluetooth(state)", publish="after")
          ```

        - **Other codegen helpers:** `uart.new_uart_component(config, *ctor_args)` (and `i2c.new_i2c_component`,
          `spi.new_spi_component`, `modbus.new_modbus_client_component`, `ble_client.new_ble_node_component`)
          constructs, registers and attaches in one call; `display.add_writer(var, config)` sets the lambda;
          `MyEnum.options("on", prefix="MY_ENUM_")` builds an enum dict; `button.restart_button_schema(cls)` (and
          `factory_reset_`, `query_`) are button presets; `fv.get_declaring_config(id)` finds a hub's config in
          final validation.

*   **Automations (Triggers, Actions, Conditions):**

    Automations have three building blocks: **Triggers** (fire when something happens), **Actions** (do something), and **Conditions** (check if something is true).

    *   **Triggers -- Callback method (preferred):**

        Use `build_callback_automation()` for simple triggers. This eliminates the need for a C++ Trigger class by using a lightweight pointer-sized forwarder struct registered directly as a callback. No `CONF_TRIGGER_ID` in the schema.

        **Python:**
        ```python
        from esphome import automation

        CONFIG_SCHEMA = cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(MyComponent),
                cv.Optional(CONF_ON_STATE): automation.validate_automation({}),
            }
        ).extend(cv.COMPONENT_SCHEMA)


        async def to_code(config):
            var = cg.new_Pvariable(config[CONF_ID])
            await cg.register_component(var, config)
            for conf in config.get(CONF_ON_STATE, []):
                await automation.build_callback_automation(
                    var, "add_on_state_callback", [(bool, "x")], conf
                )
        ```

        `build_callback_automation` arguments: `parent`, `callback_method` (C++ method name), `args` (template args as `[(type, name)]` tuples), `config`, and optional `forwarder` (defaults to `TriggerForwarder<Ts...>`).

        For boolean filtering (e.g. `on_press`/`on_release`), use built-in forwarders with `args=[]`:
        ```python
        for conf_key, forwarder in (
            (CONF_ON_PRESS, automation.TriggerOnTrueForwarder),
            (CONF_ON_RELEASE, automation.TriggerOnFalseForwarder),
        ):
            for conf in config.get(conf_key, []):
                await automation.build_callback_automation(
                    var, "add_on_state_callback", [], conf, forwarder=forwarder
                )
        ```

        When the callback's parameters differ from the automation's arguments, or the trigger should only
        fire for some values, pass `params`, `forward` and `when`; the helper then generates a capture-less
        lambda (stored inline, nothing allocated). Name the parent with `automation.parent_ref(var)` and build
        `forward` from `MockObj` calls, not f-strings of C++:
        ```python
        parent = automation.parent_ref(var)
        index = cg.RawExpression("index")
        await automation.build_callback_automation(
            var,
            "add_on_state_callback",
            [(cg.StringRef, "x"), (cg.size_t, "i")],
            conf,
            params=[(cg.size_t, "index")],
            forward=[cg.StringRef(parent.option_at(index)), index],
        )
        ```
        `build_parent_callback_automation`, `build_trigger_callback`, `build_callback_automations` and
        `build_trigger_automations` cover the other shapes; see their docstrings in `esphome/automation.py`.

        **C++ -- no trigger class needed.** The callback registration method must be templatized to accept both `std::function` and lightweight forwarder structs (which avoid heap allocation):
        ```cpp
        class MyComponent : public Component {
         public:
          // Must be a template -- accepts both std::function and pointer-sized forwarder structs
          template<typename F> void add_on_state_callback(F &&callback) {
            this->state_callback_.add(std::forward<F>(callback));
          }
         protected:
          // Use CallbackManager when callbacks are always registered (e.g. core components)
          CallbackManager<void(bool)> state_callback_;
          // Use LazyCallbackManager when callbacks are often not registered -- saves 8 bytes
          // (nullptr vs empty std::vector) per instance when no callbacks are added
          // LazyCallbackManager<void(bool)> state_callback_;
        };
        ```

    *   **Triggers -- Trigger class method:**

        Use `build_automation()` with a `Trigger<Ts...>` subclass only when the forwarder needs **mutable state beyond a single `Automation*` pointer** (e.g. edge detection tracking previous state, timing logic).

        **Python:**
        ```python
        TurnOnTrigger = my_ns.class_("TurnOnTrigger", automation.Trigger.template())

        CONFIG_SCHEMA = cv.Schema(
            {
                cv.Optional(CONF_ON_TURN_ON): automation.validate_automation(
                    {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(TurnOnTrigger)}
                ),
            }
        )


        async def to_code(config):
            for conf in config.get(CONF_ON_TURN_ON, []):
                trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
                await automation.build_automation(trigger, [], conf)
        ```

        **C++:**
        ```cpp
        class TurnOnTrigger : public Trigger<> {
         public:
          explicit TurnOnTrigger(MyComponent *parent) : last_on_{false} {
            parent->add_on_state_callback([this](bool state) {
              if (state && !this->last_on_)
                this->trigger();
              this->last_on_ = state;
            });
          }
         protected:
          bool last_on_;
        };
        ```

    *   **Actions:**
        ```cpp
        template<typename... Ts> class MyAction : public Action<Ts...> {
         public:
          explicit MyAction(MyComponent *parent) : parent_(parent) {}
          void play(const Ts &...) override { this->parent_->do_something(); }
         protected:
          MyComponent *parent_;
        };
        ```
        Register it without writing a builder:
        ```python
        automation.register_simple_action(
            "my_component.do_something", MyAction, schema, synchronous=True
        )
        ```
        The constructor receives the object named by `config[CONF_ID]`. Use `register_bare_action` for a
        no-argument constructor, `register_parented_action` for a class deriving from `Parented<T>`, and
        the `@automation.register_action(...)` decorator only when the builder must also set fields.

        Use `synchronous=True` for actions that run to completion inside `play()` without deferring. Use `synchronous=False` if the action may suspend/defer execution (e.g. `delay`, `wait_until`, `script.wait`) or store trigger arguments for later use.

        **Actions that only forward templatable values to their parent need no C++ class.** Register them
        with `register_apply_action`; do not write a `TEMPLATABLE_VALUE` class or a builder for this shape.
        ```python
        automation.register_apply_action(
            "my_component.set_gains",
            schema,
            automation.ApplyField(CONF_KP, "set_kp", cg.float_),
            automation.ApplyField(CONF_KI, "set_ki", cg.float_),
        )
        ```
        The `ApplyField`, `ApplyCall` and `register_apply_action` docstrings in `esphome/automation.py` cover
        the rest; `cover.control` and `cover.template.publish` are in-tree examples. An action whose input is a
        byte payload passes `automation.ApplyBytes(CONF_DATA, "send({})")` in the same call; a constant payload
        stays in flash and a lambda's vector is forwarded as data and size. `TEMPLATABLE_VALUE` with
        `cg.templatable` stays for actions whose `play()` has real logic beyond forwarding values.

    *   **Conditions:**
        ```cpp
        template<typename... Ts> class MyCondition : public Condition<Ts...> {
         public:
          explicit MyCondition(MyComponent *parent) : parent_(parent) {}
          bool check(const Ts &...) override { return this->parent_->is_active(); }
         protected:
          MyComponent *parent_;
        };
        ```
        Register with `automation.register_simple_condition("my_component.is_active", MyCondition, schema)`;
        `register_bare_condition`, `register_parented_condition` and the decorator follow the action rules.

        **Conditions that only test their parent need no C++ class either.** Register them with
        `register_apply_condition`; the expression is applied to the parent, and an `ApplyCall` compares
        against config values.
        ```python
        automation.register_apply_condition("my_component.is_active", schema, "is_active()")
        automation.register_apply_condition(
            "my_component.state_is",
            schema,
            automation.ApplyCall("state == {}", ((CONF_STATE, cg.bool_),)),
        )
        ```
        `cover.is_open`, `rtttl.is_playing` and `component.is_idle` are in-tree examples.

*   **Type Hints:** Type-hint all function signatures, including test functions and config validators (e.g. `def validate_x(config: ConfigType) -> ConfigType:`, `def test_x() -> None:`). Import `ConfigType` from `esphome.types`.
    An intentionally empty body (an unused `register_dumper` builder, for example) gets a one-line docstring, not `pass`.

*   **Configuration Validation:**
    *   **One schema per model:** when a component's options depend on a `model` key, use `cv.typed_schema({...},
        key=CONF_MODEL, default_type=...)` with each model's own keys and defaults, not one schema plus a validator
        that rejects keys and fills defaults afterwards.
    *   **Reuse existing validators:** Before writing a custom validator, check for an existing one in `config_validation.py` and compose it in `cv.All(...)` rather than duplicating logic across components. For example, rename a config key with `cv.rename_key(CONF_OLD, CONF_NEW, removed_in="2026.6.0")`, and reject mutually-exclusive keys with `cv.has_at_most_one_key(...)` / `cv.has_exactly_one_key(...)`. See how `api` composes `cv.has_exactly_one_key` + `cv.rename_key`.
    *   **Common Validators:** `cv.int_`, `cv.float_`, `cv.string`, `cv.boolean`, `cv.int_range(min=0, max=100)`, `cv.positive_int`, `cv.percentage`.
    *   **Complex Validation:** `cv.All(cv.string, cv.Length(min=1, max=50))`, `cv.Any(cv.int_, cv.string)`.
    *   **Platform-Specific:** `cv.only_on(["esp32", "esp8266"])`, `esp32.only_on_variant(...)`, `cv.only_on_esp32`, `cv.only_on_esp8266`, `cv.only_on_rp2040`.
    *   **Framework-Specific:** `cv.only_with_framework(...)`, `cv.only_with_arduino`. On the ESP32, Arduino is
        built as a component of ESP-IDF, so an ESP-IDF API, sdkconfig option or version floor is there on both
        frameworks: gate on the platform and on `esp32.idf_version()` (the ESP-IDF under either framework), as
        `spi`'s `psram_dma` and `wifi`'s `ap_mode: wisp` do. Reserve `cv.only_with_framework("esp-idf")` for a
        build that has been tried on Arduino and fails there, and write the actual failure next to it (`sendspin`:
        arduino-esp32 below ESP-IDF 6.0 bundles a second libsodium, and from 6.0 sendspin-cpp's enumerators
        collide with `Arduino.h` macros such as `DISPLAY`).
    *   **Schema Extensions:**
        ```python
        CONFIG_SCHEMA = cv.Schema({ ... })
         .extend(cv.COMPONENT_SCHEMA)
         .extend(uart.UART_DEVICE_SCHEMA)
         .extend(i2c.i2c_device_schema(0x48))
         .extend(spi.spi_device_schema(cs_pin_required=True))
        ```
    *   **Constants:** `esphome/const.py` is frozen — do not add new `CONF_` constants there. Define a component-local constant in the component's own `.py` (as with `CONF_PARAM` above); for a constant shared by multiple components, add it to `esphome/components/const/__init__.py`. CI (`lint_constants_usage`) fails if the same constant is defined in three or more component files. Constants used in core files (i.e. those not under `esphome/components`) may be added to `esphome/const.py` but will require adjustment to the CI validation check.

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
    *   **C++ unit tests:** gtests in `tests/components/<component>/*.cpp`, run with
        `script/cpp_unit_test.py <component>`; see `tests/components/README.md`. Set the loop clock in a test
        with `LoopBlockingGuard{nullptr, nullptr, now_ms}`; the runner resets it before every test.
    *   **Integration tests:** `tests/integration/` runs real firmware on the host. Serial components prove
        timeouts and recovery with the `uart_mock` fixture (`tests/integration/test_uart_mock_*.py`). Its
        `responses` match on content every time they see it; to answer a repeated request differently, use an
        `on_tx` lambda with a static counter and `inject_to_rx_buffer` (see `uart_mock_ld2412_requery_after_giveup.yaml`).
    *   **Keep build-test variants few:** one `test.<platform>.yaml` per platform the code actually differs
        on. A variant per board for platform-identical code only adds CI time.
    *   **A test that needs a shared component in a particular state** (`esp32_ble` security settings, an option
        that PSRAM rejects) cannot be grouped: the merged block wins and the whole batch fails. Add the component to
        `ISOLATED_COMPONENTS` in `script/analyze_component_buses.py`, or move the option to a `test-<variant>` file.
    *   **Component Tests:** YAML-based compilation tests are located in `tests/`. The structure is as follows:
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
        Run them using `script/test_build_components`. Use `-c <component>` to test specific components and `-t <target>` for specific platforms.

    *   **Config-only test files (`validate.*.yaml`):** Use this prefix when a YAML file only needs to exercise schema/validation paths and does not need to be compiled. CI runs `validate.*.yaml` files with `esphome config` only and skips them during compile. The grammar mirrors `test.*.yaml`:
        - `validate.<platform>.yaml` — base config-only test
        - `validate-<variant>.<platform>.yaml` — config-only variant

        Use this for things like deprecated-syntax migration tests, schema edge cases, or platform-specific validation branches where building firmware adds no signal. A component may have any mix of `test.*.yaml` and `validate.*.yaml` files. Validate files never participate in bus-grouping; each one runs as its own `esphome config` invocation.

        When a PR's only edits to a component are `validate.*.yaml` files (no source changes, no `test.*.yaml` changes, and the component isn't pulled in as a dependency of another changed component), CI skips the compile stage for that component entirely and only runs config validation. This is decided in `script/determine-jobs.py` via `_component_change_is_validate_only` and surfaced as the `validate_only_components` output that the `test-build-components-split` job consumes.

    *   **Test Grouping with Packages:** Components that use shared bus packages can be grouped together in CI to reduce build count. **Never define buses (uart, i2c, spi, modbus) directly in test YAML files** — always use packages from `test_build_components/common/`.

        All includes in test files must go through dict-style `packages:` so that batch grouping works correctly — the grouping scripts only understand dict-style packages. Never use list-style packages (`packages: [- !include ...]`) or top-level merge keys (`<<: !include common.yaml`). Bus packages are keyed by the bus name; the component's `common.yaml` is keyed by the component name (e.g. `cst328: !include common.yaml`):
        ```yaml
        # test.esp32-idf.yaml — everything included via named packages
        packages:
          uart: !include ../../test_build_components/common/uart_115200/esp32-idf.yaml
          my_component: !include common.yaml
        ```
        ```yaml
        # common.yaml — component config only, NO bus definitions
        my_component:
          id: my_instance

        sensor:
          - platform: my_component
            name: My Sensor
        ```
        Components that define buses directly are flagged as "NEEDS MIGRATION" and cannot be grouped, increasing CI build time.
        `!extend` and `!remove` in a `test.*.yaml` also make the component ungroupable; add a standalone entity with its own id instead.

    *   **Testing All Components Together:** To verify that all components can be tested together without ID conflicts or configuration issues, use:
        ```bash
        ./script/test_component_grouping.py -e config --all
        ```
        This tests all components in a single build to catch conflicts that might not appear when testing components individually. Use `-e config` for fast configuration validation, or `-e compile` for full compilation testing.
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

*   **Contribution Workflow (Pull Request Process):**
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

        **Why Heap Allocation Matters:**

        ESP devices run for months with small heaps shared between Wi-Fi, BLE, LWIP, and application code. Over time, repeated allocations of different sizes fragment the heap. Failures happen when the largest contiguous block shrinks, even if total free heap is still large. We have seen field crashes caused by this.

        **Heap allocation after `setup()` should be avoided unless absolutely unavoidable.** Every allocation/deallocation cycle contributes to fragmentation. ESPHome treats runtime heap allocation as a long-term reliability bug, not a performance issue. Helpers that hide allocation (`std::string`, `std::to_string`, string-returning helpers) are being deprecated and replaced with buffer and view based APIs.

        **STL Container Guidelines:**

        ESPHome runs on embedded systems with limited resources. Choose containers carefully:

        1. **Compile-time-known sizes:** Use `std::array` instead of `std::vector` when size is known at compile time.
           ```cpp
           // Bad - generates STL realloc code
           std::vector<int> values;

           // Good - no dynamic allocation
           std::array<int, MAX_VALUES> values;
           ```
           Use `cg.add_define("MAX_VALUES", count)` to set the size from Python configuration.

           **For byte buffers:** Avoid `std::vector<uint8_t>` unless the buffer needs to grow. Use `std::unique_ptr<uint8_t[]>` instead.

           > **Note:** `std::unique_ptr<uint8_t[]>` does **not** provide bounds checking or iterator support like `std::vector<uint8_t>`. Use it only when you do not need these features and want minimal overhead.

           ```cpp
           // Bad - STL overhead for simple byte buffer
           std::vector<uint8_t> buffer;
           buffer.resize(256);

           // Good - minimal overhead, single allocation
           std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(256);
           // Or if size is constant:
           std::array<uint8_t, 256> buffer;
           ```

        2. **Compile-time-known fixed sizes with vector-like API:** Use `StaticVector` from `esphome/core/helpers.h` for compile-time fixed size with `push_back()` interface (no dynamic allocation).
           ```cpp
           // Bad - generates STL realloc code (_M_realloc_insert)
           std::vector<ServiceRecord> services;
           services.reserve(5);  // Still includes reallocation machinery

           // Good - compile-time fixed size, no dynamic allocation
           StaticVector<ServiceRecord, MAX_SERVICES> services;
           services.push_back(record1);
           ```
           Use `cg.add_define("MAX_SERVICES", count)` to set the size from Python configuration.
           Like `std::array` but with vector-like API (`push_back()`, `size()`) and no STL reallocation code.

           **Listener and child-entity registration lists are the most common case, and the most commonly
           missed.** A `register_*()` method called once per child at code generation time has a count that
           is known at compile time, so it should never be a `std::vector`. Use `cg.slot_counter()`: it
           returns a function that each consumer calls once per slot it will occupy, and after every
           `to_code` has run it emits the define with the final count. When nothing registers, no define is
           emitted and the storage plus its registration method compile out entirely.
           ```python
           # hub component's __init__.py
           _request_listener_slot = cg.slot_counter("MY_COMPONENT_LISTENER_COUNT")


           async def register_listener(hub: MockObj, var: MockObj) -> None:
               _request_listener_slot()
               cg.add(hub.register_listener(var))
           ```
           When several instances each own a list declared at the same size (one per hub of a
           `MULTI_CONF` component), pass the owning object as the key, `_request_listener_slot(str(hub))`;
           the define is then the largest count any one key requested instead of the total.
           ```cpp
           #ifdef MY_COMPONENT_LISTENER_COUNT
             void register_listener(MyComponentListener *listener);
           #endif
            protected:
           #ifdef MY_COMPONENT_LISTENER_COUNT
             StaticVector<MyComponentListener *, MY_COMPONENT_LISTENER_COUNT> listeners_;
           #endif
           ```
           Request slots from `to_code`, not from a job that runs after `CoroPriority.FINAL` - a late
           request raises rather than silently undercounting.

        3. **Runtime-known sizes:** Use `FixedVector` from `esphome/core/helpers.h` when the size is only known at runtime initialization.
           ```cpp
           // Bad - generates STL realloc code (_M_realloc_insert)
           std::vector<TxtRecord> txt_records;
           txt_records.reserve(5);  // Still includes reallocation machinery

           // Good - runtime size, single allocation, no reallocation machinery
           FixedVector<TxtRecord> txt_records;
           txt_records.init(record_count);  // Initialize with exact size at runtime
           ```
           **Benefits:**
           - Eliminates `_M_realloc_insert`, `_M_default_append` template instantiations (saves 200-500 bytes per instance)
           - Single allocation, no upper bound needed
           - No reallocation overhead
           - Compatible with protobuf code generation when using `[(fixed_vector) = true]` option

        4. **Small datasets (1-16 elements):** Use `std::vector` or `std::array` with simple structs instead of `std::map`/`std::set`/`std::unordered_map`.
           ```cpp
           // Bad - 2KB+ overhead for red-black tree/hash table
           std::map<std::string, int> small_lookup;
           std::unordered_map<int, std::string> tiny_map;

           // Good - simple struct with linear search (std::vector is fine)
           struct LookupEntry {
             const char *key;
             int value;
           };
           std::vector<LookupEntry> small_lookup = {
             {"key1", 10},
             {"key2", 20},
             {"key3", 30},
           };
           // Or std::array if size is compile-time constant:
           // std::array<LookupEntry, 3> small_lookup = {{ ... }};
           ```
           Linear search on small datasets (1-16 elements) is often faster than hashing/tree overhead, but this depends on lookup frequency and access patterns. For frequent lookups in hot code paths, the O(1) vs O(n) complexity difference may still matter even for small datasets. `std::vector` with simple structs is usually fine—it's the heavy containers (`map`, `set`, `unordered_map`) that should be avoided for small datasets unless profiling shows otherwise.

        5. **Strings set once from configuration:** Use `StringRef` (`esphome/core/string_ref.h`) rather than
           `std::string`. Code generation passes a string literal that lives in flash for the life of the
           program, so storing a `std::string` copies it onto the heap for nothing. `StringRef` is a
           non-owning pointer plus length; it does not copy, and it must only ever refer to storage that
           outlives it (a string literal, or a buffer owned elsewhere).
           ```cpp
           // Bad - heap copy of a literal that is already in flash
           void set_keys(std::string keys) { this->keys_ = std::move(keys); }
           std::string keys_;
           ```
           ```cpp
           // Good - no allocation
           void set_keys(const char *keys) { this->keys_ = StringRef(keys); }
           StringRef keys_;
           ```

        6. **Avoid `std::deque`:** It allocates in 512-byte blocks regardless of element size, guaranteeing at least 512 bytes of RAM usage immediately. This is a major source of crashes on memory-constrained devices.

        7. **Never use `new (std::nothrow)`:** On ESP-IDF exceptions are disabled, so a failed nothrow allocation aborts instead of returning `nullptr`. Use `RAMAllocator` from `esphome/core/helpers.h`; CI rejects `std::nothrow`.

        8. **Detection:** Look for these patterns in compiler output:
           - Large code sections with STL symbols (vector, map, set)
           - `alloc`, `realloc`, `dealloc` in symbol names
           - `_M_realloc_insert`, `_M_default_append` (vector reallocation)
           - Red-black tree code (`rb_tree`, `_Rb_tree`)
           - Hash table infrastructure (`unordered_map`, `hash`)

        **Prioritize optimization effort for:**
        - Core components (API, network, logger)
        - Widely-used components (mdns, wifi, ble)
        - Components causing flash size complaints

        Note: Avoiding heap allocation after `setup()` is always required regardless of component type. The prioritization above is about the effort spent on container optimization (e.g., migrating from `std::vector` to `StaticVector`).

        **Callback Managers:**

        ESPHome provides two callback manager types in `esphome/core/helpers.h` for the observer pattern. Both support `std::function`, lambdas, and lightweight forwarder structs via their templatized `add()` method.

        | Type | Idle overhead (32-bit) | When to use |
        |------|----------------------|-------------|
        | `CallbackManager<void(Ts...)>` | 12 bytes (empty `std::vector`) | Callbacks are always or almost always registered |
        | `LazyCallbackManager<void(Ts...)>` | 4 bytes (`nullptr`) | Callbacks are often not registered (common case) |

        `LazyCallbackManager` is a drop-in replacement for `CallbackManager` that defers allocation until the first callback is added. Prefer it for entity-level callbacks where most instances have no subscribers.

        **Important:** Registration methods that add to a callback manager **must always be templatized** to accept both `std::function` and pointer-sized forwarder structs (used by `build_callback_automation`). Never use `std::function` in the method signature:
        ```cpp
        // Bad -- forces heap allocation for forwarder structs
        void add_on_state_callback(std::function<void(bool)> &&callback) {
          this->state_callback_.add(std::move(callback));
        }

        // Good -- accepts any callable without forcing std::function wrapping
        template<typename F> void add_on_state_callback(F &&callback) {
          this->state_callback_.add(std::forward<F>(callback));
        }
        ```

    *   **State Management:** Use `CORE.data` for component state that needs to persist during configuration generation. Avoid module-level mutable globals.

        **Bad Pattern (Module-Level Globals):**
        ```python
        # Don't do this - state persists between compilation runs
        _component_state = []
        _use_feature = None


        def enable_feature():
            global _use_feature
            _use_feature = True
        ```

        **Bad Pattern (Flat Keys):**
        ```python
        # Don't do this - keys should be namespaced under component domain
        MY_FEATURE_KEY = "my_component_feature"
        CORE.data[MY_FEATURE_KEY] = True
        ```

        **Good Pattern (dataclass):**
        ```python
        from dataclasses import dataclass, field
        from esphome.core import CORE

        DOMAIN = "my_component"


        @dataclass
        class MyComponentData:
            feature_enabled: bool = False
            item_count: int = 0
            items: list[str] = field(default_factory=list)


        def _get_data() -> MyComponentData:
            if DOMAIN not in CORE.data:
                CORE.data[DOMAIN] = MyComponentData()
            return CORE.data[DOMAIN]


        def request_feature() -> None:
            _get_data().feature_enabled = True


        def add_item(item: str) -> None:
            _get_data().items.append(item)
        ```

        If you need a real-world example, search for components that use `@dataclass` with `CORE.data` in the codebase. Note: Some components may use `TypedDict` for dictionary-based storage; both patterns are acceptable depending on your needs.

        **Why this matters:**
        - Module-level globals persist between compilation runs if the host process (e.g. device-builder) doesn't fork/exec
        - `CORE.data` automatically clears between runs
        - Namespacing under `DOMAIN` prevents key collisions between components
        - `@dataclass` provides type safety and cleaner attribute access

*   **Security:** Be mindful of security when making changes to the API, web server, or any other network-related code. Do not hardcode secrets or keys.

*   **Dependencies & Build System Integration:**
    *   **Python:** When adding a new Python dependency, add it to the appropriate `requirements*.txt` file and `pyproject.toml`.
    *   **C++ / PlatformIO:** When adding a new C++ dependency, add it to `platformio.ini` and use `cg.add_library`.
    *   **Build Flags:** Use `cg.add_build_flag(...)` to add compiler flags.

## 8. Public API and Breaking Changes

*   **Public C++ API:**
    *   **Components**: Only documented features at [esphome.io](https://esphome.io) are public API. Undocumented `public` members are internal.
    *   **Core/Base Classes** (`esphome/core/`, `Component`, `Sensor`, etc.): All `public` members are public API.
    *   **Components with Global Accessors** (`global_api_server`, etc.): All `public` members are public API (except config setters).

*   **Public Python API:**
    *   All documented configuration options at [esphome.io](https://esphome.io) are public API.
    *   Python code in `esphome/core/` actively used by existing core components is considered stable API.
    *   Other Python code is internal unless explicitly documented for external component use.

*   **Breaking Changes Policy:**
    *   Aim for **6-month deprecation window** when possible
    *   Clean breaks allowed for: signature changes, deep refactorings, resource constraints
    *   Must document migration path in PR description (generates release notes)
    *   Blog post required for core/base class changes or significant architectural changes
    *   Full details: https://developers.esphome.io/contributing/code/#public-api-and-breaking-changes

*   **Breaking Change Checklist:**
    - [ ] Clear justification (RAM/flash savings, architectural improvement)
    - [ ] Explored non-breaking alternatives
    - [ ] Added deprecation warnings if possible (use `ESPDEPRECATED` macro for C++)
    - [ ] Documented migration path in PR description with before/after examples
    - [ ] Updated all internal usage and esphome.io
    - [ ] Tested backward compatibility during deprecation period

*   **Deprecation Pattern (C++):**
    ```cpp
    // Remove before 2026.6.0
    ESPDEPRECATED("Use new_method() instead. Removed in 2026.6.0", "2025.12.0")
    void old_method() { this->new_method(); }
    ```

*   **Deprecation Pattern (Python):**
    For a renamed config key, use the shared `cv.rename_key` validator with `removed_in` (and `component` for context) — it warns and auto-migrates:
    ```python
    CONFIG_SCHEMA = cv.All(
        cv.rename_key(
            CONF_OLD_KEY, CONF_NEW_KEY, removed_in="2026.6.0", component="my_component"
        ),
        cv.Schema({...}),
    )
    ```
    For other deprecations, warn manually during validation:
    ```python
    # Remove before 2026.6.0
    if CONF_OLD_KEY in config:
        _LOGGER.warning(
            f"'{CONF_OLD_KEY}' deprecated, use '{CONF_NEW_KEY}'. Removed in 2026.6.0"
        )
        config[CONF_NEW_KEY] = config.pop(CONF_OLD_KEY)  # Auto-migrate
    ```
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
