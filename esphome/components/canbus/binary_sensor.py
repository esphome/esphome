import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID
from . import canbus_ns, CanbusComponent, CONF_CANBUS_ID, CONF_CAN_ID

print("canbus.binary_sensor.py")
DEPENDENCIES = ['canbus']

CanbusBinarySensor = canbus_ns.class_('CanbusBinarySensor', binary_sensor.BinarySensor)

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(CanbusBinarySensor),
    cv.GenerateID(CONF_CANBUS_ID): cv.use_id(CanbusComponent),
    cv.Required(CONF_CAN_ID): cv.int_range(min=0, max=255)
})


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield binary_sensor.register_binary_sensor(var, config)

    hub = yield cg.get_variable(config[CONF_CANBUS_ID])
    cg.add(var.set_can_id(config[CONF_CAN_ID]))
    cg.add(hub.register_can_device(var))
