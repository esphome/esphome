import esphome.codegen as cg
from esphome.components import ble_client, climate
import esphome.config_validation as cv
from esphome.const import CONF_UNIT_OF_MEASUREMENT

UNITS = {
    "f": "f",
    "c": "c",
}

CODEOWNERS = ["@buxtronix"]
DEPENDENCIES = ["ble_client"]

anova_ns = cg.esphome_ns.namespace("anova")
Anova = anova_ns.class_(
    "Anova", climate.Climate, ble_client.BLEClientNode, cg.PollingComponent
)

CONFIG_SCHEMA = (
    climate.climate_schema(Anova)
    .extend(
        {
            cv.Required(CONF_UNIT_OF_MEASUREMENT): cv.enum(UNITS),
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await climate.new_climate(config)
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
    cg.add(var.set_unit_of_measurement(config[CONF_UNIT_OF_MEASUREMENT]))
