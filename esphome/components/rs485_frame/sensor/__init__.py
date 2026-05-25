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
    CONF_STATE_CLASS,
    CONF_UNIT_OF_MEASUREMENT,
    DEVICE_CLASS_DURATION,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
)

from .. import (
    CONF_DECODE,
    CONF_RS485_FRAME_ID,
    SENSOR_DECODES,
    RS485FrameHub,
    rs485_frame_ns,
)

AUTO_LOAD = ["rs485_frame"]

RS485FrameSensor = rs485_frame_ns.class_(
    "RS485FrameSensor", sensor.Sensor, cg.Component
)

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
            cv.Required(CONF_DECODE): cv.one_of(*SENSOR_DECODES, lower=True),
        }
    ),
    _apply_decode_defaults,
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    hub = await cg.get_variable(config[CONF_RS485_FRAME_ID])
    cg.add(var.set_parent(hub))
    cg.add(var.set_decode(SENSOR_DECODES[config[CONF_DECODE]]))
