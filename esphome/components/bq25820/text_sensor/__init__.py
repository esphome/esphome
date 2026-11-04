from dataclasses import dataclass

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
    CHARGER_STATUS_1,
    CHARGER_STATUS_2,
    CONF_BQ25820_ID,
    DUMP_STATUS,
    STATUS,
    STATUS_PUBLISHER,
    BQ25820BaseSchema,
    bq25820_ns,
)

DEPENDENCIES = ["bq25820"]

ICON_BATTERY_CHARGING = "mdi:battery-charging"


@dataclass(frozen=True, kw_only=True)
class _StatusText:
    """The three-bit field (status[register] >> shift), named by BQ25820StatusStrings[first + field]."""

    key: str
    register: int
    shift: int
    first: int
    icon: str


_FIELDS = (
    _StatusText(
        key="charge_status",
        register=CHARGER_STATUS_1,
        shift=0,
        first=0,
        icon=ICON_BATTERY_CHARGING,
    ),
    _StatusText(
        key="temp_status",
        register=CHARGER_STATUS_2,
        shift=4,
        first=8,
        icon=ICON_THERMOMETER,
    ),
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


CONFIG_SCHEMA = cv.All(
    BQ25820BaseSchema.extend(
        {
            cv.Optional(field.key): text_sensor.text_sensor_schema(icon=field.icon)
            for field in _FIELDS
        }
    ),
    cv.has_at_least_one_key(*(field.key for field in _FIELDS)),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_BQ25820_ID])
    sensors = text_sensor.sub_text_sensors(config)
    for field in _FIELDS:
        if (var := await sensors(field.key, None)) is not None:
            _add_text_status(hub, var, field.register, field.shift, field.first)
            DUMP_STATUS.add(hub, bq25820_ns.dump_status_entity(parent_ref(var)))
