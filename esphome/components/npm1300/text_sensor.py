from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ICON_BATTERY
from esphome.cpp_generator import (
    AssignmentExpression,
    HexIntLiteral,
    VariableDeclarationExpression,
)
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
    npm1300_ns,
)

DEPENDENCIES = ["npm1300"]

CONF_CHARGER_STATUS = "charger_status"

CONFIG_SCHEMA = NPM1300_CHILD_SCHEMA.extend(
    {
        cv.Optional(CONF_CHARGER_STATUS): text_sensor.text_sensor_schema(
            icon=ICON_BATTERY,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_NPM1300_ID])
    text_sensors = text_sensor.sub_text_sensors(config)
    if (var := await text_sensors(CONF_CHARGER_STATUS, None)) is None:
        return
    last = cg.MockObj(f"esphome__{var}__last")
    # 0xFF so the first update publishes
    declaration = VariableDeclarationExpression(cg.uint8, "", last, static=True)
    STATUS_PUBLISHER.add(
        hub, AssignmentExpression(None, None, declaration, HexIntLiteral(0xFF))
    )
    STATUS_PUBLISHER.add(
        hub,
        npm1300_ns.publish_charger_status(
            parent_ref(var), last, VBUS_STATUS, CHARGE_STATUS, ERROR_REASON
        ),
    )
    DUMP.add(
        hub,
        text_sensor.text_sensor_ns.log_text_sensor(
            DUMP_TAG,
            "  ",
            cg.RawExpression('LOG_STR_LITERAL("Charger Status")'),
            parent_ref(var),
        ),
    )
