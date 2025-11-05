# PR Title Suggestion

```
[core] Deprecate get_icon(), get_device_class(), get_unit_of_measurement() and fix remaining non-MQTT usages
```

# What does this implement/fix?

This PR deprecates three inefficient methods in `entity_base.h` that return `std::string` copies and updates the remaining non-MQTT usages in the core codebase to use the more efficient `StringRef` alternatives.

## Background

PR #11731 eliminates MQTT component usages of these methods, demonstrating significant performance improvements by avoiding unnecessary string copies. This PR completes the migration by:

1. Adding deprecation warnings to the old methods
2. Fixing the remaining non-MQTT usages in prometheus, graph, and sprinkler components
3. Setting a clear removal timeline (ESPHome 2026.5.0)

**Note:** This PR should be merged **after** PR #11731 is merged to ensure all internal usages are updated before deprecation warnings are enabled.

## Changes

### 1. Deprecate inefficient methods (entity_base.h)

Added `[[deprecated]]` attributes to:
- `EntityBase::get_icon()` → Use `get_icon_ref()` instead
- `EntityBase_DeviceClass::get_device_class()` → Use `get_device_class_ref()` instead
- `EntityBase_UnitOfMeasurement::get_unit_of_measurement()` → Use `get_unit_of_measurement_ref()` instead

**Deprecation message:** "Use <method>_ref() instead for better performance (avoids string copy). Will stop working in ESPHome 2026.5.0"

### 2. Fix remaining components (prometheus, graph, sprinkler)

Updated 3 components to use `get_unit_of_measurement_ref()` instead of `get_unit_of_measurement()`:

- **prometheus_handler.cpp** (line 161): Eliminates temporary string allocation when printing metrics
- **graph.cpp** (lines 238, 374): Direct `StringRef` appending to `std::string` (uses `StringRef` operator `+=` overload)
- **sprinkler.cpp** (lines 653, 735): Zero-allocation string comparisons with `MIN_STR`

## Why deprecate instead of remove immediately?

This is a **breaking change** for external components, but we're using a gradual deprecation approach:

1. **External components:** Many third-party components may use these methods - immediate removal would break them without notice
2. **Migration path:** Gives external component authors 6 months to update with clear compiler warnings
3. **Clear timeline:** ESPHome 2026.5.0 gives a concrete removal date that can be communicated in release notes
4. **Compiler warnings:** External components will see deprecation warnings during compilation, making the migration explicit and traceable
5. **Simple migration:** The fix is straightforward - replace `get_X()` with `get_X_ref()` (add `.c_str()` if passing to functions expecting `const char*`)

## Benefits

### Performance improvements:
- **Eliminates string copies** in hot code paths (prometheus metrics, graph rendering, sprinkler logic)
- **Reduces heap fragmentation** on ESP8266 where memory is constrained
- **Faster string comparisons** (sprinkler component)
- **Safer code** (prometheus: no more `.c_str()` on temporaries)

### Code quality improvements:
- **Consistent API usage:** All core code now uses `*_ref()` methods
- **Better practices:** Encourages zero-copy string handling
- **Clear migration path:** External components have time and guidance to update

### Flash/RAM impact:
- Minimal: Deprecation attributes add no runtime overhead
- Once methods are removed in 2026.5.0: ~200-500 bytes flash savings (removes unused vtable entries and std::string conversions)

## Timeline

1. **Today:** Merge this PR (deprecates methods, fixes core usage)
   - Deprecation warnings appear during compilation for external components
   - All core components updated to use `*_ref()` methods
2. **Next 6 months:** External components see deprecation warnings, maintainers update their code
   - Component authors have time to migrate to `*_ref()` methods
   - Warnings provide clear guidance on what to change
3. **ESPHome 2026.5.0:** **BREAKING CHANGE** - Remove the three deprecated methods entirely
   - External components still using old methods will fail to compile
   - Migration is simple: replace `get_X()` with `get_X_ref()` (or `.c_str()` if needed)

## Dependencies

**This PR depends on PR #11731 being merged first.**

PR #11731 eliminates MQTT component usages (8 components × 2 calls = 16 string copies per discovery cycle). Once that's merged, this PR:
1. Adds deprecation warnings to guide external components
2. Fixes the remaining 3 components (prometheus, graph, sprinkler)
3. Completes the migration for all core components

## Merge Order

1. **First:** Merge PR #11731 (fixes MQTT components)
2. **Then:** Merge this PR (adds deprecation warnings + fixes remaining components)

## Types of changes

- [x] Code quality improvements to existing code
- [x] Deprecation of inefficient APIs
- [x] **Breaking change** (External components using these methods will break in ESPHome 2026.5.0)

## Test Environment

- [ ] ESP32 IDF
- [ ] ESP32 Arduino
- [ ] ESP8266 Arduino
- [ ] RP2040

## Example entry for `config.yaml`:

No configuration changes required. This is a transparent optimization for all components.

External component authors using the deprecated methods will see compiler warnings:

```
warning: 'get_unit_of_measurement' is deprecated: Use get_unit_of_measurement_ref() instead
for better performance (avoids string copy). Will stop working in ESPHome 2026.5.0
[-Wdeprecated-declarations]
```

## Checklist:
  - [ ] The code change is tested and works locally.
  - [ ] Tests have been added to verify that the new code works (under `tests/` folder).
    - Existing component tests cover the updated code paths

If user exposed functionality or configuration variables are added/changed:
  - [ ] Documentation added/updated in [esphome-docs](https://github.com/esphome/esphome-docs).
    - N/A - PR summary will be automatically added to release notes
