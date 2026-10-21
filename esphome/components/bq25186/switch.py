import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.types import ConfigType

from . import BQ25186_CHILD_SCHEMA, CONF_BQ25186_ID, bq25186_ns

DEPENDENCIES = ["bq25186"]

CONF_PG_GPO = "pg_gpo"

BQ25186PgGpoSwitch = bq25186_ns.class_("BQ25186PgGpoSwitch", switch.Switch)

CONFIG_SCHEMA = BQ25186_CHILD_SCHEMA.extend(
    {
        cv.Required(CONF_PG_GPO): switch.switch_schema(
            BQ25186PgGpoSwitch, default_restore_mode="DISABLED"
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_BQ25186_PG_GPO")
    hub = await cg.get_variable(config[CONF_BQ25186_ID])
    switches = switch.sub_switches(config, parent=hub)
    await switches(CONF_PG_GPO, hub.set_pg_gpo_switch)
