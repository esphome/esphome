from collections.abc import Callable
import re

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_ON_ERROR, CONF_ON_SUCCESS
from esphome.cpp_generator import MockObj, MockObjClass
from esphome.helpers import fnv1_hash_object_id

CODEOWNERS = ["@esphome/core"]

DOMAIN = "motion"
IS_PLATFORM_COMPONENT = True

#  C++ namespace / class
motion_ns = cg.esphome_ns.namespace("motion")
MotionComponent = motion_ns.class_("MotionComponent", cg.PollingComponent)

AXES = ["x", "y", "z"]

CONF_AXIS_MAP = "axis_map"
CONF_MOTION_ID = "motion_id"
CONF_TRANSFORM_MATRIX = "transform_matrix"

CalibrateLevelAction = motion_ns.class_("CalibrateLevelAction", automation.Action)
CalibrateHeadingAction = motion_ns.class_("CalibrateHeadingAction", automation.Action)
ClearCalibrationAction = motion_ns.class_("ClearCalibrationAction", automation.Action)

KEY_ACCELEROMETER = "accelerometer"
KEY_GYROSCOPE = "gyroscope"

SENSOR_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MOTION_ID): cv.use_id(MotionComponent),
    }
)

_AXIS_REGEX = re.compile(r"^[+-]?[xyz]$", re.IGNORECASE)


def _axis_map(config: dict) -> dict:
    errors = []
    for key, axis in config.items():
        if _AXIS_REGEX.fullmatch(axis) is None:
            errors.append(
                cv.Invalid(
                    "Each 'axis_map' config value must be one of 'x', 'y' or 'z' (optionally preceded by '+' or '-').",
                    path=[key],
                )
            )
    values = {x.lower().removeprefix("-").removeprefix("+") for x in config.values()}
    if values != set(AXES):
        errors.append(cv.Invalid("Each axis may be mapped only once"))
    if errors:
        raise cv.MultipleInvalid(errors)
    return config


def _axis_map_to_matrix(config: dict[str, str]) -> list[float]:
    matrix = []
    for target_axis in AXES:
        source_axis = config[target_axis].lower()
        sign = -1.0 if source_axis.startswith("-") else 1.0
        source_axis = source_axis.removeprefix("+").removeprefix("-")

        row = [0.0, 0.0, 0.0]
        row[AXES.index(source_axis)] = sign
        matrix.extend(row)

    return matrix


def _transform_matrix(value):
    """Accept a flat list of 9 floats or a 3x3 nested list."""
    if not isinstance(value, list) or len(value) == 0:
        raise cv.Invalid("Expected a list of 9 numbers or a 3x3 nested list")
    # Nested 3x3
    if isinstance(value[0], list):
        if len(value) != 3:
            raise cv.Invalid(f"3x3 matrix must have 3 rows, got {len(value)}")
        flat = []
        for i, row in enumerate(value):
            if not isinstance(row, list) or len(row) != 3:
                raise cv.Invalid("Each row must be a list of 3 numbers", path=[i])
            flat.extend(cv.float_(v) for v in row)
        return flat
    # Flat list
    if len(value) != 9:
        raise cv.Invalid(f"Flat matrix must have exactly 9 values, got {len(value)}")
    return [cv.float_(v) for v in value]


def _validate_matrix_options(config):
    if CONF_AXIS_MAP in config and CONF_TRANSFORM_MATRIX in config:
        raise cv.Invalid(
            f"'{CONF_AXIS_MAP}' and '{CONF_TRANSFORM_MATRIX}' are mutually exclusive"
        )
    return config


#  Top-level CONFIG_SCHEMA
_CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Optional(CONF_AXIS_MAP): cv.All(
                {cv.Required(k): cv.string_strict for k in AXES},
                _axis_map,
            ),
            cv.Optional(CONF_TRANSFORM_MATRIX): _transform_matrix,
        }
    )
    .extend(cv.polling_component_schema("250ms"))
    .add_extra(_validate_matrix_options)
)


def _add_data(has_accel: bool, has_gyro: bool) -> Callable[[dict], dict]:

    def validator(config):
        config = config.copy()
        config[KEY_ACCELEROMETER] = has_accel
        config[KEY_GYROSCOPE] = has_gyro
        return config

    return validator


def motion_schema(class_: MockObjClass, has_accel: bool, has_gyro: bool) -> cv.Schema:
    return _CONFIG_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(class_),
        }
    ).add_extra(_add_data(has_accel, has_gyro))


#  Code generation
async def register_motion_component(var: MockObj, config) -> None:
    await cg.register_component(var, config)
    # Set preference key for NVS save/restore (based on component ID)
    obj_id = config[CONF_ID].id
    pref_hash = fnv1_hash_object_id(obj_id)
    cg.add(var.set_calibration_key(pref_hash))
    if axis_map := config.get(CONF_AXIS_MAP):
        cg.add(var.set_matrix(_axis_map_to_matrix(axis_map)))
    elif transform_matrix := config.get(CONF_TRANSFORM_MATRIX):
        cg.add(var.set_matrix(transform_matrix))


async def new_motion_component(config: dict) -> MockObj:
    var = cg.new_Pvariable(config[CONF_ID])
    await register_motion_component(var, config)
    return var


# --- Actions ---

CONF_SAVE = "save"

CALIBRATE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(MotionComponent),
        cv.Optional(CONF_SAVE, default=False): cv.boolean,
        cv.Optional(CONF_ON_SUCCESS): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_ERROR): automation.validate_automation(single=True),
    }
)


async def _build_calibrate_action(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    if config.get(CONF_SAVE):
        cg.add(var.set_save(True))
    if on_success := config.get(CONF_ON_SUCCESS):
        await automation.build_automation(var.get_success_trigger(), [], on_success)
    if on_error := config.get(CONF_ON_ERROR):
        await automation.build_automation(var.get_error_trigger(), [], on_error)
    return var


@automation.register_action(
    "motion.calibrate_level",
    CalibrateLevelAction,
    CALIBRATE_ACTION_SCHEMA,
    synchronous=True,
)
async def calibrate_level_to_code(config, action_id, template_arg, args):
    return await _build_calibrate_action(config, action_id, template_arg, args)


@automation.register_action(
    "motion.calibrate_heading",
    CalibrateHeadingAction,
    CALIBRATE_ACTION_SCHEMA,
    synchronous=True,
)
async def calibrate_heading_to_code(config, action_id, template_arg, args):
    return await _build_calibrate_action(config, action_id, template_arg, args)


CLEAR_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(MotionComponent),
        cv.Optional(CONF_SAVE, default=False): cv.boolean,
    }
)


@automation.register_action(
    "motion.clear_calibration",
    ClearCalibrationAction,
    CLEAR_ACTION_SCHEMA,
    synchronous=True,
)
async def clear_calibration_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    if config.get(CONF_SAVE):
        cg.add(var.set_save(True))
    return var
