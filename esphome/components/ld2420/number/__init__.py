import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_DISTANCE,
    ENTITY_CATEGORY_CONFIG,
    ICON_MOTION_SENSOR,
    ICON_SCALE,
    ICON_TIMELAPSE,
    UNIT_SECOND,
)

from .. import CONF_LD2420_ID, LD2420Component, ld2420_ns

LD2420TimeoutNumber = ld2420_ns.class_("LD2420TimeoutNumber", number.Number)
LD2420MoveSensFactorNumber = ld2420_ns.class_(
    "LD2420MoveSensFactorNumber", number.Number
)
LD2420StillSensFactorNumber = ld2420_ns.class_(
    "LD2420StillSensFactorNumber", number.Number
)
LD2420MinDistanceNumber = ld2420_ns.class_("LD2420MinDistanceNumber", number.Number)
LD2420MaxDistanceNumber = ld2420_ns.class_("LD2420MaxDistanceNumber", number.Number)
LD2420GateSelectNumber = ld2420_ns.class_("LD2420GateSelectNumber", number.Number)
LD2420MoveThresholdNumbers = ld2420_ns.class_(
    "LD2420MoveThresholdNumbers", number.Number
)
LD2420StillThresholdNumbers = ld2420_ns.class_(
    "LD2420StillThresholdNumbers", number.Number
)
CONF_MIN_GATE_DISTANCE = "min_gate_distance"
CONF_MAX_GATE_DISTANCE = "max_gate_distance"
CONF_STILL_THRESHOLD = "still_threshold"
CONF_MOVE_THRESHOLD = "move_threshold"
CONF_GATE_MOVE_SENSITIVITY = "gate_move_sensitivity"
CONF_GATE_STILL_SENSITIVITY = "gate_still_sensitivity"
CONF_GATE_SELECT = "gate_select"
CONF_PRESENCE_TIMEOUT = "presence_timeout"
GATE_GROUP = "gate_group"
TIMEOUT_GROUP = "timeout_group"


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LD2420_ID): cv.use_id(LD2420Component),
        cv.Inclusive(CONF_PRESENCE_TIMEOUT, TIMEOUT_GROUP): number.number_schema(
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
        cv.Inclusive(CONF_GATE_SELECT, GATE_GROUP): number.number_schema(
            LD2420GateSelectNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_MOTION_SENSOR,
        ),
        cv.Inclusive(CONF_STILL_THRESHOLD, GATE_GROUP): number.number_schema(
            LD2420StillThresholdNumbers,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_MOTION_SENSOR,
        ),
        cv.Inclusive(CONF_MOVE_THRESHOLD, GATE_GROUP): number.number_schema(
            LD2420MoveThresholdNumbers,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_MOTION_SENSOR,
        ),
        cv.Optional(CONF_GATE_MOVE_SENSITIVITY): number.number_schema(
            LD2420MoveSensFactorNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SCALE,
        ),
        cv.Optional(CONF_GATE_STILL_SENSITIVITY): number.number_schema(
            LD2420StillSensFactorNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SCALE,
        ),
    }
)
CONFIG_SCHEMA = CONFIG_SCHEMA.extend(
    {
        cv.Optional(f"gate_{x}"): (
            {
                cv.Required(CONF_MOVE_THRESHOLD): number.number_schema(
                    LD2420MoveThresholdNumbers,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon=ICON_MOTION_SENSOR,
                ),
                cv.Required(CONF_STILL_THRESHOLD): number.number_schema(
                    LD2420StillThresholdNumbers,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon=ICON_MOTION_SENSOR,
                ),
            }
        )
        for x in range(16)
    }
)


async def to_code(config):
    LD2420_component = await cg.get_variable(config[CONF_LD2420_ID])
    if gate_timeout_config := config.get(CONF_PRESENCE_TIMEOUT):
        n = await number.new_number(
            gate_timeout_config, min_value=0, max_value=255, step=5
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
    if gate_move_sensitivity_config := config.get(CONF_GATE_MOVE_SENSITIVITY):
        n = await number.new_number(
            gate_move_sensitivity_config, min_value=0.05, max_value=1, step=0.025
        )
        await cg.register_parented(n, config[CONF_LD2420_ID])
        cg.add(LD2420_component.set_gate_move_sensitivity_factor_number(n))
    if gate_still_sensitivity_config := config.get(CONF_GATE_STILL_SENSITIVITY):
        n = await number.new_number(
            gate_still_sensitivity_config, min_value=0.05, max_value=1, step=0.025
        )
        await cg.register_parented(n, config[CONF_LD2420_ID])
        cg.add(LD2420_component.set_gate_still_sensitivity_factor_number(n))
    if config.get(CONF_GATE_SELECT):
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
            cg.add(LD2420_component.set_gate_still_threshold_numbers(0, n))
        if gate_move_threshold := config.get(CONF_MOVE_THRESHOLD):
            n = cg.new_Pvariable(gate_move_threshold[CONF_ID])
            await number.register_number(
                n, gate_move_threshold, min_value=0, max_value=65535, step=25
            )
            await cg.register_parented(n, config[CONF_LD2420_ID])
            cg.add(LD2420_component.set_gate_move_threshold_numbers(0, n))
    else:
        for x in range(16):
            if gate_conf := config.get(f"gate_{x}"):
                move_config = gate_conf[CONF_MOVE_THRESHOLD]
                n = cg.new_Pvariable(move_config[CONF_ID], x)
                await number.register_number(
                    n, move_config, min_value=0, max_value=65535, step=25
                )
                await cg.register_parented(n, config[CONF_LD2420_ID])
                cg.add(LD2420_component.set_gate_move_threshold_numbers(x, n))

                still_config = gate_conf[CONF_STILL_THRESHOLD]
                n = cg.new_Pvariable(still_config[CONF_ID], x)
                await number.register_number(
                    n, still_config, min_value=0, max_value=65535, step=25
                )
                await cg.register_parented(n, config[CONF_LD2420_ID])
                cg.add(LD2420_component.set_gate_still_threshold_numbers(x, n))
