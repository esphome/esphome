import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv

from .. import CONF_BQ25186_ID, BQ25186Component, bq25186_ns

DEPENDENCIES = ["bq25186"]

CONF_PG_GPO = "pg_gpo"
USE_BQ25186_PG_GPO_SWITCH = "USE_BQ25186_PG_GPO_SWITCH"

BQ25186PgGpoSwitch = bq25186_ns.class_("BQ25186PgGpoSwitch", switch.Switch)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BQ25186_ID): cv.use_id(BQ25186Component),
        cv.Required(CONF_PG_GPO): switch.switch_schema(
            BQ25186PgGpoSwitch, default_restore_mode="DISABLED"
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_BQ25186_ID])
    cg.add_define(USE_BQ25186_PG_GPO_SWITCH)
    sw = await switch.new_switch(config[CONF_PG_GPO])
    await cg.register_parented(sw, config[CONF_BQ25186_ID])
    cg.add(parent.set_pg_gpo_switch(sw))
