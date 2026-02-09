## ESPHome AI Coding Agent Instructions

> Canonical reference: [.ai/instructions.md](../.ai/instructions.md)

### 1. Project Architecture & Workflow
- ESPHome generates C++ firmware from YAML configs for microcontrollers (ESP32, ESP8266, RP2040, LibreTiny).
- Python parses YAML, validates config, and generates C++ code. PlatformIO compiles/flashes firmware.
- Key directories: [esphome/](esphome/) (core logic), [esphome/components/](esphome/components/) (modular components), [script/](script/) (dev scripts), [tests/](tests/) (unit/integration/component tests).
- Dashboard: [esphome/dashboard/](esphome/dashboard/) provides web UI for device management.

### 2. Coding Conventions
- Python: PEP8, snake_case, lint with ruff/flake8 ([pyproject.toml](pyproject.toml)).
- C++: Google C++ Style, lower_snake_case for methods/vars, UpperCamelCase for types, UPPER_SNAKE_CASE for constants. Use spaces (2 per indent), prefix members with `this->`.
- Avoid heap allocation after `setup()`; use `std::array`, `StaticVector`, or `FixedVector` for buffers/collections.
- Component structure: Each [component] has Python schema/codegen, C++ header/impl, and platform-specific subdirs.

### 3. Build, Test, Debug
- Use [script/run-in-env.py](script/run-in-env.py) for dev commands (e.g., lint, test).
- Run Python tests with `pytest`, C++ static analysis with `clang-tidy`.
- Component tests: [tests/components/](tests/components/) and [tests/test_build_components/](tests/test_build_components/) (run with [script/test_build_components](script/test_build_components)).
- Validate YAML configs: `esphome config <file>.yaml`. Compile: `esphome compile <file>.yaml`.
- Use dashboard for logs and OTA updates.

### 4. Patterns & Integration
- Configuration schemas use Voluptuous; extend with platform/device schemas as needed.
- Codegen: Use `cg.add_define()` for compile-time constants, `cg.add_build_flag()` for compiler flags.
- State: Use `CORE.data` (with @dataclass or TypedDict) for persistent config generation state; avoid module-level globals.
- Component metadata: `DEPENDENCIES`, `AUTO_LOAD`, `CONFLICTS_WITH`, `CODEOWNERS`, `MULTI_CONF`.
- Public API: Only documented features/components are stable; internal APIs may change.

### 5. Contribution & Deprecation
- Branch from `dev`, test all platforms, run lint, use PR template, prefix PR titles with component name.
- Deprecate features with clear migration path, use `ESPDEPRECATED` macro (C++) or warning+auto-migrate (Python).

### 6. Examples
- See [.ai/instructions.md](../.ai/instructions.md) for full schema, codegen, and container usage examples.
- Reference [README.md](README.md) for project overview and [esphome/core/defines.h](esphome/core/defines.h) for static analysis defines.

---
For full details, see [.ai/instructions.md](../.ai/instructions.md). Update this file if new conventions emerge.
