from esphomeyaml.components import binary_sensor, sonoff_rf_bridge
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_NAME
from esphomeyaml.helpers import get_variable

DEPENDENCIES = ['sonoff_rf_bridge']

PLATFORM_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(sonoff_rf_bridge.CONF_SONOFF_RF_BRIDGE_ID):
        cv.use_variable_id(sonoff_rf_bridge.SonoffRFBridge)
}).extend(sonoff_rf_bridge.FRAME_SCHEMA.schema))


def to_code(config):
    for hub in get_variable(config[sonoff_rf_bridge.CONF_SONOFF_RF_BRIDGE_ID]):
        yield
    rhs = hub.Pmake_binary_sensor(config[CONF_NAME], *sonoff_rf_bridge.get_args(config))
    binary_sensor.register_binary_sensor(rhs, config)
