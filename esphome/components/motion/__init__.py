import re

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.cpp_generator import MockObj, MockObjClass

CODEOWNERS = ["@esphome/core"]

DOMAIN = "motion"
IS_PLATFORM_COMPONENT = True

#  C++ namespace / class
motion_ns = cg.esphome_ns.namespace("motion")
MotionComponent = motion_ns.class_("MotionComponent", cg.PollingComponent)

AXES = ["x", "y", "z"]

CONF_AXIS_MAP = "axis_map"
CONF_MOTION_ID = "motion_id"

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


def _axis_map_to_matrix(config: dict[str, str]) -> list[int]:
    matrix = []
    for target_axis in AXES:
        source_axis = config[target_axis].lower()
        sign = -1 if source_axis.startswith("-") else 1
        source_axis = source_axis.removeprefix("+").removeprefix("-")

        row = [0, 0, 0]
        row[AXES.index(source_axis)] = sign
        matrix.extend(row)

    return matrix


#  Top-level CONFIG_SCHEMA
_CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_AXIS_MAP): cv.All(
            {cv.Required(k): cv.string_strict for k in AXES},
            _axis_map,
        ),
    }
).extend(cv.polling_component_schema("250ms"))


def motion_schema(class_: MockObjClass) -> cv.Schema:
    return _CONFIG_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(class_),
        }
    )


#  Code generation
async def register_motion_component(var: MockObj, config):
    await cg.register_component(var, config)
    if axis_map := config.get(CONF_AXIS_MAP):
        cg.add(var.set_matrix(_axis_map_to_matrix(axis_map)))


async def new_motion_component(config: dict) -> MockObj:
    var = cg.new_Pvariable(config[CONF_ID])
    await register_motion_component(var, config)
    return var
