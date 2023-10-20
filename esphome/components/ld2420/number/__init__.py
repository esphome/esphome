import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_TIMEOUT,
    DEVICE_CLASS_DISTANCE,
    UNIT_SECOND,
    ENTITY_CATEGORY_CONFIG,
    ICON_MOTION_SENSOR,
    ICON_TIMELAPSE,
)
from .. import CONF_LD2420_ID, LD2420Component, ld2420_ns

LD2420TimeoutNumber = ld2420_ns.class_("LD2420TimeoutNumber", number.Number)
LD2420MinDistanceNumber = ld2420_ns.class_("LD2420MinDistanceNumber", number.Number)
LD2420MaxDistanceNumber = ld2420_ns.class_("LD2420MaxDistanceNumber", number.Number)
LD2420GateSelectNumber = ld2420_ns.class_("LD2420GateSelectNumber", number.Number)
LD2420MoveThresholdNumber = ld2420_ns.class_("LD2420MoveThresholdNumber", number.Number)
LD2420StillThresholdNumber = ld2420_ns.class_(
    "LD2420StillThresholdNumber", number.Number
)

CONF_MIN_GATE_DISTANCE = "min_gate_distance"
CONF_MAX_GATE_DISTANCE = "max_gate_distance"
CONF_STILL_THRESHOLD = "still_threshold"
CONF_MOVE_THRESHOLD = "move_threshold"
CONF_GATE_SELECT = "gate_select"
SET_GATE_GROUP = "gate_group"
TIMEOUT_GROUP = "timeout"


CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2420_ID): cv.use_id(LD2420Component),
    cv.Inclusive(CONF_TIMEOUT, TIMEOUT_GROUP): number.number_schema(
        LD2420TimeoutNumber,
        unit_of_measurement=UNIT_SECOND,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_TIMELAPSE,
    ),
    cv.Inclusive(CONF_MIN_GATE_DISTANCE, TIMEOUT_GROUP): number.number_schema(
        LD2420MinDistanceNumber,
        device_class=DEVICE_CLASS_DISTANCE,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_MOTION_SENSOR,
    ),
    cv.Inclusive(CONF_MAX_GATE_DISTANCE, TIMEOUT_GROUP): number.number_schema(
        LD2420MaxDistanceNumber,
        device_class=DEVICE_CLASS_DISTANCE,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_MOTION_SENSOR,
    ),
    cv.Inclusive(CONF_GATE_SELECT, SET_GATE_GROUP): number.number_schema(
        LD2420GateSelectNumber,
        device_class=DEVICE_CLASS_DISTANCE,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_MOTION_SENSOR,
    ),
    cv.Inclusive(CONF_MOVE_THRESHOLD, SET_GATE_GROUP): number.number_schema(
        LD2420MoveThresholdNumber,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_MOTION_SENSOR,
    ),
    cv.Inclusive(CONF_STILL_THRESHOLD, SET_GATE_GROUP): number.number_schema(
        LD2420StillThresholdNumber,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_MOTION_SENSOR,
    ),
}


async def to_code(config):
    LD2420_component = await cg.get_variable(config[CONF_LD2420_ID])
    if gate_config_group := config.get(CONF_TIMEOUT):
        n = await number.new_number(
            gate_config_group, min_value=0, max_value=255, step=5
        )
        await cg.register_parented(n, config[CONF_LD2420_ID])
        cg.add(LD2420_component.set_gate_timeout_number(n))
    if min_distance_gate_config := config.get(CONF_MIN_GATE_DISTANCE):
        n = await number.new_number(
            min_distance_gate_config, min_value=0, max_value=15, step=1
        )
        await cg.register_parented(n, config[CONF_LD2420_ID])
        cg.add(LD2420_component.set_min_gate_distance_number(n))
    if max_distance_gate_config := config.get(CONF_MAX_GATE_DISTANCE):
        n = await number.new_number(
            max_distance_gate_config, min_value=1, max_value=15, step=1
        )
        await cg.register_parented(n, config[CONF_LD2420_ID])
        cg.add(LD2420_component.set_max_gate_distance_number(n))
    if gate_number := config.get(CONF_GATE_SELECT):
        n = await number.new_number(gate_number, min_value=0, max_value=15, step=1)
        await cg.register_parented(n, config[CONF_LD2420_ID])
        cg.add(LD2420_component.set_gate_select_number(n))

    if gate_still_threshold := config.get(CONF_STILL_THRESHOLD):
        n = cg.new_Pvariable(gate_still_threshold[CONF_ID])
        await number.register_number(
            n, gate_still_threshold, min_value=0, max_value=65535, step=25
        )
        await cg.register_parented(n, config[CONF_LD2420_ID])
        cg.add(LD2420_component.set_still_threshold_number(n))
    if gate_move_threshold := config.get(CONF_MOVE_THRESHOLD):
        n = cg.new_Pvariable(gate_move_threshold[CONF_ID])
        await number.register_number(
            n, gate_move_threshold, min_value=0, max_value=65535, step=25
        )
        await cg.register_parented(n, config[CONF_LD2420_ID])
        cg.add(LD2420_component.set_move_threshold_number(n))
