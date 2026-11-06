"""Optional time-of-day controls without replacing existing numeric entities."""

from functools import partial

import esphome.codegen as cg
from esphome.components import datetime
import esphome.config_validation as cv
from esphome.const import CONF_ENTITY_CATEGORY, ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .. import (
    CONF_CONTROLLER_ID,
    CONF_PARAMETER,
    ResolDeltaSolCSPlus,
    register_setting_entity,
    setting_entity_defaults,
    validate_setting_controller,
    vbus_ns,
)
from ..cs_plus_numbers import NUMBER_PARAMETERS

DEPENDENCIES = ["vbus"]
VBusSettingTime = vbus_ns.class_("VBusSettingTime", datetime.TimeEntity)
TIME_PARAMETERS = {
    key: spec
    for key, spec in NUMBER_PARAMETERS.items()
    if spec.maximum < 1440
    and (key == "system_time" or "_start_time" in key or "_end_time" in key)
}


CONFIG_SCHEMA = cv.All(
    partial(setting_entity_defaults, clock=True),
    datetime.time_schema(VBusSettingTime).extend(
        {
            cv.GenerateID(CONF_CONTROLLER_ID): cv.use_id(ResolDeltaSolCSPlus),
            cv.Required(CONF_PARAMETER): cv.one_of(*TIME_PARAMETERS, lower=True),
            cv.Optional(
                CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_CONFIG
            ): cv.entity_category,
        }
    ),
)
FINAL_VALIDATE_SCHEMA = validate_setting_controller


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_VBUS_SETTING_TIME")
    controller = await cg.get_variable(config[CONF_CONTROLLER_ID])
    var = await datetime.new_datetime(
        config, controller, TIME_PARAMETERS[config[CONF_PARAMETER]].index
    )
    register_setting_entity(controller, var)
