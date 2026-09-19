import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.sensor import (
    validate_device_class,
    validate_state_class,
    validate_unit_of_measurement,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_NAME,
    CONF_STATE_CLASS,
    CONF_UNIT_OF_MEASUREMENT,
    DEVICE_CLASS_DURATION,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
)
import esphome.final_validate as fv

from .. import (
    CONF_DECODE,
    CONF_RESPONSE_MONITOR,
    CONF_RS485_FRAME_ID,
    SENSOR_DECODES,
    RS485FrameHub,
    rs485_frame_ns,
)

AUTO_LOAD = ["rs485_frame"]

RS485FrameSensor = rs485_frame_ns.class_(
    "RS485FrameSensor", sensor.Sensor, cg.Component
)

ResponseMonitorStat = rs485_frame_ns.enum("ResponseMonitorStat")

# monitor_id: selects which response_monitor: entry a response_* decode reads from.
CONF_MONITOR_ID = "monitor_id"

RESPONSE_MONITOR_DECODES = {
    "response_success": ResponseMonitorStat.RESPONSE_MONITOR_STAT_SUCCESS,
    "response_fail": ResponseMonitorStat.RESPONSE_MONITOR_STAT_FAIL,
    "response_timeout": ResponseMonitorStat.RESPONSE_MONITOR_STAT_TIMEOUT,
    "response_not_applicable": ResponseMonitorStat.RESPONSE_MONITOR_STAT_NOT_APPLICABLE,
    "response_orphan": ResponseMonitorStat.RESPONSE_MONITOR_STAT_ORPHAN,
}

# Per-decode HA-conformant defaults. Counters that monotonically grow (and reset to 0 on
# reboot) use total_increasing so HA can render them as utility-style counters; the unit
# is the kind of thing being counted. Instantaneous gauges (queue depth, last keep-alive
# interval) use measurement so HA charts behave correctly. Users may still override any
# of these in their YAML; setdefault below leaves explicit user values untouched.
_DECODE_DEFAULTS = {
    "frames_received": {
        CONF_STATE_CLASS: STATE_CLASS_TOTAL_INCREASING,
        CONF_UNIT_OF_MEASUREMENT: "frames",
    },
    "crc_failures": {
        CONF_STATE_CLASS: STATE_CLASS_TOTAL_INCREASING,
        CONF_UNIT_OF_MEASUREMENT: "frames",
    },
    "commands_sent": {
        CONF_STATE_CLASS: STATE_CLASS_TOTAL_INCREASING,
        CONF_UNIT_OF_MEASUREMENT: "commands",
    },
    "command_drops": {
        CONF_STATE_CLASS: STATE_CLASS_TOTAL_INCREASING,
        CONF_UNIT_OF_MEASUREMENT: "commands",
    },
    "last_keepalive_ms": {
        CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
        CONF_UNIT_OF_MEASUREMENT: "ms",
        CONF_DEVICE_CLASS: DEVICE_CLASS_DURATION,
    },
    "queue_depth": {
        CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT,
        # The TX queue holds pending commands; match the commands_sent / command_drops unit.
        CONF_UNIT_OF_MEASUREMENT: "commands",
    },
}
for _decode in RESPONSE_MONITOR_DECODES:
    _DECODE_DEFAULTS[_decode] = {
        CONF_STATE_CLASS: STATE_CLASS_TOTAL_INCREASING,
        CONF_UNIT_OF_MEASUREMENT: "occurrences",
    }


# Defaults must be passed through the same validators sensor.sensor_schema uses so the
# state_class string becomes a StateClass enum (and device_class / unit are validated
# strings). Plain setdefault would leave raw strings in the config and codegen would
# refuse to convert them.
_DEFAULT_VALIDATORS = {
    CONF_STATE_CLASS: validate_state_class,
    CONF_DEVICE_CLASS: validate_device_class,
    CONF_UNIT_OF_MEASUREMENT: validate_unit_of_measurement,
}


def _apply_decode_defaults(config):
    # Apply the per-decode defaults only for keys the user did not set explicitly. We
    # cannot put these in sensor.sensor_schema(...) because that takes a single fixed
    # default per kwarg; the correct state_class/unit/device_class varies per decode.
    for key, value in _DECODE_DEFAULTS.get(config[CONF_DECODE], {}).items():
        if key in config:
            continue
        config[key] = _DEFAULT_VALIDATORS[key](value)
    return config


def _validate_monitor_id(config):
    is_response_monitor = config[CONF_DECODE] in RESPONSE_MONITOR_DECODES
    has_monitor_id = CONF_MONITOR_ID in config
    if is_response_monitor and not has_monitor_id:
        raise cv.Invalid(f"decode: {config[CONF_DECODE]} requires 'monitor_id'")
    if has_monitor_id and not is_response_monitor:
        raise cv.Invalid(
            "'monitor_id' is only valid with a response_* decode "
            f"({', '.join(RESPONSE_MONITOR_DECODES)})"
        )
    return config


# entity_category is uniform across all decodes (these are all hub diagnostics, not user
# state) and stays on the schema-level default. state_class / unit_of_measurement /
# device_class are decode-specific and handled by _apply_decode_defaults.
CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        RS485FrameSensor,
        accuracy_decimals=0,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ).extend(
        {
            cv.GenerateID(CONF_RS485_FRAME_ID): cv.use_id(RS485FrameHub),
            cv.Required(CONF_DECODE): cv.one_of(
                *SENSOR_DECODES, *RESPONSE_MONITOR_DECODES, lower=True
            ),
            # Which response_monitor: entry (by its name:) a response_* decode reads from.
            # Cross-checked against the referenced hub's own response_monitor: list in
            # FINAL_VALIDATE_SCHEMA, the same way button/__init__.py walks to its hub.
            cv.Optional(CONF_MONITOR_ID): cv.string,
        }
    ),
    _apply_decode_defaults,
    _validate_monitor_id,
)


def _final_validate(config):
    if CONF_MONITOR_ID not in config:
        return config
    full_config = fv.full_config.get()
    hub_path = full_config.get_path_for_id(config[CONF_RS485_FRAME_ID])[:-1]
    hub_config = full_config.get_config_for_path(hub_path)
    names = [entry[CONF_NAME] for entry in hub_config.get(CONF_RESPONSE_MONITOR, [])]
    if config[CONF_MONITOR_ID] not in names:
        raise cv.Invalid(
            f"monitor_id '{config[CONF_MONITOR_ID]}' is not a response_monitor entry on "
            f"the referenced hub — valid names are {sorted(names) or '(none declared)'}"
        )
    # Resolved here (not in to_code) because only final_validate can walk to the hub's own
    # config to see its response_monitor: list in declaration order.
    config["_monitor_entry_index"] = names.index(config[CONF_MONITOR_ID])
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    hub = await cg.get_variable(config[CONF_RS485_FRAME_ID])
    cg.add(var.set_parent(hub))
    if CONF_MONITOR_ID in config:
        cg.add(
            var.set_response_monitor(
                config["_monitor_entry_index"],
                RESPONSE_MONITOR_DECODES[config[CONF_DECODE]],
            )
        )
    else:
        cg.add(var.set_decode(SENSOR_DECODES[config[CONF_DECODE]]))
