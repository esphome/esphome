import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_BRIGHTNESS, CONF_CONTRAST, CONF_FILTER, CONF_TYPE
from esphome.cpp_generator import LambdaExpression

from .. import (
    CONF_EXPOSURE,
    CONF_HUE,
    CONF_MIPI_CSI_ID,
    CONF_SATURATION,
    MIPI_CSI,
    CSICameraSensor,
    camera_sensor_ns,
)

SetterNumber = camera_sensor_ns.class_("SetterNumber", number.Number)

MIPI_CSI_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MIPI_CSI_ID): cv.use_id(CSICameraSensor),
        cv.Optional(CONF_BRIGHTNESS): number.number_schema(
            SetterNumber,
        ),
        cv.Optional(CONF_CONTRAST): number.number_schema(
            SetterNumber,
        ),
        cv.Optional(CONF_EXPOSURE): number.number_schema(
            SetterNumber,
        ),
        cv.Optional(CONF_FILTER): number.number_schema(
            SetterNumber,
        ),
        cv.Optional(CONF_HUE): number.number_schema(
            SetterNumber,
        ),
        cv.Optional(CONF_SATURATION): number.number_schema(
            SetterNumber,
        ),
    }
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        MIPI_CSI: MIPI_CSI_SCHEMA,
    },
)


async def to_code(config):
    if config[CONF_TYPE] == MIPI_CSI:
        sensor_component = await cg.get_variable(config[CONF_MIPI_CSI_ID])
        if number_config := config.get(CONF_BRIGHTNESS):
            n = await number.new_number(
                number_config,
                min_value=-128,
                max_value=127,
                step=1,
            )
            cg.add(sensor_component.set_brightness_number(n))
            cg.add(
                n.set_setter(
                    LambdaExpression(
                        f"{sensor_component}->number_brightness(value);",
                        [(float, "value")],
                    )
                )
            )
        if number_config := config.get(CONF_CONTRAST):
            n = await number.new_number(
                number_config,
                min_value=0,
                max_value=128,
                step=1,
            )
            cg.add(sensor_component.set_contrast_number(n))
            cg.add(
                n.set_setter(
                    LambdaExpression(
                        f"{sensor_component}->number_contrast(value);",
                        [(float, "value")],
                    )
                )
            )
        if number_config := config.get(CONF_EXPOSURE):
            n = await number.new_number(
                number_config,
                min_value=2,
                max_value=235,
                step=1,
            )
            cg.add(sensor_component.set_exposure_number(n))
            cg.add(
                n.set_setter(
                    LambdaExpression(
                        f"{sensor_component}->number_exposure(value);",
                        [(float, "value")],
                    )
                )
            )
        if number_config := config.get(CONF_FILTER):
            n = await number.new_number(
                number_config,
                min_value=2,
                max_value=20,
                step=1,
            )
            cg.add(sensor_component.set_filter_number(n))
            cg.add(
                n.set_setter(
                    LambdaExpression(
                        f"{sensor_component}->number_filter(value);",
                        [(float, "value")],
                    )
                )
            )
        if number_config := config.get(CONF_HUE):
            n = await number.new_number(
                number_config,
                min_value=0,
                max_value=359,
                step=1,
            )
            cg.add(sensor_component.set_hue_number(n))
            cg.add(
                n.set_setter(
                    LambdaExpression(
                        f"{sensor_component}->number_hue(value);",
                        [(float, "value")],
                    )
                )
            )
        if number_config := config.get(CONF_SATURATION):
            n = await number.new_number(
                number_config,
                min_value=0,
                max_value=128,
                step=1,
            )
            cg.add(sensor_component.set_saturation_number(n))
            cg.add(
                n.set_setter(
                    LambdaExpression(
                        f"{sensor_component}->number_saturation(value);",
                        [(float, "value")],
                    )
                )
            )
