---
name: breaking-changes
description: Decide whether an ESPHome change breaks public API and handle it - what counts as public C++ and Python API, the deprecation window, ESPDEPRECATED for C++, cv.rename_key and deprecation warnings for config keys, and the migration notes a PR needs. Use when removing, renaming or changing the signature of a config option, a public C++ member or a core Python helper.
---

# Public API and breaking changes

Full policy: https://developers.esphome.io/contributing/code/#public-api-and-breaking-changes

## What is public

- **C++ in components:** only features documented at [esphome.io](https://esphome.io) are public API.
  Undocumented `public` members are internal.
- **C++ core and base classes** (`esphome/core/`, `Component`, `Sensor`, etc.): all `public` members are
  public API.
- **Components with global accessors** (`global_api_server`, etc.): all `public` members are public API
  (except config setters).
- **Python:** all documented configuration options are public API. Python code in `esphome/core/` that
  existing core components use is stable API. Other Python code is internal unless explicitly documented
  for external component use.
- Changing the signature of a lambda that code generation injects is not a breaking change.

## Policy

- Aim for a **6-month deprecation window** when possible.
- Clean breaks are allowed for signature changes, deep refactorings and resource constraints.
- The migration path goes in the PR description (it generates the release notes).
- A blog post is required for core/base class changes or significant architectural changes.

Checklist:
- [ ] Clear justification (RAM/flash savings, architectural improvement)
- [ ] Explored non-breaking alternatives
- [ ] Added deprecation warnings if possible (`ESPDEPRECATED` for C++)
- [ ] Documented migration path in the PR description with before/after examples
- [ ] Updated all internal usage and esphome.io
- [ ] Tested backward compatibility during the deprecation period

## Deprecating C++

```cpp
// Remove before 2026.6.0
ESPDEPRECATED("Use new_method() instead. Removed in 2026.6.0", "2025.12.0")
void old_method() { this->new_method(); }
```

## Deprecating config keys

For a renamed key, use the shared `cv.rename_key` validator with `removed_in` (and `component` for
context); it warns and migrates automatically:
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
Cover the old syntax with a `validate.*.yaml` test (see the `component-tests` skill), and keep the language
schema correct (see the `config-validation` skill).
