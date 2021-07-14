import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, ble_client
from esphome.const import CONF_ID

CODEOWNERS = ["@buxtronix"]
DEPENDENCIES = ["ble_client"]

anova_ns = cg.esphome_ns.namespace("anova")
Anova = anova_ns.class_(
    "Anova", climate.Climate, ble_client.BLEClientNode, cg.PollingComponent
)

CONFIG_SCHEMA = (
    climate.CLIMATE_SCHEMA.extend({cv.GenerateID(): cv.declare_id(Anova)})
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.polling_component_schema("60s"))
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield climate.register_climate(var, config)
    yield ble_client.register_ble_node(var, config)
