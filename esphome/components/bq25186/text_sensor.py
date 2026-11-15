from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.cpp_generator import (
    AssignmentExpression,
    HexIntLiteral,
    VariableDeclarationExpression,
)
from esphome.types import ConfigType

from . import (
    BQ25186_CHILD_SCHEMA,
    CONF_BQ25186_ID,
    STATUS,
    STATUS_PUBLISHER,
    bq25186_ns,
)

DEPENDENCIES = ["bq25186"]

REG_STAT0 = 0
REG_STAT1 = 1

# (key, status register, bit shift, index of the field's value 0 in the C++ string table)
_FIELDS = (
    ("charge_status", REG_STAT0, 5, 0),
    ("ts_status", REG_STAT1, 3, 4),
)


def _add_text_status(
    hub: cg.MockObj, var: cg.MockObj, reg: int, shift: int, first: int
) -> None:
    """Publish status string first + the two-bit field at status[reg] >> shift, on change."""
    last = cg.MockObj(f"esphome__{var}__last")
    # 0xFF so the first poll publishes
    declaration = VariableDeclarationExpression(cg.uint8, "", last, static=True)
    STATUS_PUBLISHER.add(
        hub, AssignmentExpression(None, None, declaration, HexIntLiteral(0xFF))
    )
    STATUS_PUBLISHER.add(
        hub,
        bq25186_ns.publish_status_text(
            parent_ref(var), last, STATUS[reg] >> shift, first
        ),
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
        if (var := await sensors(key, None)) is not None:
            _add_text_status(hub, var, reg, shift, first)
