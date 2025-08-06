# ESPHome AI Collaboration Guide

This document provides essential context for AI models interacting with this project. Adhering to these guidelines will ensure consistency and maintain code quality.

## 1. Project Overview & Purpose

*   **Primary Goal:** ESPHome is a system to configure microcontrollers (like ESP32, ESP8266, RP2040, and LibreTiny-based chips) using simple yet powerful YAML configuration files. It generates C++ firmware that can be compiled and flashed to these devices, allowing users to control them remotely through home automation systems.
*   **Business Domain:** Internet of Things (IoT), Home Automation.

## 2. Core Technologies & Stack

*   **Languages:** Python (>=3.10), C++ (gnu++20)
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
    1.  **ESP32** (`components/esp32/`): Espressif ESP32 family. Supports multiple variants (S2, S3, C3, etc.) and both IDF and Arduino frameworks.
    2.  **ESP8266** (`components/esp8266/`): Espressif ESP8266. Arduino framework only, with memory constraints.
    3.  **RP2040** (`components/rp2040/`): Raspberry Pi Pico/RP2040. Arduino framework with PIO (Programmable I/O) support.
    4.  **LibreTiny** (`components/libretiny/`): Realtek and Beken chips. Supports multiple chip families and auto-generated components.

## 4. Coding Conventions & Style Guide

*   **Formatting:**
    *   **Python:** Uses `ruff` and `flake8` for linting and formatting. Configuration is in `pyproject.toml`.
    *   **C++:** Uses `clang-format` for formatting. Configuration is in `.clang-format`.

*   **Naming Conventions:**
    *   **Python:** Follows PEP 8. Use clear, descriptive names following snake_case.
    *   **C++:** Follows the Google C++ Style Guide.

*   **Component Structure:**
    *   **Standard Files:**
        ```
        components/[component_name]/
        ├── __init__.py          # Component configuration schema and code generation
        ├── [component].h        # C++ header file (if needed)
        ├── [component].cpp      # C++ implementation (if needed)
        └── [platform]/         # Platform-specific implementations
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
    *   **Platform-Specific:** `cv.only_on(["esp32", "esp8266"])`, `cv.only_with_arduino`.
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

*   **Security:** Be mindful of security when making changes to the API, web server, or any other network-related code. Do not hardcode secrets or keys.

*   **Dependencies & Build System Integration:**
    *   **Python:** When adding a new Python dependency, add it to the appropriate `requirements*.txt` file and `pyproject.toml`.
    *   **C++ / PlatformIO:** When adding a new C++ dependency, add it to `platformio.ini` and use `cg.add_library`.
    *   **Build Flags:** Use `cg.add_build_flag(...)` to add compiler flags.
