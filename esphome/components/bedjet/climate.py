import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, ble_client, time
from esphome.const import (
    CONF_ID,
    CONF_RECEIVE_TIMEOUT,
    CONF_TIME_ID,
)

CODEOWNERS = ["@jhansche"]
DEPENDENCIES = ["ble_client"]

bedjet_ns = cg.esphome_ns.namespace("bedjet")
Bedjet = bedjet_ns.class_(
    "Bedjet", climate.Climate, ble_client.BLEClientNode, cg.PollingComponent
)

CONFIG_SCHEMA = (
    climate.CLIMATE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(Bedjet),
            cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
            cv.Optional(
                CONF_RECEIVE_TIMEOUT, default="0s"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.polling_component_schema("30s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)
    await ble_client.register_ble_node(var, config)
    if CONF_TIME_ID in config:
        time_ = await cg.get_variable(config[CONF_TIME_ID])
        cg.add(var.set_time_id(time_))
    if CONF_RECEIVE_TIMEOUT in config:
        cg.add(var.set_status_timeout(config[CONF_RECEIVE_TIMEOUT]))
