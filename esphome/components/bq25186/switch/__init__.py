import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_ID
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import CONF_BQ25186_ID, CONF_PG_MODE, BQ25186Component, bq25186_ns

DEPENDENCIES = ["bq25186"]

CONF_PG_GPO = "pg_gpo"

BQ25186PgGpoSwitch = bq25186_ns.class_("BQ25186PgGpoSwitch", switch.Switch)


def validate_pg_gpo_general_config(config: ConfigType) -> ConfigType:
    # Get the BQ25186Component config to check the pg_mode
    full_config = fv.full_config.get()
    parent_bq_configs = full_config.get("bq25186")

    if parent_bq_configs is None:
        # This shouldn't happen due to DEPENDENCIES = ["bq25186"], but check anyway
        raise cv.Invalid(
            "PG/GPO switch requires the BQ25186 component to be configured"
        )

    # Find the specific BQ25186Component config that matches the ID referenced by this switch
    parent_bq_config = None
    for bq_config in parent_bq_configs:
        if bq_config.get(CONF_ID) == config[CONF_BQ25186_ID]:
            parent_bq_config = bq_config
            break

    if parent_bq_config is None:
        raise cv.Invalid("PG/GPO switch references an unknown BQ25186 component")

    # Force pg_mode to GPO (1) since the switch will control it
    parent_bq_config[CONF_PG_MODE] = 1
    return config


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BQ25186_ID): cv.use_id(BQ25186Component),
        cv.Required(CONF_PG_GPO): switch.switch_schema(
            BQ25186PgGpoSwitch, default_restore_mode="DISABLED"
        ),
    }
)

FINAL_VALIDATE_SCHEMA = validate_pg_gpo_general_config


async def to_code(config):
    parent = await cg.get_variable(config[CONF_BQ25186_ID])
    sw = await switch.new_switch(config[CONF_PG_GPO])
    await cg.register_parented(sw, config[CONF_BQ25186_ID])
    cg.add(parent.set_pg_gpo_switch(sw))
