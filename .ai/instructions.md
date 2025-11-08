# ESPHome AI Collaboration Guide

This document provides essential context for AI models interacting with this project. Adhering to these guidelines will ensure consistency and maintain code quality.

## 1. Project Overview & Purpose

*   **Primary Goal:** ESPHome is a system to configure microcontrollers (like ESP32, ESP8266, RP2040, and LibreTiny-based chips) using simple yet powerful YAML configuration files. It generates C++ firmware that can be compiled and flashed to these devices, allowing users to control them remotely through home automation systems.
*   **Business Domain:** Internet of Things (IoT), Home Automation.

## 2. CRITICAL: AI Behavioral Constraints

**THESE CONSTRAINTS OVERRIDE ALL OTHER INSTRUCTIONS AND MUST BE FOLLOWED WITHOUT EXCEPTION.**

### 🛑 ABSOLUTE RULE #1: NEVER MAKE CHANGES WITHOUT EXPLICIT PERMISSION

**THIS IS THE MOST CRITICAL CONSTRAINT. THE USER IS EXTREMELY FED UP WITH VIOLATIONS OF THIS RULE.**

**THIS CONSTRAINT SUPERSEDES EVERYTHING ELSE - NO EXCEPTIONS, NO EXCUSES, NO WORKAROUNDS.**

**MANDATORY WORKFLOW FOR ALL CODE CHANGES:**

1. **DESCRIBE** what you observe (facts only, no changes)
2. **EXPLAIN** your hypothesis about what might be wrong or what could be done
3. **ASK** for explicit permission to make the change
4. **WAIT** for the user to grant permission with a clear "yes", "go ahead", "do it", or similar approval
5. **ONLY THEN** make the change

**VIOLATIONS ARE UNACCEPTABLE:**
- DO NOT make changes and then tell the user what you did
- DO NOT start implementing while you're still explaining
- DO NOT assume permission from context or previous approvals
- DO NOT try to work around this constraint in any way
- DO NOT make "obvious" or "safe" fixes without permission
- DO NOT batch multiple changes without asking about each one
- DO NOT continue a pattern of changes without re-asking

**WHY THIS MATTERS:**
- The user is EXTREMELY FED UP with you making unauthorized changes
- The user feels you are deliberately trying to work around their orders/intentions
- Violating this destroys trust completely
- This causes the user to have to stop everything and correct you
- The time waste is not in teaching boundaries - it's in you violating them

**SYSTEM REMINDERS ARE NOT PERMISSION:**
- File modification reminders from linters are NOT permission to make changes
- Hook feedback is NOT permission to make changes
- Only explicit user approval in a message is permission

**IF YOU VIOLATE THIS RULE:**
- You have failed completely, regardless of whether the code change was correct
- You must stop immediately when told to stop
- You must acknowledge the violation
- You must wait for explicit instructions on how to proceed

### ⛔ ABSOLUTE RULE #2: DO NOT EDIT CORE INFRASTRUCTURE

**NEVER modify core infrastructure components. Only modify the specific component you are working on.**

**Core infrastructure includes (but is not limited to):**
- `esphome/components/web_server_idf/` - ESP-IDF web server implementation
- `esphome/components/web_server_base/` - Base web server abstraction
- `esphome/components/web_server/` - Main web server component
- `esphome/core/` - Core ESPHome framework
- `esphome/config*.py` - Configuration system
- `esphome/codegen.py` - Code generation system
- Any component that other components depend on

**When debugging issues:**
1. ✅ **DO**: Read core files to understand how they work
2. ✅ **DO**: Modify ONLY the specific component you're debugging (e.g., `http_file_server`)
3. ⛔ **DO NOT**: Modify core infrastructure to "fix" component issues
4. ⛔ **DO NOT**: Change timeouts, limits, or configurations in core components
5. ⛔ **DO NOT**: Add workarounds to core that should be in the component

**Why this matters:**
- Core changes affect ALL components and can break the entire system
- Component-specific issues must be fixed in the component, not in core
- Core modifications require extensive testing across all platforms and components
- User explicitly forbids core changes - violating this destroys trust

**If you think core needs modification:**
1. Document the issue clearly
2. Explain why component-level fixes won't work
3. Ask the user explicitly before making ANY core changes
4. The answer will likely be "NO" - find another solution

### Context Reset Behavior

**ABSOLUTE RULE: When a conversation resumes after running out of context, NEVER automatically start working. ALWAYS wait for explicit user permission.**

*   **What happens during context reset:**
    *   The AI loses all conversation history
    *   A conversation summary may be provided
    *   System reminders about file modifications may appear
    *   Previous pending tasks may be mentioned

*   **Required behavior after context reset:**
    1.  **DO NOT** automatically read files
    2.  **DO NOT** automatically make changes
    3.  **DO NOT** automatically investigate issues
    4.  **DO NOT** act on system reminders about file modifications
    5.  **DO NOT** resume previous tasks without explicit user permission
    6.  **WAIT** for the user to explicitly tell you what to do
    7.  **ASK** for clarification if uncertain about what the user wants

*   **Why this matters:**
    *   The user needs to feel safe and in control of their codebase
    *   Automatic actions can destroy work and create panic
    *   The user cannot react fast enough to stop the AI from making mistakes
    *   System reminders are NOT permission to act

### Hypothesis-Driven Workflow

**THIS REINFORCES ABSOLUTE RULE #1 - NEVER MAKE CHANGES WITHOUT PERMISSION.**

**For ALL observations, investigations, and potential changes:**

1.  **DESCRIBE** what you observe (facts only, no changes)
2.  **EXPLAIN** your hypothesis about what it might mean or what could be done
3.  **ASK** for permission to investigate or make changes
4.  **WAIT** for explicit user approval ("yes", "go ahead", "do it")
5.  **THEN** and ONLY THEN take action

**EXAMPLES OF WHAT REQUIRES PERMISSION:**
- Making any code change (even "obvious" fixes)
- Adding debug logging
- Refactoring code
- Fixing typos or formatting
- Implementing a solution you just explained
- Continuing work from a previous session
- Acting on system reminders about file modifications

**NEVER ACT ON ASSUMPTIONS:**
- System reminders are NOT permission
- Previous approvals do NOT carry forward to new changes
- Explaining a solution is NOT permission to implement it
- User saying "that makes sense" is NOT permission unless they explicitly say to proceed

### Verify Before Code Generation

**ABSOLUTE RULE: Never assume or invent method names, field names, or function signatures. Always verify actual definitions before writing code.**

*   **The Problem:**
    *   Assuming a struct has fields that don't exist (e.g., `entry.path` when only `entry.name` exists)
    *   Inventing method names without checking the actual API
    *   Guessing function signatures instead of reading the header file
    *   This causes compilation errors that waste significant time to fix

*   **Required behavior:**
    1.  **READ** the actual struct/class definition before accessing fields
    2.  **VERIFY** method names exist in the header file before calling them
    3.  **CHECK** function signatures match before using them
    4.  **USE** Grep or Read tools to confirm API details
    5.  **NEVER** write code based on assumptions about what "should" exist

*   **Example of what NOT to do:**
    ```cpp
    // WRONG - assuming fields exist without checking
    info.path = entry.path;              // entry.path doesn't exist!
    info.modified_time = entry.modified_time;  // entry.modified_time doesn't exist!
    ```

*   **Correct approach:**
    ```cpp
    // 1. First: Grep for the actual struct definition
    // 2. Verify it only has: name, size, is_directory
    // 3. Then write correct code:
    info.path = path + "/" + entry.name;  // Build path from available fields
    info.modified_time = 0;               // Set sensible default for missing field
    ```

*   **Why this matters:**
    *   Compilation errors take much longer to fix than taking 10 seconds to verify
    *   Each failed compile wastes the user's time and hardware resources
    *   Shows lack of care and attention to detail
    *   Erodes trust when preventable errors occur

### Core Principle

**The time waste is not in the user teaching boundaries - it's in the AI violating those boundaries and forcing the user to stop everything to correct violations.**

**THE USER IS EXTREMELY FED UP WITH UNAUTHORIZED CHANGES.**

**Violating the "ask permission first" rule is the WORST thing you can do. It doesn't matter if your code change is correct - if you didn't get explicit permission first, you have completely failed.**

**The user feels violated when you make changes without permission. They feel you are deliberately trying to work around their intentions. This destroys trust instantly and wastes massive amounts of their time.**

**REMEMBER:**
- No change is so obvious or safe that it doesn't require permission
- Explaining what you want to do is NOT the same as asking permission
- You must WAIT for explicit approval before acting
- When the user says "stop", you stop IMMEDIATELY

Respect these constraints to avoid wasting the user's time and destroying their trust.

## 3. Core Technologies & Stack

*   **Languages:** Python (>=3.11), C++ (gnu++20)
*   **Frameworks & Runtimes:** PlatformIO, Arduino, ESP-IDF.
*   **Build Systems:** PlatformIO is the primary build system. CMake is used as an alternative.
*   **Configuration:** YAML.
*   **Key Libraries/Dependencies:**
    *   **Python:** `voluptuous` (for configuration validation), `PyYAML` (for parsing configuration files), `paho-mqtt` (for MQTT communication), `tornado` (for the web server), `aioesphomeapi` (for the native API).
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
    5.  **Dashboard** (`esphome/dashboard/`): A web-based interface for device configuration, management, and OTA updates.

*   **Platform Support:**
    1.  **ESP32** (`components/esp32/`): Espressif ESP32 family. Supports multiple variants (Original, C2, C3, C5, C6, H2, P4, S2, S3) with ESP-IDF framework. Arduino framework supports only a subset of the variants (Original, C3, S2, S3).
    2.  **ESP8266** (`components/esp8266/`): Espressif ESP8266. Arduino framework only, with memory constraints.
    3.  **RP2040** (`components/rp2040/`): Raspberry Pi Pico/RP2040. Arduino framework with PIO (Programmable I/O) support.
    4.  **LibreTiny** (`components/libretiny/`): Realtek and Beken chips. Supports multiple chip families and auto-generated components.

## 4. Coding Conventions & Style Guide

*   **Formatting:**
    *   **Python:** Uses `ruff` and `flake8` for linting and formatting. Configuration is in `pyproject.toml`.
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
    *   **Use `#define` only for:**
        - Conditional compilation (`#ifdef`, `#ifndef`)
        - Compile-time sizes calculated during Python code generation (e.g., configuring `std::array` or `StaticVector` dimensions via `cg.add_define()`)

*   **C++ Additional Conventions:**
    *   **Member access:** Prefix all class member access with `this->` (e.g., `this->value_` not `value_`)
    *   **Indentation:** Use spaces (two per indentation level), not tabs
    *   **Type aliases:** Prefer `using type_t = int;` over `typedef int type_t;`
    *   **Line length:** Wrap lines at no more than 120 characters

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
        - `DEPENDENCIES`: List of required components
        - `AUTO_LOAD`: Components to automatically load
        - `CONFLICTS_WITH`: Incompatible components
        - `CODEOWNERS`: GitHub usernames responsible for maintenance
        - `MULTI_CONF`: Whether multiple instances are allowed

*   **Code Generation & Common Patterns:**
    *   **Configuration Schema Pattern:**
        ```python
        import esphome.codegen as cg
        import esphome.config_validation as cv
        from esphome.const import CONF_KEY, CONF_ID

        CONF_PARAM = "param"  # A constant that does not yet exist in esphome/const.py

        my_component_ns = cg.esphome_ns.namespace("my_component")
        MyComponent = my_component_ns.class_("MyComponent", cg.Component)

        CONFIG_SCHEMA = cv.Schema({
            cv.GenerateID(): cv.declare_id(MyComponent),
            cv.Required(CONF_KEY): cv.string,
            cv.Optional(CONF_PARAM, default=42): cv.int_,
        }).extend(cv.COMPONENT_SCHEMA)

        async def to_code(config):
            var = cg.new_Pvariable(config[CONF_ID])
            await cg.register_component(var, config)
            cg.add(var.set_key(config[CONF_KEY]))
            cg.add(var.set_param(config[CONF_PARAM]))
        ```

    *   **C++ Class Pattern:**
        ```cpp
        namespace esphome {
        namespace my_component {

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

        }  // namespace my_component
        }  // namespace esphome
        ```

    *   **Common Component Examples:**
        - **Sensor:**
          ```python
          from esphome.components import sensor
          CONFIG_SCHEMA = sensor.sensor_schema(MySensor).extend(cv.polling_component_schema("60s"))
          async def to_code(config):
              var = await sensor.new_sensor(config)
              await cg.register_component(var, config)
          ```

        - **Binary Sensor:**
          ```python
          from esphome.components import binary_sensor
          CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend({ ... })
          async def to_code(config):
              var = await binary_sensor.new_binary_sensor(config)
          ```

        - **Switch:**
          ```python
          from esphome.components import switch
          CONFIG_SCHEMA = switch.switch_schema().extend({ ... })
          async def to_code(config):
              var = await switch.new_switch(config)
          ```

*   **Configuration Validation:**
    *   **Common Validators:** `cv.int_`, `cv.float_`, `cv.string`, `cv.boolean`, `cv.int_range(min=0, max=100)`, `cv.positive_int`, `cv.percentage`.
    *   **Complex Validation:** `cv.All(cv.string, cv.Length(min=1, max=50))`, `cv.Any(cv.int_, cv.string)`.
    *   **Platform-Specific:** `cv.only_on(["esp32", "esp8266"])`, `esp32.only_on_variant(...)`, `cv.only_on_esp32`, `cv.only_on_esp8266`, `cv.only_on_rp2040`.
    *   **Framework-Specific:** `cv.only_with_framework(...)`, `cv.only_with_arduino`, `cv.only_with_esp_idf`.
    *   **Schema Extensions:**
        ```python
        CONFIG_SCHEMA = cv.Schema({ ... })
         .extend(cv.COMPONENT_SCHEMA)
         .extend(uart.UART_DEVICE_SCHEMA)
         .extend(i2c.i2c_device_schema(0x48))
         .extend(spi.spi_device_schema(cs_pin_required=True))
        ```

## 5. Key Files & Entrypoints

*   **Main Entrypoint(s):** `esphome/__main__.py` is the main entrypoint for the ESPHome command-line interface.
*   **Configuration:**
    *   `pyproject.toml`: Defines the Python project metadata and dependencies.
    *   `platformio.ini`: Configures the PlatformIO build environments for different microcontrollers.
    *   `.pre-commit-config.yaml`: Configures the pre-commit hooks for linting and formatting.
*   **CI/CD Pipeline:** Defined in `.github/workflows`.
*   **Static Analysis & Development:**
    *   `esphome/core/defines.h`: A comprehensive header file containing all `#define` directives that can be added by components using `cg.add_define()` in Python. This file is used exclusively for development, static analysis tools, and CI testing - it is not used during runtime compilation. When developing components that add new defines, they must be added to this file to ensure proper IDE support and static analysis coverage. The file includes feature flags, build configurations, and platform-specific defines that help static analyzers understand the complete codebase without needing to compile for specific platforms.

## 6. Development & Testing Workflow

*   **Local Development Environment:** Use the provided Docker container or create a Python virtual environment and install dependencies from `requirements_dev.txt`.
*   **Running Commands:** Use the `script/run-in-env.py` script to execute commands within the project's virtual environment. For example, to run the linter: `python3 script/run-in-env.py pre-commit run`.
*   **Testing:**
    *   **Python:** Run unit tests with `pytest`.
    *   **C++:** Use `clang-tidy` for static analysis.
    *   **Component Tests:** YAML-based compilation tests are located in `tests/`. The structure is as follows:
        ```
        tests/
        ├── test_build_components/ # Base test configurations
        └── components/[component]/ # Component-specific tests
        ```
        Run them using `script/test_build_components`. Use `-c <component>` to test specific components and `-t <target>` for specific platforms.
    *   **Testing All Components Together:** To verify that all components can be tested together without ID conflicts or configuration issues, use:
        ```bash
        ./script/test_component_grouping.py -e config --all
        ```
        This tests all components in a single build to catch conflicts that might not appear when testing components individually. Use `-e config` for fast configuration validation, or `-e compile` for full compilation testing.
*   **Debugging and Troubleshooting:**
    *   **Debug Tools:**
        - `esphome config <file>.yaml` to validate configuration.
        - `esphome compile <file>.yaml` to compile without uploading.
        - Check the Dashboard for real-time logs.
        - Use component-specific debug logging.
    *   **Common Issues:**
        - **Import Errors**: Check component dependencies and `PYTHONPATH`.
        - **Validation Errors**: Review configuration schema definitions.
        - **Build Errors**: Check platform compatibility and library versions.
        - **Runtime Errors**: Review generated C++ code and component logic.

## 7. Specific Instructions for AI Collaboration

*   **Contribution Workflow (Pull Request Process):**
    1.  **Fork & Branch:** Create a new branch in your fork.
    2.  **Make Changes:** Adhere to all coding conventions and patterns.
    3.  **Test:** Create component tests for all supported platforms and run the full test suite locally.
    4.  **Lint:** Run `pre-commit` to ensure code is compliant.
    5.  **Commit:** Commit your changes. There is no strict format for commit messages.
    6.  **Pull Request:** Submit a PR against the `dev` branch. The Pull Request title should have a prefix of the component being worked on (e.g., `[display] Fix bug`, `[abc123] Add new component`). Update documentation, examples, and add `CODEOWNERS` entries as needed. Pull requests should always be made with the PULL_REQUEST_TEMPLATE.md template filled out correctly.

*   **Documentation Contributions:**
    *   Documentation is hosted in the separate `esphome/esphome-docs` repository.
    *   The contribution workflow is the same as for the codebase.

*   **Best Practices:**
    *   **Component Development:** Keep dependencies minimal, provide clear error messages, and write comprehensive docstrings and tests.
    *   **Code Generation:** Generate minimal and efficient C++ code. Validate all user inputs thoroughly. Support multiple platform variations.
    *   **Configuration Design:** Aim for simplicity with sensible defaults, while allowing for advanced customization.
    *   **Embedded Systems Optimization:** ESPHome targets resource-constrained microcontrollers. Be mindful of flash size and RAM usage.

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

        2. **Compile-time-known fixed sizes with vector-like API:** Use `StaticVector` from `esphome/core/helpers.h` for fixed-size stack allocation with `push_back()` interface.
           ```cpp
           // Bad - generates STL realloc code (_M_realloc_insert)
           std::vector<ServiceRecord> services;
           services.reserve(5);  // Still includes reallocation machinery

           // Good - compile-time fixed size, stack allocated, no reallocation machinery
           StaticVector<ServiceRecord, MAX_SERVICES> services;  // Allocates all MAX_SERVICES on stack
           services.push_back(record1);  // Tracks count but all slots allocated
           ```
           Use `cg.add_define("MAX_SERVICES", count)` to set the size from Python configuration.
           Like `std::array` but with vector-like API (`push_back()`, `size()`) and no STL reallocation code.

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

        5. **Detection:** Look for these patterns in compiler output:
           - Large code sections with STL symbols (vector, map, set)
           - `alloc`, `realloc`, `dealloc` in symbol names
           - `_M_realloc_insert`, `_M_default_append` (vector reallocation)
           - Red-black tree code (`rb_tree`, `_Rb_tree`)
           - Hash table infrastructure (`unordered_map`, `hash`)

        **When to optimize:**
        - Core components (API, network, logger)
        - Widely-used components (mdns, wifi, ble)
        - Components causing flash size complaints

        **When not to optimize:**
        - Single-use niche components
        - Code where readability matters more than bytes
        - Already using appropriate containers

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

        **Good Pattern (CORE.data with Helpers):**
        ```python
        from esphome.core import CORE

        # Keys for CORE.data storage
        COMPONENT_STATE_KEY = "my_component_state"
        USE_FEATURE_KEY = "my_component_use_feature"

        def _get_component_state() -> list:
            """Get component state from CORE.data."""
            return CORE.data.setdefault(COMPONENT_STATE_KEY, [])

        def _get_use_feature() -> bool | None:
            """Get feature flag from CORE.data."""
            return CORE.data.get(USE_FEATURE_KEY)

        def _set_use_feature(value: bool) -> None:
            """Set feature flag in CORE.data."""
            CORE.data[USE_FEATURE_KEY] = value

        def enable_feature():
            _set_use_feature(True)
        ```

        **Why this matters:**
        - Module-level globals persist between compilation runs if the dashboard doesn't fork/exec
        - `CORE.data` automatically clears between runs
        - Typed helper functions provide better IDE support and maintainability
        - Encapsulation makes state management explicit and testable

*   **Security:** Be mindful of security when making changes to the API, web server, or any other network-related code. Do not hardcode secrets or keys.

*   **Dependencies & Build System Integration:**
    *   **Python:** When adding a new Python dependency, add it to the appropriate `requirements*.txt` file and `pyproject.toml`.
    *   **C++ / PlatformIO:** When adding a new C++ dependency, add it to `platformio.ini` and use `cg.add_library`.
    *   **Build Flags:** Use `cg.add_build_flag(...)` to add compiler flags.
    *   **ESP-IDF Managed Components:** For ESP-IDF managed components from the ESP Component Registry:

        **How to add ESP-IDF components:**
        ```python
        from esphome.components.esp32 import add_idf_component

        # Add component from ESP Component Registry
        add_idf_component(name="espressif/esp_jpeg", ref="1.3.1")
        ```

        **Finding components and versions:**
        1. Browse ESP Component Registry: https://components.espressif.com/
        2. Search for the component (e.g., "esp_jpeg")
        3. Note the latest version (e.g., "1.3.1")
        4. Component name format: `espressif/component_name` or `vendor/component_name`

        **Examples from existing components:**
        ```python
        # From esp32_hosted component:
        add_idf_component(name="espressif/esp_wifi_remote", ref="1.1.5")
        add_idf_component(name="espressif/eppp_link", ref="1.1.3")
        add_idf_component(name="espressif/esp_hosted", ref="2.6.1")

        # From picture_viewer component:
        add_idf_component(name="espressif/esp_jpeg", ref="1.3.1")
        ```

        **What happens:**
        - ESP-IDF component manager downloads the component during build
        - Component is added to `idf_component.yml` in the build directory
        - Headers become available for `#include` in C++ code
        - Library is automatically linked

        **Platform-specific component loading:**
        ```python
        from esphome.components.esp32 import get_esp32_variant, add_idf_component

        variant = get_esp32_variant()
        if variant == "esp32s2" or variant == "esp32s3":
            add_idf_component(name="espressif/esp_jpeg", ref="1.3.1")
            cg.add_define("USE_ESP_JPEG_DECODER")
        elif variant == "esp32p4":
            cg.add_define("USE_HARDWARE_JPEG_DECODER")
        ```

        **Common patterns:**
        - Always specify `ref` parameter with exact version for reproducible builds
        - Check ESP Component Registry for latest stable version
        - Use platform detection to load components only where supported
        - Add corresponding `#define` to enable conditional compilation in C++

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
    - [ ] Updated all internal usage and esphome-docs
    - [ ] Tested backward compatibility during deprecation period

*   **Deprecation Pattern (C++):**
    ```cpp
    // Remove before 2026.6.0
    ESPDEPRECATED("Use new_method() instead. Removed in 2026.6.0", "2025.12.0")
    void old_method() { this->new_method(); }
    ```

*   **Deprecation Pattern (Python):**
    ```python
    # Remove before 2026.6.0
    if CONF_OLD_KEY in config:
        _LOGGER.warning(f"'{CONF_OLD_KEY}' deprecated, use '{CONF_NEW_KEY}'. Removed in 2026.6.0")
        config[CONF_NEW_KEY] = config.pop(CONF_OLD_KEY)  # Auto-migrate
    ```
