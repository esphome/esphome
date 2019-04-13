from esphome.components import binary_sensor, rdm6300
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_NAME, CONF_UID
DEPENDENCIES = ['rdm6300']

CONF_RDM6300_ID = 'rdm6300_id'

RDM6300BinarySensor = binary_sensor.binary_sensor_ns.class_('RDM6300BinarySensor',
                                                            binary_sensor.BinarySensor)

PLATFORM_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(RDM6300BinarySensor),
    cv.Required(CONF_UID): cv.uint32_t,
    cv.GenerateID(CONF_RDM6300_ID): cv.use_variable_id(rdm6300.RDM6300Component)
}))


def to_code(config):
    hub = yield get_variable(config[CONF_RDM6300_ID])
    rhs = hub.make_card(config[CONF_NAME], config[CONF_UID])
    binary_sensor.register_binary_sensor(rhs, config)
