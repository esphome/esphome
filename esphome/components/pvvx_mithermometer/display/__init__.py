import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client, display, time
from esphome.const import (
    CONF_AUTO_CLEAR_ENABLED,
    CONF_DISCONNECT_DELAY,
    CONF_ID,
    CONF_LAMBDA,
    CONF_TIME_ID,
    CONF_VALIDITY_PERIOD,
)

DEPENDENCIES = ["ble_client"]

pvvx_ns = cg.esphome_ns.namespace("pvvx_mithermometer")
PVVXDisplay = pvvx_ns.class_(
    "PVVXDisplay", cg.PollingComponent, ble_client.BLEClientNode
)
PVVXDisplayRef = PVVXDisplay.operator("ref")

CONFIG_SCHEMA = (
    display.BASIC_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(PVVXDisplay),
            cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
            cv.Optional(CONF_AUTO_CLEAR_ENABLED, default=True): cv.boolean,
            cv.Optional(CONF_DISCONNECT_DELAY, default="5s"): cv.positive_time_period,
            cv.Optional(CONF_VALIDITY_PERIOD, default="5min"): cv.All(
                cv.positive_time_period_seconds,
                cv.Range(max=cv.TimePeriod(seconds=65535)),
            ),
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)
    await ble_client.register_ble_node(var, config)
    cg.add(var.set_disconnect_delay(config[CONF_DISCONNECT_DELAY].total_milliseconds))
    cg.add(var.set_auto_clear(config[CONF_AUTO_CLEAR_ENABLED]))
    cg.add(var.set_validity_period(config[CONF_VALIDITY_PERIOD].total_seconds))

    if CONF_TIME_ID in config:
        time_ = await cg.get_variable(config[CONF_TIME_ID])
        cg.add(var.set_time(time_))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(PVVXDisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
