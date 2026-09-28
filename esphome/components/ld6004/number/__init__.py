import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import CONF_LD6004_ID, LD6004Component, ld6004_ns

CONF_Z_MIN: str = "z_min"
CONF_Z_MAX: str = "z_max"
CONF_HOLD_DELAY: str = "hold_delay"
CONF_LOW_POWER_SLEEP_TIME: str = "low_power_sleep_time"
CONF_DWELL_LIFETIME: str = "dwell_lifetime"
CONF_OUTPUT_INTERVAL: str = "output_interval"

DEPENDENCIES: list[str] = ["ld6004"]

LD6004Number: cg.MockObjClass = ld6004_ns.class_("LD6004Number", number.Number)
FIELDS: dict[str, tuple[float, float, float, str]] = {
    CONF_HOLD_DELAY: (0, 16777215, 1, "s"),
    CONF_Z_MIN: (-100, 100, 0.1, "m"),
    CONF_Z_MAX: (-100, 100, 0.1, "m"),
    CONF_LOW_POWER_SLEEP_TIME: (0, 16777215, 1, "ms"),
    CONF_DWELL_LIFETIME: (0, 16777215, 1, ""),
    CONF_OUTPUT_INTERVAL: (1, 16777215, 1, ""),
}
CONFIG_SCHEMA: cv.Schema = cv.Schema(
    {
        cv.GenerateID(CONF_LD6004_ID): cv.use_id(LD6004Component),
        **{
            cv.Optional(key): number.number_schema(
                LD6004Number,
                unit_of_measurement=details[3],
                entity_category="config",
                device_class="duration"
                if key in (CONF_HOLD_DELAY, CONF_LOW_POWER_SLEEP_TIME)
                else "distance"
                if key in (CONF_Z_MIN, CONF_Z_MAX)
                else "",
            )
            for key, details in FIELDS.items()
        },
    }
)


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_LD6004_NUMBER")
    conf: ConfigType | None
    hub: cg.MockObj = await cg.get_variable(config[CONF_LD6004_ID])
    for index, key in enumerate(FIELDS):
        if conf := config.get(key):
            entity: cg.MockObj = await number.new_number(
                conf,
                hub,
                index,
                min_value=FIELDS[key][0],
                max_value=FIELDS[key][1],
                step=FIELDS[key][2],
            )
            cg.add(hub.set_number_entity(index, entity))
