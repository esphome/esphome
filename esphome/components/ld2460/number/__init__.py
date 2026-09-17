import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_DISTANCE,
    ENTITY_CATEGORY_CONFIG,
    ICON_RULER,
    UNIT_DEGREES,
    UNIT_METER,
)
from esphome.types import ConfigType

from .. import CONF_LD2460_ID, LD2460Component, ld2460_ns

CONF_INSTALLATION_HEIGHT = "installation_height"
CONF_INSTALLATION_ANGLE = "installation_angle"
CONF_MAX_DETECTION_DISTANCE = "max_detection_distance"
CONF_DETECTION_ANGLE_MIN = "detection_angle_min"
CONF_DETECTION_ANGLE_MAX = "detection_angle_max"

InstallationHeightNumber = ld2460_ns.class_("InstallationHeightNumber", number.Number)
InstallationAngleNumber = ld2460_ns.class_("InstallationAngleNumber", number.Number)
DetectionDistanceNumber = ld2460_ns.class_("DetectionDistanceNumber", number.Number)
DetectionAngleMinNumber = ld2460_ns.class_("DetectionAngleMinNumber", number.Number)
DetectionAngleMaxNumber = ld2460_ns.class_("DetectionAngleMaxNumber", number.Number)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
        cv.GenerateID(CONF_LD2460_ID): cv.use_id(LD2460Component),
        cv.Optional(CONF_INSTALLATION_HEIGHT): number.number_schema(
            InstallationHeightNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            unit_of_measurement=UNIT_METER,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_RULER,
        ),
        cv.Optional(CONF_INSTALLATION_ANGLE): number.number_schema(
            InstallationAngleNumber,
            unit_of_measurement=UNIT_DEGREES,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_MAX_DETECTION_DISTANCE): number.number_schema(
            DetectionDistanceNumber,
            device_class=DEVICE_CLASS_DISTANCE,
            unit_of_measurement=UNIT_METER,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_RULER,
        ),
        cv.Optional(CONF_DETECTION_ANGLE_MIN): number.number_schema(
            DetectionAngleMinNumber,
            unit_of_measurement=UNIT_DEGREES,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_DETECTION_ANGLE_MAX): number.number_schema(
            DetectionAngleMaxNumber,
            unit_of_measurement=UNIT_DEGREES,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    ld2460_component = await cg.get_variable(config[CONF_LD2460_ID])

    if inst_height_config := config.get(CONF_INSTALLATION_HEIGHT):
        n = await number.new_number(
            inst_height_config,
            min_value=0.0,
            max_value=10.0,
            step=0.01,
        )
        await cg.register_parented(n, config[CONF_LD2460_ID])
        cg.add(ld2460_component.set_installation_height_number(n))

    if inst_angle_config := config.get(CONF_INSTALLATION_ANGLE):
        n = await number.new_number(
            inst_angle_config,
            min_value=0,
            max_value=90,
            step=1,
        )
        await cg.register_parented(n, config[CONF_LD2460_ID])
        cg.add(ld2460_component.set_installation_angle_number(n))

    if det_dist_config := config.get(CONF_MAX_DETECTION_DISTANCE):
        n = await number.new_number(
            det_dist_config,
            min_value=0.1,
            max_value=6.0,
            step=0.1,
        )
        await cg.register_parented(n, config[CONF_LD2460_ID])
        cg.add(ld2460_component.set_detection_distance_number(n))

    if det_angle_min_config := config.get(CONF_DETECTION_ANGLE_MIN):
        n = await number.new_number(
            det_angle_min_config,
            min_value=-60,
            max_value=360,
            step=1,
        )
        await cg.register_parented(n, config[CONF_LD2460_ID])
        cg.add(ld2460_component.set_detection_angle_min_number(n))

    if det_angle_max_config := config.get(CONF_DETECTION_ANGLE_MAX):
        n = await number.new_number(
            det_angle_max_config,
            min_value=-60,
            max_value=360,
            step=1,
        )
        await cg.register_parented(n, config[CONF_LD2460_ID])
        cg.add(ld2460_component.set_detection_angle_max_number(n))
