import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_TIMEOUT,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    DEVICE_CLASS_ILLUMINANCE,
    UNIT_SECOND,
    UNIT_PERCENT,
    ENTITY_CATEGORY_CONFIG,
)
from .. import CONF_LD2410_ID, LD2410Component, ld2410_ns

GateThresholdNumber = ld2410_ns.class_("GateThresholdNumber", number.Number)
LightThresholdNumber = ld2410_ns.class_("LightThresholdNumber", number.Number)
MaxDistanceTimeoutNumber = ld2410_ns.class_("MaxDistanceTimeoutNumber", number.Number)

CONF_MAX_MOVE_DISTANCE_GATE = "max_move_distance_gate"
CONF_MAX_STILL_DISTANCE_GATE = "max_still_distance_gate"
CONF_LIGHT_THRESHOLD = "light_threshold"

CONF_STILL_THRESHOLDS = [f"g{x}_still_threshold" for x in range(9)]

CONF_MOVE_THRESHOLDS = [f"g{x}_move_threshold" for x in range(9)]


def validate(config):
    has_some_timeout_configs = (
        CONF_TIMEOUT in config
        or CONF_MAX_MOVE_DISTANCE_GATE in config
        or CONF_MAX_STILL_DISTANCE_GATE in config
    )
    has_all_timeout_configs = (
        CONF_TIMEOUT in config
        and CONF_MAX_MOVE_DISTANCE_GATE in config
        and CONF_MAX_STILL_DISTANCE_GATE in config
    )
    if has_some_timeout_configs and not has_all_timeout_configs:
        raise cv.Invalid(
            f"{CONF_TIMEOUT}, {CONF_MAX_MOVE_DISTANCE_GATE} and {CONF_MAX_STILL_DISTANCE_GATE} are all must be set"
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
            MaxDistanceTimeoutNumber,
            unit_of_measurement=UNIT_SECOND,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:clock-time-eight-outline",
        ),
        cv.Optional(CONF_MAX_MOVE_DISTANCE_GATE): number.number_schema(
            MaxDistanceTimeoutNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:motion-sensor",
        ),
        cv.Optional(CONF_MAX_STILL_DISTANCE_GATE): number.number_schema(
            MaxDistanceTimeoutNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:motion-sensor-off",
        ),
        cv.Optional(CONF_LIGHT_THRESHOLD): number.number_schema(
            LightThresholdNumber,
            device_class=DEVICE_CLASS_ILLUMINANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:car-light-high",
        ),
    }
)

for i in range(9):
    CONFIG_SCHEMA = CONFIG_SCHEMA.extend(
        cv.Schema(
            {
                cv.Optional(CONF_MOVE_THRESHOLDS[i]): number.number_schema(
                    GateThresholdNumber,
                    device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                    unit_of_measurement=UNIT_PERCENT,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon="mdi:motion-sensor",
                ),
                cv.Optional(CONF_STILL_THRESHOLDS[i]): number.number_schema(
                    GateThresholdNumber,
                    device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                    unit_of_measurement=UNIT_PERCENT,
                    entity_category=ENTITY_CATEGORY_CONFIG,
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
        await cg.register_parented(n, config[CONF_LD2410_ID])
        cg.add(ld2410_component.set_timeout_number(n))
    if CONF_MAX_MOVE_DISTANCE_GATE in config:
        n = await number.new_number(
            config[CONF_MAX_MOVE_DISTANCE_GATE], min_value=2, max_value=8, step=1
        )
        await cg.register_parented(n, config[CONF_LD2410_ID])
        cg.add(ld2410_component.set_max_move_distance_gate_number(n))
    if CONF_MAX_STILL_DISTANCE_GATE in config:
        n = await number.new_number(
            config[CONF_MAX_STILL_DISTANCE_GATE], min_value=2, max_value=8, step=1
        )
        await cg.register_parented(n, config[CONF_LD2410_ID])
        cg.add(ld2410_component.set_max_still_distance_gate_number(n))
    if CONF_LIGHT_THRESHOLD in config:
        n = await number.new_number(
            config[CONF_LIGHT_THRESHOLD], min_value=0, max_value=255, step=1
        )
        await cg.register_parented(n, config[CONF_LD2410_ID])
        cg.add(ld2410_component.set_light_threshold_number(n))
    for x in range(9):
        move = CONF_MOVE_THRESHOLDS[x]
        still = CONF_STILL_THRESHOLDS[x]
        if move in config:
            move_config = config[move]
            n = cg.new_Pvariable(move_config[CONF_ID], x)
            await number.register_number(
                n, move_config, min_value=0, max_value=100, step=1
            )
            await cg.register_parented(n, config[CONF_LD2410_ID])
            cg.add(ld2410_component.set_gate_move_threshold_number(x, n))
        if still in config:
            still_config = config[still]
            n = cg.new_Pvariable(still_config[CONF_ID], x)
            await number.register_number(
                n, still_config, min_value=0, max_value=100, step=1
            )
            await cg.register_parented(n, config[CONF_LD2410_ID])
            cg.add(ld2410_component.set_gate_still_threshold_number(x, n))
