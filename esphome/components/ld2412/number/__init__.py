import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MOVE_THRESHOLD,
    CONF_STILL_THRESHOLD,
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

from .. import CONF_LD2412_ID, LD2412_ns, LD2412Component

GateThresholdNumber = LD2412_ns.class_("GateThresholdNumber", number.Number)
LightThresholdNumber = LD2412_ns.class_("LightThresholdNumber", number.Number)
MaxDistanceTimeoutNumber = LD2412_ns.class_("MaxDistanceTimeoutNumber", number.Number)

CONF_LIGHT_THRESHOLD = "light_threshold"
CONF_MAX_DISTANCE_GATE = "max_distance_gate"
CONF_MIN_DISTANCE_GATE = "min_distance_gate"

TIMEOUT_GROUP = "timeout"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LD2412_ID): cv.use_id(LD2412Component),
        cv.Optional(CONF_LIGHT_THRESHOLD): number.number_schema(
            LightThresholdNumber,
            device_class=DEVICE_CLASS_ILLUMINANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_LIGHTBULB,
        ),
        cv.Optional(CONF_MAX_DISTANCE_GATE): number.number_schema(
            MaxDistanceTimeoutNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_MOTION_SENSOR,
        ),
        cv.Optional(CONF_MIN_DISTANCE_GATE): number.number_schema(
            MaxDistanceTimeoutNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_MOTION_SENSOR,
        ),
        cv.Optional(CONF_TIMEOUT): number.number_schema(
            MaxDistanceTimeoutNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_TIMELAPSE,
            unit_of_measurement=UNIT_SECOND,
        ),
    }
)

CONFIG_SCHEMA = CONFIG_SCHEMA.extend(
    {
        cv.Optional(f"gate_{x}"): (
            {
                cv.Required(CONF_MOVE_THRESHOLD): number.number_schema(
                    GateThresholdNumber,
                    device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon=ICON_MOTION_SENSOR,
                    unit_of_measurement=UNIT_PERCENT,
                ),
                cv.Required(CONF_STILL_THRESHOLD): number.number_schema(
                    GateThresholdNumber,
                    device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon=ICON_MOTION_SENSOR,
                    unit_of_measurement=UNIT_PERCENT,
                ),
            }
        )
        for x in range(14)
    }
)


async def to_code(config):
    LD2412_component = await cg.get_variable(config[CONF_LD2412_ID])
    if light_threshold_config := config.get(CONF_LIGHT_THRESHOLD):
        n = await number.new_number(
            light_threshold_config, min_value=0, max_value=255, step=1
        )
        await cg.register_parented(n, config[CONF_LD2412_ID])
        cg.add(LD2412_component.set_light_threshold_number(n))
    if max_distance_gate_config := config.get(CONF_MAX_DISTANCE_GATE):
        n = await number.new_number(
            max_distance_gate_config, min_value=2, max_value=13, step=1
        )
        await cg.register_parented(n, config[CONF_LD2412_ID])
        cg.add(LD2412_component.set_max_distance_gate_number(n))
    if min_distance_gate_config := config.get(CONF_MIN_DISTANCE_GATE):
        n = await number.new_number(
            min_distance_gate_config, min_value=1, max_value=12, step=1
        )
        await cg.register_parented(n, config[CONF_LD2412_ID])
        cg.add(LD2412_component.set_min_distance_gate_number(n))
    for x in range(14):
        if gate_conf := config.get(f"gate_{x}"):
            move_config = gate_conf[CONF_MOVE_THRESHOLD]
            n = cg.new_Pvariable(move_config[CONF_ID], x)
            await number.register_number(
                n, move_config, min_value=0, max_value=100, step=1
            )
            await cg.register_parented(n, config[CONF_LD2412_ID])
            cg.add(LD2412_component.set_gate_move_threshold_number(x, n))
            still_config = gate_conf[CONF_STILL_THRESHOLD]
            n = cg.new_Pvariable(still_config[CONF_ID], x)
            await number.register_number(
                n, still_config, min_value=0, max_value=100, step=1
            )
            await cg.register_parented(n, config[CONF_LD2412_ID])
            cg.add(LD2412_component.set_gate_still_threshold_number(x, n))
    if timeout_config := config.get(CONF_TIMEOUT):
        n = await number.new_number(timeout_config, min_value=0, max_value=900, step=1)
        await cg.register_parented(n, config[CONF_LD2412_ID])
        cg.add(LD2412_component.set_timeout_number(n))
