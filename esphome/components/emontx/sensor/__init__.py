import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS,
    CONF_ID,
    CONF_STATE_CLASS,
    CONF_UNIT_OF_MEASUREMENT,
    DEVICE_CLASS_APPARENT_POWER,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_EMPTY,
    UNIT_HERTZ,
    UNIT_PULSES,
    UNIT_VOLT,
    UNIT_VOLT_AMPS,
    UNIT_WATT,
    UNIT_WATT_HOURS,
)
from esphome.types import ConfigType

from .. import CONF_EMONTX_ID, CONF_TAG_NAME, EmonTx, emontx_ns

EmonTxSensor = emontx_ns.class_("EmonTxSensor", sensor.Sensor, cg.Component)

# Known emonTx/avrdb JSON tag conventions, gathered from real firmware
# (see https://github.com/openenergymonitor/avrdb_firmware), used to decide
# whether each tag below requires a numeric index or may also appear bare:
#
#   Tag family    Bare (no index)          Numeric-indexed
#   -----------   -----------------------  ----------------------------------
#   P (power)     no                       P1, P2, ... (multi-channel boards)
#   E (energy)    no                       E1, E2, ...
#   V (voltage)   Vrms (NOT matched here,  V1, V2, V3 (per-phase boards)
#                 doesn't fit "V"+digits)
#   I (current)   no                       I1, I2, ...
#   T (temp.)     no                       T1, T2, ...
#   F (frequency) F (single mains freq.)   not seen indexed
#   PULSE         pulse (single-CT boards) PULSE1, PULSE2, ... (other variants)
#   PF (power     not seen bare            PF1, PF2, ... (currently unused/
#     factor)                              commented out in avrdb firmware)
#   AP (apparent  not seen bare            AP1, AP2, ... (not an avrdb tag at
#     power)                               all; avrdb uses "VA"+index instead,
#                                           itself currently unused/commented
#                                           out; "AP" is kept here for other
#                                           firmware/integrations using it)
#
# This is why a bare "PULSE" resolves to proper defaults below, but bare
# "PF"/"AP" fall back to generic defaults instead: only PULSE has a
# confirmed bare-tag use in real, currently-shipping firmware.

# Define sensor type configurations by prefix
SENSOR_CONFIGS = {
    "P": {
        CONF_UNIT_OF_MEASUREMENT: UNIT_WATT,
        CONF_DEVICE_CLASS: DEVICE_CLASS_POWER,
        CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
        CONF_ACCURACY_DECIMALS: 0,
    },
    "E": {
        CONF_UNIT_OF_MEASUREMENT: UNIT_WATT_HOURS,
        CONF_DEVICE_CLASS: DEVICE_CLASS_ENERGY,
        CONF_STATE_CLASS: STATE_CLASS_TOTAL_INCREASING,
        CONF_ACCURACY_DECIMALS: 0,
    },
    "V": {
        CONF_UNIT_OF_MEASUREMENT: UNIT_VOLT,
        CONF_DEVICE_CLASS: DEVICE_CLASS_VOLTAGE,
        CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
        CONF_ACCURACY_DECIMALS: 2,
    },
    "I": {
        CONF_UNIT_OF_MEASUREMENT: UNIT_AMPERE,
        CONF_DEVICE_CLASS: DEVICE_CLASS_CURRENT,
        CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
        CONF_ACCURACY_DECIMALS: 2,
    },
    "T": {
        CONF_UNIT_OF_MEASUREMENT: UNIT_CELSIUS,
        CONF_DEVICE_CLASS: DEVICE_CLASS_TEMPERATURE,
        CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
        CONF_ACCURACY_DECIMALS: 2,
    },
}

# Tags reported once, without a numeric index (e.g. "F"), matched exactly
# rather than by prefix.
EXACT_TAG_CONFIGS = {
    "F": {
        CONF_UNIT_OF_MEASUREMENT: UNIT_HERTZ,
        CONF_DEVICE_CLASS: DEVICE_CLASS_FREQUENCY,
        CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
        CONF_ACCURACY_DECIMALS: 2,
    },
}

# Pattern-based configurations. The remainder after the prefix must be a
# non-empty numeric index (like V1/I1/E1), so e.g. "APPLE" doesn't collide
# with the "AP" prefix and a bare "PF"/"AP" (no index) doesn't match.
# "PULSE" is the exception: some emonTx firmware (e.g. avrdb-based single-CT
# variants) reports a single pulse counter as a bare "pulse" tag with no
# numeric index at all, so that pattern also accepts an empty suffix.
PATTERNS_ALLOWING_BARE_TAG = {"PULSE"}

PATTERN_CONFIGS = {
    "PULSE": {
        CONF_UNIT_OF_MEASUREMENT: UNIT_PULSES,
        CONF_DEVICE_CLASS: DEVICE_CLASS_ENERGY,
        CONF_STATE_CLASS: STATE_CLASS_TOTAL_INCREASING,
        CONF_ACCURACY_DECIMALS: 0,
    },
    "PF": {
        CONF_UNIT_OF_MEASUREMENT: UNIT_EMPTY,
        CONF_DEVICE_CLASS: DEVICE_CLASS_POWER_FACTOR,
        CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
        CONF_ACCURACY_DECIMALS: 2,
    },
    "AP": {
        CONF_UNIT_OF_MEASUREMENT: UNIT_VOLT_AMPS,
        CONF_DEVICE_CLASS: DEVICE_CLASS_APPARENT_POWER,
        CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
        CONF_ACCURACY_DECIMALS: 2,
    },
}

# BASE_SCHEMA intentionally omits state_class and accuracy_decimals defaults.
# Passing them to sensor_schema() would register them via cv.Optional(key, default=...),
# making them always present in the validated config dict and preventing
# apply_tag_defaults from overriding them with the correct per-prefix values.
# They are injected by apply_tag_defaults below, after running through the
# same validators sensor_schema() would use (see _DEFAULT_VALIDATORS) so the
# values are code-generation-ready.
BASE_SCHEMA = sensor.sensor_schema(EmonTxSensor).extend(
    {
        cv.GenerateID(CONF_EMONTX_ID): cv.use_id(EmonTx),
        cv.Required(CONF_TAG_NAME): cv.string,
    }
)


_DEFAULT_VALIDATORS = {
    CONF_STATE_CLASS: sensor.validate_state_class,
    CONF_DEVICE_CLASS: sensor.validate_device_class,
    CONF_UNIT_OF_MEASUREMENT: sensor.validate_unit_of_measurement,
}


def _apply_defaults(config: ConfigType, defaults: dict) -> None:
    """Inject defaults into config, skipping keys already set by the user.
    Values are run through the same validators sensor_schema() would use, so
    they are code-generation-ready and a typo'd constant fails validation
    instead of shipping silently."""
    for key, value in defaults.items():
        if key not in config:
            if key in _DEFAULT_VALIDATORS:
                value = _DEFAULT_VALIDATORS[key](value)
            config[key] = value


def apply_tag_defaults(config: ConfigType) -> ConfigType:
    """Apply defaults based on tag prefix if applicable, but don't restrict any tags."""
    tag = config[CONF_TAG_NAME]
    tag_upper = tag.upper()

    if (exact_config := EXACT_TAG_CONFIGS.get(tag_upper)) is not None:
        _apply_defaults(config, exact_config)
        return config

    for pattern, pattern_config in PATTERN_CONFIGS.items():
        suffix = tag_upper[len(pattern) :]
        bare_ok = not suffix and pattern in PATTERNS_ALLOWING_BARE_TAG
        if tag_upper.startswith(pattern) and (suffix.isdigit() or bare_ok):
            _apply_defaults(config, pattern_config)
            return config

    # Only apply defaults for known prefixes with numeric indices (e.g. E1, V2, T3)
    if len(tag) >= 2:
        prefix = tag_upper[0]
        if prefix in SENSOR_CONFIGS and tag[1:].isdigit():
            _apply_defaults(config, SENSOR_CONFIGS[prefix])
            return config

    # Fall back to generic defaults for tags with no known prefix
    _apply_defaults(
        config,
        {
            CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
            CONF_ACCURACY_DECIMALS: 0,
        },
    )
    return config


CONFIG_SCHEMA = cv.All(BASE_SCHEMA, apply_tag_defaults)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    hub = await cg.get_variable(config[CONF_EMONTX_ID])
    cg.add(hub.register_sensor(config[CONF_TAG_NAME], var))
