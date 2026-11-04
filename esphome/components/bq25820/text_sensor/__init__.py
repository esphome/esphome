from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ICON_THERMOMETER
from esphome.cpp_generator import (
    AssignmentExpression,
    HexIntLiteral,
    VariableDeclarationExpression,
)
from esphome.types import ConfigType

from .. import (
    CONF_BQ25820_ID,
    DUMP_STATUS,
    STATUS,
    STATUS_PUBLISHER,
    BQ25820BaseSchema,
    bq25820_ns,
)

DEPENDENCIES = ["bq25820"]

ICON_BATTERY_CHARGING = "mdi:battery-charging"

# (key, status register, bit shift, index of the field's value 0 in the C++ string table, icon)
_FIELDS = (
    ("charge_status", 0, 0, 0, ICON_BATTERY_CHARGING),
    ("temp_status", 1, 4, 8, ICON_THERMOMETER),
)


def _add_text_status(
    hub: cg.MockObj, var: cg.MockObj, reg: int, shift: int, first: int
) -> None:
    """Publish status string first + the three-bit field at status[reg] >> shift, on change."""
    last = cg.MockObj(f"esphome__{var}__last")
    # 0xFF so the first poll publishes
    declaration = VariableDeclarationExpression(cg.uint8, "", last, static=True)
    STATUS_PUBLISHER.add(
        hub, AssignmentExpression(None, None, declaration, HexIntLiteral(0xFF))
    )
    STATUS_PUBLISHER.add(
        hub,
        bq25820_ns.publish_status_text(
            parent_ref(var), last, STATUS[reg] >> shift, first
        ),
    )


CONFIG_SCHEMA = BQ25820BaseSchema.extend(
    {
        cv.Optional(key): text_sensor.text_sensor_schema(icon=icon)
        for key, *_, icon in _FIELDS
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_BQ25820_ID])
    sensors = text_sensor.sub_text_sensors(config)
    for key, reg, shift, first, _ in _FIELDS:
        if (var := await sensors(key, None)) is not None:
            _add_text_status(hub, var, reg, shift, first)
            DUMP_STATUS.add(hub, bq25820_ns.dump_status_entity(parent_ref(var)))
