import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import CONF_SY6970_ID, SY6970Component, sy6970_ns

DEPENDENCIES = ["sy6970"]

SY6970TextSensor = sy6970_ns.class_(
    "SY6970TextSensor", text_sensor.TextSensor, cg.PollingComponent
)

CONF_BUS_STATUS = "bus_status"
CONF_CHARGE_STATUS = "charge_status"
CONF_NTC_STATUS = "ntc_status"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SY6970TextSensor),
        cv.GenerateID(CONF_SY6970_ID): cv.use_id(SY6970Component),
        cv.Optional(CONF_BUS_STATUS): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_CHARGE_STATUS): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_NTC_STATUS): text_sensor.text_sensor_schema(),
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_SY6970_ID])
    cg.add(var.set_parent(parent))

    if bus_status_config := config.get(CONF_BUS_STATUS):
        sens = await text_sensor.new_text_sensor(bus_status_config)
        cg.add(var.set_bus_status_text_sensor(sens))

    if charge_status_config := config.get(CONF_CHARGE_STATUS):
        sens = await text_sensor.new_text_sensor(charge_status_config)
        cg.add(var.set_charge_status_text_sensor(sens))

    if ntc_status_config := config.get(CONF_NTC_STATUS):
        sens = await text_sensor.new_text_sensor(ntc_status_config)
        cg.add(var.set_ntc_status_text_sensor(sens))
