import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.const import UNIT_COUNTS
import esphome.config_validation as cv
from esphome.const import CONF_COUNTS, CONF_TYPE, STATE_CLASS_MEASUREMENT
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import BANDS_41, BANDS_43, CONF_AS734X_ID, DOMAIN, AS734XComponent

DEPENDENCIES = [DOMAIN]

ICON_COUNTS = "mdi:counter"

_COUNTS_SENSOR = sensor.sensor_schema(
    unit_of_measurement=UNIT_COUNTS,
    icon=ICON_COUNTS,
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AS734X_ID): cv.use_id(AS734XComponent),
        cv.Optional(CONF_COUNTS, default={}): cv.Schema(
            {cv.Optional(band): _COUNTS_SENSOR for band in BANDS_43}
        ),
    }
)


def _final_validate(config: ConfigType) -> ConfigType:
    full = fv.full_config.get()
    hub = full.get_config_for_path(full.get_path_for_id(config[CONF_AS734X_ID])[:-1])
    if hub[CONF_TYPE] == "AS7341" and (
        extra := [band for band in config[CONF_COUNTS] if band not in BANDS_41]
    ):
        raise cv.Invalid(
            f"The AS7341 has no {', '.join(extra)} band", path=[CONF_COUNTS, extra[0]]
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_AS734X_ID])
    counts = config[CONF_COUNTS]
    for i, band in enumerate(BANDS_43):
        if (conf := counts.get(band)) is not None:
            sens = await sensor.new_sensor(conf)
            cg.add(hub.set_counts_sensor(sens, i))
