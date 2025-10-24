# Linting Fixes Applied to packet_transport

## Issues Fixed

### 1. Import Error - CONF_BINARY_SENSORS
**Error:** `Constant CONF_BINARY_SENSORS has already been defined in const.py`

**Fix:** Import `CONF_BINARY_SENSORS` from `esphome.const` instead of redefining it locally.

**Before:**
```python
from esphome.const import (
    CONF_BROADCAST,
    # ... other imports
)
# Later in file:
CONF_BINARY_SENSORS = "binary_sensors"
```

**After:**
```python
from esphome.const import (
    CONF_BINARY_SENSORS,
    CONF_BROADCAST,
    # ... other imports
)
```

### 2. Unused Import Warning - CONF_ID
**Error:** `F401 CONF_ID imported but unused`

**Fix:** Add explicit re-export using `CONF_ID as CONF_ID` syntax.

**Before:**
```python
from esphome.const import (
    CONF_ID,
    # ...
)
```

**After:**
```python
from esphome.const import (
    CONF_ID as CONF_ID,  # Re-exported for use in sensor.py and binary_sensor.py
    # ...
)
```

### 3. Nested If Statements - sensor_validation()
**Error:** `SIM102 Use a single if statement instead of nested if statements`

**Fix:** Combined nested if statements using `and`.

**Before:**
```python
def sensor_validation(config):
    if CONF_BROADCAST in config and config[CONF_BROADCAST] is not None:
        if CONF_REMOTE_ID in config:
            raise cv.Invalid("Cannot specify both broadcast and remote_id")
    return config
```

**After:**
```python
def sensor_validation(config):
    if (
        CONF_BROADCAST in config
        and config[CONF_BROADCAST] is not None
        and CONF_REMOTE_ID in config
    ):
        raise cv.Invalid("Cannot specify both broadcast and remote_id")
    return config
```

### 4. Nested If Statements - validate_()
**Error:** `SIM102 Use a single if statement instead of nested if statements`

**Fix:** Combined nested if statements using `and`.

**Before:**
```python
for provider in providers:
    if CONF_PING_PONG in provider and provider[CONF_PING_PONG]:
        if CONF_KEY not in config[CONF_ENCRYPTION] and CONF_KEY not in provider:
            raise cv.Invalid(...)
```

**After:**
```python
for provider in providers:
    if (
        CONF_PING_PONG in provider
        and provider[CONF_PING_PONG]
        and CONF_KEY not in config[CONF_ENCRYPTION]
        and CONF_KEY not in provider
    ):
        raise cv.Invalid(...)
```

### 5. Missing Constant - CONF_PING_PONG_ENABLE
**Error:** Referenced in `binary_sensor.py` but not defined/exported

**Fix:** Added `CONF_PING_PONG_ENABLE` constant definition.

**Added:**
```python
CONF_PING_PONG_ENABLE = "ping_pong_enable"
```

## Summary

All linting errors have been resolved:
- ✅ Removed duplicate constant definition
- ✅ Fixed unused import with explicit re-export
- ✅ Simplified nested if statements (2 instances)
- ✅ Added missing constant definition

The code now follows ESPHome linting standards and should pass CI/CD checks.
