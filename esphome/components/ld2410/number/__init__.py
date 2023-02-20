import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_TIMEOUT,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    UNIT_METER,
    UNIT_SECOND,
    UNIT_PERCENT,
)
from .. import CONF_LD2410_ID, LD2410Component, ld2410_ns

VirtualNumber = ld2410_ns.class_("VirtualNumber", number.Number)

CONF_MAX_MOVE_DISTANCE = "max_move_distance"
CONF_MAX_STILL_DISTANCE = "max_still_distance"

CONF_STILL_THRESHOLDS = [f"g{x}_still_threshold" for x in range(9)]

CONF_MOVE_THRESHOLDS = [f"g{x}_move_threshold" for x in range(9)]


def validate(config):
    has_some_timeout_configs = (
        CONF_TIMEOUT in config
        or CONF_MAX_MOVE_DISTANCE in config
        or CONF_MAX_STILL_DISTANCE in config
    )
    has_all_timeout_configs = (
        CONF_TIMEOUT in config
        and CONF_MAX_MOVE_DISTANCE in config
        and CONF_MAX_STILL_DISTANCE in config
    )
    if has_some_timeout_configs and not has_all_timeout_configs:
        raise cv.Invalid(
            f"{CONF_TIMEOUT}, {CONF_MAX_MOVE_DISTANCE} and {CONF_MAX_STILL_DISTANCE} are all must be set"
        )
    for x in range(9):
        move = CONF_MOVE_THRESHOLDS[x]
        still = CONF_STILL_THRESHOLDS[x]
        if (move in config or still in config) and (
            move not in config or still not in config
        ):
            raise cv.Invalid(f"{move} and {still} are all must be set")
    return config


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LD2410_ID): cv.use_id(LD2410Component),
        cv.Optional(CONF_TIMEOUT): number.number_schema(
            VirtualNumber,
            unit_of_measurement=UNIT_SECOND,
            icon="mdi:clock-time-eight-outline",
        ),
        cv.Optional(CONF_MAX_MOVE_DISTANCE): number.number_schema(
            VirtualNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            unit_of_measurement=UNIT_METER,
            icon="mdi:motion-sensor",
        ),
        cv.Optional(CONF_MAX_STILL_DISTANCE): number.number_schema(
            VirtualNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            unit_of_measurement=UNIT_METER,
            icon="mdi:motion-sensor-off",
        ),
    }
)

for i in range(9):
    CONFIG_SCHEMA = CONFIG_SCHEMA.extend(
        cv.Schema(
            {
                cv.Optional(CONF_MOVE_THRESHOLDS[i]): number.number_schema(
                    VirtualNumber,
                    device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                    unit_of_measurement=UNIT_PERCENT,
                    icon="mdi:motion-sensor",
                ),
                cv.Optional(CONF_STILL_THRESHOLDS[i]): number.number_schema(
                    VirtualNumber,
                    device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                    unit_of_measurement=UNIT_PERCENT,
                    icon="mdi:motion-sensor-off",
                ),
            }
        )
    )

CONFIG_SCHEMA = cv.All(CONFIG_SCHEMA, validate)


async def to_code(config):
    ld2410_component = await cg.get_variable(config[CONF_LD2410_ID])
    if CONF_TIMEOUT in config:
        n = await number.new_number(
            config[CONF_TIMEOUT], min_value=0, max_value=65535, step=1
        )
        cg.add(ld2410_component.set_timeout_number(n))
    if CONF_MAX_MOVE_DISTANCE in config:
        n = await number.new_number(
            config[CONF_MAX_MOVE_DISTANCE], min_value=0.75, max_value=6, step=0.75
        )
        cg.add(ld2410_component.set_max_move_distance_number(n))
    if CONF_MAX_STILL_DISTANCE in config:
        n = await number.new_number(
            config[CONF_MAX_STILL_DISTANCE], min_value=0.75, max_value=6, step=0.75
        )
        cg.add(ld2410_component.set_max_still_distance_number(n))
    for x in range(9):
        move = CONF_MOVE_THRESHOLDS[x]
        still = CONF_STILL_THRESHOLDS[x]
        if move in config:
            n = await number.new_number(
                config[move], min_value=0, max_value=100, step=1
            )
            cg.add(ld2410_component.set_gate_move_threshold_number(x, n))
        if still in config:
            n = await number.new_number(
                config[still], min_value=0, max_value=100, step=1
            )
            cg.add(ld2410_component.set_gate_still_threshold_number(x, n))
