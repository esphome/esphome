from esphome.components import binary_sensor, remote_base
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_NAME

DEPENDENCIES = ['remote_receiver']


BASE_SCHEMA = binary_sensor.BINARY_SENSOR_PLATFORM_SCHEMA.extend({}, extra=cv.ALLOW_EXTRA)


PLATFORM_SCHEMA = cv.nameable(cv.All(BASE_SCHEMA, remote_base.validate_binary_sensor(BASE_SCHEMA)))


def to_code(config):
    var = yield remote_base.build_binary_sensor(config)
    cg.add(var.set_name(config[CONF_NAME]))
    yield binary_sensor.register_binary_sensor(var, config)
