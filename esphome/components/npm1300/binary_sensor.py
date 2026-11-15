from dataclasses import dataclass

from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_BATTERY_CHARGING,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_PROBLEM,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from esphome.cpp_generator import HexIntLiteral
from esphome.types import ConfigType

from . import (
    CHARGE_STATUS,
    CONF_NPM1300_ID,
    DUMP,
    DUMP_TAG,
    ERROR_REASON,
    NPM1300_CHILD_SCHEMA,
    STATUS_PUBLISHER,
    VBUS_STATUS,
)

DEPENDENCIES = ["npm1300"]


@dataclass(frozen=True, kw_only=True)
class _StatusBits:
    """On when any bit of mask is set in the status register."""

    key: str
    register: cg.MockObj
    mask: int
    name: str
    device_class: str = cv.UNDEFINED
    entity_category: str = cv.UNDEFINED


_FIELDS = (
    _StatusBits(
        key="vbus_present",
        register=VBUS_STATUS,
        mask=0x01,
        name="VBUS Present",
        device_class=DEVICE_CLASS_POWER,
    ),
    _StatusBits(
        key="vbus_current_limit_active",
        register=VBUS_STATUS,
        mask=0x02,
        name="VBUS Current Limit Active",
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    # Trickle, constant current or constant voltage charging
    _StatusBits(
        key="charging",
        register=CHARGE_STATUS,
        mask=0x1C,
        name="Charging",
        device_class=DEVICE_CLASS_BATTERY_CHARGING,
    ),
    _StatusBits(
        key="charge_complete",
        register=CHARGE_STATUS,
        mask=0x02,
        name="Charge Complete",
    ),
    _StatusBits(
        key="charger_error",
        register=ERROR_REASON,
        mask=0xFF,
        name="Charger Error",
        device_class=DEVICE_CLASS_PROBLEM,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
)

CONFIG_SCHEMA = NPM1300_CHILD_SCHEMA.extend(
    {
        cv.Optional(field.key): binary_sensor.binary_sensor_schema(
            device_class=field.device_class, entity_category=field.entity_category
        )
        for field in _FIELDS
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_NPM1300_ID])
    sensors = binary_sensor.sub_binary_sensors(config)
    for field in _FIELDS:
        if (var := await sensors(field.key, None)) is not None:
            state = (field.register & HexIntLiteral(field.mask)) != 0
            STATUS_PUBLISHER.add(hub, parent_ref(var).publish_state(state))
            DUMP.add(
                hub,
                binary_sensor.binary_sensor_ns.log_binary_sensor(
                    DUMP_TAG,
                    "  ",
                    cg.RawExpression(f'LOG_STR_LITERAL("{field.name}")'),
                    parent_ref(var),
                ),
            )
