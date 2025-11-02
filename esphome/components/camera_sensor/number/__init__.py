import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_BLUE,
    CONF_BRIGHTNESS,
    CONF_CONTRAST,
    CONF_FILTER,
    CONF_GREEN,
    CONF_RED,
    CONF_TYPE,
)
from esphome.cpp_generator import LambdaExpression

from .. import CONF_HUE, CONF_ISP_ID, CONF_SATURATION, ISP, MIPI_CSI, camera_sensor_ns

SetterNumber = camera_sensor_ns.class_("SetterNumber", number.Number)

NUMBER_CONFIGS = {
    CONF_BRIGHTNESS: (-128, 127, 1),
    CONF_CONTRAST: (0, 128, 1),
    CONF_FILTER: (2, 20, 1),
    CONF_HUE: (0, 359, 1),
    CONF_SATURATION: (0, 128, 1),
    CONF_RED: (-4.0, 4.0, 0.1),
    CONF_GREEN: (-4.0, 4.0, 0.1),
    CONF_BLUE: (-4.0, 4.0, 0.1),
}

MIPI_CSI_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ISP_ID): cv.use_id(ISP),
        **{cv.Optional(k): number.number_schema(SetterNumber) for k in NUMBER_CONFIGS},
    }
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        MIPI_CSI: MIPI_CSI_SCHEMA,
    },
)


async def to_code(config):
    if config[CONF_TYPE] == MIPI_CSI:
        sensor_component = await cg.get_variable(config[CONF_ISP_ID])
        for number_config, (min_v, max_v, step_v) in NUMBER_CONFIGS.items():
            if (conf := config.get(number_config)) is None:
                continue

            name = number_config.replace("CONF_", "").lower()
            n = await number.new_number(
                conf, min_value=min_v, max_value=max_v, step=step_v
            )
            cg.add(getattr(sensor_component, f"set_{name}_number")(n))
            cg.add(
                n.set_setter(
                    LambdaExpression(
                        f"{sensor_component}->number_{name}(value);", [(float, "value")]
                    ),
                )
            )
