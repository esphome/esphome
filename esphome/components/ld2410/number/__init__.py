import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_TIMEOUT,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_ILLUMINANCE,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    ENTITY_CATEGORY_CONFIG,
    ICON_LIGHTBULB,
    ICON_MOTION_SENSOR,
    ICON_TIMELAPSE,
    UNIT_PERCENT,
    UNIT_SECOND,
)

from .. import CONF_LD2410_ID, LD2410Component, ld2410_ns

GateThresholdNumber = ld2410_ns.class_("GateThresholdNumber", number.Number)
LightThresholdNumber = ld2410_ns.class_("LightThresholdNumber", number.Number)
MaxDistanceTimeoutNumber = ld2410_ns.class_("MaxDistanceTimeoutNumber", number.Number)

CONF_MAX_MOVE_DISTANCE_GATE = "max_move_distance_gate"
CONF_MAX_STILL_DISTANCE_GATE = "max_still_distance_gate"
CONF_LIGHT_THRESHOLD = "light_threshold"
CONF_STILL_THRESHOLD = "still_threshold"
CONF_MOVE_THRESHOLD = "move_threshold"

TIMEOUT_GROUP = "timeout"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LD2410_ID): cv.use_id(LD2410Component),
        cv.Inclusive(CONF_TIMEOUT, TIMEOUT_GROUP): number.number_schema(
            MaxDistanceTimeoutNumber,
            unit_of_measurement=UNIT_SECOND,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_TIMELAPSE,
        ),
        cv.Inclusive(CONF_MAX_MOVE_DISTANCE_GATE, TIMEOUT_GROUP): number.number_schema(
            MaxDistanceTimeoutNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_MOTION_SENSOR,
        ),
        cv.Inclusive(CONF_MAX_STILL_DISTANCE_GATE, TIMEOUT_GROUP): number.number_schema(
            MaxDistanceTimeoutNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_MOTION_SENSOR,
        ),
        cv.Optional(CONF_LIGHT_THRESHOLD): number.number_schema(
            LightThresholdNumber,
            device_class=DEVICE_CLASS_ILLUMINANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_LIGHTBULB,
        ),
    }
)

CONFIG_SCHEMA = CONFIG_SCHEMA.extend(
    {
        cv.Optional(f"g{x}"): cv.Schema(
            {
                cv.Required(CONF_MOVE_THRESHOLD): number.number_schema(
                    GateThresholdNumber,
                    device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                    unit_of_measurement=UNIT_PERCENT,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon=ICON_MOTION_SENSOR,
                ),
                cv.Required(CONF_STILL_THRESHOLD): number.number_schema(
                    GateThresholdNumber,
                    device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                    unit_of_measurement=UNIT_PERCENT,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon=ICON_MOTION_SENSOR,
                ),
            }
        )
        for x in range(9)
    }
)


async def to_code(config):
    ld2410_component = await cg.get_variable(config[CONF_LD2410_ID])
    if timeout_config := config.get(CONF_TIMEOUT):
        n = await number.new_number(
            timeout_config, min_value=0, max_value=65535, step=1
        )
        await cg.register_parented(n, config[CONF_LD2410_ID])
        cg.add(ld2410_component.set_timeout_number(n))
    if max_move_distance_gate_config := config.get(CONF_MAX_MOVE_DISTANCE_GATE):
        n = await number.new_number(
            max_move_distance_gate_config, min_value=2, max_value=8, step=1
        )
        await cg.register_parented(n, config[CONF_LD2410_ID])
        cg.add(ld2410_component.set_max_move_distance_gate_number(n))
    if max_still_distance_gate_config := config.get(CONF_MAX_STILL_DISTANCE_GATE):
        n = await number.new_number(
            max_still_distance_gate_config, min_value=2, max_value=8, step=1
        )
        await cg.register_parented(n, config[CONF_LD2410_ID])
        cg.add(ld2410_component.set_max_still_distance_gate_number(n))
    if light_threshold_config := config.get(CONF_LIGHT_THRESHOLD):
        n = await number.new_number(
            light_threshold_config, min_value=0, max_value=255, step=1
        )
        await cg.register_parented(n, config[CONF_LD2410_ID])
        cg.add(ld2410_component.set_light_threshold_number(n))
    for x in range(9):
        if gate_conf := config.get(f"g{x}"):
            move_config = gate_conf[CONF_MOVE_THRESHOLD]
            n = cg.new_Pvariable(move_config[CONF_ID], x)
            await number.register_number(
                n, move_config, min_value=0, max_value=100, step=1
            )
            await cg.register_parented(n, config[CONF_LD2410_ID])
            cg.add(ld2410_component.set_gate_move_threshold_number(x, n))

            still_config = gate_conf[CONF_STILL_THRESHOLD]
            n = cg.new_Pvariable(still_config[CONF_ID], x)
            await number.register_number(
                n, still_config, min_value=0, max_value=100, step=1
            )
            await cg.register_parented(n, config[CONF_LD2410_ID])
            cg.add(ld2410_component.set_gate_still_threshold_number(x, n))
