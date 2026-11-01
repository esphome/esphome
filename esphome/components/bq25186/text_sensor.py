from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.types import ConfigType

from . import BQ25186_CHILD_SCHEMA, CONF_BQ25186_ID, add_status_statement

DEPENDENCIES = ["bq25186"]

REG_STAT0 = 0
REG_STAT1 = 1

# (key, status register, bit shift, index of the field's value 0 in the C++ string table)
_FIELDS = (
    ("charge_status", REG_STAT0, 5, 0),
    ("ts_status", REG_STAT1, 3, 4),
)

CONFIG_SCHEMA = cv.All(
    BQ25186_CHILD_SCHEMA.extend(
        {cv.Optional(key): text_sensor.text_sensor_schema() for key, *_ in _FIELDS}
    ),
    cv.has_at_least_one_key(*(key for key, *_ in _FIELDS)),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_BQ25186_ID])
    sensors = text_sensor.sub_text_sensors(config)
    for key, reg, shift, first in _FIELDS:
        if (var := await sensors(key, None)) is None:
            continue
        # 0xFF so the first poll publishes
        last = f"esphome__{var}__last"
        add_status_statement(
            hub,
            f"static uint8_t {last} = 0xFF;\n"
            f"bq25186::publish_status_text({parent_ref(var)}, {last}, "
            f"status[{reg}] >> {shift}, {first});",
            text=True,
        )
