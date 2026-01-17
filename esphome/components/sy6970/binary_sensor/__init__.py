import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, DEVICE_CLASS_CONNECTIVITY, DEVICE_CLASS_POWER

from .. import CONF_SY6970_ID, SY6970Component, sy6970_ns

DEPENDENCIES = ["sy6970"]

SY6970BinarySensor = sy6970_ns.class_(
    "SY6970BinarySensor", binary_sensor.BinarySensor, cg.PollingComponent
)

CONF_VBUS_CONNECTED = "vbus_connected"
CONF_CHARGING = "charging"
CONF_CHARGE_DONE = "charge_done"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SY6970BinarySensor),
        cv.GenerateID(CONF_SY6970_ID): cv.use_id(SY6970Component),
        cv.Optional(CONF_VBUS_CONNECTED): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_CONNECTIVITY,
        ),
        cv.Optional(CONF_CHARGING): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_POWER,
        ),
        cv.Optional(CONF_CHARGE_DONE): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_POWER,
        ),
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_SY6970_ID])
    cg.add(var.set_parent(parent))

    if vbus_connected_config := config.get(CONF_VBUS_CONNECTED):
        sens = await binary_sensor.new_binary_sensor(vbus_connected_config)
        cg.add(var.set_vbus_connected_binary_sensor(sens))

    if charging_config := config.get(CONF_CHARGING):
        sens = await binary_sensor.new_binary_sensor(charging_config)
        cg.add(var.set_charging_binary_sensor(sens))

    if charge_done_config := config.get(CONF_CHARGE_DONE):
        sens = await binary_sensor.new_binary_sensor(charge_done_config)
        cg.add(var.set_charge_done_binary_sensor(sens))
