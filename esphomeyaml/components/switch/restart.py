import esphomeyaml.config_validation as cv
from esphomeyaml.components import switch
from esphomeyaml.const import CONF_ID, CONF_NAME
from esphomeyaml.helpers import App, Pvariable

PLATFORM_SCHEMA = switch.PLATFORM_SCHEMA.extend({
    cv.GenerateID('restart_switch'): cv.register_variable_id,
}).extend(switch.MQTT_SWITCH_SCHEMA.schema)


def to_code(config):
    rhs = App.make_restart_switch(config[CONF_NAME])
    mqtt = Pvariable('switch_::MQTTSwitchComponent', config[CONF_ID], rhs)
    switch.setup_mqtt_switch(mqtt, config)
