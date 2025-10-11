# What does this implement/fix?

Removes redundant ESP32 Arduino test files for the `opentherm` component and cleans up redundant preprocessor conditionals. The ESP-IDF tests provide complete coverage since the opentherm component has no framework-specific implementation differences for ESP32.

Also fixes incorrect preprocessor conditionals - changes `#if defined(ESP32) || defined(USE_ESP_IDF)` to `#ifdef USE_ESP32`. The macro `ESP32` is only defined for the original ESP32 variant, while `USE_ESP32` covers all ESP32 variants (C3, S2, S3, etc.). The `|| defined(USE_ESP_IDF)` was unnecessary since ESP-IDF can only run on ESP32 platforms.

## Background

As part of the ongoing effort to reduce Arduino-specific test redundancy (esphome/backlog#66), this PR removes ESP32 Arduino tests that duplicate IDF test coverage.

**Analysis of opentherm component:**
- Previously used `#if defined(ESP32) || defined(USE_ESP_IDF)` to check for ESP32 **platform**
- This was incorrect: `ESP32` is only defined for the original ESP32 variant, not C3/S2/S3
- Changed to `#ifdef USE_ESP32` which covers all ESP32 variants
- The `|| defined(USE_ESP_IDF)` part was unnecessary since ESP-IDF can only run on ESP32 platforms
- ESP32 timer APIs (`timer_init`, `timer_set_counter_value`, `timer_isr_callback_add`) are ESP-IDF APIs
- These timer APIs work identically in both Arduino and ESP-IDF frameworks since Arduino is now built on ESP-IDF
- Only ESP8266 has framework-specific code (using Arduino's `timer1_*` functions)
- ESP32 implementation is identical across frameworks

## Changes

### Code Cleanup

**OpenTherm component:**
- Fixed incorrect `#if defined(ESP32) || defined(USE_ESP_IDF)` to `#ifdef USE_ESP32` in:
  - `esphome/components/opentherm/opentherm.h` (3 locations)
  - `esphome/components/opentherm/opentherm.cpp` (4 locations)
- `ESP32` is only defined for the original ESP32 variant, not C3/S2/S3
- `USE_ESP32` correctly covers all ESP32 variants
- The `|| defined(USE_ESP_IDF)` part was unnecessary since ESP-IDF can only be defined on ESP32 platforms

### Test Files Removed

- `tests/components/opentherm/test.esp32-ard.yaml`
- `tests/components/opentherm/test.esp32-c3-ard.yaml`

### Test Coverage Maintained

ESP-IDF test files remain and cover both frameworks:
- `tests/components/opentherm/test.esp32-idf.yaml`
- `tests/components/opentherm/test.esp32-c3-idf.yaml`

### Platform-Specific Tests Retained

Arduino tests remain for ESP8266 (uses Arduino-specific `timer1_*` functions):
- `tests/components/opentherm/test.esp8266-ard.yaml`

## Benefits

- **Reduces CI test time** - 2 fewer redundant test configurations
- **Simplifies code** - Removes redundant preprocessor conditionals
- **Maintains coverage** - IDF tests cover both frameworks for ESP32

## Types of changes

- [x] Code quality improvements to existing code or addition of tests

**Related issue or feature (if applicable):**

- Part of esphome/backlog#66 - Remove redundant ESP32 Arduino tests

**Pull request in [esphome-docs](https://github.com/esphome/esphome-docs) with documentation (if applicable):**

N/A - No user-facing changes

## Test Environment

- [x] ESP32
- [x] ESP32 IDF
- [ ] ESP8266
- [ ] RP2040
- [ ] BK72xx
- [ ] RTL87xx
- [ ] nRF52840

## Example entry for `config.yaml`:

N/A - No configuration changes

## Checklist:
  - [x] The code change is tested and works locally.
  - [ ] Tests have been added to verify that the new code works (under `tests/` folder).

If user exposed functionality or configuration variables are added/changed:
  - [ ] Documentation added/updated in [esphome-docs](https://github.com/esphome/esphome-docs).
