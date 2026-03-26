import esphome.codegen as cg
from esphome.components import one_wire, text_sensor
import esphome.config_validation as cv

DEPENDENCIES = ["one_wire"]

dallas_ibutton_ns = cg.esphome_ns.namespace("dallas_ibutton")

DallasIbuttonComponent = dallas_ibutton_ns.class_(
    "DallasIbuttonComponent",
    text_sensor.TextSensor,
    cg.PollingComponent,
    one_wire.OneWireDevice,
)

CONF_RESET_VALUE_AFTER = "reset_value_after"

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema(
        DallasIbuttonComponent,
    )
    .extend(
        {
            cv.Optional(CONF_RESET_VALUE_AFTER, default="3s"): cv.update_interval,
        }
    )
    .extend(one_wire.one_wire_device_schema())
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)
    await one_wire.register_one_wire_device(var, config)

    cg.add(var.set_reset_value_after(config[CONF_RESET_VALUE_AFTER]))
