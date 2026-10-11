import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_PERIOD, CONF_RESTORE_MODE
from esphome.core import TimePeriod
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import CONF_PURIFIER
from .climate import AirConditioner, midea_ac_ns

DEPENDENCIES = ["climate.midea"]

CONF_MIDEA_ID = "midea_id"
CONF_STATUS_MAX_AGE = "status_max_age"
PurifierSwitch = midea_ac_ns.class_("PurifierSwitch", switch.Switch)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_MIDEA_ID): cv.use_id(AirConditioner),
        cv.Optional(CONF_PURIFIER): switch.switch_schema(
            PurifierSwitch, block_inverted=True
        ).extend(
            {
                cv.Optional(CONF_STATUS_MAX_AGE, default="15s"): cv.All(
                    cv.positive_time_period_milliseconds,
                    cv.Range(
                        min=TimePeriod(milliseconds=1),
                        max=TimePeriod(milliseconds=0x7FFFFFFF),
                    ),
                ),
                cv.Optional(CONF_RESTORE_MODE, default="DISABLED"): cv.enum(
                    {"DISABLED": switch.RESTORE_MODES["DISABLED"]}, upper=True
                ),
            }
        ),
    }
)


def _final_validate(config: ConfigType) -> ConfigType:
    if (purifier := config.get(CONF_PURIFIER)) is not None:
        full_config = fv.full_config.get()
        parent_path = full_config.get_path_for_id(config[CONF_MIDEA_ID])[:-1]
        parent = full_config.get_config_for_path(parent_path)
        if purifier[CONF_STATUS_MAX_AGE] < parent[CONF_PERIOD]:
            raise cv.Invalid(
                "status_max_age must be at least the referenced Midea climate's period",
                path=[CONF_PURIFIER, CONF_STATUS_MAX_AGE],
            )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    if (purifier := config.get(CONF_PURIFIER)) is not None:
        hub = await cg.get_variable(config[CONF_MIDEA_ID])
        cg.add_define("USE_MIDEA_PURIFIER")
        switches = switch.sub_switches(config)
        await switches(
            CONF_PURIFIER,
            hub.set_purifier_switch,
            hub,
            purifier[CONF_STATUS_MAX_AGE].total_milliseconds,
        )
